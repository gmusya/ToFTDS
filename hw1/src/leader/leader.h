#pragma once

#include "hw1/src/common/task.h"
#include "hw1/src/common/types.h"

namespace integral {

class Leader {
public:
  explicit Leader(uint16_t discovery_port);

  double GetResult(double a, double b);

private:
  void SendTask(const Task &, WorkerId);

  void HandleTaskResult(const TaskResult, TaskResult);

  const int discovery_socket_;
};

} // namespace integral
