#pragma once

#include <functional>
#include <cstdint>

namespace integral {

using Function = std::function<double(double)>;

double EvaluateIntegral(double a, double b, uint32_t points);

}
