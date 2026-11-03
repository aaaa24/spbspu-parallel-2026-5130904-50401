#ifndef CALCULATIONS_HPP
#define CALCULATIONS_HPP

#include "geometry.hpp"

#include <cstddef>
#include <vector>

namespace chernov {
  bool isInside(double x, double y, const ellipse_t &ellipse);
  bool isInsideAny(point_t point, const std::vector< ellipse_t > &ellipses);
  size_t calc(const std::vector< ellipse_t > &ellipses, const frame_t &frame, size_t tests, size_t seed);
  double areaOf(const std::vector< ellipse_t > &ellipses, size_t threads, size_t tries, size_t seed);
}

#endif
