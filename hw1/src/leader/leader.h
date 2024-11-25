#pragma once

#include "hw1/src/common/task.h"
#include "hw1/src/common/types.h"

#include <chrono>
#include <map>
#include <poll.h>
#include <set>
#include <thread>
#include <vector>

namespace integral {

using QueryId = uint32_t;

struct HeartbeatMessage {
  uint64_t sequence_number = 0;
  std::chrono::milliseconds timestamp{};
};

struct WorkerInfo {
  HeartbeatMessage last_heartbeat = {};
  WorkerId id;
  uint32_t position_in_buffer;
  std::vector<std::pair<QueryId, TaskId>> assigned_tasks;
};

struct QueryInfo {
  QueryId id;
  double result;
  std::vector<Task> all_tasks;
  std::set<TaskId> tasks_in_progress;
  std::set<TaskId> tasks_to_do;
};

class Leader {
public:
  explicit Leader(uint16_t discovery_port, uint32_t max_workers);
  ~Leader();

  double GetResult(double a, double b);

  void Run();
  void Stop();

private:
  void RunOverWorkers();
  void SendDiscoveryMessage();
  void ReceiveDiscoveryMessage();

  void SendTask(const Task &, WorkerId);

  void HandleTaskResult(const TaskResult, TaskResult);

  const uint16_t discovery_port_;
  const int discovery_socket_;

  std::vector<pollfd> worker_fds_;
  std::map<WorkerId, WorkerInfo> known_workers_;
  std::vector<QueryInfo> queries_;
  std::vector<WorkerId> wid_to_delete_;

  std::atomic<bool> is_stopped_{false};
  std::vector<std::jthread> threads_;

  // TODO: get rid of this mutex
  std::mutex hb_updates_mutex_;
  std::map<WorkerId, HeartbeatMessage> hb_updates_;

  static constexpr std::string_view kLeaderMessage = "Leader";
};

} // namespace integral
