#include <condition_variable>
#include <csignal>
#include <iostream>
#include <mutex>
#include <thread>

#include <boost/foreach.hpp>
#include <boost/property_tree/ptree.hpp>
#include <boost/property_tree/xml_parser.hpp>

#include "fmt/format.h"
#include "opendnp3/gen/ServerAcceptMode.h"

#include "dnp3/client.hpp"
#include "dnp3/common.hpp"
#include "dnp3/server.hpp"
#include "msgbus/pusher.hpp"
#include "msgbus/subscriber.hpp"

namespace pt = boost::property_tree;

std::condition_variable cv;
std::mutex m;

void signalHandler(int) {
  cv.notify_one();
}

class ChannelListener : public opendnp3::IChannelListener {
public:
  static std::shared_ptr<ChannelListener> Create(std::string name, otsim::dnp3::Pusher pusher) {
    return std::make_shared<ChannelListener>(name, pusher);
  }

  ChannelListener(std::string name, otsim::dnp3::Pusher pusher) : name(name), pusher(pusher), currentState(opendnp3::ChannelState::CLOSED) {
    thread = std::thread(std::bind(&ChannelListener::Run, this));
  }

  ~ChannelListener() {};

  void OnStateChange(opendnp3::ChannelState state) {
    auto lock = std::unique_lock<std::mutex>(mu);
    currentState = state;

    publish();
  }

  void Run() {
    while (true) {
      {
        auto lock = std::unique_lock<std::mutex>(mu);
        publish();
      }

      std::this_thread::sleep_for(std::chrono::seconds(5));
    }
  }

private:
  void publish() {
    std::string tag   = fmt::format("{}.connected", name);
    bool        value = false;

    if (currentState == opendnp3::ChannelState::OPEN) {
      value = true;
    }

    std::cout << fmt::format("[{}] setting connected status to {}", name, value) << std::endl;

    otsim::msgbus::Points points;
    points.push_back(otsim::msgbus::Point{tag, value ? 1.0 : 0.0});

    otsim::msgbus::Status contents = {.measurements = points};
    auto env = otsim::msgbus::NewEnvelope(name, contents);

    pusher->Push("RUNTIME", env);
  }

  std::string         name;
  otsim::dnp3::Pusher pusher;

  opendnp3::ChannelState currentState;

  std::thread thread;
  std::mutex  mu;
};

int main(int argc, char** argv) {
  if (argc < 2) {
    std::cerr << "ERROR: missing path to XML config file" << std::endl;
    return 1;
  }

  // Keep servers in scope so their threads don't terminate immediately.
  std::vector<std::shared_ptr<otsim::dnp3::Server>> servers;

  // Keep clients in scope so their threads don't terminate immediately.
  std::vector<std::shared_ptr<otsim::dnp3::Client>> clients;

  // Keep client channel listeners in scope so their threads don't terminate immediately.
  std::vector<std::shared_ptr<ChannelListener>> listeners;

  // Keep subscribers in scope so their threads don't terminate immediately.
  std::vector<std::shared_ptr<otsim::msgbus::Subscriber>> subscribers;

  pt::ptree tree;
  pt::read_xml(argv[1], tree);

  BOOST_FOREACH(pt::ptree::value_type &v, tree) {
    if (v.first.compare("ot-sim") != 0) {
      std::cerr << "ERROR: missing root 'ot-sim' element in XML config" << std::endl;
      return 1;
    }

    std::string pubEndpoint;
    std::string pullEndpoint;

    try {
      auto msgbus = v.second.get_child("message-bus");
      pubEndpoint = msgbus.get<std::string>("pub-endpoint", "tcp://127.0.0.1:5678");
      pullEndpoint = msgbus.get<std::string>("pull-endpoint", "tcp://127.0.0.1:1234");
    } catch (pt::ptree_bad_path&) {}

    auto devices = v.second.equal_range("dnp3");
    for (auto iter = devices.first; iter != devices.second; ++iter) {
      auto device = iter->second;

      std::shared_ptr<otsim::msgbus::Pusher> pusher;
      std::shared_ptr<otsim::msgbus::Subscriber> sub;

      std::string name;
      std::string mode;

      try {
        name = device.get<std::string>("<xmlattr>.name");
      } catch (pt::ptree_bad_path&) {
        std::cerr << "ERROR: missing name for DNP3 device" << std::endl;
      }

      try {
        mode = device.get<std::string>("<xmlattr>.mode");
      } catch (pt::ptree_bad_path&) {
        std::cerr << "ERROR: missing mode for DNP3 device" << std::endl;
      }

      if (device.get_child_optional("pub-endpoint")) {
        auto endpoint = device.get<std::string>("pub-endpoint");
        sub = otsim::msgbus::Subscriber::Create(endpoint);
      } else {
        sub = otsim::msgbus::Subscriber::Create(pubEndpoint);
      }

      if (device.get_child_optional("pull-endpoint")) {
        auto endpoint = device.get<std::string>("pull-endpoint");
        pusher = otsim::msgbus::Pusher::Create(endpoint);
      } else {
        pusher = otsim::msgbus::Pusher::Create(pullEndpoint);
      }

      if (mode.compare("server") == 0) {
        std::cout << fmt::format("configuring DNP3 server {}", name) << std::endl;

        auto cold   = device.get<uint16_t>("cold-start-delay", 180);
        auto server = otsim::dnp3::Server::Create(cold);

        if (device.get_child_optional("endpoint")) {
          auto endpoint = device.get<std::string>("endpoint");

          std::string ip = endpoint.substr(0, endpoint.find(":"));
          std::uint16_t port = static_cast<std::uint16_t>(stoi(endpoint.substr(endpoint.find(":") + 1)));

          auto ip_endpoint = opendnp3::IPEndpoint(ip, port);

          auto mode = device.get<std::string>("endpoint.<xmlattr>.accept-mode", "CloseNew");
          auto acceptMode = opendnp3::ServerAcceptModeSpec::from_string(mode);

          server->Init(name, ip_endpoint, acceptMode);
        }

        if (device.get_child_optional("serial")) {
          auto serial = device.get_child("serial");

          opendnp3::SerialSettings settings;

          try {
            settings.deviceName = serial.get<std::string>("device");
          } catch (pt::ptree_bad_path&) {
            std::cerr << "ERROR: missing serial device setting for DNP3 server" << std::endl;
            return 1;
          }

          settings.baud     = serial.get<int>("baud-rate", 115200);
          settings.dataBits = serial.get<int>("data-bits", 8);
          settings.stopBits = opendnp3::StopBitsSpec().from_string(serial.get<std::string>("stop-bits", "One"));
          settings.parity   = opendnp3::ParitySpec().from_string(serial.get<std::string>("parity", "None"));

          server->Init(name, settings);
        }

        auto outstations = device.equal_range("outstation");
        for (auto iter = outstations.first; iter != outstations.second; ++iter) {
          auto outstn = iter->second;

          otsim::dnp3::OutstationConfig config = {
            .id         = outstn.get<std::string>("<xmlattr>.name", "dnp3-outstation"),
            .localAddr  = outstn.get<uint16_t>("local-address", 1024),
            .remoteAddr = outstn.get<uint16_t>("remote-address", 1),
          };

          otsim::dnp3::OutstationRestartConfig restart = { outstn.get<uint16_t>("warm-restart-delay", 30) };

          auto outstation = server->AddOutstation(config, restart, pusher);
          sub->AddHandler(std::bind(&otsim::dnp3::Outstation::HandleMsgBusStatus, outstation, std::placeholders::_1));

          auto inputs = outstn.equal_range("input");
          for (auto iter = inputs.first; iter != inputs.second; ++iter) {
            auto point = iter->second;

            std::string typ;

            try {
              typ = point.get<std::string>("<xmlattr>.type");
            } catch (pt::ptree_bad_path&) {
              std::cerr << "ERROR: missing type for DNP3 input" << std::endl;
              continue;
            }

            if (typ.compare("binary") == 0) {
              otsim::dnp3::BinaryInputPoint p;

              p.address = point.get<std::uint16_t>("address");
              p.tag     = point.get<std::string>("tag");

              auto sgvar = point.get<std::string>("sgvar", "Group1Var2");

              try {
                  p.svariation = opendnp3::StaticBinaryVariationSpec::from_string(sgvar);
              } catch(const std::invalid_argument&) {
                  std::cerr << fmt::format("ERROR: static group variation {} is invalid for binary input", sgvar) << std::endl;
                  continue;
              }

              auto egvar = point.get<std::string>("egvar", "Group2Var2");

              try {
                  p.evariation = opendnp3::EventBinaryVariationSpec::from_string(egvar);
              } catch(const std::invalid_argument&) {
                  std::cerr << fmt::format("ERROR: event group variation {} is invalid for binary input", egvar) << std::endl;
                  continue;
              }

              auto clazz = point.get<std::string>("class", "Class1");

              try {
                  p.clazz = opendnp3::PointClassSpec::from_string(clazz);
              } catch(const std::invalid_argument&) {
                  std::cerr << fmt::format("ERROR: {} is an invalid DNP3 class", clazz) << std::endl;
                  continue;
              }

              outstation->AddBinaryInput(p);
            } else if (typ.compare("analog") == 0) {
              otsim::dnp3::AnalogInputPoint p;

              p.address  = point.get<std::uint16_t>("address");
              p.tag      = point.get<std::string>("tag");
              p.deadband = point.get<double>("deadband", 0.0);

              auto sgvar = point.get<std::string>("sgvar", "Group30Var6");

              try {
                  p.svariation = opendnp3::StaticAnalogVariationSpec::from_string(sgvar);
              } catch(const std::invalid_argument&) {
                  std::cerr << fmt::format("ERROR: static group variation {} is invalid for analog input", sgvar) << std::endl;
                  continue;
              }

              auto egvar = point.get<std::string>("egvar", "Group32Var6");

              try {
                  p.evariation = opendnp3::EventAnalogVariationSpec::from_string(egvar);
              } catch(const std::invalid_argument&) {
                  std::cerr << fmt::format("ERROR: event group variation {} is invalid for analog input", egvar) << std::endl;
                  continue;
              }

              auto clazz = point.get<std::string>("class", "Class1");

              try {
                  p.clazz = opendnp3::PointClassSpec::from_string(clazz);
              } catch(const std::invalid_argument&) {
                  std::cerr << fmt::format("ERROR: {} is an invalid DNP3 class", clazz) << std::endl;
                  continue;
              }

              outstation->AddAnalogInput(p);
            } else {
              std::cerr << "ERROR: invalid type " << typ << " provided for DNP3 input" << std::endl;
              continue;
            }
          }

          auto outputs = outstn.equal_range("output");
          for (auto iter = outputs.first; iter != outputs.second; ++iter) {
            auto point = iter->second;

            std::string typ;

            try {
              typ = point.get<std::string>("<xmlattr>.type");
            } catch (pt::ptree_bad_path&) {
              std::cerr << "ERROR: missing type for DNP3 output" << std::endl;
            }

            if (typ.compare("binary") == 0) {
              otsim::dnp3::BinaryOutputPoint p;

              p.address = point.get<std::uint16_t>("address");
              p.tag     = point.get<std::string>("tag");
              p.sbo     = point.get<std::string>("sbo", "false") == "true";
              p.output  = true;

              auto sgvar = point.get<std::string>("sgvar", "Group10Var2");

              try {
                  p.svariation = opendnp3::StaticBinaryOutputStatusVariationSpec::from_string(sgvar);
              } catch(const std::invalid_argument&) {
                  std::cerr << fmt::format("ERROR: static group variation {} is invalid for binary output", sgvar) << std::endl;
                  continue;
              }

              auto egvar = point.get<std::string>("egvar", "Group11Var2");

              try {
                  p.evariation = opendnp3::EventBinaryOutputStatusVariationSpec::from_string(egvar);
              } catch(const std::invalid_argument&) {
                  std::cerr << fmt::format("ERROR: event group variation {} is invalid for binary output", egvar) << std::endl;
                  continue;
              }

              auto clazz = point.get<std::string>("class", "Class1");

              try {
                  p.clazz = opendnp3::PointClassSpec::from_string(clazz);
              } catch(const std::invalid_argument&) {
                  std::cerr << fmt::format("ERROR: {} is an invalid DNP3 class", clazz) << std::endl;
                  continue;
              }

              outstation->AddBinaryOutput(p);
            } else if (typ.compare("analog") == 0) {
              otsim::dnp3::AnalogOutputPoint p;

              p.address = point.get<std::uint16_t>("address");
              p.tag     = point.get<std::string>("tag");
              p.sbo     = point.get<std::string>("sbo", "false") == "true";
              p.output  = true;

              auto sgvar = point.get<std::string>("sgvar", "Group40Var4");

              try {
                  p.svariation = opendnp3::StaticAnalogOutputStatusVariationSpec::from_string(sgvar);
              } catch(const std::invalid_argument&) {
                  std::cerr << fmt::format("ERROR: static group variation {} is invalid for analog output", sgvar) << std::endl;
                  continue;
              }

              auto egvar = point.get<std::string>("egvar", "Group42Var6");

              try {
                  p.evariation = opendnp3::EventAnalogOutputStatusVariationSpec::from_string(egvar);
              } catch(const std::invalid_argument&) {
                  std::cerr << fmt::format("ERROR: event group variation {} is invalid for analog output", egvar) << std::endl;
                  continue;
              }

              auto clazz = point.get<std::string>("class", "Class1");

              try {
                  p.clazz = opendnp3::PointClassSpec::from_string(clazz);
              } catch(const std::invalid_argument&) {
                  std::cerr << fmt::format("ERROR: {} is an invalid DNP3 class", clazz) << std::endl;
                  continue;
              }

              outstation->AddAnalogOutput(p);
            } else {
              std::cerr << "ERROR: invalid type " << typ << " provided for DNP3 output" << std::endl;
              continue;
            }
          }
        }

        std::cout << fmt::format("starting DNP3 server {}", name) << std::endl;

        server->Start();
        servers.push_back(server);
      } else if (mode.compare("client") == 0) {
        std::cout << fmt::format("configuring DNP3 client {}", name) << std::endl;

        auto client   = otsim::dnp3::Client::Create();
        auto listener = ChannelListener::Create(name, pusher);

        if (device.get_child_optional("endpoint")) {
          auto endpoint = device.get<std::string>("endpoint");

          std::string ip = endpoint.substr(0, endpoint.find(":"));
          std::uint16_t port = static_cast<std::uint16_t>(stoi(endpoint.substr(endpoint.find(":") + 1)));

          auto ip_endpoint = opendnp3::IPEndpoint(ip, port);

          client->Init(name, ip_endpoint, listener);
        }

        if (device.get_child_optional("serial")) {
          auto serial = device.get_child("serial");

          opendnp3::SerialSettings settings;

          try {
            settings.deviceName = serial.get<std::string>("device");
          } catch (pt::ptree_bad_path&) {
            std::cerr << "ERROR: missing serial device setting for DNP3 client" << std::endl;
            return 1;
          }

          settings.baud     = serial.get<int>("baud-rate", 115200);
          settings.dataBits = serial.get<int>("data-bits", 8);
          settings.stopBits = opendnp3::StopBitsSpec().from_string(serial.get<std::string>("stop-bits", "One"));
          settings.parity   = opendnp3::ParitySpec().from_string(serial.get<std::string>("parity", "None"));

          client->Init(name, settings, listener);
        }

        auto masters = device.equal_range("master");
        for (auto iter = masters.first; iter != masters.second; ++iter) {
          auto mstr = iter->second;

          std::string id         = mstr.get<std::string>("<xmlattr>.name", "dnp3-master");
          std::uint16_t local    = mstr.get<std::uint16_t>("local-address", 1);
          std::uint16_t remote   = mstr.get<std::uint16_t>("remote-address", 1024);
          std::int64_t timeout   = mstr.get<std::int64_t>("timeout", 5);
          std::uint64_t scanRate = mstr.get<std::uint64_t>("scan-rate", 30);

          auto master = client->AddMaster(id, local, remote, timeout, pusher);
          sub->AddHandler(std::bind(&otsim::dnp3::Master::HandleMsgBusUpdate, master, std::placeholders::_1));

          auto inputs = mstr.equal_range("input");
          for (auto iter = inputs.first; iter != inputs.second; ++iter) {
            auto point = iter->second;

            std::string typ;

            try {
              typ = point.get<std::string>("<xmlattr>.type");
            } catch (pt::ptree_bad_path&) {
              std::cerr << "ERROR: missing type for DNP3 input" << std::endl;
              continue;
            }

            if (typ.compare("binary") == 0) {
              auto addr = point.get<std::uint16_t>("address");
              auto tag  = point.get<std::string>("tag");

              master->AddBinaryTag(addr, tag);
            } else if (typ.compare("analog") == 0) {
              auto addr = point.get<std::uint16_t>("address");
              auto tag  = point.get<std::string>("tag");

              master->AddAnalogTag(addr, tag);
            } else {
              std::cerr << "ERROR: invalid type " << typ << " provided for DNP3 input" << std::endl;
              continue;
            }
          }

          auto outputs = mstr.equal_range("output");
          for (auto iter = outputs.first; iter != outputs.second; ++iter) {
            auto point = iter->second;

            std::string typ;

            try {
              typ = point.get<std::string>("<xmlattr>.type");
            } catch (pt::ptree_bad_path&) {
              std::cerr << "ERROR: missing type for DNP3 output" << std::endl;
            }

            if (typ.compare("binary") == 0) {
              auto addr = point.get<std::uint16_t>("address");
              auto tag  = point.get<std::string>("tag");
              auto sbo  = point.get<std::string>("sbo", "false") == "true";

              master->AddBinaryTag(addr, tag, sbo);
            } else if (typ.compare("analog") == 0) {
              auto addr = point.get<std::uint16_t>("address");
              auto tag  = point.get<std::string>("tag");
              auto sbo  = point.get<std::string>("sbo", "false") == "true";

              master->AddAnalogTag(addr, tag, sbo);
            } else {
              std::cerr << "ERROR: invalid type " << typ << " provided for DNP3 output" << std::endl;
              continue;
            }
          }

          std::uint64_t all    = scanRate;
          std::uint64_t class0 = 0;
          std::uint64_t class1 = 0;
          std::uint64_t class2 = 0;
          std::uint64_t class3 = 0;

          if (mstr.get_child_optional("class-scan-rates")) {
            auto rates = mstr.get_child("class-scan-rates");

            all    = rates.get<std::uint64_t>("all", scanRate);
            class0 = rates.get<std::uint64_t>("class0", 0);
            class1 = rates.get<std::uint64_t>("class1", 0);
            class2 = rates.get<std::uint64_t>("class2", 0);
            class3 = rates.get<std::uint64_t>("class3", 0);
          }

          if (all != 0) {
            auto duration = opendnp3::TimeDuration::Seconds(all);
            master->AddClassScan(opendnp3::ClassField::AllClasses(), duration);
          }

          if (class0 != 0) {
            auto duration = opendnp3::TimeDuration::Seconds(class0);
            master->AddClassScan(opendnp3::ClassField(opendnp3::ClassField::CLASS_0), duration);
          }

          if (class1 != 0) {
            auto duration = opendnp3::TimeDuration::Seconds(class1);
            master->AddClassScan(opendnp3::ClassField(opendnp3::ClassField::CLASS_1), duration);
          }

          if (class2 != 0) {
            auto duration = opendnp3::TimeDuration::Seconds(class2);
            master->AddClassScan(opendnp3::ClassField(opendnp3::ClassField::CLASS_2), duration);
          }

          if (class3 != 0) {
            auto duration = opendnp3::TimeDuration::Seconds(class3);
            master->AddClassScan(opendnp3::ClassField(opendnp3::ClassField::CLASS_3), duration);
          }
        }

        std::cout << fmt::format("starting DNP3 client {}", name) << std::endl;

        client->Start();
        clients.push_back(client);

        listeners.push_back(listener);
      } else {
        std::cerr << "ERROR: invalid mode provided for DNP3 config" << std::endl;
        return 1;
      }

      sub->Start("RUNTIME");
      subscribers.push_back(sub);
    }
  }

  std::signal(SIGINT, signalHandler);

  std::unique_lock lk(m);
  cv.wait(lk);

  // This *should* cause any blocking subscribers to immediately return so
  // threads can exit.
  for (auto &sub : subscribers) {
    sub->Stop();
  }

  for (auto &client : clients) {
    client->Stop();
  }

  for (auto &server : servers) {
    server->Stop();
  }

  return 0;
}
