#ifndef OTSIM_MSGBUS_METRICS_HPP
#define OTSIM_MSGBUS_METRICS_HPP

#include <atomic>
#include <mutex>
#include <thread>

#include "envelope.hpp"
#include "pusher.hpp"

namespace otsim {
namespace msgbus {

class MetricsPusher {
public:
  static std::shared_ptr<MetricsPusher> Create() {
    return std::make_shared<MetricsPusher>();
  }

  void Start(std::shared_ptr<Pusher> pusher, const std::string& name);
  void Stop();

  void NewMetric(const std::string& kind, const std::string& name, const std::string& desc);
  void IncrMetric(const std::string& name);
  void IncrMetricBy(const std::string& name, int val);
  void SetMetric(const std::string& name, double val);

private:
  void run(std::shared_ptr<Pusher> pusher, const std::string& name);

  std::atomic<bool> running;
  std::thread thread;

  std::map<std::string, Metric> metrics;
  std::mutex metricsMu;
};

} // namespace msgbus
} // namespace otsim

#endif // OTSIM_MSGBUS_METRICS_HPP