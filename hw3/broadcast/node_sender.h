#pragma once

#include "hw3/broadcast/message.h"
#include "hw3/broadcast/node.h"

namespace hw3::broadcast {

class TrivialMessageSender : public IMessageSender {
public:
  void Init(Node *node) { node_ = node; }

  TrivialMessageSender() = default;

  void Send(const Message &message) { node_->ReceiveMessage(message); }

private:
  Node *node_ = nullptr;
};

} // namespace hw3::broadcast
