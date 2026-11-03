#include "processes.hpp"

#include "calculations.hpp"

#include <algorithm>
#include <cerrno>
#include <chrono>
#include <csignal>
#include <cstring>
#include <fcntl.h>
#include <limits>
#include <poll.h>
#include <stdexcept>
#include <unistd.h>
#include <sys/wait.h>

namespace chernov {
  namespace detail {
    struct Task {
      size_t calc_id;
      size_t count;
      size_t threads;
      size_t tries;
    };

    struct Result {
      size_t calc_id;
      double milliseconds;
      double area;
    };

    size_t recvAll(int &err, int rd, char *buffer, size_t size)
    {
      err = 0;
      size_t received = 0;
      while (received < size) {
        const size_t chunk = std::min(size - received, static_cast< size_t >(std::numeric_limits< ssize_t >::max()));
        const ssize_t count = read(rd, buffer + received, chunk);
        if (count == -1) {
          if (errno == EINTR) {
            continue;
          }
          err = errno;
          break;
        }
        if (count == 0) {
          break;
        }
        received += static_cast< size_t >(count);
      }
      return received;
    }

    size_t sendAll(int &err, int wr, const char *buffer, size_t size)
    {
      err = 0;
      size_t sent = 0;
      while (sent < size) {
        const size_t chunk = std::min(size - sent, static_cast< size_t >(std::numeric_limits< ssize_t >::max()));
        const ssize_t count = write(wr, buffer + sent, chunk);
        if (count == -1) {
          if (errno == EINTR) {
            continue;
          }
          err = errno;
          break;
        }
        if (count == 0) {
          err = EIO;
          break;
        }
        sent += static_cast< size_t >(count);
      }
      return sent;
    }

    void calculationLoop(int task_fd, int result_fd, size_t seed)
    {
      int exit_code = 0;
      try {
        struct sigaction action{};
        action.sa_handler = SIG_IGN;
        if (sigemptyset(&action.sa_mask) == -1 || sigaction(SIGPIPE, &action, nullptr) == -1) {
          close(task_fd);
          close(result_fd);
          _exit(1);
        }

        std::vector< ellipse_t > ellipses;
        while (true) {
          Task task{};
          int err = 0;
          const size_t received = recvAll(err, task_fd, reinterpret_cast< char * >(&task), sizeof(task));
          if (received == 0 && err == 0) {
            break;
          }
          if (err != 0 || received != sizeof(task) || task.count == 0 || task.threads == 0 || task.tries == 0) {
            exit_code = 1;
            break;
          }
          if (task.count > ellipses.max_size()
              || task.count > std::numeric_limits< size_t >::max() / sizeof(ellipse_t)) {
            exit_code = 1;
            break;
          }

          ellipses.resize(task.count);
          const size_t bytes = task.count * sizeof(ellipse_t);
          const size_t body_received = recvAll(err, task_fd, reinterpret_cast< char * >(ellipses.data()), bytes);
          if (err != 0 || body_received != bytes) {
            exit_code = 1;
            break;
          }

          const auto start = std::chrono::steady_clock::now();
          const double area = areaOf(ellipses, task.threads, task.tries, seed);
          const auto end = std::chrono::steady_clock::now();
          const double milliseconds = std::chrono::duration< double, std::milli >(end - start).count();

          Result result{task.calc_id, milliseconds, area};
          const size_t sent = sendAll(err, result_fd, reinterpret_cast< const char * >(&result), sizeof(result));
          if (err != 0 || sent != sizeof(result)) {
            exit_code = 1;
            break;
          }
        }
      } catch (...) {
        exit_code = 1;
      }

      close(task_fd);
      close(result_fd);
      _exit(exit_code);
    }
  }
}

void chernov::spawnProcess(App &app, const std::string &name, size_t seed)
{
  int to_child[2];
  int to_parent[2];
  if (pipe(to_child) == -1) {
    throw std::runtime_error("pipe error");
  }
  if (pipe(to_parent) == -1) {
    close(to_child[0]);
    close(to_child[1]);
    throw std::runtime_error("pipe error");
  }

  const pid_t pid = fork();
  if (pid == -1) {
    close(to_child[0]);
    close(to_child[1]);
    close(to_parent[0]);
    close(to_parent[1]);
    throw std::runtime_error("fork failed");
  }
  if (pid == 0) {
    close(to_child[1]);
    close(to_parent[0]);
    for (const auto &process : app.processes) {
      if (process.second.result_fd >= 0) {
        close(process.second.result_fd);
      }
      if (process.second.task_fd >= 0) {
        close(process.second.task_fd);
      }
    }
    detail::calculationLoop(to_child[0], to_parent[1], seed);
  }

  close(to_child[0]);
  close(to_parent[1]);
  const int flags = fcntl(to_parent[0], F_GETFL);
  if (flags == -1 || fcntl(to_parent[0], F_SETFL, flags | O_NONBLOCK) == -1) {
    close(to_child[1]);
    close(to_parent[0]);
    pid_t result = 0;
    do {
      result = waitpid(pid, nullptr, 0);
    } while (result == -1 && errno == EINTR);
    throw std::runtime_error("fcntl failed");
  }

  process_t process{pid, seed, to_child[1], to_parent[0], ProcessState::Active, "", {}};
  try {
    app.processes.insert({name, std::move(process)});
  } catch (...) {
    close(to_child[1]);
    close(to_parent[0]);
    pid_t result = 0;
    do {
      result = waitpid(pid, nullptr, 0);
    } while (result == -1 && errno == EINTR);
    throw;
  }
}

void chernov::startNextCalculation(App &app, process_t &process)
{
  if (process.state != ProcessState::Active || !process.current_calc_name.empty() || process.pending_calcs.empty()) {
    return;
  }
  const std::string calc_name = process.pending_calcs.front();
  calc_t &calc = app.calcs.at(calc_name);
  detail::Task task{calc.id, calc.ellipses.size(), calc.threads, calc.tries};
  if (task.count > calc.ellipses.max_size() || task.count > std::numeric_limits< size_t >::max() / sizeof(ellipse_t)) {
    throw std::runtime_error("overload");
  }

  process.current_calc_name = calc_name;
  int err = 0;
  const size_t sent = detail::sendAll(err, process.task_fd, reinterpret_cast< const char * >(&task), sizeof(task));
  bool failed = err != 0 || sent != sizeof(task);
  if (!failed) {
    const size_t bytes = task.count * sizeof(ellipse_t);
    const size_t body_sent
        = detail::sendAll(err, process.task_fd, reinterpret_cast< const char * >(calc.ellipses.data()), bytes);
    failed = err != 0 || body_sent != bytes;
  }
  if (failed) {
    if (process.task_fd >= 0) {
      close(process.task_fd);
    }
    process.task_fd = -1;
    process.state = ProcessState::Stopping;
    calc.state = TaskState::Failed;
    for (const std::string &pending_name : process.pending_calcs) {
      app.calcs.at(pending_name).state = TaskState::Failed;
    }
    process.pending_calcs.clear();
    process.current_calc_name.clear();
    return;
  }

  calc.state = TaskState::Running;
  process.pending_calcs.pop_front();
}

void chernov::receiveAvailableResult(App &app, process_t &process)
{
  if (process.result_fd < 0) {
    return;
  }

  auto close_channels = [&]()
  {
    if (process.task_fd >= 0) {
      close(process.task_fd);
      process.task_fd = -1;
    }
    close(process.result_fd);
    process.result_fd = -1;
    process.state = ProcessState::Stopping;
    if (!process.current_calc_name.empty()) {
      calc_t &calc = app.calcs.at(process.current_calc_name);
      if (calc.state == TaskState::Running) {
        calc.state = TaskState::Failed;
      }
    }
    for (const std::string &name : process.pending_calcs) {
      calc_t &calc = app.calcs.at(name);
      if (calc.state == TaskState::Pending || calc.state == TaskState::Running) {
        calc.state = TaskState::Failed;
      }
    }
    process.pending_calcs.clear();
    process.current_calc_name.clear();
    process.buffer.clear();
  };

  const size_t size = sizeof(detail::Result);
  if (process.buffer.size() > size) {
    close_channels();
    throw std::runtime_error("invalid result buffer size");
  }

  process.buffer.reserve(size);
  while (process.buffer.size() < size) {
    char buffer[sizeof(detail::Result)];
    const ssize_t count = read(process.result_fd, buffer, size - process.buffer.size());
    if (count > 0) {
      process.buffer.insert(process.buffer.end(), buffer, buffer + count);
    } else if (count == 0) {
      const bool incomplete = !process.buffer.empty() || !process.current_calc_name.empty();
      close_channels();
      if (incomplete) {
        throw std::runtime_error("result pipe closed before result received");
      }
      return;
    } else if (errno == EINTR) {
      continue;
    } else if (errno == EAGAIN || errno == EWOULDBLOCK) {
      return;
    } else {
      close_channels();
      throw std::runtime_error("result read failed");
    }
  }

  detail::Result result{};
  std::memcpy(&result, process.buffer.data(), sizeof(result));
  if (process.current_calc_name.empty()) {
    close_channels();
    throw std::runtime_error("unexpected result");
  }
  calc_t &calc = app.calcs.at(process.current_calc_name);
  if (calc.state != TaskState::Running || calc.id != result.calc_id) {
    close_channels();
    throw std::runtime_error("result does not match current calculation");
  }

  calc.milliseconds = result.milliseconds;
  calc.result = result.area;
  calc.state = TaskState::Completed;
  process.buffer.clear();
  process.current_calc_name.clear();
  startNextCalculation(app, process);
}

void chernov::checkProcesses(App &app)
{
  int wait_error = 0;
  for (auto &entry : app.processes) {
    process_t &process = entry.second;
    if (process.state == ProcessState::Stopped) {
      continue;
    }
    if (process.result_fd >= 0) {
      try {
        receiveAvailableResult(app, process);
      } catch (...) {
      }
    }

    pid_t result = 0;
    do {
      result = waitpid(process.pid, nullptr, WNOHANG);
    } while (result == -1 && errno == EINTR);
    if (result == 0) {
      continue;
    }
    if (result == -1 && errno != ECHILD) {
      wait_error = errno;
      continue;
    }

    for (auto &calc_entry : app.calcs) {
      calc_t &calc = calc_entry.second;
      if (calc.process_name == entry.first && (calc.state == TaskState::Pending || calc.state == TaskState::Running)) {
        calc.state = TaskState::Failed;
      }
    }
    if (process.task_fd >= 0) {
      close(process.task_fd);
    }
    if (process.result_fd >= 0) {
      close(process.result_fd);
    }
    process.task_fd = -1;
    process.result_fd = -1;
    process.buffer.clear();
    process.pending_calcs.clear();
    process.current_calc_name.clear();
    process.state = ProcessState::Stopped;
  }
  if (wait_error != 0) {
    throw std::runtime_error(std::strerror(wait_error));
  }
}

void chernov::waitCalculation(App &app, calc_t &calc)
{
  checkProcesses(app);
  while (calc.state == TaskState::Pending || calc.state == TaskState::Running) {
    const process_t &process = app.processes.at(calc.process_name);
    if (process.result_fd < 0) {
      throw std::runtime_error("result pipe is closed");
    }
    pollfd descriptor{process.result_fd, POLLIN, 0};
    int result = 0;
    do {
      result = poll(&descriptor, 1, -1);
    } while (result == -1 && errno == EINTR);
    if (result == -1) {
      throw std::runtime_error("poll failed");
    }
    checkProcesses(app);
  }
}

void chernov::waitCalculation(App &app, calc_t &calc, long long timeout_ms)
{
  checkProcesses(app);
  const auto start = std::chrono::steady_clock::now();
  while (calc.state == TaskState::Pending || calc.state == TaskState::Running) {
    const auto elapsed
        = std::chrono::duration_cast< std::chrono::milliseconds >(std::chrono::steady_clock::now() - start);
    if (elapsed.count() >= timeout_ms) {
      break;
    }

    const process_t &process = app.processes.at(calc.process_name);
    if (process.result_fd < 0) {
      throw std::runtime_error("result pipe is closed");
    }
    const long long remaining = timeout_ms - elapsed.count();
    const int poll_timeout
        = static_cast< int >(std::min(remaining, static_cast< long long >(std::numeric_limits< int >::max())));
    pollfd descriptor{process.result_fd, POLLIN, 0};
    const int result = poll(&descriptor, 1, poll_timeout);
    if (result == -1) {
      if (errno == EINTR) {
        continue;
      }
      throw std::runtime_error("poll failed");
    }
    checkProcesses(app);
  }
}

void chernov::killProcess(App &app, process_t &process)
{
  process.state = ProcessState::Stopping;
  for (const std::string &calc_name : process.pending_calcs) {
    calc_t &calc = app.calcs.at(calc_name);
    if (calc.state == TaskState::Pending) {
      calc.state = TaskState::Cancelled;
    }
  }
  process.pending_calcs.clear();
  if (process.task_fd >= 0) {
    close(process.task_fd);
    process.task_fd = -1;
  }

  pid_t result = 0;
  do {
    result = waitpid(process.pid, nullptr, 0);
  } while (result == -1 && errno == EINTR);
  if (result == -1 && errno != ECHILD) {
    throw std::runtime_error("waitpid failed");
  }
  if (process.result_fd >= 0) {
    try {
      receiveAvailableResult(app, process);
    } catch (...) {
    }
  }
  if (!process.current_calc_name.empty()) {
    calc_t &calc = app.calcs.at(process.current_calc_name);
    if (calc.state == TaskState::Running) {
      calc.state = TaskState::Failed;
    }
  }
  if (process.result_fd >= 0) {
    close(process.result_fd);
  }
  process.result_fd = -1;
  process.buffer.clear();
  process.current_calc_name.clear();
  process.state = ProcessState::Stopped;
}

void chernov::shutdownProcesses(App &app)
{
  for (auto &entry : app.processes) {
    process_t &process = entry.second;
    if (process.state == ProcessState::Stopped) {
      continue;
    }
    process.state = ProcessState::Stopping;
    if (process.task_fd >= 0) {
      close(process.task_fd);
      process.task_fd = -1;
    }
    for (const std::string &calc_name : process.pending_calcs) {
      calc_t &calc = app.calcs.at(calc_name);
      if (calc.state == TaskState::Pending) {
        calc.state = TaskState::Failed;
      }
    }
    process.pending_calcs.clear();
  }

  int wait_error = 0;
  for (auto &entry : app.processes) {
    process_t &process = entry.second;
    if (process.state == ProcessState::Stopped) {
      continue;
    }
    pid_t result = 0;
    do {
      result = waitpid(process.pid, nullptr, 0);
    } while (result == -1 && errno == EINTR);
    if (result == -1 && errno != ECHILD) {
      wait_error = errno;
    }
    if (process.result_fd >= 0) {
      try {
        receiveAvailableResult(app, process);
      } catch (...) {
      }
    }
    if (!process.current_calc_name.empty()) {
      calc_t &calc = app.calcs.at(process.current_calc_name);
      if (calc.state == TaskState::Running) {
        calc.state = TaskState::Failed;
      }
    }
    if (process.result_fd >= 0) {
      close(process.result_fd);
    }
    process.result_fd = -1;
    process.buffer.clear();
    process.current_calc_name.clear();
    process.state = ProcessState::Stopped;
  }
  if (wait_error != 0) {
    throw std::runtime_error(std::strerror(wait_error));
  }
}
