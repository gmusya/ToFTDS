#pragma once

#include "hw1/src/common/task.h"
#include "hw1/src/common/types.h"
#include <thread>
#include <vector>

namespace integral {

class Worker {
public:
  Worker(const uint16_t discovery_port, const uint16_t workload_port);
  ~Worker();

  void Stop();

  void Run();
  void WaitForDiscovery();
  void WaitForWorkloadConnection();

private:
  void AcceptIncomingConnection();

  void HandleReceivedTask(const Task & /*, LeaderInfo */);

  const int discovery_socket_;
  const int workload_socket_;
  const uint16_t workload_port_;

  std::atomic<bool> is_stopped_{false};
  std::vector<std::jthread> threads_;
};

} // namespace integral
