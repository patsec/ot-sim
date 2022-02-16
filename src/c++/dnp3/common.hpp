#ifndef OTSIM_DNP3_COMMON_HPP
#define OTSIM_DNP3_COMMON_HPP

#include "msgbus/metrics.hpp"
#include "msgbus/pusher.hpp"

#include "opendnp3/gen/EventAnalogVariation.h"
#include "opendnp3/gen/EventBinaryVariation.h"
#include "opendnp3/gen/PointClass.h"
#include "opendnp3/gen/StaticAnalogVariation.h"
#include "opendnp3/gen/StaticBinaryVariation.h"

namespace otsim {
namespace dnp3 {

template <typename S, typename E>
struct Point {
  std::uint16_t address {};
  std::string   tag     {};

  S svariation {};
  E evariation {};

  bool output {};
  bool sbo {};

  opendnp3::PointClass clazz;
  double deadband;
};

typedef Point<opendnp3::StaticBinaryVariation, opendnp3::EventBinaryVariation> BinaryPoint;
typedef Point<opendnp3::StaticAnalogVariation, opendnp3::EventAnalogVariation> AnalogPoint;

typedef std::shared_ptr<otsim::msgbus::Pusher> Pusher;
typedef std::shared_ptr<otsim::msgbus::MetricsPusher> MetricsPusher;

} // namespace dnp3
} // namespace otsim

#endif // OTSIM_DNP3_COMMON_HPP