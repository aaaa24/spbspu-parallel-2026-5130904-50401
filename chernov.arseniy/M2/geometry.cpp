#include "geometry.hpp"

#include <algorithm>
#include <cmath>
#include <iomanip>
#include <ostream>

std::ostream& chernov::operator<<(std::ostream& output, const point_t& point)
{
  const std::ostream::sentry sentry(output);
  if (!sentry) {
    return output;
  }

  const detail::IOguard guard(output);
  output << "(" << point.x << " " << point.y << ")";

  return output;
}

std::ostream& chernov::operator<<(std::ostream& output, const frame_t& frame)
{
  const std::ostream::sentry sentry(output);
  if (!sentry) {
    return output;
  }

  const detail::IOguard guard(output);
  output << frame.left_bottom << " " << frame.right_top;

  return output;
}

std::ostream& chernov::operator<<(std::ostream& output, const ellipse_t& ellipse)
{
  const std::ostream::sentry sentry(output);
  if (!sentry) {
    return output;
  }

  const detail::IOguard guard(output);
  if (ellipse.is_circle) {
    output << ellipse.horizontal_radius << " " << ellipse.center;
  } else {
    output << ellipse.horizontal_radius << " " << ellipse.vertical_radius << " " << ellipse.center;
    output << " " << std::fixed << std::setprecision(3) << ellipse.ksi;
  }

  return output;
}

chernov::frame_t chernov::getFrame(const ellipse_t& ellipse)
{
  const double cosine = std::cos(ellipse.ksi);
  const double sine = std::sin(ellipse.ksi);
  const double horizontal_radius = ellipse.horizontal_radius;
  const double vertical_radius = ellipse.vertical_radius;
  const double half_width = std::sqrt(
      horizontal_radius * horizontal_radius * cosine * cosine + vertical_radius * vertical_radius * sine * sine);
  const double half_height = std::sqrt(
      horizontal_radius * horizontal_radius * sine * sine + vertical_radius * vertical_radius * cosine * cosine);
  return {
      {ellipse.center.x - half_width, ellipse.center.y - half_height},
      {ellipse.center.x + half_width, ellipse.center.y + half_height},
  };
}

chernov::frame_t chernov::getFrame(const std::vector< ellipse_t >& ellipses)
{
  frame_t frame = getFrame(ellipses.front());
  for (auto it = ellipses.begin() + 1; it != ellipses.end(); ++it) {
    const frame_t ellipse_frame = getFrame(*it);
    frame.left_bottom.x = std::min(frame.left_bottom.x, ellipse_frame.left_bottom.x);
    frame.left_bottom.y = std::min(frame.left_bottom.y, ellipse_frame.left_bottom.y);
    frame.right_top.x = std::max(frame.right_top.x, ellipse_frame.right_top.x);
    frame.right_top.y = std::max(frame.right_top.y, ellipse_frame.right_top.y);
  }
  return frame;
}

chernov::detail::IOguard::IOguard(std::basic_ios< char >& s):
  s_(s),
  width_(s.width()),
  fill_(s.fill()),
  precision_(s.precision()),
  fmt_(s.flags())
{}

chernov::detail::IOguard::~IOguard()
{
  s_.width(width_);
  s_.fill(fill_);
  s_.precision(precision_);
  s_.flags(fmt_);
}
