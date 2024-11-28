#include "hw1/src/leader/leader.h"
#include "hw1/src/common/ensure.h"
#include "hw1/src/common/types.h"
#include "hw1/src/socket/socket.h"

#include <algorithm>
#include <asm-generic/socket.h>
#include <cerrno>
#include <chrono>
#include <iostream>
#include <mutex>
#include <netinet/in.h>
#include <poll.h>
#include <stdexcept>
#include <sys/poll.h>
#include <sys/socket.h>
#include <thread>
#include <unistd.h>

namespace integral {

Leader::Leader(const uint16_t discovery_port, const uint32_t max_workers)
    : discovery_port_(discovery_port),
      discovery_socket_(socket(AF_INET, SOCK_DGRAM, 0)) {
  {
    ENSURE(discovery_socket_ != -1);
    constexpr int kBroadcastOptVal = 1;
    HANDLE_C_ERROR(setsockopt(discovery_socket_, SOL_SOCKET, SO_BROADCAST,
                              &kBroadcastOptVal, sizeof(kBroadcastOptVal)));
  }
  worker_fds_.resize(max_workers);
  for (int i = 0; i < max_workers; ++i) {
    worker_fds_[i].fd = -1;
    worker_fds_[i].events = POLLIN | POLLHUP | POLLERR | POLLOUT;
  }
}

void Leader::Stop() {
  if (is_stopped_.load()) {
    return;
  }
  is_stopped_.store(true);
  threads_.clear();
}

Leader::~Leader() { Stop(); }

void Leader::Run() {
  threads_.emplace_back(&Leader::SendDiscoveryMessage, this);
  threads_.emplace_back(&Leader::ReceiveDiscoveryMessage, this);
  threads_.emplace_back(&Leader::RunOverWorkers, this);
}

void Leader::RunOverWorkers() {
  while (!is_stopped_.load()) {
    // find new workers
    std::map<WorkerId, HeartbeatMessage> upd;
    {
      std::lock_guard lg(hb_updates_mutex_);
      upd = std::move(hb_updates_);
    }

    {
      std::vector<std::pair<double, double>> new_tasks;
      {
        std::lock_guard lg(new_queries_mutex_);
        new_tasks = std::move(new_tasks_);
      }

      for (auto &[a, b] : new_tasks) {
        QueryInfo info;
        info.result = 0;
        info.id = queries_.size();
        int slices = 10;
        for (int i = 0; i < slices; ++i) {
          info.all_tasks.emplace_back(a + (b - a) / slices * i,
                                      a + (b - a) / slices * (i + 1));
          info.tasks_to_do.insert(i);
        }
        queries_.emplace_back(std::move(info));
      }
    }

    std::vector<WorkerId> new_workers_;
    // std::cerr << "Known workers = " << known_workers_.size() << std::endl;

    WorkerId unused_id = 0;
    for (auto &[wid, hb] : upd) {
      if (known_workers_.contains(wid)) {
        known_workers_.at(wid).last_heartbeat = hb;
      } else {
        while (unused_id < worker_fds_.size() &&
               worker_fds_[unused_id].fd != -1) {
          ++unused_id;
        }
        if (unused_id == worker_fds_.size()) {
          continue;
        }
        std::cerr << "Trying to add worker " << wid << std::endl;
        WorkerInfo info;
        info.id = wid;
        info.last_heartbeat = hb;
        info.position_in_buffer = unused_id;
        worker_fds_[unused_id].fd = socket(AF_INET, SOCK_STREAM, 0);

        ENSURE(worker_fds_[unused_id].fd != -1);
        {
          sockaddr_in address{};
          address.sin_family = AF_INET;
          address.sin_port = htons(wid);
          address.sin_addr.s_addr = INADDR_ANY;
          auto res = connect(worker_fds_[unused_id].fd,
                             reinterpret_cast<const sockaddr *>(&address),
                             sizeof(address));
          ++unused_id;
          known_workers_.emplace(wid, info);
          new_workers_.emplace_back(wid);
          std::cerr << "Add worker " << wid << std::endl;

          if (res == -1) {
            if (errno == EINPROGRESS) {
              std::cerr << "Connection for " << wid
                        << " is in progress (fnum = " << unused_id << ")"
                        << std::endl;
              continue;
            }
            HANDLE_C_ERROR(-1);
          }
        }
      }
    }

    // collect results
    for (auto &pfd : worker_fds_) {
      pfd.events = POLLIN | POLLHUP | POLLERR | POLLOUT;
    }

    auto workers_to_remove =
        [&worker_info = this->known_workers_]() -> std::vector<WorkerId> {
      std::vector<WorkerId> result;
      for (const auto &[id, info] : worker_info) {
        auto time_without_hbs =
            std::chrono::duration_cast<std::chrono::milliseconds>(
                std::chrono::steady_clock::now().time_since_epoch()) -
            info.last_heartbeat.timestamp;
        // if (time_without_hbs > std::chrono::milliseconds(5)) {
        //   result.emplace_back(id);
        // }
      }
      return result;
    }();

    for (auto &w : wid_to_delete_) {
      if (known_workers_.contains(w)) {
        workers_to_remove.emplace_back(w);
      }
    }

    // dedup
    std::sort(workers_to_remove.begin(), workers_to_remove.end());
    workers_to_remove.resize(
        std::unique(workers_to_remove.begin(), workers_to_remove.end()) -
        workers_to_remove.begin());

    wid_to_delete_.clear();

    std::vector<std::pair<QueryId, TaskId>> new_tasks;

    // remove non-responsive workers
    for (auto worker_to_remove : workers_to_remove) {
      auto &info = this->known_workers_.at(worker_to_remove);
      close(worker_fds_[info.position_in_buffer].fd);
      worker_fds_[info.position_in_buffer].fd = -1;

      new_tasks.insert(new_tasks.end(), info.assigned_tasks.begin(),
                       info.assigned_tasks.end());
      known_workers_.erase(worker_to_remove);
      std::cerr << "Removed worker " << worker_to_remove << std::endl;
    }

    for (auto &[query_id, task_id] : new_tasks) {
      if (known_workers_.empty()) {
        queries_[query_id].tasks_in_progress.erase(task_id);
        queries_[query_id].tasks_to_do.insert(task_id);
      }
    }

    // reassign tasks
    std::set<std::pair<uint32_t /* tasks count */, WorkerId>> workers;

    if (!known_workers_.empty()) {
      for (auto &query : queries_) {
        if (!query.tasks_to_do.empty()) {
          for (auto task_id : query.tasks_to_do) {
            new_tasks.emplace_back(query.id, task_id);
          }
          std::swap(query.tasks_to_do, query.tasks_in_progress);
        }
      }

      for (auto &[worker_id, info] : known_workers_) {
        workers.emplace(info.assigned_tasks.size(), worker_id);
      }

      for (auto &[query_id, task_id] : new_tasks) {
        auto it = workers.begin();
        auto [sz, wid] = *it;
        workers.erase(it);
        ++sz;
        known_workers_[wid].assigned_tasks.emplace_back(query_id, task_id);
        workers.emplace(sz, wid);
        std::cerr << "Reassigned task (" << query_id << ", " << task_id
                  << ") to " << wid << std::endl;
      }
    }

    HANDLE_C_ERROR(poll(worker_fds_.data(), worker_fds_.size(), 100));
    constexpr int kMaxDataBufSize = 1024;
    char data_buf[kMaxDataBufSize];
    for (size_t i = 0; i < worker_fds_.size(); ++i) {
      auto &pfd = worker_fds_[i];
      if (pfd.fd == -1) {
        continue;
      }
      WorkerId wid = worker_fds_.size();
      for (auto &[another_wid, info] : known_workers_) {
        if (info.position_in_buffer == i) {
          wid = another_wid;
          break;
        }
      }
      std::cerr << "Looking at " << wid << " (i = " << i << ", fd = " << pfd.fd
                << ")" << std::endl;
      ENSURE(wid != workers.size());

      if (pfd.revents & (POLLERR | POLLHUP)) {
        std::cerr << "Closed connection with " << wid << std::endl;
        wid_to_delete_.emplace_back(wid);
        close(pfd.fd);
      }
      if (pfd.revents & (POLLIN)) {
        auto res = recv(pfd.fd, data_buf, kMaxDataBufSize, 0);
        // HANDLE_C_ERROR(res);
        if (res == 0 || res == -1) {
          std::cerr << "Closed connection with " << wid << std::endl;
          wid_to_delete_.emplace_back(wid);
          close(pfd.fd);
        } else {
          std::stringstream ss(std::string(data_buf, res));
          double result;
          ss >> result;
          QueryId qid;
          TaskId tid;
          ss >> qid >> tid;

          if (queries_[qid].tasks_in_progress.contains(tid)) {
            known_workers_[wid].assigned_tasks.erase(known_workers_[wid].assigned_tasks.begin());
            std::cerr << "(" << qid << ", " << tid << ") is " << result
                      << std::endl;
            queries_[qid].result += result;
            queries_[qid].tasks_in_progress.erase(tid);
            if (queries_[qid].tasks_in_progress.empty() &&
                queries_[qid].tasks_to_do.empty()) {
              std::cerr << "QUERY " << qid << " is solved "
                        << queries_[qid].result << std::endl;
            }
          }
        }
      }
      if (pfd.revents & (POLLOUT)) {
        // std::cerr << "Has data to write " << std::endl;
        auto &tasks = known_workers_[wid].assigned_tasks;
        if (tasks.empty()) {
          //   std::cerr << "No tasks to do for " << wid << std::endl;
          continue;
        }
        auto task = tasks[0];
        // tasks.erase(tasks.begin());
        std::stringstream oss;
        QueryId qid = task.first;
        TaskId tid = task.second;
        double a = queries_[qid].all_tasks[tid].a;
        double b = queries_[qid].all_tasks[tid].b;
        oss << a << ' ' << b << ' ' << qid << ' ' << tid;
        std::cerr << "Sending task from " << a << " to " << b << " (" << qid
                  << ", " << tid << ") to " << wid << std::endl;
        std::string res = oss.str();
        auto bytes_written = 0;

        auto qwe = send(pfd.fd, res.data(), res.size(), 0);
        if (qwe == -1) {
          std::cerr << "Closed connection with " << wid << std::endl;
          wid_to_delete_.emplace_back(wid);
          close(pfd.fd);
          continue;
        }
        // HANDLE_C_ERROR(qwe);
        if (qwe != res.size()) {
          // TODO: fix
          std::cerr << "Internal error :(" << std::endl;
        }
      }
    }

    std::this_thread::sleep_for(std::chrono::milliseconds(1000));
  }
}

void Leader::SendDiscoveryMessage() {
  while (!is_stopped_.load()) {
    std::cerr << "Sending discovery messages..." << std::endl;
    sockaddr_in address{};
    address.sin_family = AF_INET;
    address.sin_port = htons(discovery_port_);
    address.sin_addr.s_addr = INADDR_BROADCAST;
    HANDLE_C_ERROR(sendto(
        discovery_socket_, kLeaderMessage.data(), kLeaderMessage.size(), 0,
        reinterpret_cast<const sockaddr *>(&address), sizeof(address)));

    std::this_thread::sleep_for(std::chrono::seconds(5));
  }
}

void Leader::ReceiveDiscoveryMessage() {
  while (!is_stopped_.load()) {
    // std::cerr << "Waiting for discovery..." << std::endl;

    OnReceiveAsync(discovery_socket_, [&updates = this->hb_updates_,
                                       this](int, sockaddr_storage addr,
                                             socklen_t, std::string_view data) {
      if (data == kLeaderMessage) {
        return;
      }
      const auto port = std::stoi(std::string(data));

      std::cerr << "I am discovered by " << port << std::endl;

      std::lock_guard lg(hb_updates_mutex_);
      updates[port] = HeartbeatMessage{
          .sequence_number = 0,
          .timestamp = std::chrono::duration_cast<std::chrono::milliseconds>(
              std::chrono::steady_clock::now().time_since_epoch())};
    });

    {
      //   std::lock_guard lg(hb_updates_mutex_);
      //   std::cerr << "Updates sz = " << hb_updates_.size() << std::endl;
    }

    std::this_thread::sleep_for(std::chrono::seconds(1));
  }
}

double Leader::GetResult(const double a, const double b) {
  std::lock_guard lg(new_queries_mutex_);
  new_tasks_.emplace_back(a, b);

  return 0;
}

} // namespace integral
