#include "pusher.hpp"

namespace otsim {
namespace msgbus {

Pusher::Pusher(const std::string& endpoint) {
  socket = zmq::socket_t(ctx, ZMQ_PUSH);

  socket.connect(endpoint);
  socket.set(zmq::sockopt::linger, 0);
}

Pusher::~Pusher() {
  socket.close();
  ctx.close();
}

} // namespace msgbus
} // namespace otsim