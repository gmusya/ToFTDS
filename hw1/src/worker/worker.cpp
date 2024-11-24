#include "hw1/src/worker/worker.h"
#include "hw1/src/common/ensure.h"

#include <asm-generic/socket.h>
#include <iostream>
#include <netinet/in.h>
#include <sys/socket.h>
#include <thread>

namespace integral {

Worker::Worker(const uint16_t discovery_port, const uint16_t workload_port)
    : discovery_socket_(socket(AF_INET, SOCK_DGRAM, 0)),
      workload_socket_(socket(AF_INET, SOCK_STREAM, 0)),
      workload_port_(workload_port) {
  // TODO: fix leakage on errors
  {
    ENSURE(discovery_socket_ != -1);

    constexpr int kReuseOptVal = 1;
    HANDLE_C_ERROR(setsockopt(discovery_socket_, SOL_SOCKET, SO_REUSEADDR,
                              &kReuseOptVal, sizeof(kReuseOptVal)));

    sockaddr_in address{};
    address.sin_family = AF_INET;
    address.sin_port = htons(discovery_port);
    address.sin_addr.s_addr = INADDR_ANY;
    HANDLE_C_ERROR(bind(discovery_socket_,
                        reinterpret_cast<const sockaddr *>(&address),
                        sizeof(address)));
  }
  {
    ENSURE(workload_socket_ != -1);
    sockaddr_in address{};
    address.sin_family = AF_INET;
    address.sin_port = htons(workload_port);
    address.sin_addr.s_addr = INADDR_ANY;
    HANDLE_C_ERROR(bind(workload_socket_,
                        reinterpret_cast<const sockaddr *>(&address),
                        sizeof(address)));
    constexpr int32_t kMaxQueueSize = 3;
    HANDLE_C_ERROR(listen(workload_socket_, kMaxQueueSize));
  }
}

void Worker::Run() { threads_.emplace_back(&Worker::WaitForDiscovery, this); }

void Worker::WaitForDiscovery() {
  constexpr int32_t kMaxMessageSize = 100;
  char message[kMaxMessageSize];
  while (true) {
    struct sockaddr_storage their_addr;
    socklen_t addr_len = sizeof(their_addr);

    std::cerr << "Waiting for discovery..." << std::endl;
    HANDLE_C_ERROR(recvfrom(discovery_socket_, message, kMaxMessageSize, 0,
                            reinterpret_cast<sockaddr *>(&their_addr),
                            &addr_len));
    std::cerr << "I am discovered by "
              << reinterpret_cast<sockaddr_in *>(&their_addr)->sin_addr.s_addr
              << std::endl;

    std::string message_to_send = std::to_string(workload_port_);

    HANDLE_C_ERROR(sendto(discovery_socket_, message_to_send.data(),
                          message_to_send.size(), 0,
                          reinterpret_cast<sockaddr *>(&their_addr), addr_len));
  }
}

void Worker::HandleReceivedTask(const Task &) {}

void Worker::AcceptIncomingConnection() {
  std::cerr << "Waiting for incomming connection..." << std::endl;

  struct sockaddr_storage their_addr;
  socklen_t addr_len;
  HANDLE_C_ERROR(accept(workload_socket_,
                        reinterpret_cast<sockaddr *>(&their_addr), &addr_len));

  std::cerr << "Accepted incomming connection from "
            << reinterpret_cast<sockaddr_in *>(&their_addr)->sin_addr.s_addr
            << std::endl;
}

} // namespace integral