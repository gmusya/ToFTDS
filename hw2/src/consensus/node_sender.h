#pragma once

#include "hw2/src/consensus/common.h"
#include "hw2/src/consensus/message.h"
#include "hw2/src/consensus/node.h"

namespace hw2::consensus {

class TrivialMessageSender : public IMessageSender {
public:
  void Init(Node *node) { node_ = node; }

  TrivialMessageSender() = default;

  void Send(NodeId id, const Message &message) {
    node_->AddMessage(id, message);
  }

private:
  Node *node_ = nullptr;
};

} // namespace hw2::consensus
