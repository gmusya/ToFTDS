#pragma once

#include "hw3/broadcast/timeout.h"

namespace hw3::broadcast {

class ITimeoutBuilder {
public:
  virtual std::shared_ptr<ITimeout> Build() = 0;
  virtual ~ITimeoutBuilder() = default;
};

class UniformTimerBuilder : public ITimeoutBuilder {
public:
  UniformTimerBuilder(std::chrono::milliseconds min,
                      std::chrono::milliseconds max, uint64_t seed)
      : min_(min), max_(max), seed_(seed) {}

  std::shared_ptr<ITimeout> Build() override {
    return std::make_shared<UniformTimer>(min_, max_, seed_);
  }

private:
  const std::chrono::milliseconds min_;
  const std::chrono::milliseconds max_;
  const uint64_t seed_;
};

class AttemptsTimerBuilder : public ITimeoutBuilder {
public:
  AttemptsTimerBuilder(uint32_t attempts_period)
      : attempts_period_(attempts_period) {}

  std::shared_ptr<ITimeout> Build() override {
    return std::make_shared<AttemptsTimer>(attempts_period_);
  }

private:
  const uint32_t attempts_period_;
};

} // namespace hw3::broadcast
