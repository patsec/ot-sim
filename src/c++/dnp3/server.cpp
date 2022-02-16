#include <iostream>

#include "server.hpp"

#include "opendnp3/channel/ChannelRetry.h"
#include "opendnp3/channel/IPEndpoint.h"
#include "opendnp3/channel/SerialSettings.h"
#include "opendnp3/ConsoleLogger.h"
#include "opendnp3/gen/ServerAcceptMode.h"
#include "opendnp3/logging/LogLevels.h"
#include "opendnp3/outstation/DefaultOutstationApplication.h"
#include "opendnp3/outstation/IOutstationApplication.h"
#include "opendnp3/outstation/UpdateBuilder.h"

namespace otsim {
namespace dnp3 {

Server::Server(const std::uint16_t cold) : coldRestartSecs(cold)
{
    manager.reset(new opendnp3::DNP3Manager(std::thread::hardware_concurrency(), opendnp3::ConsoleLogger::Create()));
}

bool Server::Init(const std::string& id, const std::string& endpoint, const opendnp3::ServerAcceptMode acceptMode) {
    std::string ip = endpoint.substr(0, endpoint.find(":"));
    std::uint16_t port = static_cast<std::uint16_t>(stoi(endpoint.substr(endpoint.find(":") + 1)));

    try {
        channel = manager->AddTCPServer(
            id,
            opendnp3::levels::NORMAL,
            acceptMode,
            opendnp3::IPEndpoint(ip, port),
            nullptr
        );
    } catch (std::exception& e) {
        return false;
    }

    return true;
}

std::shared_ptr<Outstation> Server::AddOutstation(OutstationConfig config, OutstationRestartConfig restart, Pusher pusher) {
    std::cout << "adding outstation " << config.remoteAddr << " --> " << config.localAddr << std::endl;

    restart.cold = coldRestartSecs;
    restart.coldRestarter = std::bind(&Server::HandleColdRestart, this, std::placeholders::_1);

    auto outstation = Outstation::Create(config, restart, pusher);
    outstations[config.localAddr] = outstation;

    return outstation;
}

void Server::Start() {
    for (const auto& kv : outstations) {
        auto outstation = kv.second;
        auto config     = outstation->Init();

        auto iOutstation = channel->AddOutstation(outstation->ID(), outstation, outstation, config);

        outstation->SetIOutstation(iOutstation);
        outstation->Enable();

        threads.push_back(std::thread(std::bind(&Outstation::Run, outstation)));
    }
}

void Server::Stop() {
    for (const auto& kv : outstations) {
        kv.second->Disable();
    }

    for (auto &t : threads) {
        if (t.joinable()) {
            t.join();
        }
    }
}

void Server::HandleColdRestart(std::uint16_t outstation) {
    for (const auto& kv : outstations) {
        std::cout << "disabling outstation " << kv.first << " for " << coldRestartSecs << " seconds" << std::endl;

        auto outstation = kv.second;

        outstation->ResetOutputs();
        outstation->Disable();
    }

    std::this_thread::sleep_for(std::chrono::seconds(coldRestartSecs));

    for (const auto& kv : outstations) {
        std::cout << "enabling outstation " << kv.first << std::endl;
        kv.second->Enable();
    }
}

} // namespace dnp3
} // namespace otsim
