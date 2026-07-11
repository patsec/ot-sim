#include <iostream>

#include "client.hpp"

#include "fmt/format.h"


namespace otsim {
namespace s7 {

Client::Client(std::string id, Pusher pusher) : id(id), pusher(pusher) {}

/*
void Server::Run(std::shared_ptr<TS7Client> ts7client, ){
    ts7client->
}*/

void Client::HandleMsgBusUpdate(const otsim::msgbus::Envelope<otsim::msgbus::Update>& env) {
    auto sender = otsim::msgbus::GetEnvelopeSender(env);

    //if the update sender is the same as the client, disregard the update and exit
    if (sender == id) {
        return;
    }
    
    //sort through each point in the envelope and handle them with appropriate functions for binary and analog
    for (auto &p : env.contents.updates) {
        if (WriteBinary(p.tag,p.value)) {
            continue;
        }

        WriteAnalog(p.tag, p.value);
    }
}

}
}