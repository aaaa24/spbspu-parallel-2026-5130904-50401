#include "commands.hpp"
#include "geometry.hpp"

#include <csignal>
#include <iostream>
#include <unordered_map>

int main()
{
  struct sigaction action{};
  action.sa_handler = SIG_IGN;
  if (sigemptyset(&action.sa_mask) == -1 || sigaction(SIGPIPE, &action, nullptr) == -1) {
    std::cerr << "cannot configure SIGPIPE\n";
    return 1;
  }

  chernov::ellipses_t ellipses;
  chernov::sets_t sets;
  chernov::processes_t processes;
  chernov::calcs_t calcs;

  chernov::App app{ellipses, sets, processes, calcs};

  std::unordered_map< std::string, chernov::cmd_t > cmds;
  cmds["circle"] = chernov::cmdCircle;
  cmds["ellipse"] = chernov::cmdEllipse;
  cmds["rotate"] = chernov::cmdRotate;
  cmds["set"] = chernov::cmdSet;
  cmds["show"] = chernov::cmdShow;
  cmds["showset"] = chernov::cmdShowSet;
  cmds["frame"] = chernov::cmdFrame;
  cmds["frameset"] = chernov::cmdFrameSet;
  cmds["spawn"] = chernov::cmdSpawn;
  cmds["area-on"] = chernov::cmdAreaOn;
  cmds["status"] = chernov::cmdStatus;
  cmds["wait"] = chernov::cmdWait;
  cmds["timeout"] = chernov::cmdTimeout;
  cmds["kill"] = chernov::cmdKill;

  auto cmd_io = chernov::CommandIO{std::cout, cmds, app};
  chernov::runCommands(std::cin, cmd_io);
  chernov::shutdownProcesses(app);
}
