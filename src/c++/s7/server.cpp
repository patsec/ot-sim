#include <iostream>

#include "server.hpp"
#include "fmt/format.h"

#include "msgbus/metrics.hpp"

#include "snap7.h"

namespace otsim {
namespace s7 {
  
  //Constructor
  Server::Server(ServerConfig config, Pusher pusher): config(config), pusher(pusher) {

    //create a Metrics Pusher and add counters for status', updates, and binary/analog writes
    metrics = otsim::msgbus::MetricsPusher::Create();
    
    metrics->NewMetric("Counter", "status_count",            "number of OT-sim status messages processed");
    metrics->NewMetric("Counter", "update_count",            "number of OT-sim update messages generated");
    metrics->NewMetric("Counter", "s7_binary_write_count", "number of S7 binary writes processed");
    metrics->NewMetric("Counter", "s7_analog_write_count", "number of S7 analog writes processed");
  }

  //start the snap7 server created in otstim-s7comm-module main.cpp
  void Server::Run(std::shared_ptr<TS7Server> ts7server){
    metrics->Start(pusher, config.id);
    //ts7server->Start();
    running = true;
    while(running){
      for (const auto& kv : binaryInputs) {
        const std::uint16_t addr = kv.first;
        const float   val  = kv.second.value;
        const std::string   tag  = kv.second.tag;

        try {
          auto lock = std::unique_lock<std::mutex>(pointsMu);
          auto point = points.at(tag);
          std::cout << fmt::format("[{}] updated binary input {} to {}", config.id, addr, point.value) << std::endl;

          WriteAnalog(addr, point.value);
          metrics->IncrMetric("s7_analog_write_count");
        } catch (const std::out_of_range&) {}
      }

      //loop through all analog outputs, get their address and value, send that information to WriteAnalog where they will be pushed to the msgbus
      for (const auto& kv : analogInputs) {
        const std::uint16_t addr = kv.first;
        const float   val  = kv.second.value;
        const std::string   tag  = kv.second.tag;

        try {
          auto lock = std::unique_lock<std::mutex>(pointsMu);
          auto point = points.at(tag);
          std::cout << fmt::format("[{}] updated analog input {} to {}", config.id, addr, point.value) << std::endl;
          WriteAnalog(addr, point.value);
          metrics->IncrMetric("s7_analog_write_count");
        } catch (const std::out_of_range&) {}
      }

      //loop through all analog outputs, get their address and value, send that information to WriteAnalog where they will be pushed to the msgbus
      for (const auto& kv : analogOutputs) {
        const std::uint16_t addr = kv.first;
        const float   val  = kv.second.value;
        const std::string   tag  = kv.second.tag;

        try {
          auto lock = std::unique_lock<std::mutex>(pointsMu);
          auto point = points.at(tag);
          std::cout << fmt::format("[{}] updated analog output {} to {}", config.id, addr, point.value) << std::endl;
          WriteAnalog(addr, point.value);
          metrics->IncrMetric("s7_analog_write_count");
        } catch (const std::out_of_range&) {}
      }

      //loop through all binary outputs, get their address and value, send that information to WriteBinary where they will be pushed to the msgbus
      for (const auto& kv : binaryOutputs) {
        const std::uint16_t addr = kv.first;
        const bool   val  = kv.second.value;
        const std::string   tag  = kv.second.tag;

        try {
          auto lock = std::unique_lock<std::mutex>(pointsMu);
          auto point = points.at(tag);
          std::cout << fmt::format("[{}] updated binary output {} to {}", config.id, addr, point.value) << std::endl;
          WriteAnalog(addr, point.value);
          metrics->IncrMetric("s7_binary_write_count");
        } catch (const std::out_of_range&) {}
      }
      std::this_thread::sleep_for(std::chrono::seconds(1));
    }
  }

  bool Server::AddBinaryInput(BinaryInputPoint point) {
    /*
    * in our binary inputs array, at the position equal to the address of the 
    * point being passed in, set the value equal to the incoming point. Then
    * create a msgbus Point structure and store it in the list of points
    */
    binaryInputs[point.address] = point;
    points[point.tag] = otsim::msgbus::Point{point.tag, 0.0, 0};

    return true; //assuming this doesn't fail, return true
  }

  bool Server::AddBinaryOutput(BinaryOutputPoint point) {
    point.output = true;

    //store the point and point tag into the binaryOutputs and points arrays respectively
    binaryOutputs[point.address] = point;
    points[point.tag] = otsim::msgbus::Point{point.tag, 0.0, 0};

    return true; 
  }

  bool Server::AddAnalogInput(AnalogInputPoint point) {

    //store the point and point tag into the analogInputs and points arrays respectively
    analogInputs[point.address] = point;
    points[point.tag] = otsim::msgbus::Point{point.tag, 0.0, 0};

    return true;
  }

  bool Server::AddAnalogOutput(AnalogOutputPoint point) {
    point.output = true;

    //store the point and point tag into the analogOutputs and points arrays respectively
    analogOutputs[point.address] = point;
    points[point.tag] = otsim::msgbus::Point{point.tag, 0.0, 0};

    return true;
  }

  //this function interacts with the message bus to store status information for tags so other modules can access it (binary)
  void Server::WriteBinary(std::uint16_t address, bool status) {
    auto iter = binaryOutputs.find(address);
    if (iter == binaryOutputs.end()) {
      return;
    }

    std::cout << fmt::format("[{}] setting tag {} to {}", config.id, iter->second.tag, status) << std::endl;

    otsim::msgbus::Points points;
    points.push_back(otsim::msgbus::Point{iter->second.tag, status ? 1.0 : 0.0});

    otsim::msgbus::Update contents = {.updates = points};
    auto env = otsim::msgbus::NewEnvelope(config.id, contents);

    pusher->Push("RUNTIME", env);
    metrics->IncrMetric("update_count");
  }

  //this function interacts with the message bus to store status information for tags so other modules can access it (analog)
  void Server::WriteAnalog(std::uint16_t address, double value) {
    auto iter = analogOutputs.find(address);
    if (iter == analogOutputs.end()) {
      return;
    }

    std::cout << fmt::format("[{}] setting tag {} to {}", config.id, iter->second.tag, value) << std::endl;

    otsim::msgbus::Points points;
    points.push_back(otsim::msgbus::Point{iter->second.tag, value});

    otsim::msgbus::Update contents = {.updates = points};
    auto env = otsim::msgbus::NewEnvelope(config.id, contents);

    pusher->Push("RUNTIME", env);
    metrics->IncrMetric("update_count");
  }

  const BinaryOutputPoint* Server::GetBinaryOutput(const uint16_t address) {
    auto iter = binaryOutputs.find(address);
    if (iter == binaryOutputs.end()) {
      return NULL;
    }

    //if the function hasn't returned, it must've found the output, so it returns the value
    return &iter->second;
  }

  const AnalogOutputPoint* Server::GetAnalogOutput(const uint16_t address) {
    auto iter = analogOutputs.find(address);
    if (iter == analogOutputs.end()) {
      return NULL;
    }

    //if the function hasn't returned, it must've found the output, so it returns the value
    return &iter->second;
  }

  /*
  * set all outputs to new points with zero values, create a new envelope with those points
  * and then push that envelope with the pusher
  */
  void Server::ResetOutputs() {
    otsim::msgbus::Points points;

    for (const auto& kv : binaryOutputs) {
      points.push_back(otsim::msgbus::Point{kv.second.tag, 0.0});
    }

    for (const auto& kv : analogOutputs) {
      points.push_back(otsim::msgbus::Point{kv.second.tag, 0.0});
    }

    if (points.size()) {
      std::cout << fmt::format("[{}] setting outputs to zero values", config.id) << std::endl;

      otsim::msgbus::Update contents = {.updates = points};
      auto env = otsim::msgbus::NewEnvelope(config.id, contents);

      pusher->Push("RUNTIME", env);
    }
  }

  void Server::HandleMsgBusStatus(const otsim::msgbus::Envelope<otsim::msgbus::Status>& env) {
    auto sender = otsim::msgbus::GetEnvelopeSender(env);
    
    //if the status sender is the current s7 device, return because the status does not need to be handled
    if (sender == config.id) {
      return;
    }

    //increment status count
    metrics->IncrMetric("status_count");

    //add each point in measurements to the points array based on tag
    for (auto &p : env.contents.measurements) {
      if (points.count(p.tag)) {
        std::cout << fmt::format("[{}] status received for tag {}", config.id, p.tag) << std::endl;

        auto lock = std::unique_lock<std::mutex>(pointsMu);
        points[p.tag] = p;
      }
    }
  }

} // namespace s7
} // namespace otsim
