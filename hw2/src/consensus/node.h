#pragma once

#include "hw2/src/common/query.h"
#include "hw2/src/consensus/common.h"
#include "hw2/src/consensus/message.h"
#include "hw2/src/consensus/state.h"

#include <cstdint>
#include <memory>
#include <mutex>

namespace hw2::consensus {

class IMessageSender {
public:
  virtual void Send(const Message &message) = 0;
};

class Node {
public:
  Node(NodeId my_id, std::map<NodeId, std::shared_ptr<IMessageSender>> channels)
      : my_id_(my_id), channels_(std::move(channels)) {}

  void AddMessage(NodeId, Message);

private:
  AppendEntriesResponse
  HandleAppendEntriesRequest(const AppendEntriesRequest &);
  RequestVoteResponse HandleRequestVoteRequest(const RequestVoteRequest &);

  void HandleAppendEntriesResponse(const AppendEntriesResponse &, NodeId from_who);
  void HandleRequestVoteResponse(const RequestVoteResponse &, NodeId from_who);

  Term &GetCurrentTerm();
  const Term &GetCurrentTerm() const;

  Log &GetLog();
  const Log &GetLog() const;

  LogItemId &GetCommitIndex();

  std::optional<NodeId> &GetVotedFor();

  Term GetLastLogTerm() const;

  LogItemId GetLastLogIndex() const;

  AppendEntriesResponse AppendEntriesUnsuccessfull() const;
  AppendEntriesResponse AppendEntriesSuccessfull() const;

  RequestVoteResponse RequestVoteUnsuccessfull() const;
  RequestVoteResponse RequestVoteSuccessfull() const;

  bool DidElectionTimeoutExpire() const;
  void ResetElectionTimer() const;

  bool DidOthersElectMe() const;

  void HandleIncomingMessages();

  void BecomeCandidate();
  void BecomeFollower();

  void SendRequestVoteToAll();

  void Tick();
  void TickFollower();
  void TickCandidate();
  void TickLeader();

  const NodeId my_id_;
  const std::map<NodeId, std::shared_ptr<IMessageSender>> channels_;

  State state_;
  std::optional<LeaderState> leader_state_;

  NodeState role_;

  std::mutex incoming_messages_lock_;
  std::vector<std::pair<NodeId, AppendEntriesRequest>>
      incoming_append_entries_requests_;
  std::vector<std::pair<NodeId, RequestVoteRequest>>
      incoming_request_vote_requests_;
  std::vector<std::pair<NodeId, AppendEntriesResponse>>
      incoming_append_entries_responses_;
  std::vector<std::pair<NodeId, RequestVoteResponse>>
      incoming_request_vote_responses_;
};

} // namespace hw2::consensus
