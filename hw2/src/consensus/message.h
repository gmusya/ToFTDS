#pragma once

#include "hw2/src/consensus/common.h"
#include "hw2/src/consensus/state.h"

#include <variant>

namespace hw2::consensus {

struct AppendEntriesRequest {
  Term leader_term;
  NodeId leader_id;
  LogItemId prev_log_index;
  Term prev_log_term;
  Log entries;
  LogItemId leader_commit;
};

struct AppendEntriesResponse {
  Term current_term;
  bool success;
  std::optional<LogItemId> matched_until;
};

struct RequestVoteRequest {
  Term candidate_term;
  NodeId candidate_id;
  LogItemId candidate_last_log_index;
  Term candidate_last_log_term;
};

struct RequestVoteResponse {
  Term current_term;
  bool vote_granted;
};

using Message = std::variant<AppendEntriesRequest, AppendEntriesResponse,
                             RequestVoteRequest, RequestVoteResponse>;

} // namespace hw2::consensus
