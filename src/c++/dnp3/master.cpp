#include <iostream>

#include "master.hpp"

#include "fmt/format.h"

namespace otsim {
namespace dnp3 {

Master::Master(std::string id, Pusher pusher) : id(id), pusher(pusher) {}

void Master::HandleMsgBusUpdate(const otsim::msgbus::Envelope<otsim::msgbus::Update>& env) {
  auto sender = otsim::msgbus::GetEnvelopeSender(env);

  if (sender == id) {
    return;
  }

  for (auto &p : env.contents.updates) {
    if (WriteBinary(p.tag, p.value)) {
      continue;
    }

    WriteAnalog(p.tag, p.value);
  }
}

void Master::Process(const opendnp3::HeaderInfo& info, const opendnp3::ICollection<opendnp3::Indexed<opendnp3::Binary>>& values) {
  values.ForeachItem([&](const opendnp3::Indexed<opendnp3::Binary>& value) {
    const char* gvar = opendnp3::GroupVariationSpec().to_string(info.gv);

    std::cout << fmt::format("[{}] BOOLEAN  GV:ADDR:VALUE:TIME = {}:{}:{}:{}", id, gvar, value.index, value.value.value, value.value.time.value) << std::endl;

    auto tag = GetBinaryTag(value.index);

    if (!tag.empty()) {
      std::cout << fmt::format("[{}] setting tag {} to {}", id, tag, value.value.value) << std::endl;

      otsim::msgbus::Points points;
      points.push_back(otsim::msgbus::Point{tag, value.value.value ? 1.0 : 0.0, value.value.time.value});

      otsim::msgbus::Status contents = {.measurements = points};
      auto env = otsim::msgbus::NewEnvelope(id, contents);

      pusher->Push("RUNTIME", env);
    } else {
      std::cout << fmt::format("[{}] data manager in master missing tag for binary at address {}", value.index) << std::endl;
    }
  });
}

void Master::Process(const opendnp3::HeaderInfo& info, const opendnp3::ICollection<opendnp3::Indexed<opendnp3::Analog>>& values) {
  values.ForeachItem([&](const opendnp3::Indexed<opendnp3::Analog>& value) {
    const char* gvar = opendnp3::GroupVariationSpec().to_string(info.gv);

    std::cout << fmt::format("[{}] ANALOG   GV:ADDR:VALUE:TIME = {}:{}:{}:{}", id, gvar, value.index, value.value.value, value.value.time.value) << std::endl;

    auto tag = GetAnalogTag(value.index);

    if (!tag.empty()) {
      std::cout << fmt::format("[{}] setting tag {} to {}", id, tag, value.value.value) << std::endl;

      otsim::msgbus::Points points;
      points.push_back(otsim::msgbus::Point{tag, value.value.value, value.value.time.value});

      otsim::msgbus::Status contents = {.measurements = points};
      auto env = otsim::msgbus::NewEnvelope(id, contents);

      pusher->Push("RUNTIME", env);
    } else {
      std::cout << fmt::format("[{}] data manager in master missing tag for analog at address {}", value.index) << std::endl;
    }
  });
}

} // namespace dnp3
} // namespace otsim
