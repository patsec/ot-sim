from __future__  import annotations

import json, time, typing, zmq

from collections import namedtuple
from pydnp3      import asiodnp3, opendnp3, openpal

import envelope
from point import Point

class OutstationConfig(typing.NamedTuple):
    id: str
    local_addr: int
    remote_addr: int
    cold_restart_delay: int
    warm_restart_delay: int
    cold_restarter: ColdRestartFunc

ColdRestartFunc = typing.Callable[[int], None]

class Outstation(opendnp3.IOutstationApplication, opendnp3.ICommandHandler):
  def __init__(self: Outstation, config: OutstationConfig, pusher: zmq.Socket):
    opendnp3.IOutstationApplication.__init__(self)
    opendnp3.ICommandHandler.__init__(self)

    self.config = config
    self.pusher = pusher

    self.cold_restart: bool = False
    self.warm_restart: bool = False

    # will be set by the server
    self.iOutstation: opendnp3.IOutstation = None

    self.binary_inputs:  typing.List[Point] = []
    self.binary_outputs: typing.List[Point] = []
    self.analog_inputs:  typing.List[Point] = []
    self.analog_outputs: typing.List[Point] = []

  def init_stack_config(self: Outstation) -> asiodnp3.OutstationStackConfig:
    sizes = opendnp3.DatabaseSizes(
      len(self.binary_inputs),
      0,
      len(self.analog_inputs),
      0,
      0,
      len(self.binary_outputs),
      len(self.analog_outputs),
      0,
    )

    stack_config = asiodnp3.OutstationStackConfig(sizes)

    stack_config.outstation.eventBufferConfig = opendnp3.EventBufferConfig().AllTypes(100)
    stack_config.outstation.params.allowUnsolicited = True
    stack_config.link.LocalAddr = self.config.local_addr
    stack_config.link.RemoteAddr = self.config.remote_addr
    stack_config.link.KeepAliveTimeout = openpal.TimeDuration().Max()

    self.init_db(stack_config.dbConfig)

    return stack_config

  def init_db(self: Outstation, db_config: asiodnp3.DatabaseConfig) -> None:
    # TODO: I don't think this is actually configuring the database entries for some reason...

    for idx, p in enumerate(self.binary_inputs):
      db_config.binary[idx].vIndex = p.address
      db_config.binary[idx].svariation = p.svariation
      db_config.binary[idx].evariation = p.evariation
      db_config.binary[idx].clazz = p.clazz

    for idx, p in enumerate(self.analog_inputs):
      db_config.analog[idx].vIndex = p.address
      db_config.analog[idx].svariation = p.svariation
      db_config.analog[idx].evariation = p.evariation
      db_config.analog[idx].clazz = p.clazz
      db_config.analog[idx].deadband = p.deadband

    for idx, p in enumerate(self.binary_outputs):
      db_config.boStatus[idx].vIndex = p.address
      db_config.boStatus[idx].svariation = p.svariation
      db_config.boStatus[idx].evariation = p.evariation
      db_config.boStatus[idx].clazz = p.clazz

    for idx, p in enumerate(self.analog_outputs):
      db_config.aoStatus[idx].vIndex = p.address
      db_config.aoStatus[idx].svariation = p.svariation
      db_config.aoStatus[idx].evariation = p.evariation
      db_config.aoStatus[idx].clazz = p.clazz
      db_config.aoStatus[idx].deadband = p.deadband

  def run(self: Outstation, tags: typing.Dict[str, float]) -> None:
    while True:
      # TODO: what happens in this loop when this outstation isn't the one that
      # initiated the cold restart? Will the disabling of the DNP3 IOutstation
      # by the server be enough?
      if self.cold_restart:
        # this will block for duration of cold restart
        self.config.cold_restarter(self.config.local_addr)
        self.cold_restart = False
        continue

      if self.warm_restart:
        self.disable()
        time.sleep(self.config.warm_restart_delay)
        self.enable()

        self.warm_restart = False
        continue

      builder = asiodnp3.UpdateBuilder()

      for idx, point in enumerate(self.binary_inputs):
        status = tags.get(point.tag, 0)
        # TODO: figure out best way to track time for points
        builder.Update(opendnp3.Binary(status != 0), idx)

      for idx, point in enumerate(self.binary_outputs):
        status = tags.get(point.tag, 0)
        # TODO: figure out best way to track time for points
        builder.Update(opendnp3.BinaryOutputStatus(status != 0), idx)

      for idx, point in enumerate(self.analog_inputs):
        value = tags.get(point.tag, 0.0)
        # TODO: figure out best way to track time for points
        builder.Update(opendnp3.Analog(value, opendnp3.Flags(0), opendnp3.DNPTime(0)), idx)

      for idx, point in enumerate(self.analog_outputs):
        value = tags.get(point.tag, 0.0)
        # TODO: figure out best way to track time for points
        builder.Update(opendnp3.AnalogOutputStatus(value, opendnp3.Flags(0), opendnp3.DNPTime(0)), idx)

      update = builder.Build()
      self.iOutstation.Apply(update)

      time.sleep(1)

  def add_binary(self: Outstation, point: Point) -> bool:
    if point.output:
      self.binary_outputs.append(point)
    else:
      self.binary_inputs.append(point)

    return True

  def add_analog(self: Outstation, point: Point) -> bool:
    if point.output:
      self.analog_outputs.append(point)
    else:
      self.analog_inputs.append(point)

    return True

  def reset_outputs(self: Outstation) -> None:
    for point in self.binary_outputs:
      update: envelope.Point = {'tag': point.tag, 'value': 0.0}
      env = envelope.new_update_envelope('dnp3', {'updates': [update]})

      self.pusher.send_multipart((b'RUNTIME', json.dumps(env).encode()))

    for point in self.analog_outputs:
      update: envelope.Point = {'tag': point.tag, 'value': 0.0}
      env = envelope.new_update_envelope('dnp3', {'updates': [update]})

      self.pusher.send_multipart((b'RUNTIME', json.dumps(env).encode()))

  def enable(self: Outstation) -> bool:
    return self.iOutstation.Enable()

  def disable(self: Outstation) -> bool:
    return self.iOutstation.Disable()

  # Overridden IOutstationApplication method
  def ColdRestartSupport(self: Outstation) -> opendnp3.RestartMode:
    return opendnp3.RestartMode.SUPPORTED_DELAY_COARSE

  # Overridden IOutstationApplication method
  def ColdRestart(self: Outstation) -> int:
    self.cold_restart = True
    return self.config.cold_restart_delay

  # Overridden IOutstationApplication method
  def WarmRestartSupport(self: Outstation) -> opendnp3.RestartMode:
    return opendnp3.RestartMode.SUPPORTED_DELAY_COARSE

  # Overridden IOutstationApplication method
  def WarmRestart(self: Outstation) -> int:
    self.warm_restart = True
    return self.config.warm_restart_delay

  # Overridden ICommandHandler method
  def Start(self: Outstation) -> None:
    pass

  # Overridden ICommandHandler method
  def End(self: Outstation) -> None:
    pass

  # Overridden ICommandHandler method
  def Select(self: Outstation, command: typing.Any, index: int) -> opendnp3.CommandStatus:
    if isinstance(command, opendnp3.ControlRelayOutputBlock):
      point = next(filter(lambda p: p.address == index, self.binary_outputs), None)

      if not point:
        return opendnp3.CommandStatus.OUT_OF_RANGE

      return opendnp3.CommandStatus.SUCCESS

    if isinstance(command, opendnp3.AnalogOutputFloat32):
      point = next(filter(lambda p: p.address == index, self.analog_outputs), None)

      if not point:
        return opendnp3.CommandStatus.OUT_OF_RANGE

      return opendnp3.CommandStatus.SUCCESS

    return opendnp3.CommandStatus.NOT_SUPPORTED

  # Overridden ICommandHandler method
  def Operate(self: Outstation, command: typing.Any, index: int, op_type: opendnp3.OperateType) -> opendnp3.CommandStatus:
    if isinstance(command, opendnp3.ControlRelayOutputBlock):
      point = next(filter(lambda p: p.address == index, self.binary_outputs), None)

      if not point:
        return opendnp3.CommandStatus.OUT_OF_RANGE

      if point.sbo and op_type is not opendnp3.OperateType.SELECT_BEFORE_OPERATE:
        return opendnp3.CommandStatus.NO_SELECT

      status: float = 1.0 if command.functionCode == opendnp3.ControlCode.LATCH_ON else 0.0

      update: envelope.Point = {'tag': point.tag, 'value': status}
      env = envelope.new_update_envelope('dnp3', {'updates': [update]})

      self.pusher.send_multipart((b'RUNTIME', json.dumps(env).encode()))

      return opendnp3.CommandStatus.SUCCESS

    if isinstance(command, opendnp3.AnalogOutputFloat32):
      point = next(filter(lambda p: p.address == index, self.binary_outputs), None)

      if not point:
        return opendnp3.CommandStatus.OUT_OF_RANGE

      if point.sbo and op_type is not opendnp3.OperateType.SELECT_BEFORE_OPERATE:
        return opendnp3.CommandStatus.NO_SELECT

      value: float = command.value

      update: envelope.Point = {'tag': point.tag, 'value': value}
      env = envelope.new_update_envelope('dnp3', {'updates': [update]})

      self.pusher.send_multipart((b'RUNTIME', json.dumps(env).encode()))

      return opendnp3.CommandStatus.SUCCESS

    return opendnp3.CommandStatus.NOT_SUPPORTED
