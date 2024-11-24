#pragma once

#include <stdexcept>
#include <string>

namespace integral {

inline std::string MessageWithFileLine(const std::string &file, const int line,
                                       const std::string &message) {
  return std::string(file) + ":" + std::to_string(line) + ":" + message;
}

#define MESSAGE_WITH_FILE_LINE(cond)                                           \
  MessageWithFileLine(__FILE__, __LINE__, (cond))

#define ENSURE(cond)                                                           \
  do {                                                                         \
    if (!(cond)) {                                                             \
      throw std::runtime_error(                                                \
          MESSAGE_WITH_FILE_LINE(#cond + std::string(" is not satisfied")));   \
    }                                                                          \
  } while (false)

} // namespace integral
