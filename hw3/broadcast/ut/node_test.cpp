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

class NodeTest : public ::testing::Test {};

TEST(NodeTest, Trivial) {
  hw3::broadcast::Node node(0, {});
  Payload payload;
  payload.data.emplace_back("a", "b");

  ASSERT_EQ(node.GetInternalState().size(), 0);
  node.AppendNewPayload(payload);
  ASSERT_EQ(node.GetInternalState().size(), 0);
  node.Tick();
  ASSERT_EQ(node.GetInternalState().size(), 1);
  auto state = node.GetInternalState();
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
  ASSERT_EQ(node0.GetInternalState().size(), 0);

  node1.Tick();
  ASSERT_EQ(node1.GetInternalState().size(), 1);
  ASSERT_EQ(node1.GetInternalState()[0].payload, payload0);
  ASSERT_EQ(node1.GetInternalState()[0].vector_clock, clock0);

  node0.Tick();
  ASSERT_EQ(node0.GetInternalState().size(), 2);
  ASSERT_EQ(node0.GetInternalState()[0].payload, payload0);
  ASSERT_EQ(node0.GetInternalState()[0].vector_clock, clock0);
  ASSERT_EQ(node0.GetInternalState()[1].payload, payload1);
  ASSERT_EQ(node0.GetInternalState()[1].vector_clock, clock1);

  node1.Tick();
  ASSERT_EQ(node1.GetInternalState().size(), 2);
  ASSERT_EQ(node1.GetInternalState()[1].payload, payload1);
  ASSERT_EQ(node1.GetInternalState()[1].vector_clock, clock1);

  node2.Tick();
  ASSERT_EQ(node2.GetInternalState().size(), 2);
  ASSERT_EQ(node2.GetInternalState()[0].payload, payload0);
  ASSERT_EQ(node2.GetInternalState()[0].vector_clock, clock0);
  ASSERT_EQ(node2.GetInternalState()[1].payload, payload1);
  ASSERT_EQ(node2.GetInternalState()[1].vector_clock, clock1);

  node0.Tick();
  ASSERT_EQ(node0.GetInternalState().size(), 3);
  ASSERT_EQ(node0.GetInternalState()[2].payload, payload2);
  ASSERT_EQ(node0.GetInternalState()[2].vector_clock, clock2);

  node2.Tick();
  ASSERT_EQ(node2.GetInternalState().size(), 3);
  ASSERT_EQ(node2.GetInternalState()[2].payload, payload2);
  ASSERT_EQ(node2.GetInternalState()[2].vector_clock, clock2);

  node1.Tick();
  ASSERT_EQ(node1.GetInternalState().size(), 3);
  ASSERT_EQ(node1.GetInternalState()[2].payload, payload2);
  ASSERT_EQ(node1.GetInternalState()[2].vector_clock, clock2);
}

TEST(NodeTest, CausalOrder) {
  constexpr int kTotalNodes = 5;

  std::vector<std::vector<std::shared_ptr<TrivialMessageSender>>>
      trivial_senders(
          kTotalNodes,
          std::vector<std::shared_ptr<TrivialMessageSender>>(kTotalNodes));
  std::vector<std::vector<std::shared_ptr<BufferingMessageSender>>>
      lazy_senders(
          kTotalNodes,
          std::vector<std::shared_ptr<BufferingMessageSender>>(kTotalNodes));

  for (size_t i = 0; i < kTotalNodes; ++i) {
    for (size_t j = 0; j < kTotalNodes; ++j) {
      trivial_senders[i][j] = std::make_shared<TrivialMessageSender>();
      lazy_senders[i][j] =
          std::make_shared<BufferingMessageSender>(trivial_senders[i][j], 1);
    }
  }

  std::vector<std::unique_ptr<hw3::broadcast::Node>> nodes;
  for (size_t i = 0; i < kTotalNodes; ++i) {
    std::map<NodeId, std::shared_ptr<IMessageSender>> channels;
    for (size_t j = 0; j < kTotalNodes; ++j) {
      if (i == j) {
        continue;
      }
      channels[j] = lazy_senders[i][j];
    }

    nodes.emplace_back(std::make_unique<Node>(i, channels));

    for (size_t j = 0; j < kTotalNodes; ++j) {
      trivial_senders[j][i]->Init(nodes[i].get());
    }
  }

  Payload payload;
  payload.data.emplace_back("0", "0");

  for (int l : {0, 1, 2}) {
    for (int r : {3, 4}) {
      lazy_senders[l][r]->ChangeBufferSize(10'000);
    }
  }

  nodes[0]->AppendNewPayload(payload);
  for (int iter = 0; iter < 3; ++iter) {
    for (int i = 0; i < 5; ++i) {
      nodes[i]->Tick();
    }
  }

  ASSERT_EQ(nodes[0]->GetInternalState().size(), 1);
  ASSERT_EQ(nodes[1]->GetInternalState().size(), 1);
  ASSERT_EQ(nodes[2]->GetInternalState().size(), 1);
  ASSERT_EQ(nodes[3]->GetInternalState().size(), 0);
  ASSERT_EQ(nodes[4]->GetInternalState().size(), 0);

  for (int l : {1, 2}) {
    for (int r : {3, 4}) {
      lazy_senders[l][r]->LoseMessages();
      lazy_senders[l][r]->ChangeBufferSize(1);
    }
  }

  Payload payload1;
  payload1.data.emplace_back("1", "1");
  nodes[2]->AppendNewPayload(payload1);
  for (int iter = 0; iter < 3; ++iter) {
    for (int i : {2, 3, 4}) {
      nodes[i]->Tick();
    }
  }

  ASSERT_EQ(nodes[0]->GetInternalState().size(), 1);
  ASSERT_EQ(nodes[1]->GetInternalState().size(), 1);
  ASSERT_EQ(nodes[2]->GetInternalState().size(), 2);
  ASSERT_EQ(nodes[3]->GetInternalState().size(), 0);
  ASSERT_EQ(nodes[4]->GetInternalState().size(), 0);

  for (int r : {3, 4}) {
    lazy_senders[0][r]->ChangeBufferSize(1);
  }

  for (int iter = 0; iter < 3; ++iter) {
    for (int i : {0, 3, 4}) {
      nodes[i]->Tick();
    }
  }

  ASSERT_EQ(nodes[0]->GetInternalState().size(), 2);
  ASSERT_EQ(nodes[1]->GetInternalState().size(), 1);
  ASSERT_EQ(nodes[2]->GetInternalState().size(), 2);
  ASSERT_EQ(nodes[3]->GetInternalState().size(), 2);
  ASSERT_EQ(nodes[4]->GetInternalState().size(), 2);

  nodes[1]->Tick();
  ASSERT_EQ(nodes[1]->GetInternalState().size(), 2);

  auto state = nodes[0]->GetInternalState();
  VectorClock expeceted_vector_clock0 = {1, 0, 0, 0, 0};
  VectorClock expeceted_vector_clock1 = {1, 0, 1, 0, 0};

  ASSERT_EQ(state[0].vector_clock, expeceted_vector_clock0);
  ASSERT_EQ(state[1].vector_clock, expeceted_vector_clock1);
}

} // namespace
} // namespace hw3::broadcast
