#include "hw1/src/worker/worker.h"

#include "absl/flags/flag.h"
#include "absl/flags/parse.h"
#include "hw1/src/worker/worker.h"

ABSL_FLAG(uint16_t, discovery_port, 0, "");
ABSL_FLAG(uint16_t, worker_port, 0, "");

int main(int argc, char **argv) {
  absl::ParseCommandLine(argc, argv);

  const auto discovery_port = absl::GetFlag(FLAGS_discovery_port);
  const auto worker_port = absl::GetFlag(FLAGS_worker_port);

  if (discovery_port == 0) {
    std::cerr << "discovery port is not set" << std::endl;
    return 1;
  }

  if (worker_port == 0) {
    std::cerr << "worker port is not set" << std::endl;
    return 1;
  }

  integral::Worker worker(discovery_port, worker_port);
  worker.Run();

  return 0;
}
