#include "hw1/src/leader/leader.h"
#include "hw1/src/common/ensure.h"

#include <asm-generic/socket.h>
#include <cerrno>
#include <chrono>
#include <iostream>
#include <netinet/in.h>
#include <sys/socket.h>
#include <thread>

namespace integral {

Leader::Leader(const uint16_t discovery_port)
    : discovery_port_(discovery_port),
      discovery_socket_(socket(AF_INET, SOCK_DGRAM, 0)) {
  {
    ENSURE(discovery_socket_ != -1);
    constexpr int kBroadcastOptVal = 1;
    HANDLE_C_ERROR(setsockopt(discovery_socket_, SOL_SOCKET, SO_BROADCAST,
                              &kBroadcastOptVal, sizeof(kBroadcastOptVal)));
  }
}

void Leader::Run() {
  threads_.emplace_back(&Leader::SendHeartbeats, this);
  threads_.emplace_back(&Leader::ReceiveHeartbeats, this);
}

void Leader::SendHeartbeats() {
  while (true) {
    sockaddr_in address{};
    address.sin_family = AF_INET;
    address.sin_port = htons(discovery_port_);
    address.sin_addr.s_addr = INADDR_BROADCAST;
    HANDLE_C_ERROR(sendto(
        discovery_socket_, kLeaderMessage.data(), kLeaderMessage.size(), 0,
        reinterpret_cast<const sockaddr *>(&address), sizeof(address)));

    std::this_thread::sleep_for(std::chrono::seconds(2));
  }
}

void Leader::ReceiveHeartbeats() {
  constexpr int32_t kMaxMessageSize = 100;
  char message[kMaxMessageSize];

  while (true) {
    std::cerr << "Waiting for discovery..." << std::endl;

    while (true) {
      struct sockaddr_storage their_addr;
      socklen_t addr_len = sizeof(their_addr);

      auto result =
          recvfrom(discovery_socket_, message, kMaxMessageSize, MSG_DONTWAIT,
                   reinterpret_cast<sockaddr *>(&their_addr), &addr_len);
      if (result == -1) {
        break;
      }

      auto data = std::string_view(message, result);
      if (data == kLeaderMessage) {
        continue;
      }

      std::cerr << "I am discovered by " << data << std::endl;
    }
    if (errno != EAGAIN) {
      HANDLE_C_ERROR(errno);
    }

    std::this_thread::sleep_for(std::chrono::seconds(2));
  }
}

double Leader::GetResult(const double a, const double b) { return b - a; }

} // namespace integral
