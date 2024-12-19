#include "absl/flags/flag.h"
#include "absl/flags/parse.h"

#include "hw3/broadcast/common.h"
#include "hw3/broadcast/message.h"
#include "hw3/broadcast/node.h"
#include "hw3/broadcast/node_sender.h"
#include "hw3/broadcast/timeout_builder.h"
#include "hw3/communication.grpc.pb.h"
#include "hw3/communication.pb.h"
#include <absl/flags/internal/flag.h>
#include <cpprest/http_listener.h>
#include <cpprest/http_msg.h>
#include <cpprest/json.h>
#include <future>
#include <grpcpp/client_context.h>
#include <grpcpp/create_channel.h>
#include <grpcpp/security/credentials.h>
#include <grpcpp/server_builder.h>
#include <grpcpp/support/status.h>
#include <mutex>
#include <openssl/rsa.h>
#include <variant>

#include "hw3/log.h"

using namespace web;
using namespace web::http;
using namespace web::http::experimental::listener;

#include <iostream>
#include <map>
#include <string>

class State {
public:
  State(hw3::broadcast::NodeId my_id,
        std::map<hw3::broadcast::NodeId,
                 std::shared_ptr<hw3::broadcast::IMessageSender>>
            channels,
        std::shared_ptr<hw3::broadcast::ITimeoutBuilder> timeout_builder_)
      : node(my_id, std::move(channels), timeout_builder_) {}

  std::map<std::string, std::string> GetDictionary() {
    auto state = node.GetObservableState();
    return state;
  }

  void AddPatch(hw3::broadcast::Payload payload) {
    auto future = node.AppendNewPayload(payload);
    future.get();
  }

  void Tick() { node.Tick(); }

  hw3::broadcast::Node *GetNodePtr() { return &node; }

private:
  hw3::broadcast::Node node;
};

std::shared_ptr<State> state;

class CommunicationImpl : public communication::Receiver::Service {
public:
  CommunicationImpl(std::shared_ptr<hw3::broadcast::IMessageSender> sender)
      : sender_(sender) {}

  grpc::Status SendRequest(grpc::ServerContext *context,
                           const communication::Request *request,
                           communication::TrivialResponse *response) override {
    LOG("Handling SendRequest from " + context->peer() +
        " (value = " + request->ShortDebugString() + ")");

    const auto author = request->author();
    const auto sender = request->sender();
    hw3::broadcast::Payload payload;
    for (auto elem : request->payload()) {
      auto key = elem.key();
      auto value = elem.value();
      payload.data.emplace_back(key, value);
    }
    hw3::broadcast::VectorClock clock;
    for (auto elem : request->vector_clock()) {
      clock.emplace_back(elem);
    }
    std::set<hw3::broadcast::NodeId> already_received_nodes;
    for (auto elem : request->already_received_nodes()) {
      already_received_nodes.insert(elem);
    }

    hw3::broadcast::Request req;
    req.sender = sender;
    req.author_id = author;
    req.payload = std::move(payload);
    req.vector_clock = std::move(clock);
    req.already_received_nodes = std::move(already_received_nodes);

    sender_->Send(req);

    return grpc::Status::OK;
  }

  grpc::Status SendResponse(grpc::ServerContext *context,
                            const communication::Response *request,
                            communication::TrivialResponse *response) override {
    LOG("Handling SendResponse from " + context->peer() +
        " (value = " + request->ShortDebugString() + ")");

    const uint32_t author = request->author();
    const uint32_t seqno = request->seqno_at_author();
    hw3::broadcast::MessageId id{.author_id = author, .on_author_id = seqno};

    std::set<hw3::broadcast::NodeId> received_nodes;
    for (auto elem : request->received_nodes()) {
      received_nodes.insert(elem);
    }

    hw3::broadcast::Response resp;
    resp.message_id = id;
    resp.received_nodes = std::move(received_nodes);

    sender_->Send(resp);

    return grpc::Status::OK;
  }

private:
  std::shared_ptr<hw3::broadcast::IMessageSender> sender_;
};

class NetworkMessageSender : public hw3::broadcast::IMessageSender {
public:
  NetworkMessageSender(const std::string addr) : addr_(addr) {}

  void Send(const hw3::broadcast::Message &message) override {
    std::lock_guard lg(lock_);
    auto task = [addr = addr_, msg = message]() -> void {
      LOG("Sending message to " + addr);
      auto channel =
          grpc::CreateChannel(addr, grpc::InsecureChannelCredentials());
      auto ci = communication::Receiver::NewStub(channel);

      grpc::ClientContext ctx;
      if (std::holds_alternative<hw3::broadcast::Request>(msg)) {
        const auto &request = std::get<hw3::broadcast::Request>(msg);
        communication::Request req;
        req.set_sender(request.sender);
        req.set_author(request.author_id);
        for (const auto &[k, v] : request.payload.data) {
          auto item = req.add_payload();
          item->set_key(k);
          item->set_value(v);
        }
        for (const auto id : request.vector_clock) {
          req.add_vector_clock(id);
        }
        for (const auto id : request.already_received_nodes) {
          req.add_already_received_nodes(id);
        }

        communication::TrivialResponse response;
        auto res = ci->SendRequest(&ctx, req, &response);
        if (!res.ok()) {
          std::stringstream ss;
          ss << res.error_code() << ' ' << res.error_details() << ' '
             << res.error_message();
          LOG(ss.str());
        } else {
          LOG("Message sended successfully to " + addr + " (" +
              req.ShortDebugString() + ")");
        }
      } else {
        const auto &response = std::get<hw3::broadcast::Response>(msg);
        communication::Response resp;
        resp.set_author(response.message_id.author_id);
        resp.set_seqno_at_author(response.message_id.on_author_id);

        for (auto id : response.received_nodes) {
          resp.add_received_nodes(id);
        }

        communication::TrivialResponse result;
        auto res = ci->SendResponse(&ctx, resp, &result);
        if (!res.ok()) {
          std::stringstream ss;
          ss << res.error_code() << ' ' << res.error_details() << ' '
             << res.error_message();
          LOG(ss.str());
        } else {
          LOG("Message sended successfully to " + addr + " (" +
              resp.ShortDebugString() + ")");
        }
      }
    };

    tasks_.emplace_back(std::async(std::move(task)));
  }

private:
  const std::string addr_;
  std::mutex lock_;
  std::deque<std::future<void>> tasks_;
};

void handle_get(http_request request) {
  LOG("\nhandle GET\n");

  auto answer = json::value::object();
  auto dict = state->GetDictionary();

  for (auto const &p : dict) {
    answer[p.first] = json::value::string(p.second);
  }

  request.reply(status_codes::OK, answer);
}

void handle_request(
    http_request request,
    std::function<void(json::value const &, json::value &)> action) {
  auto answer = json::value::object();

  request.extract_json()
      .then([&answer, &action](pplx::task<json::value> task) {
        try {
          auto const &jvalue = task.get();

          if (!jvalue.is_null()) {
            action(jvalue, answer);
          }
        } catch (http_exception const &e) {
          std::cout << e.what() << std::endl;
        }
      })
      .wait();

  request.reply(status_codes::OK, answer);
}

void handle_patch(http_request request) {
  LOG("\nhandle PATCH\n");

  handle_request(request, [](json::value const &jvalue, json::value &answer) {
    if (!jvalue.is_object()) {
      throw http_exception(422, "Object is expected");
    }
    hw3::broadcast::Payload payload;
    for (auto const &e : jvalue.as_object()) {
      const auto &key = e.first;
      const auto &value = e.second;
      if (!value.is_string()) {
        throw http_exception(422, "Value for " + std::string(key) +
                                      " is not string");
      }
      payload.data.emplace_back(key, value.as_string());
    }

    state->AddPatch(std::move(payload));
  });
}

ABSL_FLAG(std::string, host, "0.0.0.0", "");
ABSL_FLAG(uint16_t, port, 0, "");
ABSL_FLAG(uint16_t, min_port, 0, "");
ABSL_FLAG(uint16_t, max_port, 0, "");

int main(int argc, char **argv) {
  absl::ParseCommandLine(argc, argv);

  const std::string host = absl::GetFlag(FLAGS_host);
  const auto port = absl::GetFlag(FLAGS_port);

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

  std::map<hw3::broadcast::NodeId,
           std::shared_ptr<hw3::broadcast::IMessageSender>>
      channels_;

  for (uint16_t other_port = min_port; other_port <= max_port; ++other_port) {
    if (port == other_port) {
      continue;
    }
    const std::string other_addr = host + ":" + std::to_string(other_port + 10);
    channels_[other_port - min_port] =
        std::make_shared<NetworkMessageSender>(other_addr);
  }

  std::shared_ptr<hw3::broadcast::ITimeoutBuilder> timeout_builder_ =
      std::make_shared<hw3::broadcast::AttemptsTimerBuilder>(10);

  state = std::make_shared<State>(port - min_port, channels_, timeout_builder_);

  using namespace std::chrono_literals;
  std::atomic<bool> broadcast_stopped = false;
  auto spin_broadcast = [&]() {
    while (!broadcast_stopped.load()) {
      state->Tick();
      std::this_thread::sleep_for(1s);
    }
  };
  std::thread run_consensus(spin_broadcast);

  auto sender_to_myself =
      std::make_shared<hw3::broadcast::TrivialMessageSender>();

  sender_to_myself->Init(state->GetNodePtr());

  std::unique_ptr<CommunicationImpl> communication_service =
      std::make_unique<CommunicationImpl>(sender_to_myself);

  std::unique_ptr<grpc::Server> communication_server = [&]() {
    grpc::ServerBuilder builder;
    builder.AddListeningPort(host + ":" + std::to_string(port + 10),
                             grpc::InsecureServerCredentials());
    builder.RegisterService(communication_service.get());
    return std::unique_ptr<grpc::Server>(builder.BuildAndStart());
  }();

  auto spin_communication_server = [&]() { communication_server->Wait(); };
  std::thread run_communication_server(spin_communication_server);

  http_listener listener("http://localhost:" + std::to_string(port));

  listener.support(methods::GET, handle_get);
  listener.support(methods::PATCH, handle_patch);

  try {
    listener.open()
        .then([&listener]() { LOG("\nstarting to listen\n"); })
        .wait();

    while (true) {
      std::string action;
      std::cin >> action;
      if (action == "dump") {
        std::stringstream result;
        auto dict = state->GetDictionary();
        result << "{\n";
        for (const auto &[k, v] : dict) {
          result << "  " << k << ": " << v << "\n";
        }
        result << "}";
        std::cerr << result.str() << std::endl;
      } else if (action == "stop") {
      }
    }
  } catch (std::exception const &e) {
    std::cout << e.what() << std::endl;
  }

  return 0;
}