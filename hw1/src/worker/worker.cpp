#include "hw1/src/worker/worker.h"
#include "hw1/src/common/ensure.h"
#include "hw1/src/socket/socket.h"

#include <asm-generic/socket.h>
#include <chrono>
#include <iostream>
#include <netinet/in.h>
#include <sys/socket.h>
#include <thread>

namespace integral {

Worker::Worker(const uint16_t discovery_port, const uint16_t workload_port)
    : discovery_socket_(socket(AF_INET, SOCK_DGRAM, 0)),
      workload_socket_(socket(AF_INET, SOCK_STREAM | SOCK_NONBLOCK, 0)),
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

Worker::~Worker() { Stop(); }

void Worker::Stop() {
  if (is_stopped_.load()) {
    return;
  }
  is_stopped_.store(true);
  threads_.clear();
}

void Worker::Run() {
  threads_.emplace_back(&Worker::WaitForDiscovery, this);
  threads_.emplace_back(&Worker::AcceptIncomingConnection, this);
}

void Worker::WaitForDiscovery() {
  while (!is_stopped_.load()) {
    std::cerr << "Waiting for discovery..." << std::endl;

    OnReceiveAsync(discovery_socket_, [workload_port = this->workload_port_](
                                          int socket, sockaddr_storage addr,
                                          socklen_t addr_len,
                                          std::string_view) {
      std::cerr << "I am discovered" << std::endl;
      std::string data = std::to_string(workload_port);
      HANDLE_C_ERROR(sendto(socket, data.data(), data.size(), 0,
                            reinterpret_cast<sockaddr *>(&addr), addr_len));
    });

    std::this_thread::sleep_for(std::chrono::milliseconds(2000));
  }
}

void Worker::HandleReceivedTask(const Task &) {}

void Worker::AcceptIncomingConnection() {
  while (!is_stopped_.load()) {
    std::cerr << "Waiting for incomming connection..." << std::endl;

    while (true) {
      struct sockaddr_storage their_addr;
      socklen_t addr_len;

      auto fd = accept(workload_socket_,
                       reinterpret_cast<sockaddr *>(&their_addr), &addr_len);
      if (fd == -1) {
        if (errno == EAGAIN) {
          break;
        }
        HANDLE_C_ERROR(fd);
      }
    }

    std::this_thread::sleep_for(std::chrono::milliseconds(2000));
  }
}

} // namespace integral