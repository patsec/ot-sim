#include <iostream>

#include "server.hpp"
#include "fmt/format.h"

#include "msgbus/metrics.hpp"

namespace otsim {
namespace s7 {
  Server::Server(ServerConfig config, Pusher pusher) {
    metrics = otsim::msgbus::MetricsPusher::Create();

    metrics->NewMetric("Counter", "status_count",            "number of OT-sim status messages processed");
    metrics->NewMetric("Counter", "update_count",            "number of OT-sim update messages generated");
    metrics->NewMetric("Counter", "s7_binary_write_count", "number of S7 binary writes processed");
    metrics->NewMetric("Counter", "s7_analog_write_count", "number of S7 analog writes processed");
  }

} // namespace s7
} // namespace otsim
