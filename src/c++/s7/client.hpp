#ifndef OTSIM_S7_MASTER_HPP
#define OTSIM_S7_MASTER_HPP

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
    std::string address;
};

class Client{
public:
    //used for dynamically creating Clients
    static std::shared_ptr<Client> Create(std::string id, Pusher pusher) {
        return std::make_shared<Client>(id, pusher);
    }

    Client(std::string id, Pusher pusher);
    ~Client() {};

    std::string ID() { return id; }
    std::string Address() { return address; }
    
    //setter function for id and address (config)
    ClientConfig BuildConfig(std::string idVal, std::string local) {
        address = local;

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
/*
    // TODO: Finish WriteBinary function, currently uses dnp3 functions
    bool WriteBinary(std::string tag, bool status) {
        auto iter = binaryOutputs.find(tag);
        if (iter == binaryOutputs.end()) {
        return false;
        }

        auto point = iter->second;

        if (!point.output) {
        return false;
        }
        
        // TODO: Learn about S7 equivalent of ICommandTaskResult, OperationType, CommandSet, ControlRelayOutputBlock
        // TODO: Replace this block of code with S7 method of writing binary
            auto callback = [](const opendnp3::ICommandTaskResult&) -> void {};

            if (point.sbo) {
            opendnp3::OperationType code = status ? opendnp3::OperationType::LATCH_ON : opendnp3::OperationType::LATCH_OFF;
            opendnp3::ControlRelayOutputBlock crob(code);

            master->SelectAndOperate(opendnp3::CommandSet({ WithIndex(crob, point.address) }), callback);
            } else {
            opendnp3::OperationType code = status ? opendnp3::OperationType::LATCH_ON : opendnp3::OperationType::LATCH_OFF;
            opendnp3::ControlRelayOutputBlock crob(code);

            master->DirectOperate(opendnp3::CommandSet({ WithIndex(crob, point.address) }), callback);
            }

        return true;
    }
    
    // TODO: Implement WriteAnalog function below

    
    bool WriteAnalog(std::string tag, double value) {
        auto iter = analogOutputs.find(tag);
        if (iter == analogOutputs.end()) {
        return false;
        }

        auto point = iter->second;

        if (!point.output) {
        return false;
        }

        auto callback = [](const opendnp3::ICommandTaskResult&) -> void {};

        if (point.sbo) {
        auto val = static_cast<opendnp3::AnalogOutputFloat32>(value);
        master->SelectAndOperate(opendnp3::CommandSet({ WithIndex(val, point.address) }), callback);
        } else {
        auto val = static_cast<opendnp3::AnalogOutputFloat32>(value);
        master->DirectOperate(opendnp3::CommandSet({ WithIndex(val, point.address) }), callback);
        }

        return true;
    }*/

  void HandleMsgBusUpdate(const otsim::msgbus::Envelope<otsim::msgbus::Update>& env);


private:
  std::string   id;
  std::string address;

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

#endif // OTSIM_S7_MASTER_HPP