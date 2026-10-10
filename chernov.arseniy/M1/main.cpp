#include <algorithm>
#include <cstddef>
#include <functional>
#include <future>
#include <iostream>
#include <random>
#include <vector>
#include <stdexcept>
#include <string>
#include <utility>

namespace chernov
{
  struct point_t
  {
    double x, y;
  };

  struct ellipse_t
  {
    point_t center;
    double horizontal_radius;
    double vertical_radius;
  };

  struct frame_t
  {
    point_t left_bottom;
    point_t right_top;
  };

  void inputCircles(std::vector< ellipse_t > &ellipses)
  {
    double horizontal_radius = 0, vertical_radius = 0, x = 0, y = 0;
    while (std::cin >> horizontal_radius >> vertical_radius >> x >> y)
    {
      if (vertical_radius == 0)
      {
        vertical_radius = horizontal_radius;
      }
      ellipses.push_back(ellipse_t{
          .center = {.x = x, .y = y}, .horizontal_radius = horizontal_radius, .vertical_radius = vertical_radius});
    }

    if (!std::cin.eof())
    {
      throw std::runtime_error("input error");
    }
  }

  bool isInside(const double x, const double y, const ellipse_t &ellipse)
  {
    const double dx = x - ellipse.center.x;
    const double dy = y - ellipse.center.y;
    const double a = ellipse.horizontal_radius;
    const double b = ellipse.vertical_radius;
    return dx * dx / (a * a) + dy * dy / (b * b) <= 1;
  }

  size_t ellipsesOf(point_t point, const std::vector< ellipse_t > &ellipses)
  {
    const auto bound_is_inside = std::bind(isInside, point.x, point.y, std::placeholders::_1);
    return std::count_if(ellipses.begin(), ellipses.end(), bound_is_inside);
  }

  std::pair< size_t, size_t > calc(
      const std::vector< ellipse_t > &ellipses, const frame_t &frame, const size_t tests, const size_t seed)
  {
    std::default_random_engine gen(seed);
    std::uniform_real_distribution<> dis_x(frame.left_bottom.x, frame.right_top.x);
    std::uniform_real_distribution<> dis_y(frame.left_bottom.y, frame.right_top.y);

    size_t count_in_any = 0;
    size_t count_in_all = 0;
    for (size_t i = 0; i < tests; ++i)
    {
      const double x = dis_x(gen);
      const double y = dis_y(gen);
      const size_t count_insides = ellipsesOf({.x = x, .y = y}, ellipses);

      if (count_insides == ellipses.size())
      {
        ++count_in_all;
      }
      if (count_insides)
      {
        ++count_in_any;
      }
    }
    return {count_in_any, count_in_all};
  }

  frame_t getFrameOf(const std::vector< ellipse_t > &ellipses)
  {
    auto cmp_min_x = [](const ellipse_t &a, const ellipse_t &b)
    {
      return a.center.x - a.horizontal_radius < b.center.x - b.horizontal_radius;
    };
    auto cmp_min_y = [](const ellipse_t &a, const ellipse_t &b)
    {
      return a.center.y - a.vertical_radius < b.center.y - b.vertical_radius;
    };
    auto cmp_max_x = [](const ellipse_t &a, const ellipse_t &b)
    {
      return a.center.x + a.horizontal_radius < b.center.x + b.horizontal_radius;
    };
    auto cmp_max_y = [](const ellipse_t &a, const ellipse_t &b)
    {
      return a.center.y + a.vertical_radius < b.center.y + b.vertical_radius;
    };
    const auto it_min_x = std::min_element(ellipses.begin(), ellipses.end(), cmp_min_x);
    const auto it_min_y = std::min_element(ellipses.begin(), ellipses.end(), cmp_min_y);
    const auto it_max_x = std::max_element(ellipses.begin(), ellipses.end(), cmp_max_x);
    const auto it_max_y = std::max_element(ellipses.begin(), ellipses.end(), cmp_max_y);
    const double min_x = it_min_x->center.x - it_min_x->horizontal_radius;
    const double min_y = it_min_y->center.y - it_min_y->vertical_radius;
    const double max_x = it_max_x->center.x + it_max_x->horizontal_radius;
    const double max_y = it_max_y->center.y + it_max_y->vertical_radius;
    const frame_t frame{.left_bottom = {.x = min_x, .y = min_y}, .right_top = {.x = max_x, .y = max_y}};
    return frame;
  }

  std::pair< double, double > areaOf(
      const std::vector< ellipse_t > &ellipses, size_t threads, size_t tries, size_t seed)
  {
    frame_t frame = getFrameOf(ellipses);

    std::vector< std::future< std::pair< size_t, size_t > > > futures;
    const size_t batch = tries / threads;
    futures.emplace_back(
        std::async(std::launch::async, calc, std::cref(ellipses), frame, batch + tries % threads, seed));
    for (size_t i = 1; i < threads; ++i)
    {
      futures.emplace_back(std::async(std::launch::async, calc, std::cref(ellipses), frame, batch, seed + i));
    }

    size_t count_in_any = 0;
    size_t count_in_all = 0;
    for (size_t i = 0; i < threads; ++i)
    {
      const std::pair< size_t, size_t > thread_result = futures[i].get();
      count_in_any += thread_result.first;
      count_in_all += thread_result.second;
    }

    const double frame_area = (frame.right_top.x - frame.left_bottom.x) * (frame.right_top.y - frame.left_bottom.y);
    const double union_area = frame_area * count_in_any / tries;
    const double intersection_area = frame_area * count_in_all / tries;
    return {union_area, intersection_area};
  }
}

int main(int argc, char **argv)
{
  using namespace chernov;

  if (argc < 3 || argc > 4)
  {
    std::cerr << "count of arguments is incorrect\n";
    return 1;
  }

  long long threads = 0, tries = 0, seed = 0;

  try
  {
    threads = std::stoll(argv[1]);
  }
  catch (const std::invalid_argument &e)
  {
    std::cerr << "count of threads is incorrect\n";
    return 1;
  }
  if (threads < 0)
  {
    std::cerr << "count of threads is incorrect\n";
    return 1;
  }
  if (threads == 0)
  {
    threads = 1;
  }

  try
  {
    tries = std::stoll(argv[2]);
  }
  catch (const std::invalid_argument &e)
  {
    std::cerr << "count of tries is incorrect\n";
    return 1;
  }
  if (tries < 1)
  {
    std::cerr << "count of tries is incorrect\n";
    return 1;
  }

  if (argc == 4)
  {
    try
    {
      seed = std::stoll(argv[3]);
    }
    catch (const std::invalid_argument &e)
    {
      std::cerr << "seed is incorrect\n";
      return 1;
    }
    if (seed < 0)
    {
      std::cerr << "seed is incorrect\n";
      return 1;
    }
  }

  std::vector< ellipse_t > ellipses;
  try
  {
    inputCircles(ellipses);
  }
  catch (const std::runtime_error &e)
  {
    std::cerr << e.what() << '\n';
    return 1;
  }

  const std::pair< double, double > area = areaOf(ellipses, threads, tries, seed);
  const double union_area = area.first;
  const double intersection_area = area.second;
  std::cout << union_area << " " << intersection_area << "\n";
}
