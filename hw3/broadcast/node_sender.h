#pragma once

#include "hw3/broadcast/message.h"
#include "hw3/broadcast/node.h"

namespace hw3::broadcast {

class TrivialMessageSender : public IMessageSender {
public:
  void Init(Node *node) { node_ = node; }

  TrivialMessageSender() = default;

  void Send(const Message &message) override { node_->ReceiveMessage(message); }

private:
  Node *node_ = nullptr;
};

class BufferingMessageSender : public IMessageSender {
public:
  BufferingMessageSender(std::shared_ptr<IMessageSender> sender,
                         int buffer_size = 1)
      : sender_(sender), max_buffer_size_(buffer_size) {}

  void Send(const Message &message) override {
    messages_.emplace_back(message);
    if (messages_.size() == max_buffer_size_) {
      ForceSend();
    }
  }

  void ChangeBufferSize(int new_buffer_size) {
    max_buffer_size_ = new_buffer_size;
    if (messages_.size() >= max_buffer_size_) {
      ForceSend();
    }
  }

  void LoseMessages() { messages_.clear(); }

  void ForceSend() {
    for (const auto &msg : messages_) {
      sender_->Send(msg);
    }
    messages_.clear();
  }

private:
  int max_buffer_size_ = 1;
  std::shared_ptr<IMessageSender> sender_;

  std::vector<Message> messages_;
};

} // namespace hw3::broadcast
