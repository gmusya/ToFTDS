#include "gtest/gtest.h"
#include <future>
#include <memory>
#include <string>
#include <thread>

#include "hw3/broadcast/common.h"
#include "hw3/broadcast/message.h"
#include "hw3/broadcast/node.h"
#include "hw3/broadcast/node_sender.h"

namespace hw3::broadcast {
namespace {

class StateTest : public ::testing::Test {};

TEST(StateTest, Trivial) {
  Node::CommitedMessage msg0;
  msg0.author = 0;
  msg0.payload.data = {{"a", "b"}};
  msg0.vector_clock = {{1, 0, 0}};

  std::vector<Node::CommitedMessage> messages = {msg0};

  auto result = Node::GetObservableMessages(messages);
  EXPECT_EQ(result.size(), 1);

  auto state = Node::StateToObservableState(messages);
  EXPECT_EQ(state["a"], "b");
}

} // namespace
} // namespace hw3::broadcast
