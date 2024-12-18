#include "gtest/gtest.h"
#include <future>
#include <string>
#include <thread>

#include "hw3/broadcast/message.h"
#include "hw3/broadcast/node.h"

namespace hw3::broadcast {
namespace {

class NodeTest : public ::testing::Test {};

TEST(NodeTest, Trivial) {
  hw3::broadcast::Node node(0, {});
  Payload payload;
  payload.data.emplace_back("a", "b");

  ASSERT_EQ(node.GetState().size(), 0);
  node.AppendNewPayload(payload);
  ASSERT_EQ(node.GetState().size(), 0);
  node.Tick(2);
  ASSERT_EQ(node.GetState().size(), 1);
}

} // namespace
} // namespace hw3::broadcast
