#include "commands.hpp"

#include <iomanip>
#include <iostream>
#include <istream>
#include <limits>

std::istream &chernov::operator>>(std::istream &input, const CommandIO &cmd_io)
{
  const std::istream::sentry sentry(input);
  if (!sentry) {
    return input;
  }

  std::string command;
  input >> command;
  std::string arguments;
  std::getline(input, arguments);
  detail::CommandBuffer buffer(arguments);
  std::istream command_input(&buffer);

  const auto command_it = cmd_io.cmds.find(command);
  if (command_it == cmd_io.cmds.end()) {
    cmd_io.output << "<UNKNOWN COMMAND>\n";
  } else {
    try {
      command_it->second(command_input, cmd_io.output, cmd_io.app);
    } catch (...) {
      cmd_io.output << "<INVALID COMMAND>\n";
    }
  }

  return input;
}

void chernov::runCommands(std::istream &input, CommandIO &cmd_io)
{
  if (!(input >> cmd_io)) {
    return;
  }
  runCommands(input, cmd_io);
}

void chernov::cmdCircle(std::istream &input, std::ostream &, App &app)
{
  std::string name;
  double r = 0;
  double x = 0;
  double y = 0;
  if (!(input >> name >> r >> x >> y)) {
    throw std::runtime_error("invalid input");
  }
  detail::requireEnd(input);

  if (!detail::isValidName(name) || app.ellipses.find(name) != app.ellipses.end()) {
    throw std::runtime_error("ellipse name already exists");
  }
  if (r <= 0) {
    throw std::runtime_error("ellipse radius must be greater than zero");
  }

  app.ellipses.insert({name, ellipse_t{point_t{x, y}, r, r, 0, true}});
}

void chernov::cmdEllipse(std::istream &input, std::ostream &, App &app)
{
  std::string name;
  double horizontal_radius = 0;
  double vertical_radius = 0;
  double x = 0;
  double y = 0;
  if (!(input >> name >> horizontal_radius >> vertical_radius >> x >> y)) {
    throw std::runtime_error("invalid input");
  }
  detail::requireEnd(input);

  if (!detail::isValidName(name) || app.ellipses.find(name) != app.ellipses.end()) {
    throw std::runtime_error("ellipse name already exists");
  }
  if (horizontal_radius <= 0 || vertical_radius <= 0) {
    throw std::runtime_error("ellipse radii must be greater than zero");
  }

  app.ellipses.insert({name, ellipse_t{point_t{x, y}, horizontal_radius, vertical_radius, 0, false}});
}

void chernov::cmdRotate(std::istream &input, std::ostream &, App &app)
{
  std::string name;
  double ksi = 0;
  if (!(input >> name >> ksi)) {
    throw std::runtime_error("invalid input");
  }
  detail::requireEnd(input);

  if (app.ellipses.find(name) == app.ellipses.end()) {
    throw std::runtime_error("ellipse name not found");
  }

  app.ellipses.at(name).ksi += ksi;
}

void chernov::cmdSet(std::istream &input, std::ostream &, App &app)
{
  std::string name;
  long long size = 0;
  if (!(input >> name >> size)) {
    throw std::runtime_error("invalid input");
  }

  if (app.sets.find(name) != app.sets.end()) {
    throw std::runtime_error("set name already exists");
  }
  if (size <= 0) {
    throw std::runtime_error("set size must be greater than zero");
  }

  set_t set;
  set.reserve(size);
  std::string shape;
  for (long long i = 0; i < size; ++i) {
    if (!(input >> shape)) {
      throw std::runtime_error("invalid input");
    }
    if (app.ellipses.find(shape) == app.ellipses.end()) {
      throw std::runtime_error("shape name not found");
    }

    set.push_back(shape);
  }
  detail::requireEnd(input);

  if (!detail::isValidName(name)) {
    throw std::runtime_error("invalid set name");
  }
  app.sets.insert({name, std::move(set)});
}

void chernov::cmdShow(std::istream &input, std::ostream &output, App &app)
{
  std::string name;
  if (!(input >> name)) {
    throw std::runtime_error("invalid input");
  }
  detail::requireEnd(input);

  if (app.ellipses.find(name) == app.ellipses.end()) {
    throw std::runtime_error("ellipse name not found");
  }

  output << app.ellipses.at(name) << "\n";
}

void chernov::cmdShowSet(std::istream &input, std::ostream &output, App &app)
{
  std::string name;
  if (!(input >> name)) {
    throw std::runtime_error("invalid input");
  }
  detail::requireEnd(input);

  if (app.sets.find(name) == app.sets.end()) {
    throw std::runtime_error("set name not found");
  }

  const set_t &set = app.sets.at(name);
  for (const std::string &ellipse_name : set) {
    output << app.ellipses.at(ellipse_name) << "\n";
  }
}

void chernov::cmdFrame(std::istream &input, std::ostream &output, App &app)
{
  std::string name;
  if (!(input >> name)) {
    throw std::runtime_error("invalid input");
  }
  detail::requireEnd(input);

  if (app.ellipses.find(name) == app.ellipses.end()) {
    throw std::runtime_error("ellipse name not found");
  }

  const frame_t frame = getFrame(app.ellipses.at(name));
  output << frame << "\n";
}

void chernov::cmdFrameSet(std::istream &input, std::ostream &output, App &app)
{
  std::string name;
  if (!(input >> name)) {
    throw std::runtime_error("invalid input");
  }
  detail::requireEnd(input);

  if (app.sets.find(name) == app.sets.end()) {
    throw std::runtime_error("set name not found");
  }

  const set_t &set = app.sets.at(name);
  std::vector< ellipse_t > ellipses_vector;
  ellipses_vector.reserve(set.size());

  for (const std::string &ellipse_name : set) {
    ellipses_vector.push_back(app.ellipses.at(ellipse_name));
  }

  const frame_t frame = getFrame(ellipses_vector);
  output << frame << "\n";
}

void chernov::cmdSpawn(std::istream &input, std::ostream &, App &app)
{
  std::string process_name;
  long long seed = 0;
  if (!(input >> process_name >> seed)) {
    throw std::runtime_error("invalid input");
  }
  detail::requireEnd(input);

  if (!detail::isValidName(process_name) || seed < 0) {
    throw std::runtime_error("seed must not be negative");
  }

  if (app.processes.find(process_name) != app.processes.end()) {
    throw std::runtime_error("process name already exists");
  }
  spawnProcess(app, process_name, static_cast< size_t >(seed));
}

void chernov::cmdAreaOn(std::istream &input, std::ostream &, App &app)
{
  std::string process_name, calc_name, set_name;
  long long threads = 0;
  long long tries = 0;
  if (!(input >> process_name >> calc_name >> set_name >> threads >> tries)) {
    throw std::runtime_error("invalid input");
  }
  detail::requireEnd(input);

  if (!detail::isValidName(calc_name)) {
    throw std::runtime_error("invalid calc name");
  }

  if (app.processes.find(process_name) == app.processes.end()) {
    throw std::runtime_error("process name not found");
  }

  if (app.calcs.find(calc_name) != app.calcs.end()) {
    throw std::runtime_error("calc name already exists");
  }

  if (app.sets.find(set_name) == app.sets.end()) {
    throw std::runtime_error("set name not found");
  }

  if (threads <= 0) {
    throw std::runtime_error("threads must be positive");
  }

  if (tries <= 0) {
    throw std::runtime_error("tries must be positive");
  }

  process_t &process = app.processes.at(process_name);
  if (process.state != ProcessState::Active) {
    throw std::runtime_error("process state must be active");
  }

  if (app.id_counter == std::numeric_limits< size_t >::max()) {
    throw std::runtime_error("calculation id limit reached");
  }

  std::vector< ellipse_t > ellipses;
  ellipses.reserve(app.sets.at(set_name).size());
  for (const std::string &ellipse_name : app.sets.at(set_name)) {
    ellipses.push_back(app.ellipses.at(ellipse_name));
  }

  calc_t calc{process_name, TaskState::Pending, 0, 0, app.id_counter, std::move(ellipses),
      static_cast< size_t >(threads), static_cast< size_t >(tries)};

  app.calcs.insert({calc_name, std::move(calc)});
  try {
    process.pending_calcs.push_back(calc_name);
  } catch (...) {
    app.calcs.erase(calc_name);
    throw;
  }

  ++app.id_counter;
  startNextCalculation(app, process);
}

void chernov::cmdStatus(std::istream &input, std::ostream &output, App &app)
{
  std::string calc_name;
  if (!(input >> calc_name)) {
    throw std::runtime_error("invalid input");
  }
  detail::requireEnd(input);

  if (app.calcs.find(calc_name) == app.calcs.end()) {
    throw std::runtime_error("calc name not found");
  }

  checkProcesses(app);
  detail::printCalcStatus(output, app.calcs.at(calc_name));
}

void chernov::cmdWait(std::istream &input, std::ostream &output, App &app)
{
  std::string calc_name;
  if (!(input >> calc_name)) {
    throw std::runtime_error("invalid input");
  }
  detail::requireEnd(input);

  if (app.calcs.find(calc_name) == app.calcs.end()) {
    throw std::runtime_error("calc name not found");
  }

  calc_t &calc = app.calcs.at(calc_name);
  waitCalculation(app, calc);
  detail::printCalcStatus(output, calc);
}

void chernov::cmdTimeout(std::istream &input, std::ostream &output, App &app)
{
  std::string calc_name;
  long long timeout_ms = 0;
  if (!(input >> calc_name >> timeout_ms)) {
    throw std::runtime_error("invalid input");
  }
  detail::requireEnd(input);

  if (app.calcs.find(calc_name) == app.calcs.end()) {
    throw std::runtime_error("calc name not found");
  }
  if (timeout_ms < 0) {
    throw std::runtime_error("timeout must not be negative");
  }

  calc_t &calc = app.calcs.at(calc_name);
  waitCalculation(app, calc, timeout_ms);
  detail::printCalcStatus(output, calc);
}

void chernov::cmdKill(std::istream &input, std::ostream &, App &app)
{
  std::string process_name;
  if (!(input >> process_name)) {
    throw std::runtime_error("invalid input");
  }
  detail::requireEnd(input);

  if (app.processes.find(process_name) == app.processes.end()) {
    throw std::runtime_error("process name not found");
  }

  process_t &process = app.processes.at(process_name);
  if (process.state != ProcessState::Active) {
    throw std::runtime_error("process state must be active");
  }

  killProcess(app, process);
}

chernov::detail::CommandBuffer::CommandBuffer(std::string &line)
{
  if (line.empty()) {
    setg(nullptr, nullptr, nullptr);
  } else {
    char *const begin = &line[0];
    setg(begin, begin, begin + line.size());
  }
}

void chernov::detail::printCalcStatus(std::ostream &output, const calc_t &calc)
{
  const IOguard guard(output);
  output << calc.process_name << " ";
  if (calc.state == TaskState::Completed) {
    output << std::fixed << std::setprecision(3) << calc.milliseconds << " " << calc.result;
  } else if (calc.state == TaskState::Cancelled) {
    output << "<CANCELED>";
  } else if (calc.state == TaskState::Failed) {
    output << "<FAILED>";
  } else {
    output << "<IN PROGRESS>";
  }
  output << "\n";
}

bool chernov::detail::isValidName(const std::string &name)
{
  return !name.empty()
      && ((name.front() >= 'a' && name.front() <= 'z') || (name.front() >= 'A' && name.front() <= 'Z'));
}

void chernov::detail::requireEnd(std::istream &input)
{
  input >> std::ws;
  if (!input.eof()) {
    throw std::runtime_error("extra arguments");
  }
}
