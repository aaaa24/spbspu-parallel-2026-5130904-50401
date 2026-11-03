#ifndef GEOMETRY_HPP
#define GEOMETRY_HPP

#include <ios>
#include <string>
#include <unordered_map>
#include <vector>

namespace chernov {
  struct point_t {
    double x, y;
  };

  struct frame_t {
    point_t left_bottom;
    point_t right_top;
  };

  struct ellipse_t {
    point_t center;
    double horizontal_radius;
    double vertical_radius;
    double ksi;
    bool is_circle;
  };

  using ellipses_t = std::unordered_map< std::string, ellipse_t >;
  using set_t = std::vector< std::string >;
  using sets_t = std::unordered_map< std::string, set_t >;

  std::ostream& operator<<(std::ostream& output, const point_t& point);
  std::ostream& operator<<(std::ostream& output, const frame_t& frame);
  std::ostream& operator<<(std::ostream& output, const ellipse_t& ellipse);

  frame_t getFrame(const ellipse_t& ellipse);
  frame_t getFrame(const std::vector< ellipse_t >& ellipses);

  namespace detail {
    class IOguard {
    public:
      explicit IOguard(std::basic_ios< char >& s);
      ~IOguard();
    private:
      std::basic_ios< char >& s_;
      std::streamsize width_;
      char fill_;
      std::streamsize precision_;
      std::basic_ios< char >::fmtflags fmt_;
    };
  }
}

#endif
