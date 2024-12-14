#include "hw2/src/common/timeout.h"
#include "hw2/src/consensus/common.h"
#include "hw2/src/consensus/message.h"
#include "hw2/src/consensus/node.h"
#include "hw2/src/consensus/node_sender.h"
#include "hw2/src/consensus/state.h"
#include "hw2/src/kv_store/kv_store.grpc.pb.h"
#include "hw2/src/worker/communication.grpc.pb.h"
#include "hw2/src/worker/communication.pb.h"

#include "absl/flags/flag.h"
#include "absl/flags/parse.h"
#include "grpcpp/grpcpp.h"
#include "hw2/src/common/log.h"
#include "hw2/src/common/query.h"
#include "hw2/src/common/storage.h"

#include <chrono>
#include <future>
#include <grpcpp/server_builder.h>
#include <grpcpp/support/status.h>
#include <memory>
#include <mutex>
#include <optional>
#include <sstream>
#include <string>
#include <thread>

namespace hw2 {
class KeyValueStoreImpl : public kv_store::KeyValueStore::Service {
public:
  void ActualizeState() {
    std::lock_guard lg(lock_);
    auto commands =
        consensus_node_
            ->GetPersistentCommands(last_persisted_id_ + 1, 1'000'000'00)
            .get();
    for (const auto &command : commands) {
      LOG("Handling command " + command +
          ", id = " + std::to_string(last_persisted_id_));
      ++last_persisted_id_;
      std::stringstream c(command);
      std::string key, value;
      c >> key >> value;
      if (key == "" || value == "") {
        LOG("Strange command: " + command);
      }
      storage_.Set(key, value);
    }
  }

  KeyValueStoreImpl(std::shared_ptr<consensus::Node> consensus_node)
      : consensus_node_(consensus_node) {}

  grpc::Status Read(grpc::ServerContext *context,
                    const kv_store::ReadRequest *request,
                    kv_store::ReadResponse *response) override {
    LOG("Handling read query from " + context->peer() +
        " (key = " + request->key() + ")");
    ActualizeState();
    std::lock_guard lg(lock_);
    auto maybe_result = storage_.Get(request->key());
    response->set_value(maybe_result.value_or(std::string("")));
    return grpc::Status::OK;
  }

  grpc::Status Write(grpc::ServerContext *context,
                     const kv_store::WriteRequest *request,
                     kv_store::WriteResponse *response) override {
    LOG("Handling write query from " + context->peer() +
        " (key = " + request->key() + ", value = " + request->value() + ")");
    ActualizeState();
    auto future_result =
        consensus_node_->AddCommand(request->key() + " " + request->value());
    auto result = std::move(future_result).get();
    if (result.CommandPersisted()) {
      response->set_value_persisted(true);
    } else {
      response->set_master_id(result.PotentialLeader());
    }
    return grpc::Status::OK;
  }

  void DumpState() {
    ActualizeState();
    std::lock_guard lg(lock_);
    std::cerr << storage_.ToString() << std::endl;
  }

private:
  consensus::LogItemId last_persisted_id_ = 0;
  std::shared_ptr<consensus::Node> consensus_node_;

  std::mutex lock_;
  Storage<Key, Value> storage_;
};

class CommunicationImpl : public communication::Receiver::Service {
public:
  CommunicationImpl(std::shared_ptr<hw2::consensus::IMessageSender> sender)
      : sender_(sender) {}

  grpc::Status
  SendAnnotatedMessage(grpc::ServerContext *context,
                       const communication::AnnotatedMessage *request,
                       communication::Response *response) override {
    LOG("Handling SendAnnotatedMessage from " + context->peer() +
        " (value = " + request->ShortDebugString() + ")");
    if (!request->has_message()) {
      response->set_comment("!has_message()");
      return grpc::Status::CANCELLED;
    }
    int32_t from = request->sender_id();
    auto message = request->message();
    if (message.has_ae_req()) {
      auto proto_req = message.ae_req();
      consensus::AppendEntriesRequest request;
      request.leader_term = proto_req.leader_term();
      request.leader_id = proto_req.leader_id();
      request.prev_log_index = proto_req.prev_log_index();
      request.prev_log_term = proto_req.prev_log_term();
      request.leader_term = proto_req.leader_term();
      for (auto item : proto_req.item()) {
        request.entries.emplace_back(consensus::LogItem{
            .command = item.command(),
            .leader_term = static_cast<uint32_t>(item.leader_term())});
      }
      sender_->Send(from, request);
    } else if (message.has_ae_resp()) {
      auto proto_resp = message.ae_resp();
      consensus::AppendEntriesResponse response;
      response.current_term = proto_resp.current_term();
      response.success = proto_resp.success();
      if (response.success) {
        response.matched_until = proto_resp.matched_until();
      }
      sender_->Send(from, response);
    } else if (message.has_rv_req()) {
      auto proto_req = message.rv_req();
      consensus::RequestVoteRequest request;
      request.candidate_term = proto_req.candidate_term();
      request.candidate_id = proto_req.candidate_id();
      request.candidate_last_log_index = proto_req.candidate_last_log_index();
      request.candidate_last_log_term = proto_req.candidate_last_log_term();
      sender_->Send(from, request);
    } else if (message.has_rv_resp()) {
      auto proto_resp = message.rv_resp();
      consensus::RequestVoteResponse response;
      response.current_term = proto_resp.current_term();
      response.vote_granted = proto_resp.vote_granted();
      sender_->Send(from, response);
    } else {
      response->set_comment("Empty message");
      return grpc::Status::CANCELLED;
    }
    return grpc::Status::OK;
  }

private:
  std::shared_ptr<hw2::consensus::IMessageSender> sender_;
};

class OtherNodesMessageSender : public consensus::IMessageSender {
public:
  OtherNodesMessageSender(const std::string addr) : addr_(addr) {}

  void Send(consensus::NodeId from,
            const consensus::Message &message) override {
    std::lock_guard lg(lock_);
    auto task = [addr = addr_, msg = message, from]() -> void {
      LOG("Sending message to " + addr);
      auto channel =
          grpc::CreateChannel(addr, grpc::InsecureChannelCredentials());
      auto ci = communication::Receiver::NewStub(channel);

      grpc::ClientContext ctx;
      communication::AnnotatedMessage request;

      auto proto_msg = request.mutable_message();
      std::visit(
          [&](auto &&arg) {
            using ArgType = std::decay_t<decltype(arg)>;

            if constexpr (std::is_same_v<ArgType,
                                         consensus::AppendEntriesRequest>) {
              auto ae_req = proto_msg->mutable_ae_req();
              ae_req->set_leader_term(arg.leader_term);
              ae_req->set_leader_id(arg.leader_id);
              ae_req->set_prev_log_index(arg.prev_log_index);
              ae_req->set_prev_log_term(arg.prev_log_term);
              for (const auto &elem : arg.entries) {
                auto item = ae_req->add_item();
                item->set_command(elem.command);
                item->set_leader_term(elem.leader_term);
              }
              ae_req->set_leader_commit(arg.leader_commit);
            } else if constexpr (std::is_same_v<
                                     ArgType, consensus::RequestVoteRequest>) {
              auto rv_req = proto_msg->mutable_rv_req();
              rv_req->set_candidate_term(arg.candidate_term);
              rv_req->set_candidate_id(arg.candidate_id);
              rv_req->set_candidate_last_log_index(
                  arg.candidate_last_log_index);
              rv_req->set_candidate_last_log_term(arg.candidate_last_log_term);
            } else if constexpr (std::is_same_v<
                                     ArgType,
                                     consensus::AppendEntriesResponse>) {
              auto ae_resp = proto_msg->mutable_ae_resp();
              ae_resp->set_current_term(arg.current_term);
              ae_resp->set_success(arg.success);
              if (arg.success) {
                ae_resp->set_matched_until(arg.matched_until.value());
              }
            } else if constexpr (std::is_same_v<
                                     ArgType, consensus::RequestVoteResponse>) {
              auto rv_resp = proto_msg->mutable_rv_resp();
              rv_resp->set_current_term(arg.current_term);
              rv_resp->set_vote_granted(arg.vote_granted);
            } else {
              throw std::runtime_error(
                  MESSAGE_WITH_FILE_LINE("Internal error"));
            }
          },
          msg);

      request.set_sender_id(from);

      communication::Response response;

      auto res = ci->SendAnnotatedMessage(&ctx, request, &response);
      if (!res.ok()) {
        std::stringstream ss;
        ss << res.error_code() << ' ' << res.error_details() << ' '
           << res.error_message();
        LOG(ss.str());
      } else {
        LOG("Message sended successfully to " + addr + " (" +
            request.ShortDebugString() + ")");
      }
    };

    tasks_.emplace_back(std::async(std::move(task)));
  }

private:
  const std::string addr_;
  std::mutex lock_;
  std::deque<std::future<void>> tasks_;
};
} // namespace hw2

ABSL_FLAG(std::string, host, "0.0.0.0", "");
ABSL_FLAG(uint16_t, my_port, 0, "");
ABSL_FLAG(uint16_t, min_port, 0, "");
ABSL_FLAG(uint16_t, max_port, 0, "");

int main(int argc, char **argv) {
  absl::ParseCommandLine(argc, argv);

  const std::string host = absl::GetFlag(FLAGS_host);
  const auto port = absl::GetFlag(FLAGS_my_port);

  const auto min_port = absl::GetFlag(FLAGS_min_port);
  const auto max_port = absl::GetFlag(FLAGS_max_port);

  if (port == 0 || max_port == 0 || min_port == 0) {
    std::cerr << "port is not set" << std::endl;
    return 1;
  }

  if (!(min_port <= port && port <= max_port)) {
    std::cerr
        << "condition !(min_port <= port && port <= max_port) is not satisfied"
        << std::endl;
    return 1;
  }

  const auto nodes_count = max_port - min_port + 1;
  if (!(nodes_count == 1 || nodes_count == 3 || nodes_count == 5)) {
    std::cerr << "Unexpected number of nodes (" << nodes_count << ")"
              << std::endl;
    return 1;
  }

  const std::string addr = host + ":" + std::to_string(port);

  std::map<hw2::consensus::NodeId,
           std::shared_ptr<hw2::consensus::IMessageSender>>
      channels_;

  for (uint16_t other_port = min_port; other_port <= max_port; ++other_port) {
    if (port == other_port) {
      continue;
    }
    const std::string other_addr = host + ":" + std::to_string(other_port + 10);
    channels_[other_port] =
        std::make_shared<hw2::OtherNodesMessageSender>(other_addr);
  }

  using namespace std::chrono_literals;
  auto consensus_node = std::make_shared<hw2::consensus::Node>(
      port, channels_, std::make_shared<hw2::UniformTimer>(7s, 15s, port));

  auto sender_to_myself = consensus_node->GetSenderToItself();

  std::unique_ptr<hw2::KeyValueStoreImpl> kv_service =
      std::make_unique<hw2::KeyValueStoreImpl>(consensus_node);

  std::unique_ptr<hw2::CommunicationImpl> communication_service =
      std::make_unique<hw2::CommunicationImpl>(sender_to_myself);

  std::unique_ptr<grpc::Server> kv_server = [&]() {
    grpc::ServerBuilder builder;
    builder.AddListeningPort(addr, grpc::InsecureServerCredentials());
    builder.RegisterService(kv_service.get());
    return std::unique_ptr<grpc::Server>(builder.BuildAndStart());
  }();

  std::unique_ptr<grpc::Server> communication_server = [&]() {
    grpc::ServerBuilder builder;
    builder.AddListeningPort(host + ":" + std::to_string(port + 10),
                             grpc::InsecureServerCredentials());
    builder.RegisterService(communication_service.get());
    return std::unique_ptr<grpc::Server>(builder.BuildAndStart());
  }();

  std::atomic<bool> consensus_stopped_ = false;
  auto spin_consensus = [&]() {
    while (!consensus_stopped_.load()) {
      consensus_node->Tick();
      std::this_thread::sleep_for(1s);
    }
  };
  std::thread run_consensus(spin_consensus);

  auto spin_kv_server = [&]() { kv_server->Wait(); };
  std::thread run_kv_server(spin_kv_server);

  auto spin_communication_server = [&]() { communication_server->Wait(); };
  std::thread run_communication_server(spin_communication_server);

  while (true) {
    std::string command;
    std::cin >> command;
    if (command == "dump") {
      kv_service->DumpState();
    } else if (command == "stop") {
      communication_server->Shutdown();
      run_communication_server.join();

      kv_server->Shutdown();
      run_kv_server.join();

      consensus_stopped_.store(true);
      run_consensus.join();
      break;
    }
  }
  return 0;
}
