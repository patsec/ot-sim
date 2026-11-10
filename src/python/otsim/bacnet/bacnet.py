'''BACnet/IP server module for OT-sim devices.

Each configured BACnet object follows one OT-sim tag: Status messages from the
message bus set its present-value, and an accepted BACnet write to it is sent to
the message bus as an Update message. See README.md in this directory for the
configuration and the behavior in detail.
'''

from __future__ import annotations

import asyncio, contextlib, ipaddress, json, logging, math, re, signal, sys, time, typing

import otsim.msgbus.envelope as envelope
import xml.etree.ElementTree as ET

from otsim.msgbus.envelope import MetricKind


# OT-sim's exit status for "do not restart this module" (src/go/util/exit.go).
EXIT_NO_RESTART = 101

# Seconds to wait before exiting after an unexpected failure, since the CPU
# module restarts a module right away.
RESTART_PAUSE = 5.0

RUNTIME_TOPIC = b'RUNTIME'
HEALTH_TOPIC  = b'HEALTH'

DEFAULT_PUB_ENDPOINT  = 'tcp://127.0.0.1:5678'
DEFAULT_PULL_ENDPOINT = 'tcp://127.0.0.1:1234'

DEFAULT_PORT = 47808

# Object and device instance numbers are 22 bits, and 4194303 is the wildcard.
MAX_INSTANCE = 4194302

# The largest magnitude a BACnet Real (IEEE 754 single) holds. HELICS gives the
# I/O module -1e49 for a value no federate has published yet, which does not fit
# and is not a measurement.
REAL_MAX = 3.4028234663852886e38

# Seconds between pushes of this module's metrics (same as the other modules).
METRICS_PERIOD = 5.0

ANALOG  = ('analog-input', 'analog-output', 'analog-value')
BINARY  = ('binary-input', 'binary-output', 'binary-value')
MULTI   = ('multi-state-value',)
INPUTS  = ('analog-input', 'binary-input')
OUTPUTS = ('analog-output', 'binary-output')  # always commandable (ASHRAE 135, clause 19.2)

OBJECT_TYPES = ANALOG + BINARY + MULTI

DEVICE_SETTINGS = ('instance', 'name', 'description', 'vendor-name', 'model-name')
OBJECT_SETTINGS = ('instance', 'tag', 'name', 'description', 'units', 'cov-increment', 'commandable', 'min', 'max', 'state', 'override-tag')

ENDPOINT = re.compile(r'^(?P<ip>\d+\.\d+\.\d+\.\d+)/(?P<prefix>\d+)(?::(?P<port>\d+))?$')


class ConfigError(ValueError):
  '''The configuration is wrong, so restarting the module cannot help.'''


def _text(el: ET.Element, name: str, default: str = None) -> str:
  text = el.findtext(name)

  if text is None or not text.strip():
    return default

  return text.strip()


def _integer(text: str, what: str, low: int, high: int) -> int:
  try:
    value = int(text)
  except (TypeError, ValueError):
    raise ConfigError(f'{what} must be an integer from {low} to {high}') from None

  if not low <= value <= high:
    raise ConfigError(f'{what} must be an integer from {low} to {high}')

  return value


def _number(text: str, what: str) -> float:
  try:
    value = float(text)
  except (TypeError, ValueError):
    raise ConfigError(f'{what} must be a number') from None

  if not math.isfinite(value):
    raise ConfigError(f'{what} must be a number')

  return value


def _boolean(text: str, what: str) -> bool:
  if text.lower() in ('true', 'yes', '1'):
    return True

  if text.lower() in ('false', 'no', '0'):
    return False

  raise ConfigError(f'{what} must be true or false')


def _unknown(el: ET.Element, known: typing.Sequence[str], what: str) -> None:
  for child in el:
    if child.tag not in known:
      raise ConfigError(f'{what} has an unknown setting: {child.tag}')


class ObjectConfig:
  '''One BACnet object and the OT-sim tag it follows.'''

  def __init__(self: ObjectConfig, el: ET.Element, server: str):
    self.type = el.get('type')

    if self.type not in OBJECT_TYPES:
      raise ConfigError(f'object type for {server} must be one of {", ".join(OBJECT_TYPES)} (not {self.type})')

    where = f'{self.type} object for {server}'
    _unknown(el, OBJECT_SETTINGS, where)

    self.instance = _integer(_text(el, 'instance'), f'instance of {where}', 0, MAX_INSTANCE)
    where = f'{self.type},{self.instance} for {server}'

    self.tag = _text(el, 'tag')

    if self.tag is None:
      raise ConfigError(f'tag element missing from {where}')

    self.name         = _text(el, 'name', self.tag)
    self.description  = _text(el, 'description', '')
    self.units        = _text(el, 'units')
    self.override_tag = _text(el, 'override-tag')

    self.min: float = None
    self.max: float = None

    # how far an analog present-value moves before COV subscribers are notified
    self.cov_increment = 0.1

    self.states: typing.List[str] = [(s.text or '').strip() for s in el.findall('state')]

    # An output always has a priority array. A value object has one when it is
    # configured to, and an input never does.
    commandable = _text(el, 'commandable')

    if commandable is None:
      self.commandable = self.type in OUTPUTS
    else:
      self.commandable = _boolean(commandable, f'commandable setting of {where}')

    if self.type in INPUTS and self.commandable:
      raise ConfigError(f'{where} is an input object, which cannot be commandable')

    if self.type in OUTPUTS and not self.commandable:
      raise ConfigError(f'{where} is an output object, which is always commandable')

    if self.override_tag is not None and not self.commandable:
      raise ConfigError(f'{where} has an override-tag but is not commandable')

    if self.units is not None and self.type not in ANALOG:
      raise ConfigError(f'{where} has units, which only analog objects have')

    for name in ('min', 'max'):
      text = _text(el, name)

      if text is None:
        continue

      if self.type not in ANALOG:
        raise ConfigError(f'{where} has a {name}, which only analog objects have')

      setattr(self, name, _number(text, f'{name} of {where}'))

    if self.min is not None and self.max is not None and self.min > self.max:
      raise ConfigError(f'min is greater than max for {where}')

    text = _text(el, 'cov-increment')

    if text is not None:
      if self.type not in ANALOG:
        raise ConfigError(f'{where} has a cov-increment, which only analog objects have')

      self.cov_increment = _number(text, f'cov-increment of {where}')

      if self.cov_increment < 0:
        raise ConfigError(f'cov-increment of {where} must be 0 or more')

    if self.type in MULTI:
      if not self.states or not all(self.states):
        raise ConfigError(f'{where} needs one state element per state')
    elif self.states:
      raise ConfigError(f'{where} has states, which only a multi-state-value has')


class ServerConfig:
  '''One `bacnet` element of an OT-sim configuration file.'''

  def __init__(self: ServerConfig, el: ET.Element, pub: str = DEFAULT_PUB_ENDPOINT, pull: str = DEFAULT_PULL_ENDPOINT):
    self.name = el.get('name', default='bacnet')

    mode = el.get('mode', default='server')

    if mode != 'server':
      raise ConfigError(f'{self.name} has mode {mode}, but the BACnet module only supports server mode')

    self.pub_endpoint  = _text(el, 'pub-endpoint',  pub)
    self.pull_endpoint = _text(el, 'pull-endpoint', pull)

    self.endpoint = self.__endpoint(_text(el, 'endpoint'))

    device = el.find('device')

    if device is None:
      raise ConfigError(f'device element missing for {self.name}')

    _unknown(device, DEVICE_SETTINGS, f'device element for {self.name}')

    self.device_instance    = _integer(_text(device, 'instance'), f'device instance for {self.name}', 0, MAX_INSTANCE)
    self.device_name        = _text(device, 'name', self.name)
    self.device_description = _text(device, 'description', f'OT-sim BACnet server ({self.name})')
    self.vendor_name        = _text(device, 'vendor-name', 'OT-sim')
    self.model_name         = _text(device, 'model-name', 'OT-sim BACnet module')

    # Seconds without a Status message before an object reports a fault. Off by
    # default: a tag that is only ever written gets no Status messages.
    self.stale_after = _number(_text(el, 'stale-after', '0'), f'stale-after for {self.name}')

    if self.stale_after < 0:
      raise ConfigError(f'stale-after for {self.name} must be 0 or more')

    self.objects = [ObjectConfig(o, self.name) for o in el.findall('object')]

    # Object identifiers and object names are unique within a BACnet device.
    ids, names = set(), {self.device_name}

    for obj in self.objects:
      if (obj.type, obj.instance) in ids:
        raise ConfigError(f'{self.name} has more than one {obj.type} object with instance {obj.instance}')

      if obj.name in names:
        raise ConfigError(f'{self.name} has more than one object named {obj.name}')

      ids.add((obj.type, obj.instance))
      names.add(obj.name)

  def __endpoint(self: ServerConfig, text: str) -> str:
    if text is None:
      raise ConfigError(f'endpoint element missing for {self.name}')

    match = ENDPOINT.match(text)

    if match is None:
      raise ConfigError(f'endpoint for {self.name} must be ip/prefix[:port], the address of the interface with its prefix length (not {text})')

    try:
      address = ipaddress.IPv4Address(match['ip'])
    except ValueError:
      raise ConfigError(f'endpoint for {self.name} is not an IPv4 address: {text}') from None

    # BACpypes3 listens for broadcasts on the subnet's directed broadcast
    # address, which it can only work out from a real address and its prefix.
    if address.is_unspecified:
      raise ConfigError(f'endpoint for {self.name} must be the address of an interface, not 0.0.0.0')

    if not 1 <= int(match['prefix']) <= 32:
      raise ConfigError(f'endpoint for {self.name} has a prefix length out of range: {text}')

    port = int(match['port']) if match['port'] else DEFAULT_PORT

    if not 1 <= port <= 65535:
      raise ConfigError(f'endpoint for {self.name} has a port out of range: {text}')

    return f'{address}/{match["prefix"]}:{port}'


def load(root: ET.Element) -> typing.List[ServerConfig]:
  '''The BACnet servers configured in an OT-sim configuration document.'''

  mb = root.find('message-bus')

  pub  = _text(mb, 'pub-endpoint',  DEFAULT_PUB_ENDPOINT)  if mb is not None else DEFAULT_PUB_ENDPOINT
  pull = _text(mb, 'pull-endpoint', DEFAULT_PULL_ENDPOINT) if mb is not None else DEFAULT_PULL_ENDPOINT

  configs = [ServerConfig(el, pub, pull) for el in root.findall('bacnet')]

  if not configs:
    raise ConfigError('no bacnet element in the config')

  return configs


def status_points(env: typing.Any) -> typing.List[typing.Tuple[str, float]]:
  '''The (tag, value) pairs of a Status envelope, and nothing for anything else.

  A value that is not a number a BACnet Real can hold is left out.
  '''

  if not isinstance(env, dict) or 'kind' not in env:
    return []

  status = envelope.status_from_envelope(env)

  if not isinstance(status, dict):
    return []

  points = []

  for point in status.get('measurements') or []:
    try:
      tag, value = point['tag'], float(point['value'])
    except (KeyError, TypeError, ValueError):
      continue

    if math.isfinite(value) and abs(value) <= REAL_MAX:
      points.append((tag, value))

  return points


class Point:
  '''The link between one BACnet object and its OT-sim tag.

  It wraps the BACpypes3 object instead of extending it so the rules for
  values and writes can be read (and tested) apart from the BACnet stack.
  '''

  def __init__(self: Point, config: ObjectConfig, server: Server):
    self.config = config
    self.server = server

    # the BACpypes3 object, set by build_object
    self.obj: typing.Any = None

    # when the last Status message for the tag arrived (None before the first)
    self.seen: float = None

    self.fault = False

  def to_bacnet(self: Point, value: float) -> typing.Any:
    '''An OT-sim tag value as the object's present-value.'''

    if self.config.type in ANALOG:
      return float(value)

    if self.config.type in BINARY:
      return 'active' if value != 0 else 'inactive'

    # multi-state values are 1-based
    return min(max(round(value), 1), len(self.config.states))

  def to_tag(self: Point, value: typing.Any) -> float:
    '''A present-value as an OT-sim tag value.'''

    if self.config.type in BINARY:
      return 1.0 if int(value) else 0.0

    return float(value)

  def commanded(self: Point) -> bool:
    '''True while a slot of the object's priority array holds a command.'''

    if not self.config.commandable:
      return False

    return any(slot.null is None for slot in self.obj.priorityArray)

  def measured(self: Point, value: float) -> None:
    '''Apply a value for the tag from a Status message.'''

    self.seen = time.monotonic()

    if self.fault:
      self.fault = False
      self.obj.reliability = 'noFaultDetected'

    value = self.to_bacnet(value)

    if not self.config.commandable:
      self.obj.presentValue = value
    elif not self.commanded():
      # With nothing commanded, present-value is the relinquish default, so that
      # is what follows the tag. While a command holds, the tag (at best) echoes
      # the command, and the relinquish default keeps the value from before it.
      self.obj.relinquishDefault = value
      self.obj.recalculating()

  def check_stale(self: Point, stale_after: float) -> None:
    if self.fault or stale_after <= 0:
      return

    if self.seen is None or time.monotonic() - self.seen > stale_after:
      self.fault = True
      self.obj.reliability = 'communicationFailure'

  def check_write(self: Point, prop: str, value: typing.Any, priority: int) -> None:
    '''Refuse a write this object does not accept by raising a PropertyError.'''

    from bacpypes3.errors        import PropertyError
    from bacpypes3.primitivedata import Null

    if prop != 'presentValue' or not self.config.commandable:
      raise PropertyError('writeAccessDenied')

    if isinstance(value, Null):
      if priority is None: # a NULL relinquishes a priority, so it needs one
        raise PropertyError('invalidDataType')

      return

    if self.config.type in ANALOG:
      value = float(value)

      if not math.isfinite(value):
        raise PropertyError('valueOutOfRange')

      if self.config.min is not None and value < self.config.min:
        raise PropertyError('valueOutOfRange')

      if self.config.max is not None and value > self.config.max:
        raise PropertyError('valueOutOfRange')
    elif self.config.type in MULTI:
      if not 1 <= int(value) <= len(self.config.states):
        raise PropertyError('valueOutOfRange')

  def written(self: Point) -> None:
    '''Send the present-value that an accepted write left in force to the bus.'''

    updates = [{'tag': self.config.tag, 'value': self.to_tag(self.obj.presentValue), 'ts': 0}]

    if self.config.override_tag:
      updates.append({'tag': self.config.override_tag, 'value': 1.0 if self.commanded() else 0.0, 'ts': 0})

    self.server.push_update(updates)


def _property_name(obj: typing.Any, prop: typing.Any) -> str:
  '''A property reference in BACpypes3's attribute form (e.g. presentValue).'''

  if isinstance(prop, str):
    return prop

  name = getattr(prop, 'attr', None)

  if isinstance(name, str):
    return name

  return str(obj._property_identifier_class(prop).attr)


def build_object(point: Point) -> typing.Any:
  '''The BACpypes3 object for a point, which asks the point before a write.'''

  from bacpypes3.local     import analog, binary, multistate
  from bacpypes3.local.cmd import Commandable

  classes = {
    'analog-input':      analog.AnalogInputObject,
    'analog-output':     analog.AnalogOutputObject,
    'analog-value':      analog.AnalogValueObject,
    'binary-input':      binary.BinaryInputObject,
    'binary-output':     binary.BinaryOutputObject,
    'binary-value':      binary.BinaryValueObject,
    'multi-state-value': multistate.MultiStateValueObject,
  }

  config = point.config
  base   = classes[config.type]

  if config.commandable and not issubclass(base, Commandable):
    # a value object that takes commands gets a priority array too
    base = type(f'Commandable{base.__name__}', (Commandable, base), {})

  class Guarded(base):
    async def write_property(self, attr, value, index = None, priority = None):
      try:
        point.check_write(_property_name(self, attr), value, priority)
      except Exception:
        point.server.count('write_error_count')
        raise

      await super().write_property(attr, value, index, priority)

      point.server.count('write_count')
      point.written()

  Guarded.__name__ = Guarded.__qualname__ = f'Guarded{base.__name__}'

  kwargs = {
    'objectIdentifier': (config.type, config.instance),
    'objectName':       config.name,
    'description':      config.description,
    'statusFlags':      [0, 0, 0, 0],
    'eventState':       'normal',
    'outOfService':     False,
    'reliability':      'noFaultDetected',
  }

  if config.type in ANALOG:
    kwargs['units']        = config.units or 'noUnits'
    kwargs['covIncrement'] = config.cov_increment

    if config.type != 'analog-input':
      if config.min is not None:
        kwargs['minPresValue'] = config.min

      if config.max is not None:
        kwargs['maxPresValue'] = config.max
  elif config.type in MULTI:
    kwargs['numberOfStates'] = len(config.states)
    kwargs['stateText']      = list(config.states)

  if config.type in ('binary-input', 'binary-output'):
    kwargs['polarity'] = 'normal'

  # The value until the first Status message. A commandable object's
  # present-value is the result of its priority array, so it starts from the
  # relinquish default.
  kwargs['relinquishDefault' if config.commandable else 'presentValue'] = point.to_bacnet(0.0)

  try:
    obj = Guarded(**kwargs)
  except Exception as ex:
    # e.g. a units name that is not a BACnet engineering unit
    raise ConfigError(f'{config.type},{config.instance} for {point.server.name}: {ex}') from ex

  point.obj = obj

  if config.commandable:
    obj.recalculating()

  return obj


def _read_only(base: type) -> type:
  '''The Device or Network Port object class, refusing every write.'''

  class ReadOnly(base):
    async def write_property(self, attr, value, index = None, priority = None):
      from bacpypes3.errors import PropertyError

      raise PropertyError('writeAccessDenied')

  ReadOnly.__name__ = ReadOnly.__qualname__ = f'ReadOnly{base.__name__}'

  return ReadOnly


class Server:
  '''A BACnet/IP device, its objects, and its message bus sockets.'''

  def __init__(self: Server, config: ServerConfig):
    self.config = config
    self.name   = config.name

    # the BACpypes3 application, set by build
    self.app: typing.Any = None

    # map OT-sim tags --> the points of the objects that follow them
    self.points: typing.Dict[str, typing.List[Point]] = {}

    self.metrics: typing.Dict[str, typing.Dict] = {}

    self.new_metric(MetricKind.COUNTER, 'status_count',      'number of status messages processed')
    self.new_metric(MetricKind.COUNTER, 'update_count',      'number of update messages generated')
    self.new_metric(MetricKind.COUNTER, 'write_count',       'number of BACnet writes accepted')
    self.new_metric(MetricKind.COUNTER, 'write_error_count', 'number of BACnet writes refused')

    self.ctx:   typing.Any = None
    self.tasks: typing.List[asyncio.Task] = []
    self.queue: asyncio.Queue = None

  def log(self: Server, msg: str) -> None:
    print(f'[{self.name}] {msg}', flush=True)

  def new_metric(self: Server, kind: MetricKind, name: str, desc: str) -> None:
    self.metrics[name] = {'kind': kind.value, 'name': f'{self.name}_{name}', 'desc': desc, 'value': 0.0}

  def count(self: Server, name: str) -> None:
    self.metrics[name]['value'] += 1.0

  def build(self: Server) -> None:
    '''Create the BACnet application, which binds the endpoint.

    Must be called with the asyncio event loop running.
    '''

    from bacpypes3.app               import Application
    from bacpypes3.local.device      import DeviceObject
    from bacpypes3.local.networkport import NetworkPortObject

    config = self.config

    device = _read_only(DeviceObject)(
      objectIdentifier = ('device', config.device_instance),
      objectName       = config.device_name,
      description      = config.device_description,
      vendorName       = config.vendor_name,
      modelName        = config.model_name,
    )

    port = _read_only(NetworkPortObject)(
      config.endpoint,
      objectIdentifier     = ('network-port', 1),
      objectName           = 'NetworkPort-1',
      networkNumber        = 0,
      networkNumberQuality = 'unknown',
    )

    objects = []

    for obj in config.objects:
      point = Point(obj, self)

      objects.append(build_object(point))
      self.points.setdefault(obj.tag, []).append(point)

    self.app = Application.from_object_list([device, port, *objects])

  def connect(self: Server) -> None:
    '''Connect to the message bus and start this server's tasks.

    Must be called with the asyncio event loop running.
    '''

    import zmq, zmq.asyncio

    self.ctx   = zmq.asyncio.Context()
    self.queue = asyncio.Queue()

    pusher = self.ctx.socket(zmq.PUSH)
    pusher.setsockopt(zmq.LINGER, 0)
    pusher.connect(self.config.pull_endpoint)

    subscriber = self.ctx.socket(zmq.SUB)
    subscriber.setsockopt(zmq.LINGER, 0)
    subscriber.setsockopt(zmq.SUBSCRIBE, RUNTIME_TOPIC)
    subscriber.connect(self.config.pub_endpoint)

    self.tasks = [
      asyncio.create_task(self.__subscribe(subscriber)),
      asyncio.create_task(self.__push(pusher)),
      asyncio.create_task(self.__push_metrics()),
      asyncio.create_task(self.__watch()),
    ]

  async def close(self: Server) -> None:
    for task in self.tasks:
      task.cancel()

    for task in self.tasks:
      with contextlib.suppress(asyncio.CancelledError):
        await task

    self.tasks = []

    if self.ctx is not None:
      self.ctx.destroy(linger=0)
      self.ctx = None

    if self.app is not None:
      self.app.close()
      self.app = None

  def push_update(self: Server, updates: typing.List[envelope.Point]) -> None:
    '''Queue an Update message for the message bus.'''

    for point in updates:
      self.log(f'updating tag {point["tag"]} --> {point["value"]}')

    env = envelope.new_update_envelope(self.name, {'updates': updates})

    self.queue.put_nowait((RUNTIME_TOPIC, env))
    self.count('update_count')

  def handle_msgbus_status(self: Server, env: typing.Any) -> int:
    '''Apply a Status message, returning how many objects it gave a value.'''

    count = 0

    for tag, value in status_points(env):
      for point in self.points.get(tag, []):
        point.measured(value)
        count += 1

    return count

  async def __subscribe(self: Server, socket: typing.Any) -> None:
    while True:
      frames = await socket.recv_multipart()

      if len(frames) < 2 or frames[0] != RUNTIME_TOPIC:
        continue

      try:
        env = json.loads(frames[1])

        if isinstance(env, dict) and env.get('kind') == envelope.EnvelopeKind.STATUS.value:
          self.count('status_count')
          self.handle_msgbus_status(env)
      except Exception as ex: # one bad message must not stop the module
        self.log(f'[ERROR] handling message from the message bus: {ex}')

  async def __push(self: Server, socket: typing.Any) -> None:
    while True:
      topic, env = await self.queue.get()
      await socket.send_multipart((topic, json.dumps(env).encode()))

  async def __push_metrics(self: Server) -> None:
    while True:
      await asyncio.sleep(METRICS_PERIOD)

      env = envelope.new_metric_envelope(self.name, {'metrics': list(self.metrics.values())})
      self.queue.put_nowait((HEALTH_TOPIC, env))

  async def __watch(self: Server) -> None:
    '''Mark the objects whose tags have gone quiet as faulted.'''

    if self.config.stale_after <= 0:
      return

    period = max(0.1, min(5.0, self.config.stale_after / 3))

    while True:
      await asyncio.sleep(period)

      before = sum(1 for points in self.points.values() for p in points if p.fault)

      for points in self.points.values():
        for point in points:
          point.check_stale(self.config.stale_after)

      after = sum(1 for points in self.points.values() for p in points if p.fault)

      if after > before:
        self.log(f'[ERROR] {after} object(s) have had no value from the message bus for {self.config.stale_after:g}s')


async def run(configs: typing.List[ServerConfig]) -> None:
  '''Run the configured servers until SIGINT or SIGTERM.'''

  stop = asyncio.Event()
  loop = asyncio.get_running_loop()

  for sig in (signal.SIGINT, signal.SIGTERM):
    loop.add_signal_handler(sig, stop.set)

  servers = [Server(config) for config in configs]

  try:
    for server in servers:
      server.build()
      server.connect()

      config = server.config
      server.log(f'serving BACnet device {config.device_instance} ({config.device_name}) with {len(config.objects)} objects on {config.endpoint}')

    await stop.wait()
  finally:
    for server in servers:
      await server.close()


def main():
  if len(sys.argv) < 2:
    print('no config file provided')
    sys.exit(EXIT_NO_RESTART)

  try:
    root = ET.parse(sys.argv[1]).getroot()

    if root.tag != 'ot-sim':
      raise ConfigError('root element of the config is not ot-sim')

    configs = load(root)
  except (ConfigError, OSError, ET.ParseError) as ex:
    print(f'[bacnet] [ERROR] {ex}', flush=True)
    sys.exit(EXIT_NO_RESTART)

  # BACpypes3 reports the requests it could not serve through logging only.
  logging.basicConfig(stream=sys.stdout, level=logging.ERROR, format='[bacnet] [ERROR] %(name)s: %(message)s')

  try:
    asyncio.run(run(configs))
  except ConfigError as ex:
    print(f'[bacnet] [ERROR] {ex}', flush=True)
    sys.exit(EXIT_NO_RESTART)
  except Exception as ex:
    # BACpypes3 itself keeps retrying a bind that fails (an address that is not
    # on the interface yet), so this is something unexpected. Slow down the CPU
    # module's restart loop.
    print(f'[bacnet] [ERROR] {type(ex).__name__}: {ex}', flush=True)
    time.sleep(RESTART_PAUSE)
    sys.exit(1)
