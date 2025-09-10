#ifndef OTSIM_S7_SERVER_HPP
#define OTSIM_S7_SERVER_HPP

#include <atomic>
#include <mutex>
#include <thread>

#include "common.hpp"

#include "msgbus/envelope.hpp"
#include "msgbus/metrics.hpp"
#include "msgbus/pusher.hpp"

#include "snap7.h"

namespace otsim {
namespace s7 {

struct ServerConfig {
    std::string id;
    std::uint16_t address;
    std::string logLevel = "info";
};

class Server : public std::enable_shared_from_this<Server>{
public:
    static std::shared_ptr<Server> Create(ServerConfig config, Pusher pusher) {
        return std::make_shared<Server>(config, pusher);
    }

    Server(ServerConfig config, Pusher pusher);
    ~Server() {};

    std::string ID() { return config.id; }

    void Run(std::shared_ptr<TS7Server> ts7server);

    bool AddBinaryInput(BinaryInputPoint point);
    bool AddBinaryOutput(BinaryOutputPoint point);
    bool AddAnalogInput(AnalogInputPoint point);
    bool AddAnalogOutput(AnalogOutputPoint point);

    void WriteBinary(uint16_t address, bool value);
    void WriteAnalog(uint16_t address, double value);

    const BinaryOutputPoint* GetBinaryOutput(const uint16_t address);
    const AnalogOutputPoint* GetAnalogOutput(const uint16_t address);

    void ResetOutputs();

    void HandleMsgBusStatus(const otsim::msgbus::Envelope<otsim::msgbus::Status>& env);

private:
  ServerConfig config;

  Pusher pusher;
  MetricsPusher metrics;

  std::map<std::uint16_t, BinaryInputPoint> binaryInputs;
  std::map<std::uint16_t, BinaryOutputPoint> binaryOutputs;
  std::map<std::uint16_t, AnalogInputPoint> analogInputs;
  std::map<std::uint16_t, AnalogOutputPoint> analogOutputs;

  std::map<std::string, otsim::msgbus::Point> points;
  std::mutex pointsMu;

  std::atomic<bool> running;
};

} // namespace s7
} // namespace otsim

#endif // OTSIM_S7_SERVER_HPP
