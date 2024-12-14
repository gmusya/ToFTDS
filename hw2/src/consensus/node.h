#pragma once

#include "hw2/src/common/timeout.h"
#include "hw2/src/consensus/common.h"
#include "hw2/src/consensus/message.h"
#include "hw2/src/consensus/state.h"

#include <cstdint>
#include <future>
#include <memory>
#include <mutex>

namespace hw2::consensus {

class Node {
public:
  Node(NodeId my_id, std::map<NodeId, std::shared_ptr<IMessageSender>> channels,
       std::shared_ptr<ITimeout> election_timeout);

  void AddMessage(NodeId, Message);

  void Tick(uint64_t ticks = 1);

  class AddCommandResult {
  public:
    AddCommandResult() = default;
    AddCommandResult(NodeId node_id) : leader_id_(node_id) {}

    bool CommandPersisted() const { return !leader_id_.has_value(); }
    NodeId PotentialLeader() const { return leader_id_.value(); };

  private:
    std::optional<NodeId> leader_id_;
  };

  const auto GetSenderToItself() const { return channels_.at(my_id_); }

  std::future<AddCommandResult> AddCommand(Command command);

  std::future<std::vector<Command>>
  GetPersistentCommands(LogItemId from, uint64_t max_count) const;

  NodeState GetRole() const { return role_.load(); }

private:
  // used only for debugging purposes
  uint64_t tick_number_{};

  AppendEntriesResponse
  HandleAppendEntriesRequest(const AppendEntriesRequest &);
  RequestVoteResponse HandleRequestVoteRequest(const RequestVoteRequest &);

  void HandleAppendEntriesResponse(const AppendEntriesResponse &,
                                   NodeId from_who);
  void HandleRequestVoteResponse(const RequestVoteResponse &, NodeId from_who);

  Term &GetCurrentTerm();
  const Term &GetCurrentTerm() const;

  Log &GetLog();
  const Log &GetLog() const;

  LogItemId &GetCommitIndex();
  const LogItemId &GetCommitIndex() const;

  std::optional<NodeId> &GetVotedFor();

  Term GetLastLogTerm() const;

  LogItemId GetLastLogIndex() const;

  AppendEntriesResponse AppendEntriesUnsuccessfull() const;
  AppendEntriesResponse AppendEntriesSuccessfull(LogItemId id) const;

  RequestVoteResponse RequestVoteUnsuccessfull() const;
  RequestVoteResponse RequestVoteSuccessfull(Term candidate_term) const;

  bool DidElectionTimeoutExpire() const;
  void ResetElectionTimer();

  bool DidOthersElectMe() const;

  void HandleGetPersistentCommandRequests() const;
  void HandleIncomingMessages();

  void BecomeCandidate();
  void BecomeFollower();
  void BecomeLeader();

  void CommitIfPossible();

  void AddNewCommandsToLog();

  void SendAppendEntriesToSomeone(NodeId who, bool heartbeat = true);
  void SendRequestVoteToAll();
  void SendAppendEntriesToAll(bool heartbeat = true);

  void TickFollower();
  void TickCandidate();
  void TickLeader();

  const NodeId my_id_;
  const std::map<NodeId, std::shared_ptr<IMessageSender>> channels_;
  const uint64_t total_nodes_;

  std::shared_ptr<ITimeout> election_timeout_;

  State state_{};
  std::optional<LeaderState> leader_state_;
  std::optional<CandidateState> candidate_state_;

  std::atomic<NodeState> role_{NodeState::kFollower};

  mutable std::mutex incoming_messages_lock_;
  std::vector<std::pair<NodeId, AppendEntriesRequest>>
      incoming_append_entries_requests_;
  std::vector<std::pair<NodeId, RequestVoteRequest>>
      incoming_request_vote_requests_;
  std::vector<std::pair<NodeId, AppendEntriesResponse>>
      incoming_append_entries_responses_;
  std::vector<std::pair<NodeId, RequestVoteResponse>>
      incoming_request_vote_responses_;

  mutable std::mutex incoming_commands_lock_;
  std::vector<std::pair<Command, std::promise<AddCommandResult>>>
      incoming_commands_;

  using PersCommandsQuery = std::pair<LogItemId, uint64_t>;
  mutable std::mutex get_persistent_commands_lock_;
  mutable std::vector<
      std::pair<PersCommandsQuery, std::promise<std::vector<Command>>>>
      get_persistent_commands_reqs_;

  std::map<LogItemId, std::promise<AddCommandResult>> items_to_response_;

  mutable std::mutex log_lock_;
};

} // namespace hw2::consensus
