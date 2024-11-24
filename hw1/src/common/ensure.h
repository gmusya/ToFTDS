#pragma once

#include <cerrno>
#include <cstring>
#include <stdexcept>
#include <string>

namespace integral {

inline std::string MessageWithFileLine(const std::string &file, const int line,
                                       const std::string &message) {
  return std::string(file) + ":" + std::to_string(line) + ": " + message;
}

#define MESSAGE_WITH_FILE_LINE(msg)                                            \
  MessageWithFileLine(__FILE__, __LINE__, (msg))

#define ENSURE(cond)                                                           \
  do {                                                                         \
    if (!(cond)) {                                                             \
      const std::string message = #cond + std::string(" is not satisfied");    \
      throw std::runtime_error(MESSAGE_WITH_FILE_LINE(message));               \
    }                                                                          \
  } while (false)

#define HANDLE_C_ERROR(func)                                                   \
  do {                                                                         \
    auto &&result = (func);                                                    \
    if (result == -1) {                                                        \
      throw std::runtime_error(MESSAGE_WITH_FILE_LINE(std::strerror(errno)));  \
    }                                                                          \
  } while (false)

} // namespace integral
