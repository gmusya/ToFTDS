#include "gtest/gtest.h"

#include "hw2/src/consensus/node.h"

namespace hw2::consensus {
namespace {

class NodeTest : public ::testing::Test {};

// TODO: check for non-linear function
TEST(NodeTest, Trivial) {
  hw2::consensus::Node node(123, {});
}

} // namespace
} // namespace integral
