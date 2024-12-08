#pragma once

#include <cstdint>

namespace hw2::consensus {

using Term = uint64_t;
using NodeId = uint64_t;

enum class NodeState { kLeader, kFollower, kCandidate };

} // namespace hw2
