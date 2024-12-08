#pragma once

#include "hw2/src/common/query.h"
#include <cstdint>
#include <optional>

namespace hw2 {

using WorkerId = uint32_t;

class Worker {
public:
  void Run();
  void Stop();

  std::optional<Value> Get(Key key);

  // returns std::nullopt if operation was successfull
  // returns WorkerId of current master if operation was not successfull
  std::optional<WorkerId> Set(Key key, Value value);
};

} // namespace hw2
