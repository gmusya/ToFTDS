#include "hw1/src/leader/leader.h"

#include "absl/flags/flag.h"
#include "absl/flags/parse.h"

ABSL_FLAG(uint16_t, discovery_port, 0, "");

int main(int argc, char **argv) {
  absl::ParseCommandLine(argc, argv);

  const auto discovery_port = absl::GetFlag(FLAGS_discovery_port);

  if (discovery_port == 0) {
    std::cerr << "discovery port is not set" << std::endl;
    return 1;
  }

  integral::Leader leader(discovery_port);
  leader.Run();

  double a, b;
  std::cin >> a >> b;

  std::cout << leader.GetResult(a, b) << std::endl;
  return 0;
}
