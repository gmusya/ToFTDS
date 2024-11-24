#include "hw1/src/leader/leader.h"
#include "hw1/src/common/ensure.h"

#include <asm-generic/socket.h>
#include <iostream>
#include <netinet/in.h>
#include <sys/socket.h>

namespace integral {

Leader::Leader(const uint16_t discovery_port)
    : discovery_socket_(socket(AF_INET, SOCK_DGRAM, 0)) {
  {
    ENSURE(discovery_socket_ != -1);
    constexpr int kBroadcastOptVal = 1;
    HANDLE_C_ERROR(setsockopt(discovery_socket_, SOL_SOCKET, SO_BROADCAST,
                              &kBroadcastOptVal, sizeof(kBroadcastOptVal)));
  }
  {
    sockaddr_in address{};
    address.sin_family = AF_INET;
    address.sin_port = htons(discovery_port);
    address.sin_addr.s_addr = INADDR_BROADCAST;
    HANDLE_C_ERROR(sendto(discovery_socket_, "qwe", 3, 0,
                          reinterpret_cast<const sockaddr *>(&address),
                          sizeof(address)));
  }
}

double Leader::GetResult(const double a, const double b) { return b - a; }

} // namespace integral
