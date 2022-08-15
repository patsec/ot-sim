#ifndef OTSIM_E2E_DNP3_MASTER_HANDLER_HPP
#define OTSIM_E2E_DNP3_MASTER_HANDLER_HPP

#include <map>
#include <mutex>

#include "opendnp3/master/ISOEHandler.h"

class TestHandler : public opendnp3::ISOEHandler {
public:
  void Reset() {
    binaryInput.clear();
    binaryOutput.clear();
    analogInput.clear();
    analogOutput.clear();
  }

  bool GetBinaryInput(int idx) {
    std::scoped_lock<std::mutex> guard(binaryMu);
    return binaryInput.at(idx);
  }

  bool GetBinaryOutput(int idx) {
    std::scoped_lock<std::mutex> guard(binaryMu);
    return binaryOutput.at(idx);
  }

  double GetAnalogInput(int idx) {
    std::scoped_lock<std::mutex> guard(analogMu);
    return analogInput.at(idx);
  }

  double GetAnalogOutput(int idx) {
    std::scoped_lock<std::mutex> guard(analogMu);
    return analogOutput.at(idx);
  }

  void Process(const opendnp3::HeaderInfo& info, const opendnp3::ICollection<opendnp3::Indexed<opendnp3::Binary>>& values) {
    std::scoped_lock<std::mutex> guard(binaryMu);

    values.ForeachItem([&](const opendnp3::Indexed<opendnp3::Binary>& value) {
      binaryInput[value.index] = value.value.value;
    });
  }

  void Process(const opendnp3::HeaderInfo& info, const opendnp3::ICollection<opendnp3::Indexed<opendnp3::BinaryOutputStatus>>& values) {
    std::scoped_lock<std::mutex> guard(binaryMu);

    values.ForeachItem([&](const opendnp3::Indexed<opendnp3::BinaryOutputStatus>& value) {
      binaryOutput[value.index] = value.value.value;
    });
  }

  void Process(const opendnp3::HeaderInfo& info, const opendnp3::ICollection<opendnp3::Indexed<opendnp3::Analog>>& values) {
    std::scoped_lock<std::mutex> guard(analogMu);

    values.ForeachItem([&](const opendnp3::Indexed<opendnp3::Analog>& value) {
      analogInput[value.index] = value.value.value;
    });
  }

  void Process(const opendnp3::HeaderInfo& info, const opendnp3::ICollection<opendnp3::Indexed<opendnp3::AnalogOutputStatus>>& values) {
    std::scoped_lock<std::mutex> guard(analogMu);

    values.ForeachItem([&](const opendnp3::Indexed<opendnp3::AnalogOutputStatus>& value) {
      analogOutput[value.index] = value.value.value;
    });
  }

  // BEGIN NOT IMPLEMENTED
  virtual void Process(const opendnp3::HeaderInfo& info, const opendnp3::ICollection<opendnp3::Indexed<opendnp3::DoubleBitBinary>>& values) override {}
  virtual void Process(const opendnp3::HeaderInfo& info, const opendnp3::ICollection<opendnp3::Indexed<opendnp3::Counter>>& values) override {}
  virtual void Process(const opendnp3::HeaderInfo& info, const opendnp3::ICollection<opendnp3::Indexed<opendnp3::FrozenCounter>>& values) override {}
  virtual void Process(const opendnp3::HeaderInfo& info, const opendnp3::ICollection<opendnp3::Indexed<opendnp3::OctetString>>& values) override {}
  virtual void Process(const opendnp3::HeaderInfo& info, const opendnp3::ICollection<opendnp3::Indexed<opendnp3::TimeAndInterval>>& values) override {}
  virtual void Process(const opendnp3::HeaderInfo& info, const opendnp3::ICollection<opendnp3::Indexed<opendnp3::BinaryCommandEvent>>& values) override {}
  virtual void Process(const opendnp3::HeaderInfo& info, const opendnp3::ICollection<opendnp3::Indexed<opendnp3::AnalogCommandEvent>>& values) override {}
  virtual void Process(const opendnp3::HeaderInfo& info, const opendnp3::ICollection<opendnp3::DNPTime>& values) override {}

  virtual void BeginFragment(const opendnp3::ResponseInfo& info) final {}
  virtual void EndFragment(const opendnp3::ResponseInfo& info) final {}
  // END NOT IMPLEMENTED

private:
  std::map<int, bool>   binaryInput;
  std::map<int, bool>   binaryOutput;
  std::map<int, double> analogInput;
  std::map<int, double> analogOutput;

  std::mutex binaryMu;
  std::mutex analogMu;
};

#endif // OTSIM_E2E_DNP3_MASTER_HANDLER_HPP
