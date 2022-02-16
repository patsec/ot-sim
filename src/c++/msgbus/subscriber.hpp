#ifndef OTSIM_MSGBUS_SUBSCRIBER_HPP
#define OTSIM_MSGBUS_SUBSCRIBER_HPP

#include <atomic>
#include <functional>
#include <thread>

#include "envelope.hpp"
#include "cppzmq/zmq.hpp"

namespace otsim {
namespace msgbus {

typedef std::function<void (const Envelope<Status>&)> StatusHandler;
typedef std::function<void (const Envelope<Update>&)> UpdateHandler;

class Subscriber {
public:
  static std::shared_ptr<Subscriber> Create(const std::string& endpoint) {
    return std::make_shared<Subscriber>(endpoint);
  }

  Subscriber(const std::string& endpoint);
  ~Subscriber();

  void AddHandler(StatusHandler handler) {
    statusHandlers.push_back(handler);
  }

  void AddHandler(UpdateHandler handler) {
    updateHandlers.push_back(handler);
  }

  void Start(const std::string& topic);
  void Stop();

private:
  void run(const std::string& topic);

  zmq::context_t ctx;
  zmq::socket_t socket;

  std::atomic<bool> running;
  std::thread thread;

  std::vector<StatusHandler> statusHandlers;
  std::vector<UpdateHandler> updateHandlers;
};

} // namespace msgbus
} // namespace otsim

#endif // OTSIM_MSGBUS_SUBSCRIBER_HPP