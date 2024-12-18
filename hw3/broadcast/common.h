#pragma once

#include <cstdint>
#include <string>
#include <vector>

namespace hw3::broadcast {

using NodeId = uint64_t;
using SequenceNumber = uint64_t;
using Key = std::string;
using Value = std::string;
using VectorClock = std::vector<SequenceNumber>;

struct MessageId {
  NodeId author_id;
  SequenceNumber on_author_id;
};

} // namespace hw3::broadcast
