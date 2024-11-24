#include "hw1/src/common/integral.h"
#include "hw1/src/common/ensure.h"

namespace integral {

double EvaluateIntegral(double a, double b, uint32_t points,
                        const Function &function) {
  ENSURE(a < b);

  double result_raw = 0;
  double step_length = (b - a) / points;
  for (double x = a + step_length / 2; x < b; x += step_length) {
    result_raw += function(x);
  }
  double result_scaled = result_raw * step_length;
  return result_scaled;
}

} // namespace integral
