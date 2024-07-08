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

  bool Server::AddBinaryInput(BinaryInputPoint point) {
    binaryInputs[point.address] = point;
    points[point.tag] = otsim::msgbus::Point{point.tag, 0.0, 0};

    return true;
  }

  bool Server::AddBinaryOutput(BinaryOutputPoint point) {
    point.output = true;

    binaryOutputs[point.address] = point;
    points[point.tag] = otsim::msgbus::Point{point.tag, 0.0, 0};

    return true;
  }

  bool Server::AddAnalogInput(AnalogInputPoint point) {
    analogInputs[point.address] = point;
    points[point.tag] = otsim::msgbus::Point{point.tag, 0.0, 0};

    return true;
  }

  bool Server::AddAnalogOutput(AnalogOutputPoint point) {
    point.output = true;

    analogOutputs[point.address] = point;
    points[point.tag] = otsim::msgbus::Point{point.tag, 0.0, 0};

    return true;
  }

  
} // namespace s7
} // namespace otsim
