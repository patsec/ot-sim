#ifndef OTSIM_S7_CLIENT_HPP
#define OTSIM_S7_CLIENT_HPP

#include <condition_variable>
#include <iostream>
#include <map>

#include "common.hpp"

#include "msgbus/envelope.hpp"

#include "snap7.h"

namespace otsim {
namespace s7 {

struct ClientConfig {
    std::string id;
    std::uint16_t address;
    std::string logLevel = "info";
};

class Client : public std::enable_shared_from_this<Client>{
public:
    //used for dynamically creating Clients
    static std::shared_ptr<Client> Create(std::string id, Pusher pusher) {
        return std::make_shared<Client>(id, pusher);
    }

    Client(std::string id, Pusher pusher);
    ~Client() {};

    //void Run(std::shared_ptr<TS7Client> ts7client);

    std::string ID() { return id; }
    std::uint16_t Address() { return address; }
    
    //setter function for id and address (config)
    ClientConfig BuildConfig(std::string idVal, std::uint16_t local) {
        address = local;
        id = idVal;

        ClientConfig config;

        config.address  = local;
        config.id = idVal;

        std::cout << "initializing client " << config.address << std::endl;

        return config;
    }

    void AddBinaryTag(std::uint16_t address, std::string tag) {
        binaryInputTags[address] = tag;
    }

    void AddBinaryTag(std::uint16_t address, std::string tag, bool sbo) {
        binaryOutputTags[address] = tag;

        BinaryOutputPoint point = {.address = address, .tag = tag, .output = true, .sbo = sbo};
        binaryOutputs[tag] = point;
    }

    void AddAnalogTag(std::uint16_t address, std::string tag) {
        analogInputTags[address] = tag;
    }

    void AddAnalogTag(std::uint16_t address, std::string tag, bool sbo) {
        analogOutputTags[address] = tag;

        AnalogOutputPoint point = {.address = address, .tag = tag, .output = true, .sbo = sbo};
        analogOutputs[tag] = point;
    }
  
    std::string GetBinaryTag(std::uint16_t address, bool output = false) {
        if (output) {
        auto iter = binaryOutputTags.find(address);
        if (iter != binaryOutputTags.end()) {
            return iter->second;
        }

        return {};
        }

        auto iter = binaryInputTags.find(address);
        if (iter != binaryInputTags.end()) {
        return iter->second;
        }

        return {};
    }

    std::string GetAnalogTag(std::uint16_t address, bool output = false) {
        if (output) {
        auto iter = analogOutputTags.find(address);
        if (iter != analogOutputTags.end()) {
            return iter->second;
        }

        return {};
        }

        auto iter = analogInputTags.find(address);
        if (iter != analogInputTags.end()) {
        return iter->second;
        }

        return {};
    }

    bool WriteBinary(std::string tag, bool status) {
        auto iter = binaryOutputs.find(tag);
        if (iter == binaryOutputs.end()) {
        return false;
        }

        auto point = iter->second;

        if (!point.output) {
        return false;
        }

        // TODO: 
        

        return true;
    }
    
    bool WriteAnalog(std::string tag, double value) {
        auto iter = analogOutputs.find(tag);
        if (iter == analogOutputs.end()) {
        return false;
        }

        auto point = iter->second;

        if (!point.output) {
        return false;
        }

        // TODO:
        

        return true;
    }

  void HandleMsgBusUpdate(const otsim::msgbus::Envelope<otsim::msgbus::Update>& env);

private:
  std::string   id;
  std::uint16_t address;

  Pusher pusher;

  std::shared_ptr<otsim::s7::Client> client;

  std::map<std::uint16_t, std::string> binaryInputTags;
  std::map<std::uint16_t, std::string> binaryOutputTags;
  std::map<std::uint16_t, std::string> analogInputTags;
  std::map<std::uint16_t, std::string> analogOutputTags;
  std::map<std::string, BinaryOutputPoint> binaryOutputs;
  std::map<std::string, AnalogOutputPoint> analogOutputs;
};

} // namespace s7
} // namespace otsim

#endif // OTSIM_S7_CLIENT_HPP