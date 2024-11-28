#pragma once

#include <cstdint>
#include <functional>

namespace integral {

using Function = std::function<double(double)>;

double EvaluateIntegral(double a, double b, uint32_t points,
                        const Function &function);

} // namespace integral
