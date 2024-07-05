#ifndef OTSIM_S7_SERVER_HPP
#define OTSIM_S7_SERVER_HPP

#include <condition_variable>
#include <iostream>
#include <map>

#include "msgbus/envelope.hpp"
#include "msgbus/pusher.hpp"

#include "snap7.h"

namespace otsim {
namespace snap7 {
class Server {
public:
    /*static std::shared_ptr<Server> Create(std::string id, otsim::msgbus::Pusher pusher) {
        return std::make_shared<Server>(id, pusher);
    }*/

    Server(std::string id, otsim::msgbus::Pusher pusher);

    ~Server() {};

    std::string ID() { return id; }
    std::uint16_t Address() { return address; }

    void HandleMsgBusStatus(const otsim::msgbus::Envelope<otsim::msgbus::Status>& env);

    // TODO: finish BuildConfig function
    void BuildConfig(std::uint16_t ip_address) {
        address = ip_address;
    }
    // TODO: add binary/analog code

private:
  std::string   id;
  std::uint16_t address;

  otsim::msgbus::Pusher pusher;

  std::map<std::uint16_t, std::string> binaryInputTags;
  std::map<std::uint16_t, std::string> binaryOutputTags;
  std::map<std::uint16_t, std::string> analogInputTags;
  std::map<std::uint16_t, std::string> analogOutputTags;

  //need to implement binaryOutputs / analogOutputs typedef 
        //std::map<std::string, BinaryOutputPoint> binaryOutputs;
        //std::map<std::string, AnalogOutputPoint> analogOutputs;
};
} // namespace s7
} // namespace otsim

#endif // OTSIM_DNP3_MASTER_HPP
