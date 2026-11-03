#include "calculations.hpp"

#include <cmath>
#include <functional>
#include <future>
#include <random>

namespace chernov {
  namespace detail {
    struct prepared_ellipse_t {
      point_t center;
      double xx;
      double xy;
      double yy;
    };

    std::vector< prepared_ellipse_t > prepareEllipses(const std::vector< ellipse_t > &ellipses)
    {
      std::vector< prepared_ellipse_t > prepared;
      prepared.reserve(ellipses.size());
      for (const ellipse_t &ellipse : ellipses) {
        const double cosine = std::cos(ellipse.ksi);
        const double sine = std::sin(ellipse.ksi);
        const double inverse_horizontal = 1.0 / (ellipse.horizontal_radius * ellipse.horizontal_radius);
        const double inverse_vertical = 1.0 / (ellipse.vertical_radius * ellipse.vertical_radius);
        prepared.push_back({ellipse.center, cosine * cosine * inverse_horizontal + sine * sine * inverse_vertical,
            2 * cosine * sine * (inverse_horizontal - inverse_vertical),
            sine * sine * inverse_horizontal + cosine * cosine * inverse_vertical});
      }
      return prepared;
    }
  }
}

bool chernov::isInside(const double x, const double y, const ellipse_t &ellipse)
{
  const double dx = x - ellipse.center.x;
  const double dy = y - ellipse.center.y;
  const double cosine = std::cos(ellipse.ksi);
  const double sine = std::sin(ellipse.ksi);
  const double local_x = dx * cosine + dy * sine;
  const double local_y = -dx * sine + dy * cosine;
  const double a = ellipse.horizontal_radius;
  const double b = ellipse.vertical_radius;
  return local_x * local_x / (a * a) + local_y * local_y / (b * b) <= 1;
}

bool chernov::isInsideAny(point_t point, const std::vector< ellipse_t > &ellipses)
{
  for (const ellipse_t &ellipse : ellipses) {
    if (isInside(point.x, point.y, ellipse)) {
      return true;
    }
  }
  return false;
}

size_t chernov::calc(
    const std::vector< ellipse_t > &ellipses, const frame_t &frame, const size_t tests, const size_t seed)
{
  std::default_random_engine gen(seed);
  constexpr double random_min = std::default_random_engine::min();
  constexpr double random_range = static_cast< double >(std::default_random_engine::max()) - random_min + 1;
  const double x_scale = (frame.right_top.x - frame.left_bottom.x) / random_range;
  const double y_scale = (frame.right_top.y - frame.left_bottom.y) / random_range;
  const std::vector< detail::prepared_ellipse_t > prepared = detail::prepareEllipses(ellipses);

  size_t count = 0;
  for (size_t i = 0; i < tests; ++i) {
    const double x = frame.left_bottom.x + (gen() - random_min) * x_scale;
    const double y = frame.left_bottom.y + (gen() - random_min) * y_scale;
    for (const detail::prepared_ellipse_t &ellipse : prepared) {
      const double dx = x - ellipse.center.x;
      const double dy = y - ellipse.center.y;
      if (ellipse.xx * dx * dx + ellipse.xy * dx * dy + ellipse.yy * dy * dy <= 1) {
        ++count;
        break;
      }
    }
  }
  return count;
}

double chernov::areaOf(const std::vector< ellipse_t > &ellipses, const size_t threads, const size_t tries, size_t seed)
{
  const frame_t frame = getFrame(ellipses);
  std::vector< std::future< size_t > > futures;
  const size_t batch = tries / threads;
  futures.emplace_back(std::async(std::launch::async, calc, std::cref(ellipses), frame, batch + tries % threads, seed));
  for (size_t i = 1; i < threads; ++i) {
    futures.emplace_back(std::async(std::launch::async, calc, std::cref(ellipses), frame, batch, seed + i));
  }

  size_t count = 0;
  for (size_t i = 0; i < threads; ++i) {
    count += futures[i].get();
  }
  const double frame_area = (frame.right_top.x - frame.left_bottom.x) * (frame.right_top.y - frame.left_bottom.y);
  return frame_area * count / tries;
}
