#pragma once

#include <sys/socket.h>

#include <cerrno>
#include <functional>
#include <string_view>

namespace integral {

void OnReceiveAsync(int socket,
                    const std::function<void(int, sockaddr_storage, socklen_t,
                                             std::string_view)> &handler);

} // namespace integral
