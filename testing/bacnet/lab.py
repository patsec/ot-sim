'''A loopback lab for the BACnet module.

The lab runs the module's server in the test's event loop, binds the two ZeroMQ
sockets of a message bus for it to connect to, and gives the test a BACpypes3
client. Nothing leaves the loopback interface.
'''

from __future__ import annotations

import asyncio, contextlib, json, socket, typing

import otsim.bacnet.bacnet as bacnet
import otsim.msgbus.envelope as envelope
import xml.etree.ElementTree as ET


TOPIC = b'RUNTIME'

# One of every object type, plus a second object on the fan's tag.
CONFIG = '''
<ot-sim>
  <message-bus>
    <pull-endpoint>tcp://127.0.0.1:{pull}</pull-endpoint>
    <pub-endpoint>tcp://127.0.0.1:{pub}</pub-endpoint>
  </message-bus>
  <bacnet name="bacnet-server" mode="server">
    <endpoint>127.0.0.1/8:{port}</endpoint>
    <device>
      <instance>3003</instance>
      <name>AHU-1</name>
    </device>
    <stale-after>{stale}</stale-after>
    <object type="analog-input">
      <instance>1</instance>
      <tag>ahu.supply_temp</tag>
      <name>AHU1.SAT</name>
      <description>Supply air temperature</description>
      <units>degreesFahrenheit</units>
      <cov-increment>0.5</cov-increment>
    </object>
    <object type="analog-output">
      <instance>1</instance>
      <tag>ahu.damper_pct</tag>
      <name>AHU1.DMPR</name>
      <units>percent</units>
      <min>0</min>
      <max>100</max>
    </object>
    <object type="analog-value">
      <instance>1</instance>
      <tag>ahu.setpoint</tag>
      <name>AHU1.SP</name>
      <units>degreesFahrenheit</units>
      <commandable>true</commandable>
      <min>50</min>
      <max>80</max>
      <override-tag>ahu.setpoint.override</override-tag>
    </object>
    <object type="analog-value">
      <instance>2</instance>
      <tag>ahu.runtime_hours</tag>
    </object>
    <object type="binary-input">
      <instance>1</instance>
      <tag>ahu.fan_status</tag>
      <name>AHU1.FAN.STS</name>
    </object>
    <object type="binary-output">
      <instance>1</instance>
      <tag>ahu.fan</tag>
      <name>AHU1.FAN.CMD</name>
    </object>
    <object type="binary-input">
      <instance>2</instance>
      <tag>ahu.fan</tag>
      <name>AHU1.FAN.FB</name>
    </object>
    <object type="binary-value">
      <instance>1</instance>
      <tag>ahu.alarm</tag>
      <name>AHU1.ALM</name>
    </object>
    <object type="multi-state-value">
      <instance>1</instance>
      <tag>ahu.mode</tag>
      <name>AHU1.MODE</name>
      <commandable>true</commandable>
      <state>occupied</state>
      <state>unoccupied</state>
      <state>standby</state>
    </object>
  </bacnet>
</ot-sim>
'''


def config_xml(pull: int = 1, pub: int = 2, port: int = 47808, stale: float = 0) -> str:
  return CONFIG.format(pull=pull, pub=pub, port=port, stale=stale)


def free_port(kind: int) -> int:
  '''A free loopback port (SOCK_STREAM for ZeroMQ, SOCK_DGRAM for BACnet).'''

  with socket.socket(socket.AF_INET, kind) as sock:
    sock.bind(('127.0.0.1', 0))
    return sock.getsockname()[1]


def status(values: typing.Dict[str, float]) -> bytes:
  '''A Status message as the I/O module pushes it.'''

  points = [{'tag': tag, 'value': float(value), 'ts': 0} for tag, value in values.items()]

  return json.dumps(envelope.new_status_envelope('io', {'measurements': points})).encode()


class Lab:
  def __init__(self: Lab, server, client, pub, pull, port: int):
    from bacpypes3.pdu import Address

    self.server  = server
    self.client  = client
    self.pub     = pub
    self.pull    = pull
    self.port    = port
    self.address = Address(f'127.0.0.1:{port}')

  async def status(self: Lab, values: typing.Dict[str, float]) -> None:
    '''Publish a Status message until the module has applied it.

    A ZeroMQ subscriber misses what is published before its connection is up.
    '''

    points = [p for tag in values for p in self.server.points.get(tag, [])]
    marks  = [p.seen for p in points]

    for _ in range(100):
      await self.pub.send_multipart((TOPIC, status(values)))
      await asyncio.sleep(0.02)

      if all(p.seen != mark for p, mark in zip(points, marks)):
        return

    raise AssertionError(f'the module never applied {values}')

  async def update(self: Lab, timeout: float = 2.0) -> typing.Dict[str, float]:
    '''The next Update message the module pushed, as a map of tags to values.'''

    while True:
      frames = await asyncio.wait_for(self.pull.recv_multipart(), timeout)

      if frames[0] == TOPIC:
        break

    env = json.loads(frames[1])

    assert env['version'] == 'v1' and env['kind'] == 'Update'
    assert env['metadata']['sender'] == self.server.name

    update = envelope.update_from_envelope(env)

    return {point['tag']: point['value'] for point in update['updates']}

  async def no_update(self: Lab, wait: float = 0.3) -> None:
    with contextlib.suppress(asyncio.TimeoutError):
      update = await self.update(wait)
      raise AssertionError(f'unexpected Update message: {update}')

  async def read(self: Lab, obj: str, prop: str = 'present-value') -> typing.Any:
    from bacpypes3.primitivedata import ObjectIdentifier

    return await self.client.read_property(self.address, ObjectIdentifier(obj), prop)

  async def write(self: Lab, obj: str, value: typing.Any, prop: str = 'present-value', priority: int = None) -> typing.Any:
    '''Write a property, returning None or the (class, code) of the error.'''

    from bacpypes3.apdu          import ErrorRejectAbortNack
    from bacpypes3.primitivedata import ObjectIdentifier

    try:
      result = await self.client.write_property(self.address, ObjectIdentifier(obj), prop, value, priority=priority)
    except ErrorRejectAbortNack as ex:
      result = ex

    if result is None:
      return None

    return (str(getattr(result, 'errorClass', '')), str(getattr(result, 'errorCode', result)))


@contextlib.asynccontextmanager
async def lab(stale: float = 0) -> typing.AsyncIterator[Lab]:
  import zmq, zmq.asyncio

  from bacpypes3.app      import Application
  from bacpypes3.argparse import SimpleArgumentParser

  pull_port = free_port(socket.SOCK_STREAM)
  pub_port  = free_port(socket.SOCK_STREAM)
  port      = free_port(socket.SOCK_DGRAM)

  root = ET.fromstring(config_xml(pull_port, pub_port, port, stale))

  # The message bus's side of the sockets: it binds, and the modules connect.
  ctx = zmq.asyncio.Context()

  pull = ctx.socket(zmq.PULL)
  pull.bind(f'tcp://127.0.0.1:{pull_port}')

  pub = ctx.socket(zmq.PUB)
  pub.bind(f'tcp://127.0.0.1:{pub_port}')

  (config,) = bacnet.load(root)

  server = bacnet.Server(config)
  server.build()
  server.connect()

  args   = ['--name', 'client', '--instance', '999', '--address', f'127.0.0.2/8:{free_port(socket.SOCK_DGRAM)}']
  client = Application.from_args(SimpleArgumentParser().parse_args(args))

  await asyncio.sleep(0.2) # the datagram endpoints bind in tasks

  try:
    yield Lab(server, client, pub, pull, port)
  finally:
    client.close()
    await server.close()
    ctx.destroy(linger=0)
