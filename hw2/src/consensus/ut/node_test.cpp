#include "gtest/gtest.h"
#include <thread>

#include "hw2/src/common/timeout.h"
#include "hw2/src/consensus/node.h"

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

} // namespace
} // namespace hw2::consensus
