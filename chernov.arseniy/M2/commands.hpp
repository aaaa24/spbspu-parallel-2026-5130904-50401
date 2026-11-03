#ifndef COMMANDS_HPP
#define COMMANDS_HPP
#include "geometry.hpp"
#include "processes.hpp"

#include <functional>
#include <iosfwd>
#include <streambuf>
#include <string>
#include <unordered_map>

namespace chernov {
  using cmd_t = std::function< void(std::istream &, std::ostream &, App &) >;
  struct CommandIO {
    std::ostream &output;
    const std::unordered_map< std::string, cmd_t > &cmds;
    App &app;
  };

  std::istream &operator>>(std::istream &input, const CommandIO &cmd_io);

  void runCommands(std::istream &input, CommandIO &cmd_io);

  void cmdCircle(std::istream &, std::ostream &, App &);
  void cmdEllipse(std::istream &, std::ostream &, App &);
  void cmdRotate(std::istream &, std::ostream &, App &);
  void cmdSet(std::istream &, std::ostream &, App &);
  void cmdShow(std::istream &, std::ostream &, App &);
  void cmdShowSet(std::istream &, std::ostream &, App &);
  void cmdFrame(std::istream &, std::ostream &, App &);
  void cmdFrameSet(std::istream &, std::ostream &, App &);
  void cmdSpawn(std::istream &, std::ostream &, App &);
  void cmdAreaOn(std::istream &, std::ostream &, App &);
  void cmdStatus(std::istream &, std::ostream &, App &);
  void cmdWait(std::istream &, std::ostream &, App &);
  void cmdTimeout(std::istream &, std::ostream &, App &);
  void cmdKill(std::istream &, std::ostream &, App &);

  namespace detail {
    class CommandBuffer: public std::streambuf {
    public:
      explicit CommandBuffer(std::string &line);
    };

    void printCalcStatus(std::ostream &output, const calc_t &calc);
    bool isValidName(const std::string &name);
    void requireEnd(std::istream &input);
  }
}

#endif
