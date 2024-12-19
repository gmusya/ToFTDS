#pragma once

#include <iostream>
#include <string>

inline std::string MessageWithFileLine(const std::string &file, const int line,
                                       const std::string &message) {
  return std::string(file) + ":" + std::to_string(line) + ": " + message;
}

#define MESSAGE_WITH_FILE_LINE(msg)                                            \
  MessageWithFileLine(__FILE__, __LINE__, (msg))

#define LOG(msg) std::cerr << MESSAGE_WITH_FILE_LINE(msg) << std::endl;
