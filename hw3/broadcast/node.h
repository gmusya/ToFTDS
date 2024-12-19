#pragma once

#include "hw3/broadcast/common.h"
#include "hw3/broadcast/message.h"
#include "hw3/broadcast/timeout.h"
#include "hw3/broadcast/timeout_builder.h"
#include "hw3/log.h"

#include <algorithm>
#include <atomic>
#include <cstdint>
#include <future>
#include <map>
#include <memory>
#include <mutex>
#include <set>
#include <utility>

namespace hw3::broadcast {

class Node {
public:
  Node(NodeId my_id, std::map<NodeId, std::shared_ptr<IMessageSender>> channels,
       std::shared_ptr<ITimeoutBuilder> timeout_builder)
      : total_nodes_(channels.size() + 1), my_id_(my_id),
        channels_(std::move(channels)), timeout_builder_(timeout_builder) {
    commited_sequence_numbers_.resize(total_nodes_);
  }

  void ReceiveMessage(Message msg) {
    std::lock_guard lg(new_messages_to_process_lock_);
    new_messages_to_process_.emplace_back(std::move(msg));
  }

  void Tick(uint64_t ticks = 1) {
    while (ticks-- != 0) {
      std::vector<Message> messages_to_process_;
      {
        std::lock_guard lg(new_messages_to_process_lock_);
        messages_to_process_ = std::move(new_messages_to_process_);
      }

      for (const auto &msg : messages_to_process_) {
        std::visit([this](auto &&arg) { HandleMessage(arg); }, msg);
      }

      CheckForDeliveredMessages();

      RetrySomeUndeliveredMessages();
    }
  }

  std::future<void> AppendNewPayload(const Payload &payload) {
    auto seq_number = my_sequence_number_.fetch_add(1);
    auto clock = commited_sequence_numbers_;
    clock[my_id_] = seq_number + 1;
    Request request{.sender = my_id_,
                    .author_id = my_id_,
                    .payload = payload,
                    .vector_clock = clock,
                    .already_received_nodes = {my_id_}};
    ReceiveMessage(request);

    std::lock_guard lg(responses_lock_);
    auto result = responses_[seq_number + 1].get_future();
    return result;
  }

  struct CommitedMessage {
    NodeId author;
    Payload payload;
    VectorClock vector_clock;

    bool operator==(const CommitedMessage &other) const = default;
  };

  std::vector<CommitedMessage> GetInternalState() const {
    std::lock_guard lg(commited_messages_lock_);
    return commited_messages_;
  }

  std::map<Key, Value> GetObservableState() {
    return StateToObservableState(GetInternalState());
  }

  static std::map<Key, Value>
  StateToObservableState(const std::vector<CommitedMessage> &messages) {
    std::map<Key, std::vector<CommitedMessage>> key_to_messages;
    for (const auto &message : messages) {
      for (const auto &[key, value] : message.payload.data) {
        key_to_messages[key].emplace_back(message);
      }
    }

    std::map<Key, Value> result;
    for (auto &[k, v] : key_to_messages) {
      v = GetObservableMessages(v);
      std::sort(v.begin(), v.end(), [](const auto &lhs, const auto &rhs) {
        return lhs.author > rhs.author;
      });
      for (const auto &[k1, v1] : v[0].payload.data) {
        if (k == k1) {
          result[k] = v1;
        }
      }
    }

    return result;
  }

  static std::vector<CommitedMessage>
  GetObservableMessages(const std::vector<CommitedMessage> &messages) {
    std::vector<CommitedMessage> observable_messages;

    for (size_t i = 0; i < messages.size(); ++i) {
      bool i_is_observable = true;
      for (size_t j = 0; j < messages.size(); ++j) {
        if (i == j) {
          continue;
        }
        bool j_hides_i = true;
        for (size_t id = 0; id < messages[i].vector_clock.size(); ++id) {
          if (messages[i].vector_clock[id] > messages[j].vector_clock[id]) {
            j_hides_i = false;
            break;
          }
        }

        if (j_hides_i) {
          i_is_observable = false;
          break;
        }
      }

      if (i_is_observable) {
        observable_messages.emplace_back(messages[i]);
      }
    }

    return observable_messages;
  }

private:
  void HandleMessage(const Request &req) {
    MessageId message_id{.author_id = req.author_id,
                         .on_author_id = req.vector_clock.at(req.author_id)};
    if (!all_messages_.contains(message_id)) {
      all_messages_[message_id] =
          MessageInfo{.payload = req.payload,
                      .received_nodes = {req.author_id, my_id_},
                      .vector_clock = req.vector_clock,
                      .is_commited = false,
                      .timeout_ = timeout_builder_->Build()};
    }

    all_messages_.at(message_id).received_nodes.insert(my_id_);
    for (auto &id : req.already_received_nodes) {
      all_messages_.at(message_id).received_nodes.insert(id);
    }

    if (req.author_id == req.sender || req.sender == my_id_) {
      Request my_request = req;
      my_request.already_received_nodes.insert(my_id_);
      my_request.sender = my_id_;
      for (const auto &[id, channel] : channels_) {
        if (my_request.already_received_nodes.contains(id)) {
          continue;
        }
        channel->Send(my_request);
      }
    }

    if (req.author_id != my_id_ && req.sender != my_id_) {
      Response response;
      response.message_id = message_id;
      response.received_nodes = all_messages_.at(message_id).received_nodes;
      channels_.at(req.author_id)->Send(response);
      channels_.at(req.sender)->Send(response);
    }
  }

  void HandleMessage(const Response &resp) {
    const auto &id = resp.message_id;
    if (!all_messages_.contains(id)) {
      LOG("HandleResponse: unexpected");
      return;
    }
    auto &info = all_messages_.at(id);
    for (const auto node_id : resp.received_nodes) {
      info.received_nodes.insert(node_id);
    }
  }

  void RetrySomeUndeliveredMessages() {
    for (const auto &[id, info] : all_messages_) {
      if (info.received_nodes.size() != total_nodes_ &&
          info.timeout_->IsExpired()) {
        Request request{.sender = my_id_,
                        .author_id = id.author_id,
                        .payload = info.payload,
                        .vector_clock = info.vector_clock,
                        .already_received_nodes = info.received_nodes};
        HandleMessage(request);
        info.timeout_->Reset();
      }
    }
  }

  void CheckForDeliveredMessages() {
    bool has_at_least_one_delivered_message = true;
    while (has_at_least_one_delivered_message) {
      has_at_least_one_delivered_message = false;
      for (auto &[id, info] : all_messages_) {
        if (!info.is_commited &&
            info.received_nodes.size() * 2 > total_nodes_) { // state changed
          bool matches_commited = true;
          for (uint32_t i = 0; i < total_nodes_; ++i) {
            if (i == id.author_id) {
              matches_commited &=
                  info.vector_clock[i] == commited_sequence_numbers_[i] + 1;
            } else {
              matches_commited &=
                  info.vector_clock[i] <= commited_sequence_numbers_[i];
            }
          }

          if (matches_commited) {
            has_at_least_one_delivered_message = true;

            info.is_commited = true;
            ++commited_sequence_numbers_[id.author_id];

            {
              std::lock_guard lg(commited_messages_lock_);
              commited_messages_.emplace_back(
                  CommitedMessage{.author = id.author_id,
                                  .payload = info.payload,
                                  .vector_clock = info.vector_clock});
              LOG("Commited message (mynodeid = " + std::to_string(my_id_) +
                  ", nodeid = " + std::to_string(id.author_id) +
                  ", sequence number = " + std::to_string(id.on_author_id));
            }

            if (id.author_id == my_id_) {
              std::lock_guard lg(responses_lock_);
              auto commited_seqno = commited_sequence_numbers_[id.author_id];
              if (!responses_.contains(commited_seqno)) {
                LOG("Unexpected");
                continue;
              }
              responses_.at(commited_seqno).set_value();
              LOG("Response sended (mynodeid = " + std::to_string(my_id_) +
                  ", sequence number = " + std::to_string(id.on_author_id));
            }
          }
        }
      }
    }
  }

  const uint32_t total_nodes_;
  const NodeId my_id_;
  std::map<NodeId, std::shared_ptr<IMessageSender>> channels_;

  std::atomic<SequenceNumber> my_sequence_number_{0};
  VectorClock commited_sequence_numbers_;

  struct MessageInfo {
    Payload payload;
    std::set<NodeId> received_nodes;
    VectorClock vector_clock;
    bool is_commited;
    std::shared_ptr<ITimeout> timeout_;
  };
  std::map<MessageId, MessageInfo> all_messages_;

  mutable std::mutex responses_lock_;
  std::map<SequenceNumber, std::promise<void>> responses_;

  mutable std::mutex new_messages_to_process_lock_;
  std::vector<Message> new_messages_to_process_;

  mutable std::mutex commited_messages_lock_;
  std::vector<CommitedMessage> commited_messages_;

  std::shared_ptr<ITimeoutBuilder> timeout_builder_;
};

} // namespace hw3::broadcast
