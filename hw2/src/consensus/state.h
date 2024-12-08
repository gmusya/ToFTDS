#pragma once

#include "hw2/src/consensus/common.h"

#include <map>
#include <optional>
#include <string>
#include <vector>

namespace hw2::consensus {

using LogItemId = uint64_t;

struct LogItem {
  std::string command;
  Term leader_term;

  auto operator<=>(const LogItem &other) const = default;
};

using Log = std::vector<LogItem>;

struct PersistentState {
  Term current_term;
  std::optional<NodeId> voted_for;
  Log log;
};

struct VolatileState {
  LogItemId commit_index;
  LogItemId last_applied;
};

struct State {
  PersistentState persistent_state;
  VolatileState volatile_state;
};

struct LeaderState {
  std::map<NodeId, LogItemId> next_index;
  std::map<NodeId, LogItemId> match_index;
};

} // namespace hw2::consensus
