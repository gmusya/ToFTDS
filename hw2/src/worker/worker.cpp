#include "hw2/src/common/timeout.h"
#include "hw2/src/consensus/common.h"
#include "hw2/src/consensus/node.h"
#include "hw2/src/worker/kv_store.grpc.pb.h"

#include "absl/flags/flag.h"
#include "absl/flags/parse.h"
#include "hw2/src/common/query.h"
#include "hw2/src/common/storage.h"
#include <chrono>
#include <grpcpp/server_builder.h>
#include <grpcpp/support/status.h>
#include <optional>
#include <string>
#include <thread>

namespace hw2 {
class KeyValueStoreImpl : public kv_store::KeyValueStore::Service {
public:
  KeyValueStoreImpl(
      consensus::NodeId id,
      std::map<consensus::NodeId, std::shared_ptr<consensus::IMessageSender>>
          channels,
      std::shared_ptr<ITimeout> election_timeout)
      : consensus_node_(id, std::move(channels), std::move(election_timeout)) {}

  grpc::Status Read(grpc::ServerContext *context,
                    const kv_store::ReadRequest *request,
                    kv_store::ReadResponse *response) override {
    lock_.lock();
    auto maybe_result = storage_.Get(request->key());
    response->set_value(maybe_result.value_or(std::string("")));
    return grpc::Status::OK;
  }

  grpc::Status Write(grpc::ServerContext *context,
                     const kv_store::WriteRequest *request,
                     kv_store::WriteResponse *response) override {
    lock_.lock();
    auto maybe_result = storage_.Get(request->key());
    response->set_previous_value(maybe_result.value_or(std::string("")));
    storage_.Set(request->key(), request->value());
    return grpc::Status::OK;
  }

private:
  consensus::Node consensus_node_;

  std::mutex lock_;
  Storage<Key, Value> storage_;
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

  using namespace std::chrono_literals;
  std::unique_ptr<hw2::KeyValueStoreImpl> service =
      std::make_unique<hw2::KeyValueStoreImpl>(
          port, std::move(channels_),
          std::make_shared<hw2::UniformTimer>(200ms, 400ms, port));

  grpc::ServerBuilder builder;
  builder.AddListeningPort(addr, grpc::InsecureServerCredentials());
  builder.RegisterService(service.get());
  std::unique_ptr<grpc::Server> server(builder.BuildAndStart());

  auto spin_server = [&]() { server->Wait(); };

  std::thread run_server(spin_server);

  std::string x;
  std::cin >> x;
  server->Shutdown();
  run_server.join();
  return 0;
}
