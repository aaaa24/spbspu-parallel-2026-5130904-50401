#ifndef PROCESSES_HPP
#define PROCESSES_HPP

#include "geometry.hpp"

#include <cstddef>
#include <sys/types.h>

#include <deque>
#include <string>
#include <unordered_map>
#include <vector>

namespace chernov {
  enum class ProcessState { Active, Stopping, Stopped };

  struct process_t {
    pid_t pid;
    size_t seed;
    int task_fd;
    int result_fd;
    ProcessState state;
    std::string current_calc_name;
    std::deque< std::string > pending_calcs;
    std::vector< char > buffer{};
  };

  enum class TaskState { Pending, Running, Completed, Cancelled, Failed };

  struct calc_t {
    std::string process_name;
    TaskState state;
    double milliseconds;
    double result;
    size_t id;
    std::vector< ellipse_t > ellipses;
    size_t threads;
    size_t tries;
  };

  using processes_t = std::unordered_map< std::string, process_t >;
  using calcs_t = std::unordered_map< std::string, calc_t >;

  struct App {
    ellipses_t &ellipses;
    sets_t &sets;
    processes_t &processes;
    calcs_t &calcs;
    size_t id_counter = 0;
  };

  void spawnProcess(App &app, const std::string &name, size_t seed);
  void startNextCalculation(App &app, process_t &process);
  void receiveAvailableResult(App &app, process_t &process);
  void checkProcesses(App &app);
  void waitCalculation(App &app, calc_t &calc);
  void waitCalculation(App &app, calc_t &calc, long long timeout_ms);
  void killProcess(App &app, process_t &process);
  void shutdownProcesses(App &app);
}

#endif
