#include "gtest/gtest.h"
#include <thread>

#include "hw2/src/common/timeout.h"
#include "hw2/src/consensus/node.h"
#include "hw2/src/consensus/node_sender.h"

using namespace std::chrono_literals;

namespace hw2::consensus {
namespace {

class NodeTest : public ::testing::Test {};

TEST(NodeTest, Trivial) {
  hw2::consensus::Node node(123, {},
                            std::make_shared<hw2::UniformTimer>(9ms, 10ms, 42));
  auto future = node.AddCommand("I AM COMMAND");
  node.Tick(5);
  std::this_thread::sleep_for(10ms);
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

  auto future = node1.AddCommand("I AM COMMAND");
  node1.Tick(5);
  std::this_thread::sleep_for(10ms);
  node1.Tick(5);

  auto future_status = future.wait_for(0ms);
  ASSERT_EQ(future_status, std::future_status::timeout);

  node2.Tick(5);
  node1.Tick(5);
  node2.Tick(5);
  node1.Tick(5);
  node2.Tick(5);
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

} // namespace
} // namespace hw2::consensus
