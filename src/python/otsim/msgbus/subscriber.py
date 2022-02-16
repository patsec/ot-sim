from __future__ import annotations

import json, threading, typing, zmq

from otsim.msgbus.envelope import Envelope

status_handler = typing.Callable[[Envelope], None]
update_handler = typing.Callable[[Envelope], None]

class Subscriber:
  def __init__(self: Subscriber, endpoint: str):
    self.status_handlers: typing.List[status_handler] = []
    self.update_handlers: typing.List[update_handler] = []

    self.running = False

    self.ctx    = zmq.Context()
    self.socket = self.ctx.socket(zmq.SUB)

    self.socket.connect(endpoint)
    self.socket.setsockopt(zmq.LINGER, 0)

  def add_status_handler(self: Subscriber, handler: status_handler) -> None:
    self.status_handlers.append(handler)

  def add_update_handler(self: Subscriber, handler: update_handler) -> None:
    self.update_handlers.append(handler)

  def start(self: Subscriber, topic: str) -> None:
    self.running = True

    self.thread = threading.Thread(target=self.__run, args=(topic,))
    self.thread.start()

  def stop(self: Subscriber) -> None:
    self.running = False

    self.socket.close()
    self.ctx.term()

    self.thread.join()

  def __run(self: Subscriber, topic: str) -> None:
    self.socket.setsockopt(zmq.SUBSCRIBE, topic.encode())

    while self.running:
      data = self.socket.recv_multipart()

      # this should never happen...
      if data[0].decode() != topic:
        continue

      env = json.loads(data[1])

      if env['kind'] == 'Status':
        for handler in self.status_handlers:
          handler(env)
      elif env['kind'] == 'Update':
        for handler in self.update_handlers:
          handler(env)