#include "hw2/src/consensus/node.h"
#include "hw2/src/consensus/common.h"
#include "hw2/src/consensus/message.h"
#include "hw2/src/consensus/state.h"
#include <mutex>
#include <type_traits>

namespace hw2::consensus {

Term &Node::GetCurrentTerm() { return state_.persistent_state.current_term; }
const Term &Node::GetCurrentTerm() const {
  return state_.persistent_state.current_term;
}

Log &Node::GetLog() { return state_.persistent_state.log; }
const Log &Node::GetLog() const { return state_.persistent_state.log; }

LogItemId &Node::GetCommitIndex() { return state_.volatile_state.commit_index; }

AppendEntriesResponse Node::AppendEntriesUnsuccessfull() const {
  return AppendEntriesResponse{.current_term = GetCurrentTerm(),
                               .success = false};
}

AppendEntriesResponse Node::AppendEntriesSuccessfull() const {
  return AppendEntriesResponse{.current_term = GetCurrentTerm(),
                               .success = true};
}

RequestVoteResponse Node::RequestVoteUnsuccessfull() const {
  return RequestVoteResponse{.current_term = GetCurrentTerm(),
                             .vote_granted = false};
}

RequestVoteResponse Node::RequestVoteSuccessfull() const {
  return RequestVoteResponse{.current_term = GetCurrentTerm(),
                             .vote_granted = true};
}

std::optional<NodeId> &Node::GetVotedFor() {
  return state_.persistent_state.voted_for;
}

void Node::BecomeFollower() { role_ = NodeState::kFollower; }

AppendEntriesResponse
Node::HandleAppendEntriesRequest(const AppendEntriesRequest &req) {
  if (GetCurrentTerm() < req.leader_term) {
    GetCurrentTerm() = req.leader_term;
    BecomeFollower();
  }

  // 1. Reply false if term < currentTerm
  const auto current_term = GetCurrentTerm();
  if (req.leader_term < current_term) {
    return AppendEntriesUnsuccessfull();
  }

  if (role_ != NodeState::kFollower) {
    // TODO: unexpected, log this
  }

  // 2. Reply false if log doesn't contain an entry at prevLogIndex whose term
  // matches prevLogTerm
  auto &log = GetLog();
  if (log.size() <= req.prev_log_index) {
    return AppendEntriesUnsuccessfull();
  }

  if (log[req.prev_log_index].leader_term != req.prev_log_term) {
    return AppendEntriesUnsuccessfull();
  }

  // 3. If an existing entry conflicts with a new one (same index but different
  // terms), delete the existing entry and all that follow it
  auto pos_in_request = 0;
  for (size_t pos_in_request = 0; pos_in_request < req.entries.size();
       ++pos_in_request) {
    if (log.size() <= req.prev_log_index + pos_in_request + 1) {

      if (log[req.prev_log_index + pos_in_request + 1] ==
          req.entries[pos_in_request]) {
        continue;
      }

      if (log[req.prev_log_index + pos_in_request + 1].leader_term ==
          req.entries[pos_in_request].leader_term) {
        // TODO: unexpected, log this
      }

      log.resize(req.prev_log_index + pos_in_request + 1);
    }

    // 4. Append any new entries not already in the log
    log.push_back(req.entries[pos_in_request]);
  }

  // 5. If leaderCommit > commitIndex, set commitIndex = min(leaderCommit, index
  // of last new entry)
  if (req.leader_commit > GetCommitIndex()) {
    GetCommitIndex() =
        std::min(req.leader_commit, req.prev_log_index + req.entries.size());
  }

  return AppendEntriesSuccessfull();
}

Term Node::GetLastLogTerm() const { return GetLog().back().leader_term; }

LogItemId Node::GetLastLogIndex() const { return GetLog().size(); }

RequestVoteResponse
Node::HandleRequestVoteRequest(const RequestVoteRequest &req) {
  if (GetCurrentTerm() < req.candidate_term) {
    GetCurrentTerm() = req.candidate_term;
    BecomeFollower();
  }

  // 1. Reply false if term < currentTerm
  const auto current_term = GetCurrentTerm();
  if (req.candidate_term < current_term) {
    return RequestVoteResponse();
  }

  if (role_ != NodeState::kFollower) {
    // TODO: unexpected, log this
  }

  // 2. If votedFor is null or candidateId, and candidate's log is at least as
  // up-to-date as receiver's log, grant vote
  if (GetVotedFor().value_or(req.candidate_id) == req.candidate_id) {
    if (GetLastLogTerm() <= req.candidate_last_log_term &&
        GetLastLogIndex() <= req.candidate_last_log_index) {
      GetVotedFor() = req.candidate_id;
      return RequestVoteSuccessfull();
    }
  }

  return RequestVoteUnsuccessfull();
}

void Node::Tick() {
  switch (role_) {
  case NodeState::kFollower:
    TickFollower();
    return;
  case NodeState::kCandidate:
    TickCandidate();
    return;
  case NodeState::kLeader:
    TickLeader();
    return;
  }
}

void Node::HandleIncomingMessages() {
  std::vector<std::pair<NodeId, AppendEntriesRequest>> ae_reqs;
  std::vector<std::pair<NodeId, RequestVoteRequest>> rv_reqs;
  std::vector<std::pair<NodeId, AppendEntriesResponse>> ae_resps;
  std::vector<std::pair<NodeId, RequestVoteResponse>> rv_resps;

  {
    std::lock_guard lg(incoming_messages_lock_);
    ae_reqs = std::move(incoming_append_entries_requests_);
    rv_reqs = std::move(incoming_request_vote_requests_);
    ae_resps = std::move(incoming_append_entries_responses_);
    rv_resps = std::move(incoming_request_vote_responses_);
  }

  for (const auto &[sender, ae_req] : ae_reqs) {
    if (!channels_.contains(sender)) {
      // TOOD: unexpected, log this
    }
    auto &channel = channels_.at(sender);
    channel->Send(HandleAppendEntriesRequest(ae_req));
  }

  for (const auto &[sender, rv_req] : rv_reqs) {
    if (!channels_.contains(sender)) {
      // TOOD: unexpected, log this
    }
    auto &channel = channels_.at(sender);
    channel->Send(HandleRequestVoteRequest(rv_req));
  }

  for (const auto &[sender, ae_resp] : ae_resps) {
    HandleAppendEntriesResponse(ae_resp, sender);
  }

  for (const auto &[sender, rv_resp] : rv_resps) {
    HandleRequestVoteResponse(rv_resp, sender);
  }
}

void Node::TickFollower() {
  HandleIncomingMessages();

  if (DidElectionTimeoutExpire()) {
    BecomeCandidate();
  }
}

void Node::TickCandidate() {
  // it is fine to use TickFollower functions, because of checks in
  // AppendEntries and RequestVote functions
  TickFollower();
}

void Node::SendRequestVoteToAll() {
  RequestVoteRequest request{.candidate_term = GetCurrentTerm(),
                             .candidate_id = my_id_,
                             .candidate_last_log_index = GetLastLogIndex(),
                             .candidate_last_log_term = GetLastLogTerm()};

  for (auto &[node_id, channel] : channels_) {
    channel->Send(request);
  }
}

void Node::BecomeCandidate() {
  ++GetCurrentTerm();
  GetVotedFor() = my_id_;
  ResetElectionTimer();
  SendRequestVoteToAll();
}

void Node::AddMessage(NodeId id, Message msg) {
  std::lock_guard lg(incoming_messages_lock_);
  std::visit(
      [&](auto &&arg) {
        using ArgType = std::decay_t<decltype(arg)>;
        if constexpr (std::is_same_v<ArgType, AppendEntriesRequest>) {
          incoming_append_entries_requests_.emplace_back(id, std::move(arg));
        } else if constexpr (std::is_same_v<ArgType, RequestVoteRequest>) {
          incoming_request_vote_requests_.emplace_back(id, std::move(arg));
        } else if constexpr (std::is_same_v<ArgType, AppendEntriesResponse>) {
          incoming_append_entries_responses_.emplace_back(id, std::move(arg));
        } else if constexpr (std::is_same_v<ArgType, RequestVoteResponse>) {
          incoming_request_vote_responses_.emplace_back(id, std::move(arg));
        }
      },
      std::move(msg));
}

} // namespace hw2::consensus
