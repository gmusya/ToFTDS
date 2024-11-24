#include "hw1/src/socket/socket.h"

#include "hw1/src/common/ensure.h"

namespace integral {

void OnReceiveAsync(int socket,
                    const std::function<void(int, sockaddr_storage, socklen_t,
                                             std::string_view)> &handler) {
  constexpr int32_t kMaxMessageSize = 100;
  char message[kMaxMessageSize];

  struct sockaddr_storage their_addr;
  socklen_t addr_len = sizeof(their_addr);

  while (true) {
    auto bytes_received =
        recvfrom(socket, message, kMaxMessageSize, MSG_DONTWAIT,
                 reinterpret_cast<sockaddr *>(&their_addr), &addr_len);
    if (bytes_received == -1) {
      if (errno == EAGAIN) {
        return;
      }
      HANDLE_C_ERROR(bytes_received);
    }

    auto data = std::string_view(message, bytes_received);
    handler(socket, their_addr, addr_len, data);
  }
}

} // namespace integral
