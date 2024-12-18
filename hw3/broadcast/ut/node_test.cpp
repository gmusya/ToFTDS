#include "gtest/gtest.h"
#include <future>
#include <string>
#include <thread>

#include "hw3/broadcast/common.h"
#include "hw3/broadcast/message.h"
#include "hw3/broadcast/node.h"
#include "hw3/broadcast/node_sender.h"

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
  node.Tick();
  ASSERT_EQ(node.GetState().size(), 1);
  auto state = node.GetState();
  VectorClock expeceted_vector_clock(1, 1u);
  ASSERT_EQ(state[0].payload, payload);
  ASSERT_EQ(state[0].vector_clock, expeceted_vector_clock);
}

TEST(NodeTest, ThreeConflictingNodes) {
  auto sender0 = std::make_shared<TrivialMessageSender>();
  auto sender1 = std::make_shared<TrivialMessageSender>();
  auto sender2 = std::make_shared<TrivialMessageSender>();

  hw3::broadcast::Node node0(0, {{1, sender1}, {2, sender2}});
  hw3::broadcast::Node node1(1, {{0, sender0}, {2, sender2}});
  hw3::broadcast::Node node2(2, {{0, sender0}, {1, sender1}});

  sender0->Init(&node0);
  sender1->Init(&node1);
  sender2->Init(&node2);

  Payload payload0;
  payload0.data.emplace_back("a0", "b0");
  Payload payload1;
  payload1.data.emplace_back("a1", "b1");
  Payload payload2;
  payload2.data.emplace_back("a2", "b2");

  node0.AppendNewPayload(payload0);
  node1.AppendNewPayload(payload1);
  node2.AppendNewPayload(payload2);

  VectorClock clock0{1, 0, 0};
  VectorClock clock1{0, 1, 0};
  VectorClock clock2{0, 0, 1};

  node0.Tick();
  ASSERT_EQ(node0.GetState().size(), 0);

  node1.Tick();
  ASSERT_EQ(node1.GetState().size(), 1);
  ASSERT_EQ(node1.GetState()[0].payload, payload0);
  ASSERT_EQ(node1.GetState()[0].vector_clock, clock0);

  node0.Tick();
  ASSERT_EQ(node0.GetState().size(), 2);
  ASSERT_EQ(node0.GetState()[0].payload, payload0);
  ASSERT_EQ(node0.GetState()[0].vector_clock, clock0);
  ASSERT_EQ(node0.GetState()[1].payload, payload1);
  ASSERT_EQ(node0.GetState()[1].vector_clock, clock1);

  node1.Tick();
  ASSERT_EQ(node1.GetState().size(), 2);
  ASSERT_EQ(node1.GetState()[1].payload, payload1);
  ASSERT_EQ(node1.GetState()[1].vector_clock, clock1);

  node2.Tick();
  ASSERT_EQ(node2.GetState().size(), 2);
  ASSERT_EQ(node2.GetState()[0].payload, payload0);
  ASSERT_EQ(node2.GetState()[0].vector_clock, clock0);
  ASSERT_EQ(node2.GetState()[1].payload, payload1);
  ASSERT_EQ(node2.GetState()[1].vector_clock, clock1);

  node0.Tick();
  ASSERT_EQ(node0.GetState().size(), 3);
  ASSERT_EQ(node0.GetState()[2].payload, payload2);
  ASSERT_EQ(node0.GetState()[2].vector_clock, clock2);

  node2.Tick();
  ASSERT_EQ(node2.GetState().size(), 3);
  ASSERT_EQ(node2.GetState()[2].payload, payload2);
  ASSERT_EQ(node2.GetState()[2].vector_clock, clock2);

  node1.Tick();
  ASSERT_EQ(node1.GetState().size(), 3);
  ASSERT_EQ(node1.GetState()[2].payload, payload2);
  ASSERT_EQ(node1.GetState()[2].vector_clock, clock2);
}

} // namespace
} // namespace hw3::broadcast
