#ifndef OTSIM_DNP3_MASTER_HPP
#define OTSIM_DNP3_MASTER_HPP

#include <condition_variable>
#include <iostream>
#include <map>

#include "common.hpp"

#include "msgbus/envelope.hpp"

#include "opendnp3/master/CommandSet.h"
#include "opendnp3/master/IMaster.h"
#include "opendnp3/master/ISOEHandler.h"
#include "opendnp3/master/MasterStackConfig.h"
#include "opendnp3/master/ResponseInfo.h"

namespace otsim {
namespace dnp3 {

class Master : public opendnp3::ISOEHandler, public std::enable_shared_from_this<Master> {
public:
  static std::shared_ptr<Master> Create(std::string id, Pusher pusher) {
    return std::make_shared<Master>(id, pusher);
  }

  Master(std::string id, Pusher pusher);
  ~Master() {};

  std::string ID() { return id; }
  std::uint16_t Address() { return address; }

  void SetIMaster(std::shared_ptr<opendnp3::IMaster> m) { master = m; }

  bool Enable() { return master->Enable(); }
  bool Disable() { return master->Disable(); }

  void AddClassScan(const opendnp3::ClassField& field, opendnp3::TimeDuration period) {
    master->AddClassScan(field, period, shared_from_this());
  }

  std::int64_t Restart(opendnp3::RestartType type) {
    std::condition_variable wait;
    std::int64_t duration;

    auto callback = [&](opendnp3::RestartOperationResult res) -> void {
      duration = std::chrono::duration_cast<std::chrono::milliseconds>(res.restartTime.value).count();
      wait.notify_all();
    };

    master->Restart(type, callback);

    std::mutex m; std::unique_lock<std::mutex> lock(m);
    wait.wait(lock);

    return duration;
  }

  opendnp3::MasterStackConfig BuildConfig(std::uint16_t local, std::uint16_t remote) {
    address = local;

    opendnp3::MasterStackConfig config;

    config.master.disableUnsolOnStartup       = false;
    config.master.startupIntegrityClassMask   = opendnp3::ClassField(opendnp3::ClassField::CLASS_0);
    config.master.unsolClassMask              = opendnp3::ClassField(opendnp3::ClassField::CLASS_0);
    config.master.integrityOnEventOverflowIIN = false;

    config.link.LocalAddr  = local;
    config.link.RemoteAddr = remote;

    std::cout << "initializing master " << config.link.LocalAddr << " --> " << config.link.RemoteAddr << std::endl;

    return config;
  }

  void AddBinaryTag(std::uint16_t address, std::string tag) {
    binaryInputTags[address] = tag;
  }

  void AddBinaryTag(std::uint16_t address, std::string tag, bool sbo) {
    binaryOutputTags[address] = tag;

    BinaryOutputPoint point = {.address = address, .tag = tag, .output = true, .sbo = sbo};
    binaryOutputs[tag] = point;
  }

  void AddAnalogTag(std::uint16_t address, std::string tag) {
    analogInputTags[address] = tag;
  }

  void AddAnalogTag(std::uint16_t address, std::string tag, bool sbo) {
    analogOutputTags[address] = tag;

    AnalogOutputPoint point = {.address = address, .tag = tag, .output = true, .sbo = sbo};
    analogOutputs[tag] = point;
  }

  std::string GetBinaryTag(std::uint16_t address, bool output = false) {
    if (output) {
      auto iter = binaryOutputTags.find(address);
      if (iter != binaryOutputTags.end()) {
        return iter->second;
      }

      return {};
    }

    auto iter = binaryInputTags.find(address);
    if (iter != binaryInputTags.end()) {
      return iter->second;
    }

    return {};
  }

  std::string GetAnalogTag(std::uint16_t address, bool output = false) {
    if (output) {
      auto iter = analogOutputTags.find(address);
      if (iter != analogOutputTags.end()) {
        return iter->second;
      }

      return {};
    }

    auto iter = analogInputTags.find(address);
    if (iter != analogInputTags.end()) {
      return iter->second;
    }

    return {};
  }

  bool WriteBinary(std::string tag, bool status) {
    auto iter = binaryOutputs.find(tag);
    if (iter == binaryOutputs.end()) {
      return false;
    }

    auto point = iter->second;

    if (!point.output) {
      return false;
    }

    auto callback = [](const opendnp3::ICommandTaskResult&) -> void {};

    if (point.sbo) {
      opendnp3::OperationType code = status ? opendnp3::OperationType::LATCH_ON : opendnp3::OperationType::LATCH_OFF;
      opendnp3::ControlRelayOutputBlock crob(code);

      master->SelectAndOperate(opendnp3::CommandSet({ WithIndex(crob, point.address) }), callback);
    } else {
      opendnp3::OperationType code = status ? opendnp3::OperationType::LATCH_ON : opendnp3::OperationType::LATCH_OFF;
      opendnp3::ControlRelayOutputBlock crob(code);

      master->DirectOperate(opendnp3::CommandSet({ WithIndex(crob, point.address) }), callback);
    }

    return true;
  }

  bool WriteAnalog(std::string tag, double value) {
    auto iter = analogOutputs.find(tag);
    if (iter == analogOutputs.end()) {
      return false;
    }

    auto point = iter->second;

    if (!point.output) {
      return false;
    }

    auto callback = [](const opendnp3::ICommandTaskResult&) -> void {};

    // TODO: use point group and variation to determine which type of analog
    // value to write.

    if (point.sbo) {
      auto val = static_cast<opendnp3::AnalogOutputFloat32>(value);
      master->SelectAndOperate(opendnp3::CommandSet({ WithIndex(val, point.address) }), callback);
    } else {
      auto val = static_cast<opendnp3::AnalogOutputFloat32>(value);
      master->DirectOperate(opendnp3::CommandSet({ WithIndex(val, point.address) }), callback);
    }

    return true;
  }

  void HandleMsgBusUpdate(const otsim::msgbus::Envelope<otsim::msgbus::Update>& env);

  // BEGIN ISOEHandler Implementation

  virtual void Process(const opendnp3::HeaderInfo& info, const opendnp3::ICollection<opendnp3::Indexed<opendnp3::Binary>>& values) override;
  virtual void Process(const opendnp3::HeaderInfo& info, const opendnp3::ICollection<opendnp3::Indexed<opendnp3::DoubleBitBinary>>& values) override {}
  virtual void Process(const opendnp3::HeaderInfo& info, const opendnp3::ICollection<opendnp3::Indexed<opendnp3::Analog>>& values) override;
  virtual void Process(const opendnp3::HeaderInfo& info, const opendnp3::ICollection<opendnp3::Indexed<opendnp3::Counter>>& values) override {}
  virtual void Process(const opendnp3::HeaderInfo& info, const opendnp3::ICollection<opendnp3::Indexed<opendnp3::FrozenCounter>>& values) override {}
  virtual void Process(const opendnp3::HeaderInfo& info, const opendnp3::ICollection<opendnp3::Indexed<opendnp3::BinaryOutputStatus>>& values) override;
  virtual void Process(const opendnp3::HeaderInfo& info, const opendnp3::ICollection<opendnp3::Indexed<opendnp3::AnalogOutputStatus>>& values) override;
  virtual void Process(const opendnp3::HeaderInfo& info, const opendnp3::ICollection<opendnp3::Indexed<opendnp3::OctetString>>& values) override {}
  virtual void Process(const opendnp3::HeaderInfo& info, const opendnp3::ICollection<opendnp3::Indexed<opendnp3::TimeAndInterval>>& values) override {}
  virtual void Process(const opendnp3::HeaderInfo& info, const opendnp3::ICollection<opendnp3::Indexed<opendnp3::BinaryCommandEvent>>& values) override {}
  virtual void Process(const opendnp3::HeaderInfo& info, const opendnp3::ICollection<opendnp3::Indexed<opendnp3::AnalogCommandEvent>>& values) override {}
  virtual void Process(const opendnp3::HeaderInfo& info, const opendnp3::ICollection<opendnp3::DNPTime>& values) override {}

  virtual void BeginFragment(const opendnp3::ResponseInfo& info) final {}
  virtual void EndFragment(const opendnp3::ResponseInfo& info) final {}

  // END ISOEHandler Implementation

private:
  std::string   id;
  std::uint16_t address;

  Pusher pusher;

  std::shared_ptr<opendnp3::IMaster> master;

  std::map<std::uint16_t, std::string> binaryInputTags;
  std::map<std::uint16_t, std::string> binaryOutputTags;
  std::map<std::uint16_t, std::string> analogInputTags;
  std::map<std::uint16_t, std::string> analogOutputTags;
  std::map<std::string, BinaryOutputPoint> binaryOutputs;
  std::map<std::string, AnalogOutputPoint> analogOutputs;
};

} // namespace dnp3
} // namespace otsim

#endif // OTSIM_DNP3_MASTER_HPP
