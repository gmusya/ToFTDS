#pragma once

#include <set>
#include <utility>
#include <variant>
#include <vector>

#include "hw3/broadcast/common.h"

namespace hw3::broadcast {

struct Payload {
  std::vector<std::pair<Key, Value>> data;

  bool operator==(const Payload &other) const {
    if (data.size() != other.data.size()) {
      return false;
    }
    for (size_t i = 0; i < data.size(); ++i) {
      if (data[i] != other.data[i]) {
        return false;
      }
    }
    return true;
  }
};

struct Request {
  NodeId sender;
  NodeId author_id;
  Payload payload;
  VectorClock vector_clock;
  std::set<NodeId> already_received_nodes;
};

struct Response {
  MessageId message_id;
  std::set<NodeId> received_nodes;
};

using Message = std::variant<Request, Response>;

class IMessageSender {
public:
  virtual void Send(const Message &message) = 0;
};

} // namespace hw3::broadcast
