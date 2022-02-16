from __future__  import annotations

import json, time, typing, zmq

from collections import namedtuple
from pydnp3      import asiodnp3, opendnp3, openpal

import envelope
from logger import Logger
from point  import Point

class MasterConfig(typing.NamedTuple):
  id: str
  local_addr: int
  remote_addr: int

class TaskCallback(opendnp3.ITaskCallback):
  def __init__(self: TaskCallback, id: str, logger: Logger):
    opendnp3.ITaskCallback.__init__(self)

    self.id = id
    self.logger = logger

    self.logger.log(f'creating new task callback {self.id}')

  def OnStart(self: TaskCallback) -> None:
    self.logger.log(f'Task callback {self.id} started.')

  def OnComplete(self: TaskCallback, c: opendnp3.TaskCompletion) -> None:
    self.logger.log(f'Task callback {self.id} completed: {c}')

  def OnDestroyed(self: TaskCallback) -> None:
    self.logger.log(f'Task callback {self.id} destroyed.')

class Master(opendnp3.ISOEHandler):
  def __init__(self: Master, config: MasterConfig, logger: Logger, pusher: zmq.Socket):
    opendnp3.ISOEHandler.__init__(self)

    self.logger = logger
    self.config = config
    self.pusher = pusher

    # will be set by the client
    self.iMaster: opendnp3.IMaster = None

    self.binary_inputs:  typing.List[Point] = []
    self.binary_outputs: typing.List[Point] = []
    self.analog_inputs:  typing.List[Point] = []
    self.analog_outputs: typing.List[Point] = []

  def init_stack_config(self: Master) -> asiodnp3.MasterStackConfig:
    stack_config = asiodnp3.MasterStackConfig()
    stack_config.master.responseTimeout = openpal.TimeDuration().Seconds(2)
    stack_config.link.LocalAddr = self.config.local_addr
    stack_config.link.RemoteAddr = self.config.remote_addr

    return stack_config

  def run(self: Master) -> None:
    # TODO: periodically query outstations
    # this will likely just be handled by class scans

    while True:
      print('hello, world!')
      time.sleep(5)

  def add_binary(self: Master, point: Point) -> bool:
    if point.output:
      self.binary_outputs.append(point)
    else:
      self.binary_inputs.append(point)

    return True

  def add_analog(self: Master, point: Point) -> bool:
    if point.output:
      self.analog_outputs.append(point)
    else:
      self.analog_inputs.append(point)

    return True

  def enable(self: Master) -> bool:
    return self.iMaster.Enable()

  def disable(self: Master) -> bool:
    return self.iMaster.Disable()

  # TODO: implement
  def add_class_scan(self: Master, field: opendnp3.ClassField, period: openpal.TimeDuration) -> None:
    self.logger.log(f'adding class scan to {self.config.id}')
    self.iMaster.AddClassScan(field, period, opendnp3.TaskConfig().With(TaskCallback('class-scan', self.logger)))

  # TODO: implement
  def restart(self: Master, typ: opendnp3.RestartType) -> int:
    return 0

  # Overridden ISOEHandler method
  def Start(self: Master) -> None:
    pass

  # Overridden ISOEHandler method
  def End(self: Master) -> None:
    pass

  # Overridden ISOEHandler method
  def Process(self: Master, info: opendnp3.HeaderInfo, values: typing.Any) -> None:
    # TODO: publish point status messages

    if isinstance(values, opendnp3.ICollectionIndexedBinary):
      pass

    if isinstance(values, opendnp3.ICollectionIndexedAnalog):
      pass

    if isinstance(values, opendnp3.ICollectionIndexedBinaryOutputStatus):
      pass

    if isinstance(values, opendnp3.ICollectionIndexedAnalogOutputStatus):
      pass