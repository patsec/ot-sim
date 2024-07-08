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
namespace snap7 {

struct ServerConfig {
    std::string id; //id = ip address
    std::string logLevel = "info";
};

class Server {
public:
    static std::shared_ptr<Server> Create(ServerConfig config, otsim::s7::Pusher pusher) {
        return std::make_shared<Server>(config, pusher);
    }

    Server(ServerConfig config, otsim::s7::Pusher pusher);
    ~Server() {};

    std::string ID() { return config.id; }

    void Run();

    bool AddBinaryInput(otsim::s7::BinaryInputPoint point);
    bool AddBinaryOutput(otsim::s7::BinaryOutputPoint point);
    bool AddAnalogInput(otsim::s7::AnalogInputPoint point);
    bool AddAnalogOutput(otsim::s7::AnalogOutputPoint point);

    void WriteBinary(uint16_t address, bool value);
    void WriteAnalog(uint16_t address, double value);

    const otsim::s7::BinaryOutputPoint* GetBinaryOutput(const uint16_t address);
    const otsim::s7::AnalogOutputPoint* GetAnalogOutput(const uint16_t address);

    void ResetOutputs();

    void HandleMsgBusStatus(const otsim::msgbus::Envelope<otsim::msgbus::Status>& env);

private:
  ServerConfig config;

  otsim::s7::Pusher pusher;
  otsim::msgbus::MetricsPusher metrics;

  std::map<std::uint16_t, otsim::s7::BinaryInputPoint> binaryInputs;
  std::map<std::uint16_t, otsim::s7::BinaryOutputPoint> binaryOutputs;
  std::map<std::uint16_t, otsim::s7::AnalogInputPoint> analogInputs;
  std::map<std::uint16_t, otsim::s7::AnalogOutputPoint> analogOutputs;

  std::map<std::string, otsim::msgbus::Point> points;
  std::mutex pointsMu;

  std::atomic<bool> running;
};
} // namespace s7
} // namespace otsim

#endif // OTSIM_DNP3_MASTER_HPP
