#ifndef OTSIM_DNP3_OUTSTATION_HPP
#define OTSIM_DNP3_OUTSTATION_HPP

#include <atomic>
#include <mutex>
#include <thread>

#include "common.hpp"

#include "msgbus/envelope.hpp"
#include "msgbus/metrics.hpp"
#include "msgbus/pusher.hpp"

#include "opendnp3/gen/RestartType.h"
#include "opendnp3/outstation/DefaultOutstationApplication.h"
#include "opendnp3/outstation/ICommandHandler.h"
#include "opendnp3/outstation/IOutstation.h"
#include "opendnp3/outstation/OutstationStackConfig.h"
#include "opendnp3/outstation/Updates.h"

namespace otsim {
namespace dnp3 {

typedef std::function<void(std::uint16_t)> ColdRestartFunc;

struct OutstationRestartConfig {
  std::uint16_t warm {};
  std::uint16_t cold {};

  std::atomic<bool> coldRestart;
  std::atomic<bool> warmRestart;

  ColdRestartFunc coldRestarter;

  OutstationRestartConfig(std::uint16_t w) : warm(w) {}
  OutstationRestartConfig(std::uint16_t w, std::uint16_t c, ColdRestartFunc f) : warm(w), cold(c), coldRestarter(f) {}
  OutstationRestartConfig(OutstationRestartConfig& c) : OutstationRestartConfig(c.warm, c.cold, c.coldRestarter) {}
};

struct OutstationConfig {
  std::string   id         {};
  std::uint16_t localAddr  {};
  std::uint16_t remoteAddr {};

  std::string logLevel = "info";
};

class Outstation : public opendnp3::DefaultOutstationApplication, public opendnp3::ICommandHandler {
public:
  static std::shared_ptr<Outstation> Create(OutstationConfig config, OutstationRestartConfig restart, Pusher pusher) {
    return std::make_shared<Outstation>(config, restart, pusher);
  }

  Outstation(OutstationConfig config, OutstationRestartConfig restart, Pusher pusher);
  ~Outstation() {};

  opendnp3::OutstationStackConfig Init();

  std::string ID() { return config.id; }
  std::uint16_t Address() { return config.localAddr; }

  void SetIOutstation(std::shared_ptr<opendnp3::IOutstation> o) { outstation = o; }

  bool Enable() {
    running.store(true);
    return outstation->Enable();
  }

  bool Disable() {
    running.store(false);
    return outstation->Disable();
  }

  void Run();

  bool AddBinaryInput(BinaryPoint point);
  bool AddBinaryOutput(BinaryPoint point);
  bool AddAnalogInput(AnalogPoint point);
  bool AddAnalogOutput(AnalogPoint point);

  void WriteBinary(uint16_t address, bool value);
  void WriteAnalog(uint16_t address, double value);

  const BinaryPoint* GetBinary(const uint16_t address);
  const AnalogPoint* GetAnalog(const uint16_t address);

  void ResetOutputs();
  void HandleMsgBusStatus(const otsim::msgbus::Envelope<otsim::msgbus::Status>& env);

  // BEGIN IOutstationApplication Implementation

  opendnp3::RestartMode ColdRestartSupport() const final override {
    return opendnp3::RestartMode::SUPPORTED_DELAY_COARSE;
  }

  opendnp3::RestartMode WarmRestartSupport() const final override {
    return opendnp3::RestartMode::SUPPORTED_DELAY_COARSE;
  }

  uint16_t ColdRestart() final override;
  uint16_t WarmRestart() final override;

  // END IOutstationApplication Implementation

  // BEGIN ICommandHandler Implementation
  opendnp3::CommandStatus Select(const opendnp3::ControlRelayOutputBlock& arCommand, std::uint16_t aIndex) override final;
  opendnp3::CommandStatus Operate(const opendnp3::ControlRelayOutputBlock& arCommand, std::uint16_t aIndex, opendnp3::IUpdateHandler& handler, opendnp3::OperateType opType) override final;
  opendnp3::CommandStatus Select(const opendnp3::AnalogOutputInt16& arCommand, std::uint16_t aIndex) override final {return opendnp3::CommandStatus::NOT_SUPPORTED;}
  opendnp3::CommandStatus Operate(const opendnp3::AnalogOutputInt16& arCommand, std::uint16_t aIndex, opendnp3::IUpdateHandler& handler, opendnp3::OperateType opType) override final {return opendnp3::CommandStatus::NOT_SUPPORTED;}
  opendnp3::CommandStatus Select(const opendnp3::AnalogOutputInt32& arCommand, std::uint16_t aIndex) override final {return opendnp3::CommandStatus::NOT_SUPPORTED;}
  opendnp3::CommandStatus Operate(const opendnp3::AnalogOutputInt32& arCommand, std::uint16_t aIndex, opendnp3::IUpdateHandler& handler, opendnp3::OperateType opType) override final {return opendnp3::CommandStatus::NOT_SUPPORTED;}
  opendnp3::CommandStatus Select(const opendnp3::AnalogOutputFloat32& arCommand, std::uint16_t aIndex) override final;
  opendnp3::CommandStatus Operate(const opendnp3::AnalogOutputFloat32& arCommand, std::uint16_t aIndex, opendnp3::IUpdateHandler& handler, opendnp3::OperateType opType) override final;
  opendnp3::CommandStatus Select(const opendnp3::AnalogOutputDouble64& arCommand, std::uint16_t aIndex) override final {return opendnp3::CommandStatus::NOT_SUPPORTED;}
  opendnp3::CommandStatus Operate(const opendnp3::AnalogOutputDouble64& arCommand, std::uint16_t aIndex, opendnp3::IUpdateHandler& handler, opendnp3::OperateType opType) override final {return opendnp3::CommandStatus::NOT_SUPPORTED;}
  // END ICommandHandler Implementation

protected:
  // BEGIN ICommandHandler Implementation
  virtual void Begin() override {}
  virtual void End() override {}
  // END ICommandHandler Implementation

private:
  OutstationConfig config;
  OutstationRestartConfig restartConfig;

  Pusher pusher;
  MetricsPusher metrics;

  std::shared_ptr<opendnp3::IOutstation> outstation;

  // NOTE: this assumes inputs and outputs will not use the same addresses.
  std::map<std::uint16_t, BinaryPoint> binaryPoints;
  std::map<std::uint16_t, AnalogPoint> analogPoints;

  std::map<std::string, otsim::msgbus::Point> points;
  std::mutex pointsMu;

  std::atomic<bool> running;
};

} // namespace dnp3
} // namespace otsim

#endif // OTSIM_DNP3_OUTSTATION_HPP