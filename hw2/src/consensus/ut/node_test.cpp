#include "gtest/gtest.h"
#include <future>
#include <string>
#include <thread>

#include "hw2/src/common/timeout.h"
#include "hw2/src/consensus/common.h"
#include "hw2/src/consensus/node.h"
#include "hw2/src/consensus/node_sender.h"
#include "hw2/src/consensus/state.h"

using namespace std::chrono_literals;

namespace hw2::consensus {
namespace {

class NodeTest : public ::testing::Test {};

TEST(NodeTest, Leader) {
  hw2::consensus::Node node(123, {},
                            std::make_shared<hw2::UniformTimer>(9ms, 10ms, 42));
  std::this_thread::sleep_for(15ms);
  node.Tick(5);
  EXPECT_EQ(node.GetRole(), NodeState::kLeader);
}

TEST(NodeTest, Trivial) {
  hw2::consensus::Node node(123, {},
                            std::make_shared<hw2::UniformTimer>(9ms, 10ms, 42));
  std::this_thread::sleep_for(15ms);
  node.Tick(5);
  EXPECT_EQ(node.GetRole(), NodeState::kLeader);
  std::cerr << "APPEND" << std::endl;
  auto future = node.AddCommand("I AM COMMAND");
  node.Tick(5);

  auto future_status = future.wait_for(0ms);
  ASSERT_EQ(future_status, std::future_status::ready);
  auto result = future.get();

  ASSERT_TRUE(result.CommandPersisted());

  node.Tick(5);
  auto future_commands = node.GetPersistentCommands(0, 10);
  node.Tick(5);
  ASSERT_EQ(future_status, std::future_status::ready);
  auto commands = future_commands.get();
  EXPECT_EQ(commands, (std::vector<Command>{"I AM COMMAND"}));
}

std::shared_ptr<hw2::UniformTimer> MakeTimer() {
  return std::make_shared<hw2::UniformTimer>(9ms, 10ms, 42);
}

TEST(NodeTest, ThreeNodes) {
  auto sender1 = std::make_shared<TrivialMessageSender>();
  auto sender2 = std::make_shared<TrivialMessageSender>();
  auto sender3 = std::make_shared<TrivialMessageSender>();

  hw2::consensus::Node node1(1, {{2, sender2}, {3, sender3}}, MakeTimer());
  hw2::consensus::Node node2(2, {{1, sender1}, {3, sender3}}, MakeTimer());
  hw2::consensus::Node node3(3, {{1, sender1}, {2, sender2}}, MakeTimer());

  sender1->Init(&node1);
  sender2->Init(&node2);
  sender3->Init(&node3);

  node1.Tick(5);
  std::this_thread::sleep_for(10ms);
  node1.Tick(5);

  node2.Tick(5);
  node1.Tick(5);
  node2.Tick(5);
  node1.Tick(5);
  node2.Tick(5);
  node1.Tick(5);

  auto future = node1.AddCommand("I AM COMMAND");
  auto future_status = future.wait_for(0ms);
  node1.Tick(5);
  node2.Tick(5);

  future_status = future.wait_for(0ms);
  ASSERT_EQ(future_status, std::future_status::timeout);

  node1.Tick(5);

  future_status = future.wait_for(0ms);
  ASSERT_EQ(future_status, std::future_status::ready);

  auto result = future.get();
  ASSERT_TRUE(result.CommandPersisted());

  node1.Tick(5);
  auto future_commands = node1.GetPersistentCommands(0, 10);
  node1.Tick(5);
  ASSERT_EQ(future_status, std::future_status::ready);
  auto commands = future_commands.get();
  EXPECT_EQ(commands, (std::vector<Command>{"I AM COMMAND"}));
}

TEST(NodeTest, ThrowOutUncommited) {
  auto sender1 = std::make_shared<TrivialMessageSender>();
  auto sender2 = std::make_shared<TrivialMessageSender>();
  auto sender3 = std::make_shared<TrivialMessageSender>();

  hw2::consensus::Node node1(1, {{2, sender2}, {3, sender3}}, MakeTimer());
  hw2::consensus::Node node2(2, {{1, sender1}, {3, sender3}}, MakeTimer());
  hw2::consensus::Node node3(3, {{1, sender1}, {2, sender2}}, MakeTimer());

  sender1->Init(&node1);
  sender2->Init(&node2);
  sender3->Init(&node3);

  {
    node1.Tick(5);
    std::this_thread::sleep_for(10ms);
    node1.Tick(5);

    node2.Tick(5);
    node1.Tick(5);
    node2.Tick(5);
    node1.Tick(5);

    auto future = node1.AddCommand("1s1");

    auto future_status = future.wait_for(0ms);
    ASSERT_EQ(future_status, std::future_status::timeout);

    node2.Tick(5);
    node1.Tick(5);
    node2.Tick(5);
    node1.Tick(5);

    future_status = future.wait_for(0ms);
    ASSERT_EQ(future_status, std::future_status::ready);
  }
  {
    node3.AddCommand("3s1");
    std::vector<std::future<Node::AddCommandResult>> futures1;
    std::vector<std::future<Node::AddCommandResult>> futures2;
    std::vector<std::future<Node::AddCommandResult>> futures3;
    for (int i = 0; i < 100; ++i) {
      futures1.emplace_back(node1.AddCommand("A"));
      futures2.emplace_back(node2.AddCommand("B"));
      futures3.emplace_back(node3.AddCommand("C"));
    }

    for (int j = 0; j < 100; ++j) {
      ASSERT_TRUE(futures1[j].wait_for(0ms) == std::future_status::timeout);
      ASSERT_TRUE(futures2[j].wait_for(0ms) == std::future_status::ready);
      ASSERT_TRUE(futures3[j].wait_for(0ms) == std::future_status::ready);
    }
  }
}

TEST(NodeTest, LeaderChanges) {
  auto sender1 = std::make_shared<TrivialMessageSender>();
  auto sender2 = std::make_shared<TrivialMessageSender>();
  auto sender3 = std::make_shared<TrivialMessageSender>();

  hw2::consensus::Node node1(1, {{2, sender2}, {3, sender3}}, MakeTimer());
  hw2::consensus::Node node2(2, {{1, sender1}, {3, sender3}}, MakeTimer());
  hw2::consensus::Node node3(3, {{1, sender1}, {2, sender2}}, MakeTimer());

  sender1->Init(&node1);
  sender2->Init(&node2);
  sender3->Init(&node3);

  node1.Tick(5);
  std::this_thread::sleep_for(10ms);
  node1.Tick(2);
  for (int i = 0; i < 5; ++i) {
    node1.Tick(5);
    node2.Tick(5);
  }

  EXPECT_EQ(node1.GetRole(), NodeState::kLeader);
  EXPECT_EQ(node2.GetRole(), NodeState::kFollower);

  std::vector<Command> expected_commands;
  std::vector<std::future<Node::AddCommandResult>> good_futures;
  std::vector<std::future<Node::AddCommandResult>> bad_futures;

  node3.Tick(1);
  std::this_thread::sleep_for(20ms);

  for (int i = 0; i < 5; ++i) {
    node3.Tick(5);
    node2.Tick(5);
  }

  EXPECT_EQ(node2.GetRole(), NodeState::kFollower);
  EXPECT_EQ(node3.GetRole(), NodeState::kLeader);
  for (int j = 0; j < 3; ++j) {
    good_futures.emplace_back(node3.AddCommand("g" + std::to_string(j)));
    bad_futures.emplace_back(node2.AddCommand("b" + std::to_string(j)));

    node2.Tick();
    node3.Tick();
    node2.Tick();
    node3.Tick();
    node2.Tick();
    node3.Tick();
    node2.Tick();
    node3.Tick();
    node2.Tick();
    node3.Tick();
    node2.Tick();
    node3.Tick();

    expected_commands.emplace_back("g" + std::to_string(j));

    std::this_thread::sleep_for(11ms);
    node2.Tick();
    EXPECT_EQ(node2.GetRole(), NodeState::kCandidate);
    EXPECT_EQ(node3.GetRole(), NodeState::kLeader);
    node3.Tick();
    EXPECT_EQ(node2.GetRole(), NodeState::kCandidate);
    EXPECT_EQ(node3.GetRole(), NodeState::kFollower);
    node2.Tick();
    node3.Tick();
    node2.Tick();
    EXPECT_EQ(node2.GetRole(), NodeState::kLeader);
    EXPECT_EQ(node3.GetRole(), NodeState::kFollower);

    std::this_thread::sleep_for(11ms);
    node3.Tick();
    node2.Tick();
    node3.Tick();
    node2.Tick();
    node3.Tick();
    node2.Tick();
    node3.Tick();
    node2.Tick();
  }

  for (auto &elem : good_futures) {
    ASSERT_EQ(elem.wait_for(0ms), std::future_status::ready);
    ASSERT_TRUE(elem.get().CommandPersisted());
  }

  for (auto &elem : bad_futures) {
    ASSERT_EQ(elem.wait_for(0ms), std::future_status::ready);
    ASSERT_FALSE(elem.get().CommandPersisted());
  }

  EXPECT_EQ(node1.GetRole(), NodeState::kLeader); // it is stale
  std::cerr << "Command send" << std::endl;
  auto future = node1.AddCommand("I AM STALE COMMAND");
  for (int i = 0; i < 5; ++i) {
    node1.Tick();
    node2.Tick();
    node3.Tick();
  }

  ASSERT_EQ(future.wait_for(0ms), std::future_status::ready);
  auto val = future.get();
  EXPECT_FALSE(val.CommandPersisted());

  for (int i = 0; i < 10; ++i) {
    node1.Tick();
    node2.Tick();
    node3.Tick();
  }

  std::future<std::vector<Command>> commands1 =
      node1.GetPersistentCommands(0, 100);
  std::future<std::vector<Command>> commands2 =
      node2.GetPersistentCommands(0, 100);
  std::future<std::vector<Command>> commands3 =
      node3.GetPersistentCommands(0, 100);

  for (int i = 0; i < 10; ++i) {
    node1.Tick();
    node2.Tick();
    node3.Tick();
  }

  EXPECT_EQ(node1.GetRole(), NodeState::kFollower);
  EXPECT_EQ(node2.GetRole(), NodeState::kFollower);
  EXPECT_EQ(node3.GetRole(), NodeState::kLeader);

  ASSERT_EQ(commands1.wait_for(0ms), std::future_status::ready);
  ASSERT_EQ(commands2.wait_for(0ms), std::future_status::ready);
  ASSERT_EQ(commands3.wait_for(0ms), std::future_status::ready);

  ASSERT_EQ(commands1.get(), expected_commands);
  ASSERT_EQ(commands2.get(), expected_commands);
  ASSERT_EQ(commands3.get(), expected_commands);
}

} // namespace
} // namespace hw2::consensus
