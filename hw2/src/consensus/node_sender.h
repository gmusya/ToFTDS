#pragma once

#include "hw2/src/consensus/common.h"
#include "hw2/src/consensus/message.h"
#include "hw2/src/consensus/node.h"

namespace hw2::consensus {

class TrivialMessageSender : public IMessageSender {
public:
  TrivialMessageSender(NodeId id, Node *node) : id_(id), node_(node) {}

  void Send(const Message &message) override {
    node_->AddMessage(id_, message);
  }

private:
  NodeId id_;
  Node *node_;
};

} // namespace hw2::consensus
