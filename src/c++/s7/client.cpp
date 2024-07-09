#include <iostream>

#include "client.hpp"

#include "fmt/format.h"


namespace otsim {
namespace s7 {

void Client::HandleMsgBusUpdate(const otsim::msgbus::Envelope<otsim::msgbus::Update>& env) {
    auto sender = otsim::msgbus::GetEnvelopeSender(env);

    //if the update sender is the same as the client, disregard the update and exit
    if (sender == id) {
        return;
    }

    for (auto &p : env.contents.updates) {
        if (WriteBinary(p.tag,p.value)) {
            continue;
        }

        WriteAnalog(p.tag, p.value);
    }
}

}
}