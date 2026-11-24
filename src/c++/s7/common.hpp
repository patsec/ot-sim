#ifndef OTSIM_S7_COMMON_HPP
#define OTSIM_S7_COMMON_HPP

#include "msgbus/metrics.hpp"
#include "msgbus/pusher.hpp"

namespace otsim {
namespace s7 {

template <typename T>
struct Point {
  std::uint16_t address {};
  std::string   tag     {};

  T value {};

  bool output {};
  bool sbo {};

  double deadband;
};

typedef Point<bool> BinaryInputPoint;
typedef Point<float> AnalogInputPoint;

typedef Point<bool> BinaryOutputPoint;
typedef Point<float> AnalogOutputPoint;

typedef std::shared_ptr<otsim::msgbus::Pusher> Pusher;
typedef std::shared_ptr<otsim::msgbus::MetricsPusher> MetricsPusher;

} // namespace s7
} // namespace otsim

#endif // OTSIM_S7_COMMON_HPP