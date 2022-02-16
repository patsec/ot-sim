from __future__ import annotations

import zmq

class Logger:
  def __init__(self: Logger, name: str, pusher: zmq.Socket):
    self.name = name
    self.pusher = pusher

  def log(self: Logger, msg: str) -> None:
    self.pusher.send_multipart((b'LOG', f'[{self.name}] {msg}'.encode()))