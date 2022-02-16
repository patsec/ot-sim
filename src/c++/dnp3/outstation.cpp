#include <iostream>

#include "outstation.hpp"

#include "fmt/format.h"
#include "opendnp3/outstation/UpdateBuilder.h"

namespace otsim {
namespace dnp3 {

Outstation::Outstation(OutstationConfig config, OutstationRestartConfig restart, Pusher pusher) :
  DefaultOutstationApplication(opendnp3::TimeDuration::Minutes(1)), config(config), restartConfig(restart), pusher(pusher)
{
  metrics = otsim::msgbus::MetricsPusher::Create();
  metrics->NewMetric("Counter", "status_count", "number of status messages processed");
}

opendnp3::OutstationStackConfig Outstation::Init() {
  opendnp3::DatabaseConfig db = opendnp3::DatabaseConfig();

  for (const auto& kv : binaryPoints) {
    db.binary_input[kv.first] = {};

    db.binary_input[kv.first].svariation = kv.second.svariation;
    db.binary_input[kv.first].evariation = kv.second.evariation;
    db.binary_input[kv.first].clazz = kv.second.clazz;
  }

  for (const auto& kv : analogPoints) {
    db.analog_input[kv.first] = {};

    db.analog_input[kv.first].svariation = kv.second.svariation;
    db.analog_input[kv.first].evariation = kv.second.evariation;
    db.analog_input[kv.first].clazz = kv.second.clazz;
    db.analog_input[kv.first].deadband = kv.second.deadband;
  }

  opendnp3::OutstationStackConfig stack(db);

  stack.outstation.eventBufferConfig = opendnp3::EventBufferConfig::AllTypes(100);
  stack.link.LocalAddr  = config.localAddr;
  stack.link.RemoteAddr = config.remoteAddr;

  return stack;
}

void Outstation::Run() {
  metrics->Start(pusher, config.id);

  while (running) {
    if (restartConfig.coldRestart) {
      restartConfig.coldRestarter(config.localAddr);
      restartConfig.coldRestart.store(false);

      continue;
    }

    if (restartConfig.warmRestart) {
      Disable();
      std::this_thread::sleep_for(std::chrono::seconds(restartConfig.warm));
      Enable();

      restartConfig.warmRestart.store(false);
      continue;
    }

    opendnp3::UpdateBuilder builder;

    for (const auto& kv : binaryPoints) {
      const std::uint16_t addr = kv.first;
      const std::string   tag  = kv.second.tag;

      try {
        auto lock = std::unique_lock<std::mutex>(pointsMu);

        auto point = points.at(tag);
        builder.Update(opendnp3::Binary(point.value != 0), addr);

        std::cout << fmt::format("[{}] updated binary {} to {}", config.id, addr, point.value) << std::endl;
      } catch (const std::out_of_range&) {}
    }

    for (const auto& kv : analogPoints) {
      const std::uint16_t addr = kv.first;
      const std::string   tag  = kv.second.tag;

      try {
        auto lock = std::unique_lock<std::mutex>(pointsMu);

        auto point = points.at(tag);
        builder.Update(opendnp3::Analog(point.value, opendnp3::Flags(0), opendnp3::DNPTime(point.ts)), addr);

        std::cout << fmt::format("[{}] updated analog {} to {}", config.id, addr, point.value) << std::endl;
      } catch (const std::out_of_range&) {}
    }

    outstation->Apply(builder.Build());
    std::this_thread::sleep_for(std::chrono::seconds(1));
  }

  metrics->Stop();
}

bool Outstation::AddBinaryInput(BinaryPoint point) {
  binaryPoints[point.address] = point;
  points[point.tag] = otsim::msgbus::Point{point.tag, 0.0, 0};

  return true;
}

bool Outstation::AddBinaryOutput(BinaryPoint point) {
  point.output = true;

  binaryPoints[point.address] = point;
  points[point.tag] = otsim::msgbus::Point{point.tag, 0.0, 0};

  return true;
}

bool Outstation::AddAnalogInput(AnalogPoint point) {
  analogPoints[point.address] = point;
  points[point.tag] = otsim::msgbus::Point{point.tag, 0.0, 0};

  return true;
}

bool Outstation::AddAnalogOutput(AnalogPoint point) {
  point.output = true;

  analogPoints[point.address] = point;
  points[point.tag] = otsim::msgbus::Point{point.tag, 0.0, 0};

  return true;
}

void Outstation::WriteBinary(std::uint16_t address, bool status) {
  auto iter = binaryPoints.find(address);
  if (iter == binaryPoints.end()) {
    return;
  }

  std::cout << fmt::format("[{}] setting tag {} to {}", config.id, iter->second.tag, status) << std::endl;

  otsim::msgbus::Points points;
  points.push_back(otsim::msgbus::Point{iter->second.tag, status ? 1.0 : 0.0});

  otsim::msgbus::Update contents = {.updates = points};
  auto env = otsim::msgbus::NewEnvelope(config.id, contents);

  pusher->Push("RUNTIME", env);
}

void Outstation::WriteAnalog(std::uint16_t address, double value) {
  auto iter = analogPoints.find(address);
  if (iter == analogPoints.end()) {
    return;
  }

  std::cout << fmt::format("[{}] setting tag {} to {}", config.id, iter->second.tag, value) << std::endl;

  otsim::msgbus::Points points;
  points.push_back(otsim::msgbus::Point{iter->second.tag, value});

  otsim::msgbus::Update contents = {.updates = points};
  auto env = otsim::msgbus::NewEnvelope(config.id, contents);

  pusher->Push("RUNTIME", env);
}

const BinaryPoint* Outstation::GetBinary(const uint16_t address) {
  auto iter = binaryPoints.find(address);
  if (iter == binaryPoints.end()) {
    return NULL;
  }

  return &iter->second;
}

const AnalogPoint* Outstation::GetAnalog(const uint16_t address) {
  auto iter = analogPoints.find(address);
  if (iter == analogPoints.end()) {
    return NULL;
  }

  return &iter->second;
}

void Outstation::ResetOutputs() {
  otsim::msgbus::Points points;

  for (const auto& kv : binaryPoints) {
    if (kv.second.output) {
      points.push_back(otsim::msgbus::Point{kv.second.tag, 0.0});
    }
  }

  for (const auto& kv : analogPoints) {
    if (kv.second.output) {
      points.push_back(otsim::msgbus::Point{kv.second.tag, 0.0});
    }
  }

  if (points.size()) {
    std::cout << fmt::format("[{}] setting outputs to zero values", config.id) << std::endl;

    otsim::msgbus::Update contents = {.updates = points};
    auto env = otsim::msgbus::NewEnvelope(config.id, contents);

    pusher->Push("RUNTIME", env);
  }
}

void Outstation::HandleMsgBusStatus(const otsim::msgbus::Envelope<otsim::msgbus::Status>& env) {
  auto sender = otsim::msgbus::GetEnvelopeSender(env);

  if (sender == config.id) {
    return;
  }

  metrics->IncrMetric("status_count");

  for (auto &p : env.contents.measurements) {
    if (points.count(p.tag)) {
      std::cout << fmt::format("[{}] status received for tag {}", config.id, p.tag) << std::endl;

      auto lock = std::unique_lock<std::mutex>(pointsMu);
      points[p.tag] = p;
    }
  }
}

uint16_t Outstation::ColdRestart() {
  restartConfig.coldRestart.store(true);
  return restartConfig.cold;
}

uint16_t Outstation::WarmRestart() {
  restartConfig.warmRestart.store(true);
  return restartConfig.warm;
}

opendnp3::CommandStatus Outstation::Select(const opendnp3::ControlRelayOutputBlock& arCommand, std::uint16_t aIndex) {
    if (!GetBinary(aIndex)) {
        // This is our best guess at what status to return when the address
        // being selected doesn't exist locally.
        return opendnp3::CommandStatus::OUT_OF_RANGE;
    }

    return opendnp3::CommandStatus::SUCCESS;
}

opendnp3::CommandStatus Outstation::Operate(const opendnp3::ControlRelayOutputBlock& arCommand, std::uint16_t aIndex, opendnp3::IUpdateHandler& handler, opendnp3::OperateType opType) {
    auto point = GetBinary(aIndex);

    if (!point) {
        // This is our best guess at what status to return when the address
        // being selected doesn't exist locally.
        return opendnp3::CommandStatus::OUT_OF_RANGE;
    }

    if (point->sbo && opType != opendnp3::OperateType::SelectBeforeOperate) {
        return opendnp3::CommandStatus::NO_SELECT;
    }

    bool val;

    switch (arCommand.opType) {
      case opendnp3::OperationType::LATCH_ON:
        val = true;
        break;
      case opendnp3::OperationType::LATCH_OFF:
        val = false;
        break;
      case opendnp3::OperationType::PULSE_ON:
        switch(arCommand.tcc) {
          case opendnp3::TripCloseCode::TRIP:
            val = false;
            break;
          case opendnp3::TripCloseCode::CLOSE:
            val = true;
            break;
          default:
            return opendnp3::CommandStatus::NOT_SUPPORTED;
        }

        break;
      default:
        return opendnp3::CommandStatus::NOT_SUPPORTED;
    }

    WriteBinary(aIndex, val);
    return opendnp3::CommandStatus::SUCCESS;
}

opendnp3::CommandStatus Outstation::Select(const opendnp3::AnalogOutputFloat32& arCommand, std::uint16_t aIndex) {
    if (!GetAnalog(aIndex)) {
        // This is our best guess at what status to return when the address
        // being selected doesn't exist locally.
        return opendnp3::CommandStatus::OUT_OF_RANGE;
    }

    return opendnp3::CommandStatus::SUCCESS;
}

opendnp3::CommandStatus Outstation::Operate(const opendnp3::AnalogOutputFloat32& arCommand, std::uint16_t aIndex, opendnp3::IUpdateHandler& handler, opendnp3::OperateType opType) {
    auto point = GetAnalog(aIndex);

    if (!point) {
        // This is our best guess at what status to return when the address
        // being selected doesn't exist locally.
        return opendnp3::CommandStatus::OUT_OF_RANGE;
    }

    if (point->sbo && opType != opendnp3::OperateType::SelectBeforeOperate) {
        return opendnp3::CommandStatus::NO_SELECT;
    }

    WriteAnalog(aIndex, arCommand.value);
    return opendnp3::CommandStatus::SUCCESS;
}

} // namespace dnp3
} // namespace otsim