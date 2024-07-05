#include <iostream>

#include "fmt/format.h"

namespace otsim {
namespace s7 {

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

} // namespace s7
} // namespace otsim
