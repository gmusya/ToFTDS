#pragma once

#include "hw2/src/common/log.h"
#include <chrono>
#include <random>
#include <sstream>

namespace hw2 {

class ITimeout {
public:
  virtual void Reset() = 0;
  virtual bool IsExpired() const = 0;
};

class UniformTimer : public ITimeout {
public:
  UniformTimer(std::chrono::milliseconds min, std::chrono::milliseconds max,
               uint64_t seed)
      : rnd_(seed), uid_ms_(min.count(), max.count()), expires_at_(0) {}

  void Reset() override {
    const auto period = std::chrono::milliseconds(uid_ms_(rnd_));
    expires_at_ = Now() + period;
  }

  bool IsExpired() const override {
    std::stringstream ss;
    ss << "Now = " << Now() << ", expires_at = " << expires_at_;
    LOG(ss.str());
    return Now() > expires_at_;
  }

private:
  static std::chrono::milliseconds Now() {
    return std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::steady_clock::now().time_since_epoch());
  }

  std::mt19937 rnd_;
  std::uniform_int_distribution<int64_t> uid_ms_;
  std::chrono::milliseconds expires_at_;
};

} // namespace hw2
