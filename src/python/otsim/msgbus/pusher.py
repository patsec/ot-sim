from __future__ import annotations

import json, zmq

from otsim.msgbus.envelope import Envelope

class Pusher:
  def __init__(self: Pusher, endpoint: str):
    self.ctx    = zmq.Context()
    self.socket = self.ctx.socket(zmq.PUSH)

    self.socket.connect(endpoint)
    self.socket.setsockopt(zmq.LINGER, 0)

  def push(self: Pusher, topic: str, env: Envelope) -> None:
    self.socket.send_multipart((topic.encode(), json.dumps(env).encode()))