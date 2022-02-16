#ifndef OTSIM_DNP3_SERVER_HPP
#define OTSIM_DNP3_SERVER_HPP

#include <thread>

#include "outstation.hpp"

#include "opendnp3/DNP3Manager.h"

namespace otsim {
namespace dnp3 {

class Server : public std::enable_shared_from_this<Server> {
public:
  static std::shared_ptr<Server> Create(const std::uint16_t cold) {
    return std::make_shared<Server>(cold);
  }

  Server(const std::uint16_t cold);
  ~Server() {};

  bool Init(const std::string& id, const std::string& endpoint, const opendnp3::ServerAcceptMode acceptMode = opendnp3::ServerAcceptMode::CloseNew);

  std::shared_ptr<Outstation> AddOutstation(OutstationConfig config, OutstationRestartConfig restart, Pusher pusher);

  void Start();
  void Stop();

  void HandleColdRestart(std::uint16_t outstation);

private:
  std::shared_ptr<opendnp3::DNP3Manager> manager;    // Outstation stack manager
  std::shared_ptr<opendnp3::IChannel> channel;       // TCPServer channel

  std::uint16_t coldRestartSecs;

  // Keep track of warm restart delay and binary/analog points per-outstation.
  // The key is the outstation local address.
  std::map<std::uint16_t, std::shared_ptr<Outstation>> outstations;

  // Keep outstation threads in scope so they don't terminate immediately.
  std::vector<std::thread> threads;
};

} // namespace dnp3
} // namespace otsim

#endif // OTSIM_DNP3_SERVER_HPP
