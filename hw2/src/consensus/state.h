#pragma once

#include "hw2/src/consensus/common.h"

#include <map>
#include <optional>
#include <set>
#include <string>
#include <vector>

namespace hw2::consensus {

using LogItemId = uint64_t;
using Command = std::string;

struct LogItem {
  Command command;
  Term leader_term{};

  auto operator<=>(const LogItem &other) const = default;
};

using Log = std::vector<LogItem>;

struct PersistentState {
  Term current_term{};
  std::optional<NodeId> voted_for;
  Log log;
};

struct VolatileState {
  LogItemId commit_index{};
  //   LogItemId last_applied;
};

struct State {
  PersistentState persistent_state;
  VolatileState volatile_state;
};

struct CandidateState {
  std::set<NodeId> votes;
};

struct LeaderState {
  std::map<NodeId, LogItemId> next_index;
  std::map<NodeId, LogItemId> match_index;
};

} // namespace hw2::consensus
