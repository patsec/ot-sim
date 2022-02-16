from __future__ import annotations

import json, signal, sys, threading, time, typing, zmq
import xml.etree.ElementTree as ET

from pydnp3 import asiodnp3, asiopal, opendnp3

import envelope, variations

from logger     import Logger
from outstation import Outstation, OutstationConfig
from point      import Point

LOG_LEVELS  = opendnp3.levels.NORMAL | opendnp3.levels.ALL_COMMS
Outstations = typing.Dict[int, Outstation]

class Server:
  def __init__(self: Server, logger: Logger, cold: int):
    self.logger = logger
    self.cold_restart_secs = cold

  def init_server(self: Server, id: str, endpoint: str) -> bool:
    try:
      host, port = endpoint.split(':')
    except ValueError:
      host = endpoint
      port = 20000

    self.manager = asiodnp3.DNP3Manager(1, asiodnp3.ConsoleLogger().Create())

    self.channel = self.manager.AddTCPServer(
      id,
      LOG_LEVELS,
      asiopal.ChannelRetry().Default(),
      host,
      int(port),
      None,
    )

    self.outstations: Outstations = {}
    return True

  def add_outstation(self: Server, id: str, local: int, remote: int, warm: int, pusher: zmq.Socket) -> Outstation:
    config = OutstationConfig(id, local, remote, self.cold_restart_secs, warm, self.handle_cold_restart)
    outstation = Outstation(config, pusher)
    self.outstations[local] = outstation

    return outstation

  def start(self: Server, sub: zmq.Socket) -> None:
    tags: typing.Dict[str, float] = {}

    t = threading.Thread(target=self.handle_publications, args=(sub, tags,), daemon=True)
    t.start()

    for outstation in self.outstations.values():
      config = outstation.init_stack_config()
      iOutstation = self.channel.AddOutstation(outstation.config.id, outstation, outstation, config)

      outstation.iOutstation = iOutstation
      outstation.enable()

      t = threading.Thread(target=outstation.run, args=(tags,), daemon=True)
      t.start()

      self.logger.log(f'started outstation {outstation.config.id}')

    self.logger.log('started server')

  def handle_cold_restart(self: Server, _: int) -> None:
    for outstation in self.outstations.values():
      outstation.reset_outputs()
      outstation.disable()

    time.sleep(self.cold_restart_secs)

    for outstation in self.outstations.values():
      outstation.enable()

  def handle_publications(self: Server, sub: zmq.Socket, tags: typing.Dict[str, float]) -> None:
    while True:
      data = sub.recv_multipart()

      # this should never happen...
      if data[0].decode() != 'RUNTIME':
        continue

      env = json.loads(data[1])
      md  = env.get('metadata', {})

      if md.get('sender', '') == 'dnp3':
        continue

      status = envelope.status_from_envelope(env)

      if not status:
        continue

      for point in status['measurements']:
        self.logger.log(f"[DNP3] setting tag {point['tag']} to value {point['value']}")
        tags[point['tag']] = point['value']