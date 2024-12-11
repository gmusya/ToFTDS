#include "hw2/src/consensus/node.h"
#include "hw2/src/common/log.h"
#include "hw2/src/consensus/common.h"
#include "hw2/src/consensus/message.h"
#include "hw2/src/consensus/state.h"
#include <future>
#include <mutex>
#include <stdexcept>
#include <string>
#include <type_traits>

#include "hw2/src/consensus/node_sender.h"

namespace hw2::consensus {

namespace {

std::string RoleAsString(NodeState s) {
  switch (s) {
  case NodeState::kCandidate:
    return "Candidate";
  case NodeState::kFollower:
    return "Follower";
  case NodeState::kLeader:
    return "Leader";
  }
}

} // namespace

#define PRETTY_LOG(msg)                                                        \
  LOG("(Node id = " + std::to_string(my_id_) + ", tick = " +                   \
      std::to_string(tick_number_) + ", role = " + RoleAsString(role_) +       \
      "), " + std::string(__PRETTY_FUNCTION__) + ": " + msg)

Term &Node::GetCurrentTerm() { return state_.persistent_state.current_term; }
const Term &Node::GetCurrentTerm() const {
  return state_.persistent_state.current_term;
}

Log &Node::GetLog() { return state_.persistent_state.log; }
const Log &Node::GetLog() const { return state_.persistent_state.log; }

LogItemId &Node::GetCommitIndex() { return state_.volatile_state.commit_index; }
const LogItemId &Node::GetCommitIndex() const {
  return state_.volatile_state.commit_index;
}

AppendEntriesResponse Node::AppendEntriesUnsuccessfull() const {
  return AppendEntriesResponse{.current_term = GetCurrentTerm(),
                               .success = false,
                               .matched_until = std::nullopt};
}

AppendEntriesResponse Node::AppendEntriesSuccessfull(LogItemId id) const {
  return AppendEntriesResponse{
      .current_term = GetCurrentTerm(), .success = true, .matched_until = id};
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

Node::Node(NodeId my_id,
           std::map<NodeId, std::shared_ptr<IMessageSender>> channels,
           std::shared_ptr<ITimeout> election_timeout)
    : my_id_(my_id),
      channels_([this, my_id, chan = std::move(channels)]() mutable {
        auto sender = std::make_shared<TrivialMessageSender>();
        sender->Init(this);
        chan[my_id] = sender;
        return std::move(chan);
      }()),
      total_nodes_(channels_.size()), election_timeout_(election_timeout) {
  state_.persistent_state.log.emplace_back("", 0);
}

AppendEntriesResponse
Node::HandleAppendEntriesRequest(const AppendEntriesRequest &req) {
  PRETTY_LOG("")

  if (GetCurrentTerm() < req.leader_term) {
    GetCurrentTerm() = req.leader_term;
    BecomeFollower();
  }

  ResetElectionTimer();

  // 1. Reply false if term < currentTerm
  const auto current_term = GetCurrentTerm();
  if (req.leader_term < current_term) {
    return AppendEntriesUnsuccessfull();
  }

  if (role_ != NodeState::kFollower && req.leader_id != my_id_) {
    PRETTY_LOG("UNEXPECTED");
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
    if (log.size() > req.prev_log_index + pos_in_request + 1) {

      if (log[req.prev_log_index + pos_in_request + 1] ==
          req.entries[pos_in_request]) {
        continue;
      }

      if (log[req.prev_log_index + pos_in_request + 1].leader_term ==
          req.entries[pos_in_request].leader_term) {
        PRETTY_LOG("UNEXPECTED");
        // TODO: unexpected, log this
      }

      log.resize(req.prev_log_index + pos_in_request + 1);
    }

    PRETTY_LOG("PUSH BACK, pos in request = " + std::to_string(pos_in_request));
    // 4. Append any new entries not already in the log
    log.push_back(req.entries[pos_in_request]);
  }

  // 5. If leaderCommit > commitIndex, set commitIndex = min(leaderCommit, index
  // of last new entry)
  if (req.leader_commit > GetCommitIndex()) {
    GetCommitIndex() =
        std::min(req.leader_commit, req.prev_log_index + req.entries.size());
  }

  return AppendEntriesSuccessfull(
      std::min(static_cast<uint64_t>(GetLog().size() - 1),
               req.prev_log_index + req.entries.size()));
}

Term Node::GetLastLogTerm() const { return GetLog().back().leader_term; }

LogItemId Node::GetLastLogIndex() const { return GetLog().size() - 1; }

RequestVoteResponse
Node::HandleRequestVoteRequest(const RequestVoteRequest &req) {
  PRETTY_LOG("")

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

void Node::SendAppendEntriesToSomeone(NodeId who, bool heartbeat) {
  if (GetRole() != NodeState::kLeader) {
    return;
  }
  const auto id = who;
  auto &channel = channels_.at(who);
  if (GetLastLogIndex() >= leader_state_->next_index[id] || heartbeat) {
    const auto &log = GetLog();
    Log log_to_send;
    for (size_t i = leader_state_->next_index[id]; i <= GetLastLogIndex();
         ++i) {
      log_to_send.emplace_back(log[i]);
    }
    AppendEntriesRequest request{
        .leader_term = GetCurrentTerm(),
        .leader_id = my_id_,
        .prev_log_index = leader_state_->next_index[id] - 1,
        .prev_log_term = log[leader_state_->next_index[id] - 1].leader_term,
        .entries = log_to_send,
        .leader_commit = GetCommitIndex()};
    channel->Send(my_id_, request);
  }
}

void Node::HandleAppendEntriesResponse(const AppendEntriesResponse &resp,
                                       NodeId from_who) {
  PRETTY_LOG("");
  if (GetCurrentTerm() < resp.current_term) {
    GetCurrentTerm() = resp.current_term;
    BecomeFollower();
  }
  if (role_.load() != NodeState::kLeader) {
    return;
  }

  if (!resp.success) {
    return;
  }

  leader_state_->match_index[from_who] =
      std::max(leader_state_->match_index[from_who], *resp.matched_until);
  PRETTY_LOG("From = " + std::to_string(from_who) +
             ", matched until = " + std::to_string(*resp.matched_until));

  leader_state_->next_index[from_who] =
      std::max(leader_state_->next_index[from_who],
               leader_state_->match_index[from_who] + 1);
}

void Node::HandleRequestVoteResponse(const RequestVoteResponse &resp,
                                     NodeId from_who) {
  PRETTY_LOG("");
  if (GetCurrentTerm() < resp.current_term) {
    GetCurrentTerm() = resp.current_term;
    BecomeFollower();
  }
  if (role_.load() != NodeState::kCandidate) {
    return;
  }
  candidate_state_->votes.insert(from_who);
  PRETTY_LOG("Votes = " + std::to_string(candidate_state_->votes.size()));
  if (candidate_state_->votes.size() * 2 >= total_nodes_ + 1) {
    BecomeLeader();
  }
}

std::future<std::vector<Command>>
Node::GetPersistentCommands(LogItemId from, uint64_t max_count) const {
  std::promise<std::vector<Command>> promise;
  auto result = promise.get_future();

  if (from == 0) {
    from = 1;
  }
  PersCommandsQuery query(from, max_count);

  std::lock_guard lg(get_persistent_commands_lock_);
  get_persistent_commands_reqs_.emplace_back(query, std::move(promise));
  return result;
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
    channel->Send(my_id_, HandleAppendEntriesRequest(ae_req));
  }

  for (const auto &[sender, rv_req] : rv_reqs) {
    if (!channels_.contains(sender)) {
      // TOOD: unexpected, log this
    }
    auto &channel = channels_.at(sender);
    channel->Send(my_id_, HandleRequestVoteRequest(rv_req));
  }

  for (const auto &[sender, ae_resp] : ae_resps) {
    HandleAppendEntriesResponse(ae_resp, sender);
  }

  for (const auto &[sender, rv_resp] : rv_resps) {
    HandleRequestVoteResponse(rv_resp, sender);
  }
}

void Node::HandleGetPersistentCommandRequests() const {
  std::vector<std::pair<PersCommandsQuery, std::promise<std::vector<Command>>>>
      reqs;

  {
    std::lock_guard lg(get_persistent_commands_lock_);
    reqs = std::move(get_persistent_commands_reqs_);
  }

  for (auto &req : reqs) {
    PRETTY_LOG("CommitIndex() = " + std::to_string(GetCommitIndex()));
    const auto &[from, cnt] = req.first;

    std::lock_guard lg(log_lock_);
    const auto &log = GetLog();
    std::vector<Command> result;
    for (size_t i = from; i < from + cnt && i <= GetCommitIndex(); ++i) {
      result.push_back(log[i].command);
    }

    req.second.set_value(std::move(result));
  }
}

void Node::Tick(uint64_t times) {
  while (times--) {
    ++tick_number_;
    HandleGetPersistentCommandRequests();

    switch (role_) {
    case NodeState::kFollower:
      TickFollower();
      break;
    case NodeState::kCandidate:
      TickCandidate();
      break;
    case NodeState::kLeader:
      TickLeader();
      break;
    }
  }
}

void Node::CommitIfPossible() {
  std::optional<LogItemId> id_to_commit;
  PRETTY_LOG("GetCommitIndex() = " + std::to_string(GetCommitIndex()));
  PRETTY_LOG("GetLastLogIndex() = " + std::to_string(GetLastLogIndex()));
  for (LogItemId i = GetCommitIndex() + 1; i <= GetLastLogIndex(); ++i) {
    uint64_t cnt = 0;
    for (const auto &[k, v] : leader_state_->match_index) {
      PRETTY_LOG("v = " + std::to_string(v));
      cnt += v >= i;
    }
    if (cnt * 2 >= total_nodes_ + 1) {
      id_to_commit = i;
    }
  }
  if (!id_to_commit.has_value()) {
    return;
  }
  PRETTY_LOG("Commit until item " + std::to_string(id_to_commit.value()));
  for (size_t i = GetCommitIndex() + 1; i <= *id_to_commit; ++i) {
    PRETTY_LOG(std::to_string(i));
    auto it = items_to_response_.find(i);
    if (it != items_to_response_.end()) {
      PRETTY_LOG("Response to command " + std::to_string(i));
      it->second.set_value(AddCommandResult());
      items_to_response_.erase(it);
    }
  }
  GetCommitIndex() = *id_to_commit;
}

void Node::AddNewCommandsToLog() {
  std::vector<std::pair<Command, std::promise<AddCommandResult>>> new_commands;
  {
    std::lock_guard lg(incoming_commands_lock_);
    new_commands = std::move(incoming_commands_);
  }

  auto &log = GetLog();
  for (auto &command : new_commands) {
    log.emplace_back(command.first, GetCurrentTerm());
    items_to_response_[log.size() - 1] = std::move(command.second);
    PRETTY_LOG("Added new command to log = " + std::to_string(log.size() - 1));
  }
}

void Node::TickLeader() {
  CommitIfPossible();
  HandleIncomingMessages();

  // maybe i am not leader anymore
  if (GetRole() == NodeState::kLeader) {
    SendAppendEntriesToAll();

    AddNewCommandsToLog();
  }
}

bool Node::DidElectionTimeoutExpire() const {
  return election_timeout_->IsExpired();
}

void Node::ResetElectionTimer() { election_timeout_->Reset(); }

void Node::TickFollower() {
  {
    std::vector<std::pair<Command, std::promise<AddCommandResult>>>
        new_commands;
    {
      std::lock_guard lg(incoming_commands_lock_);
      new_commands = std::move(incoming_commands_);
    }
    for (auto &command : new_commands) {
      command.second.set_value(AddCommandResult(0));
    }
  }

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
  PRETTY_LOG("")
  RequestVoteRequest request{.candidate_term = GetCurrentTerm(),
                             .candidate_id = my_id_,
                             .candidate_last_log_index = GetLastLogIndex(),
                             .candidate_last_log_term = GetLastLogTerm()};

  for (auto &[node_id, channel] : channels_) {
    channel->Send(my_id_, request);
  }
}

void Node::SendAppendEntriesToAll(bool heartbeat) {
  PRETTY_LOG("")
  for (auto &[node_id, channel] : channels_) {
    SendAppendEntriesToSomeone(node_id, heartbeat);
  }
}

void Node::BecomeFollower() {
  PRETTY_LOG("")
  role_.store(NodeState::kFollower);
  GetVotedFor().reset();
  candidate_state_.reset();
  leader_state_.reset();
  ResetElectionTimer();
}

void Node::BecomeCandidate() {
  PRETTY_LOG("")
  ++GetCurrentTerm();
  GetVotedFor() = my_id_;
  ResetElectionTimer();
  SendRequestVoteToAll();
  role_.store(NodeState::kCandidate);
  candidate_state_.emplace();
  leader_state_.reset();
}

void Node::BecomeLeader() {
  PRETTY_LOG("")
  GetVotedFor().reset();
  candidate_state_.reset();
  leader_state_.emplace();
  role_.store(NodeState::kLeader);
  for (const auto &[id, _] : channels_) {
    leader_state_->next_index[id] = GetLastLogIndex() + 1;
    leader_state_->match_index[id] = 0;
  }
  SendAppendEntriesToAll();
}

void Node::AddMessage(NodeId id, Message msg) {
  PRETTY_LOG("FROM " + std::to_string(id) +
             ", type = " + std::to_string(msg.index()));

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
        } else {
          throw std::runtime_error(MESSAGE_WITH_FILE_LINE("Internal error"));
        }
      },
      std::move(msg));
}

std::future<Node::AddCommandResult> Node::AddCommand(Command command) {
  std::promise<Node::AddCommandResult> promise;
  std::future<Node::AddCommandResult> result = promise.get_future();

  if (role_.load() != NodeState::kLeader) {
    promise.set_value(Node::AddCommandResult(0));
    return result;
  }

  std::lock_guard lg(incoming_commands_lock_);
  incoming_commands_.emplace_back(std::move(command), std::move(promise));
  return result;
}

} // namespace hw2::consensus
