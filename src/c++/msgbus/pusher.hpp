#ifndef OTSIM_MSGBUS_PUSHER_HPP
#define OTSIM_MSGBUS_PUSHER_HPP

#include <atomic>
#include <functional>

#include "envelope.hpp"
#include "cppzmq/zmq.hpp"
#include "nlohmann/json.hpp"

using json = nlohmann::json;

namespace otsim {
namespace msgbus {

class Pusher {
public:
  static std::shared_ptr<Pusher> Create(const std::string& endpoint) {
    return std::make_shared<Pusher>(endpoint);
  }

  Pusher(const std::string& endpoint);
  ~Pusher();

  template<typename T> // must be implemented in header file since it's templated
  void Push(const std::string& topic, const Envelope<T>& env) {
    json j = env;

    std::stringstream msg;
    msg << j;

    socket.send(zmq::message_t(topic), zmq::send_flags::sndmore);
    socket.send(zmq::message_t(msg.str()), zmq::send_flags::none);
  }

private:
  zmq::context_t ctx;
  zmq::socket_t socket;
};

} // namespace msgbus
} // namespace otsim

#endif // OTSIM_MSGBUS_PUSHER_HPP