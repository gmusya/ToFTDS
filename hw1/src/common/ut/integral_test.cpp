#include "gtest/gtest.h"

#include "hw1/src/common/integral.h"

namespace integral {
namespace {

class IntegralTest : public ::testing::Test {};

Function GetLinearFunction(double k, double b) {
  return [k, b](double x) -> double { return k * x + b; };
}

double GetLinearFunctionResult(double a, double b, const Function &function) {
  return (function(a) + function(b)) * (b - a) / 2;
}

// TODO: check for non-linear function
TEST(IntegralTest, Trivial) {
  const std::vector<std::pair<double, double>> func_params = {
      {0, 5}, {-1, 3}, {10, 10}, {10, 0}};
  const std::vector<std::pair<double, double>> integral_borders = {{0, 2},
                                                                   {5, 10}};

  for (const auto &[k, x] : func_params) {
    const auto func = GetLinearFunction(k, x);
    for (const auto &[a, b] : integral_borders) {
      const auto correct_result = GetLinearFunctionResult(a, b, func);

      const auto approx_result = EvaluateIntegral(a, b, 3, func);
      EXPECT_NEAR(approx_result, correct_result, 1e-6);
    }
  }
}

} // namespace
} // namespace integral
