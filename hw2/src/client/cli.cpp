#include "absl/flags/flag.h"
#include "absl/flags/parse.h"
#include "hw2/src/common/query.h"
#include "hw2/src/kv_store/kv_store.grpc.pb.h"
#include "hw2/src/kv_store/kv_store.pb.h"

#include <grpcpp/grpcpp.h>
#include <grpcpp/server_builder.h>
#include <grpcpp/support/status.h>
#include <optional>
#include <string>

namespace hw2 {} // namespace hw2

ABSL_FLAG(std::string, host, "0.0.0.0", "");
ABSL_FLAG(uint16_t, port, 0, "");
ABSL_FLAG(hw2::Key, key, "", "");
ABSL_FLAG(hw2::Key, value, "", "");
ABSL_FLAG(std::string, operation, "", "");

int main(int argc, char **argv) {
  absl::ParseCommandLine(argc, argv);

  const std::string host = absl::GetFlag(FLAGS_host);
  const auto port = absl::GetFlag(FLAGS_port);

  const hw2::Key key = absl::GetFlag(FLAGS_key);
  const hw2::Value value = absl::GetFlag(FLAGS_value);

  if (port == 0) {
    std::cerr << "port is not set" << std::endl;
    return 1;
  }

  const std::string addr = host + ":" + std::to_string(port);

  const auto operation = absl::GetFlag(FLAGS_operation);

  auto channel = grpc::CreateChannel(addr, grpc::InsecureChannelCredentials());
  auto ci = kv_store::KeyValueStore::NewStub(channel);

  if (operation == "read") {
    if (key == "") {
      std::cerr << "Key is not set" << std::endl;
      return 1;
    }
    grpc::ClientContext ctx;
    kv_store::ReadRequest request;
    kv_store::ReadResponse response;

    *request.mutable_key() = key;
    auto res = ci->Read(&ctx, request, &response);
    if (!res.ok()) {
      std::cerr << "Error: " << res.error_code() << ' ' << res.error_details()
                << ' ' << res.error_message() << std::endl;
    } else {
      std::cout << response.DebugString() << std::endl;
    }
  } else if (operation == "write") {
    if (key == "") {
      std::cerr << "Key is not set" << std::endl;
      return 1;
    }
    if (value == "") {
      std::cerr << "Value is not set" << std::endl;
      return 1;
    }
    grpc::ClientContext ctx;
    kv_store::WriteRequest request;
    kv_store::WriteResponse response;

    *request.mutable_key() = key;
    *request.mutable_value() = value;
    auto res = ci->Write(&ctx, request, &response);
    if (!res.ok()) {
      std::cerr << "Error: " << res.error_code() << ' ' << res.error_details()
                << ' ' << res.error_message() << std::endl;
    } else {
      std::cout << response.DebugString() << std::endl;
    }
  } else {
    std::cerr << "Unknown operation: " << operation << std::endl;
    return 1;
  }
  return 0;
}
