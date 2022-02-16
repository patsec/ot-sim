#include "subscriber.hpp"
#include "nlohmann/json.hpp"

using json = nlohmann::json;

namespace otsim {
namespace msgbus {

Subscriber::Subscriber(const std::string& endpoint) {
  socket = zmq::socket_t(ctx, ZMQ_SUB);

  socket.connect(endpoint);
  socket.set(zmq::sockopt::linger, 0);
}

Subscriber::~Subscriber() {
  socket.close();
  ctx.close();
}

void Subscriber::Start(const std::string& topic) {
  thread = std::thread(&Subscriber::run, this, topic);
}

void Subscriber::Stop() {
  running.store(false);
  ctx.shutdown();

  if (thread.joinable()) {
    thread.join();
  }
}

void Subscriber::run(const std::string& topic) {
  socket.set(zmq::sockopt::subscribe, topic);

  running.store(true);

  while (running) {
    zmq::message_t t;
    zmq::recv_result_t ret;
    
    try {
      ret = socket.recv(t);
      if (!ret.has_value()) {
        continue;
      }
    } catch (zmq::error_t&) {
      continue;
    }

    // This shouldn't ever really happen...
    if (t.to_string() != topic) {
      continue;
    }

    zmq::message_t msg;

    try {
      ret = socket.recv(msg);
      if (!ret.has_value()) {
        continue;
      }
    } catch (zmq::error_t&) {
      continue;
    }

    std::stringstream str(msg.to_string());

    json j;
    str >> j;

    if (j["kind"] == "Status") {
      auto env = j.get<Envelope<Status>>();

      for (auto &handler : statusHandlers) {
        handler(env);
      }
    }

    if (j["kind"] == "Update") {
      auto env = j.get<Envelope<Update>>();

      for (auto &handler : updateHandlers) {
        handler(env);
      }
    }
  }
}

} // namespace msgbus
} // namespace otsim