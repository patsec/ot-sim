from __future__ import annotations

import json, threading, zmq

from otsim.msgbus.envelope import Envelope

class Pusher:
  def __init__(self: Pusher, endpoint: str):
    self.ctx    = zmq.Context()
    self.socket = self.ctx.socket(zmq.PUSH)

    self.socket.connect(endpoint)
    self.socket.setsockopt(zmq.LINGER, 0)

    # Modules push from more than one thread and ZeroMQ sockets are not thread
    # safe, so sends are serialized to keep the frames of each message together.
    self.lock = threading.Lock()

  def push(self: Pusher, topic: str, env: Envelope) -> None:
    body = json.dumps(env).encode()

    with self.lock:
      self.socket.send_multipart((topic.encode(), body))