#ifndef OTSIM_E2E_DNP3_MASTER_HANDLER_HPP
#define OTSIM_E2E_DNP3_MASTER_HANDLER_HPP

#include <map>
#include <mutex>

#include "opendnp3/master/ISOEHandler.h"

class TestHandler : public opendnp3::ISOEHandler {
public:
  void Reset() {
    binary.clear();
    analog.clear();
  }

  bool GetBinary(int idx) {
    std::scoped_lock<std::mutex> guard(binaryMu);
    return binary.at(idx);
  }

  double GetAnalog(int idx) {
    std::scoped_lock<std::mutex> guard(analogMu);
    return analog.at(idx);
  }

  void Process(const opendnp3::HeaderInfo& info, const opendnp3::ICollection<opendnp3::Indexed<opendnp3::Binary>>& values) {
    std::scoped_lock<std::mutex> guard(binaryMu);

    values.ForeachItem([&](const opendnp3::Indexed<opendnp3::Binary>& value) {
      binary[value.index] = value.value.value;
    });
  }

  void Process(const opendnp3::HeaderInfo& info, const opendnp3::ICollection<opendnp3::Indexed<opendnp3::Analog>>& values) {
    std::scoped_lock<std::mutex> guard(analogMu);

    values.ForeachItem([&](const opendnp3::Indexed<opendnp3::Analog>& value) {
      analog[value.index] = value.value.value;
    });
  }

  // BEGIN NOT IMPLEMENTED
  virtual void Process(const opendnp3::HeaderInfo& info, const opendnp3::ICollection<opendnp3::Indexed<opendnp3::DoubleBitBinary>>& values) override {}
  virtual void Process(const opendnp3::HeaderInfo& info, const opendnp3::ICollection<opendnp3::Indexed<opendnp3::Counter>>& values) override {}
  virtual void Process(const opendnp3::HeaderInfo& info, const opendnp3::ICollection<opendnp3::Indexed<opendnp3::FrozenCounter>>& values) override {}
  virtual void Process(const opendnp3::HeaderInfo& info, const opendnp3::ICollection<opendnp3::Indexed<opendnp3::BinaryOutputStatus>>& values) override {}
  virtual void Process(const opendnp3::HeaderInfo& info, const opendnp3::ICollection<opendnp3::Indexed<opendnp3::AnalogOutputStatus>>& values) override {}
  virtual void Process(const opendnp3::HeaderInfo& info, const opendnp3::ICollection<opendnp3::Indexed<opendnp3::OctetString>>& values) override {}
  virtual void Process(const opendnp3::HeaderInfo& info, const opendnp3::ICollection<opendnp3::Indexed<opendnp3::TimeAndInterval>>& values) override {}
  virtual void Process(const opendnp3::HeaderInfo& info, const opendnp3::ICollection<opendnp3::Indexed<opendnp3::BinaryCommandEvent>>& values) override {}
  virtual void Process(const opendnp3::HeaderInfo& info, const opendnp3::ICollection<opendnp3::Indexed<opendnp3::AnalogCommandEvent>>& values) override {}
  virtual void Process(const opendnp3::HeaderInfo& info, const opendnp3::ICollection<opendnp3::DNPTime>& values) override {}

  virtual void BeginFragment(const opendnp3::ResponseInfo& info) final {}
  virtual void EndFragment(const opendnp3::ResponseInfo& info) final {}
  // END NOT IMPLEMENTED

private:
  std::map<int, bool>   binary;
  std::map<int, double> analog;

  std::mutex binaryMu;
  std::mutex analogMu;
};

#endif // OTSIM_E2E_DNP3_MASTER_HANDLER_HPP
