#pragma once

#include "hw1/src/common/task.h"
#include "hw1/src/common/types.h"
#include <thread>
#include <vector>

namespace integral {

class Leader {
public:
  explicit Leader(uint16_t discovery_port);
  ~Leader();

  double GetResult(double a, double b);

  void Run();
  void Stop();

private:
  void SendHeartbeats();
  void ReceiveHeartbeats();

  void SendTask(const Task &, WorkerId);

  void HandleTaskResult(const TaskResult, TaskResult);

  const uint16_t discovery_port_;
  const int discovery_socket_;

  std::atomic<bool> is_stopped_{false};
  std::vector<std::jthread> threads_;

  static constexpr std::string_view kLeaderMessage = "Leader";
};

} // namespace integral
