#ifndef OTSIM_DNP3_COMMON_HPP
#define OTSIM_DNP3_COMMON_HPP

#include "msgbus/metrics.hpp"
#include "msgbus/pusher.hpp"

#include "opendnp3/gen/EventAnalogVariation.h"
#include "opendnp3/gen/EventAnalogOutputStatusVariation.h"
#include "opendnp3/gen/EventBinaryVariation.h"
#include "opendnp3/gen/EventBinaryOutputStatusVariation.h"
#include "opendnp3/gen/PointClass.h"
#include "opendnp3/gen/StaticAnalogVariation.h"
#include "opendnp3/gen/StaticAnalogOutputStatusVariation.h"
#include "opendnp3/gen/StaticBinaryVariation.h"
#include "opendnp3/gen/StaticBinaryOutputStatusVariation.h"

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

typedef Point<opendnp3::StaticBinaryVariation, opendnp3::EventBinaryVariation> BinaryInputPoint;
typedef Point<opendnp3::StaticAnalogVariation, opendnp3::EventAnalogVariation> AnalogInputPoint;

typedef Point<opendnp3::StaticBinaryOutputStatusVariation, opendnp3::EventBinaryOutputStatusVariation> BinaryOutputPoint;
typedef Point<opendnp3::StaticAnalogOutputStatusVariation, opendnp3::EventAnalogOutputStatusVariation> AnalogOutputPoint;

typedef std::shared_ptr<otsim::msgbus::Pusher> Pusher;
typedef std::shared_ptr<otsim::msgbus::MetricsPusher> MetricsPusher;

} // namespace dnp3
} // namespace otsim

#endif // OTSIM_DNP3_COMMON_HPP