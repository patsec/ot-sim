#include <future>
#include <iostream>
#include <thread>

#include "opendnp3/ConsoleLogger.h"
#include "opendnp3/DNP3Manager.h"
#include "opendnp3/master/DefaultMasterApplication.h"

#include "handler.hpp"

int main(int argc, char** argv) {
  std::shared_ptr<opendnp3::DNP3Manager> manager(new opendnp3::DNP3Manager(std::thread::hardware_concurrency(), opendnp3::ConsoleLogger::Create()));
  std::shared_ptr<TestHandler> handler(new TestHandler());

  auto channel = manager->AddTCPClient(
      "dnp3-test-client",
      opendnp3::levels::NORMAL,
      opendnp3::ChannelRetry::Default(),
      std::vector<opendnp3::IPEndpoint>{opendnp3::IPEndpoint("127.0.0.1", 20000)},
      "0.0.0.0",
      nullptr
  );

  opendnp3::MasterStackConfig config;

  config.master.disableUnsolOnStartup       = true;
  config.master.startupIntegrityClassMask   = opendnp3::ClassField(opendnp3::ClassField::None());
  config.master.unsolClassMask              = opendnp3::ClassField(opendnp3::ClassField::None());
  config.master.integrityOnEventOverflowIIN = false;

  config.link.LocalAddr  = 1;
  config.link.RemoteAddr = 1024;

  auto master = channel->AddMaster("dnp3-test-master", handler, opendnp3::DefaultMasterApplication::Create(), config);

  auto duration = opendnp3::TimeDuration::Seconds(10);
  auto scan = master->AddClassScan(opendnp3::ClassField(opendnp3::ClassField::CLASS_0), duration, handler);

  master->Enable();

  std::cout << "getting initial values" << std::endl;

  handler->Reset();
  scan->Demand();

  bool closed;
  double kW;

  while(true) {
    try {
      closed = handler->GetBinary(10);
      kW = handler->GetAnalog(0);

      break;
    } catch (const std::out_of_range&) {
      std::this_thread::sleep_for(std::chrono::seconds(1));
    }
  }

  std::cout << "closed: " << closed << std::endl;
  std::cout << "kW:     " << kW << std::endl;

  if (!closed) {
    return 1;
  }

  if (kW < 4.0 || kW > 5.0) {
    return 1;
  }

  auto code = opendnp3::OperationType::PULSE_ON;
  opendnp3::ControlRelayOutputBlock crob(code, opendnp3::TripCloseCode::TRIP);

  std::promise<bool> p;
  std::future<bool> f = p.get_future();

  auto callback = [&](const opendnp3::ICommandTaskResult& r) -> void {
    if (r.summary == opendnp3::TaskCompletion::SUCCESS) {
      p.set_value(true);
    } else {
      p.set_value(false);
    }
  };

  std::cout << "sending command" << std::endl;

  master->SelectAndOperate(opendnp3::CommandSet({ WithIndex(crob, 10) }), callback);

  if (!f.get()) {
    return 1;
  }

  std::cout << "sleeping for 10s" << std::endl;

  std::this_thread::sleep_for(std::chrono::seconds(10));

  std::cout << "getting updated values" << std::endl;

  handler->Reset();
  scan->Demand();

  while(true) {
    try {
      closed = handler->GetBinary(10);
      kW = handler->GetAnalog(0);

      break;
    } catch (const std::out_of_range&) {
      std::this_thread::sleep_for(std::chrono::seconds(1));
    }
  }

  std::cout << "closed: " << closed << std::endl;
  std::cout << "kW:     " << kW << std::endl;

  if (closed) {
    return 1;
  }

  if (kW != 0.0) {
    return 1;
  }

  return 0;
}
