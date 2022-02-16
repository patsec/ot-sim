from __future__ import annotations

import json, signal, sys, threading, time, typing, zmq
import xml.etree.ElementTree as ET

from pydnp3 import asiodnp3, asiopal, opendnp3, openpal

import variations

from logger import Logger
from master import Master, MasterConfig
from point  import Point

LOG_LEVELS = opendnp3.levels.NORMAL | opendnp3.levels.ALL_COMMS
Masters    = typing.Dict[int, Master]

class Client:
  def __init__(self: Client, logger: Logger):
    self.logger = logger

  def init_client(self: Client, id: str, endpoint: str) -> bool:
    try:
      host, port = endpoint.split(':')
    except ValueError:
      host = endpoint
      port = 20000

    self.manager = asiodnp3.DNP3Manager(1, asiodnp3.ConsoleLogger().Create())

    self.channel = self.manager.AddTCPClient(
      id,
      LOG_LEVELS,
      asiopal.ChannelRetry().Default(),
      host,
      '0.0.0.0',
      int(port),
      None,
    )

    self.masters: Masters = {}
    return True

  def add_master(self: Client, id: str, local: int, remote: int, pusher: zmq.Socket) -> Master:
    config = MasterConfig(id, local, remote)
    master = Master(config, self.logger, pusher)
    self.masters[local] = master

    return master

  def start(self: Client, sub: zmq.Socket) -> None:
    for master in self.masters.values():
      config = master.init_stack_config()
      iMaster = self.channel.AddMaster(master.config.id, master, asiodnp3.DefaultMasterApplication().Create(), config)

      master.iMaster = iMaster

      master.add_class_scan(opendnp3.ClassField().AllClasses(), openpal.TimeDuration().Seconds(10))
      master.enable()

      self.logger.log(f'started master {master.config.id}')

    self.logger.log('started client')