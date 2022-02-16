#include "metrics.hpp"

namespace otsim {
namespace msgbus {

void MetricsPusher::Start(std::shared_ptr<Pusher> pusher, const std::string& name) {
  thread = std::thread(&MetricsPusher::run, this, pusher, name);
}

void MetricsPusher::Stop() {
  running.store(false);

  if (thread.joinable()) {
    thread.join();
  }
}

void MetricsPusher::NewMetric(const std::string& kind, const std::string& name, const std::string& desc) {
  Metric metric = {
    .kind = kind,
    .name = name,
    .desc = desc,
  };

  metrics[name] = metric;
}

void MetricsPusher::IncrMetric(const std::string &name) {
  try {
    auto lock = std::unique_lock<std::mutex>(metricsMu);

    auto metric = metrics.at(name);
    metric.value += 1.0;

    metrics[name] = metric;
  } catch(const std::out_of_range&) {}
}

void MetricsPusher::IncrMetricBy(const std::string &name, int val) {
  try {
    auto lock = std::unique_lock<std::mutex>(metricsMu);

    auto metric = metrics.at(name);
    metric.value += val;

    metrics[name] = metric;
  } catch(const std::out_of_range&) {}
}

void MetricsPusher::SetMetric(const std::string &name, double val) {
  try {
    auto lock = std::unique_lock<std::mutex>(metricsMu);

    auto metric = metrics.at(name);
    metric.value = val;

    metrics[name] = metric;
  } catch(const std::out_of_range&) {}
}

void MetricsPusher::run(std::shared_ptr<Pusher> pusher, const std::string& name) {
  auto prefix = name + "_";

  running.store(true);

  while (running) {
    std::vector<Metric> updates;

    {
      auto lock = std::unique_lock<std::mutex>(metricsMu);

      for (auto [name, metric] : metrics) {
        auto copy = metric;

        auto pos = std::mismatch(prefix.begin(), prefix.end(), copy.name.begin());
        if (pos.first != name.end()) {
          copy.name = prefix + copy.name;
        }

        updates.push_back(copy);
      }
    }

    if (updates.size() > 0) {
      auto env = NewEnvelope(name, Metrics{.metrics = updates});
      pusher->Push("HEALTH", env);
    }

    std::this_thread::sleep_for(std::chrono::seconds(5));
  }
}

} // namespace msgbus
} // namespace otsim