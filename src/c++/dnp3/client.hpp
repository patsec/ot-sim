#ifndef OTSIM_DNP3_CLIENT_HPP
#define OTSIM_DNP3_CLIENT_HPP

#include "common.hpp"
#include "master.hpp"

#include "opendnp3/DNP3Manager.h"

namespace otsim {
namespace dnp3 {

class Client : public std::enable_shared_from_this<Client>
{
public:
  static std::shared_ptr<Client> Create() {
    return std::make_shared<Client>();
  }

  Client();
  ~Client() {};

  bool Init(const std::string& id, const std::string& endpoint);

  std::shared_ptr<Master> AddMaster(std::string id, std::uint16_t local, std::uint16_t remote, Pusher pusher);

  void Start();
  void Stop();

private:
  std::shared_ptr<opendnp3::DNP3Manager> manager; // DNP3 stack manager
  std::shared_ptr<opendnp3::IChannel> channel;    // TCPServer channel

  std::map<std::uint16_t, std::shared_ptr<Master>> masters;
};

} // namespace dnp3
} // namespace otsim

#endif // OTSIM_DNP3_CLIENT_HPP
