'''Tests for the BACnet module (src/python/otsim/bacnet).

The module's server runs against a real BACpypes3 client and real ZeroMQ
sockets, all on loopback. Run them with `python3 -m pytest testing/bacnet`.
'''

from __future__ import annotations

import asyncio, json, socket, struct, sys, typing

import otsim.bacnet.bacnet as bacnet
import otsim.msgbus.envelope as envelope
import pytest
import xml.etree.ElementTree as ET

from lab import Lab, config_xml, lab, status


DEVICE = 'device,3003'
DENIED = ('property', 'write-access-denied')
RANGE  = ('property', 'value-out-of-range')


def run(test: typing.Callable[[Lab], typing.Awaitable[None]], **settings) -> None:
  async def body() -> None:
    async with lab(**settings) as bench:
      await test(bench)

  asyncio.run(asyncio.wait_for(body(), 60))


def config(xml: str = None, **changes: str) -> bacnet.ServerConfig:
  '''The lab's config (or `xml`) with each key of `changes` replaced by its value.'''

  xml = xml or config_xml()

  for old, new in changes.items():
    assert old in xml, old
    xml = xml.replace(old, new)

  (cfg,) = bacnet.load(ET.fromstring(xml))

  return cfg


# ---------------------------------------------------------------- configuration


def test_the_config_comes_from_the_bacnet_element():
  cfg = config()

  assert cfg.name == 'bacnet-server'
  assert cfg.endpoint == '127.0.0.1/8:47808'
  assert (cfg.device_instance, cfg.device_name) == (3003, 'AHU-1')
  assert (cfg.pull_endpoint, cfg.pub_endpoint) == ('tcp://127.0.0.1:1', 'tcp://127.0.0.1:2')
  assert cfg.stale_after == 0

  objects = {(o.type, o.instance): o for o in cfg.objects}

  damper = objects[('analog-output', 1)]
  assert (damper.tag, damper.name, damper.units) == ('ahu.damper_pct', 'AHU1.DMPR', 'percent')
  assert (damper.min, damper.max) == (0, 100)

  assert objects[('multi-state-value', 1)].states == ['occupied', 'unoccupied', 'standby']
  assert objects[('analog-value', 1)].override_tag == 'ahu.setpoint.override'

  # An object is named after its tag unless it says otherwise.
  assert objects[('analog-value', 2)].name == 'ahu.runtime_hours'


def test_outputs_are_commandable_and_values_only_when_they_say_so():
  commandable = {(o.type, o.instance): o.commandable for o in config().objects}

  assert commandable == {
    ('analog-input', 1):      False,
    ('analog-output', 1):     True,
    ('analog-value', 1):      True,
    ('analog-value', 2):      False,
    ('binary-input', 1):      False,
    ('binary-output', 1):     True,
    ('binary-input', 2):      False,
    ('binary-value', 1):      False,
    ('multi-state-value', 1): True,
  }


def test_the_port_defaults_to_47808():
  assert config(**{'127.0.0.1/8:47808': '10.11.12.13/24'}).endpoint == '10.11.12.13/24:47808'


def test_the_module_has_its_own_message_bus_endpoints_when_it_sets_them():
  cfg = config(**{'<endpoint>127': '<pub-endpoint>tcp://127.0.0.1:9</pub-endpoint><endpoint>127'})

  assert (cfg.pull_endpoint, cfg.pub_endpoint) == ('tcp://127.0.0.1:1', 'tcp://127.0.0.1:9')

  cfg = config('<ot-sim><bacnet><endpoint>10.0.0.1/24</endpoint><device><instance>1</instance></device></bacnet></ot-sim>')

  assert (cfg.pull_endpoint, cfg.pub_endpoint) == (bacnet.DEFAULT_PULL_ENDPOINT, bacnet.DEFAULT_PUB_ENDPOINT)
  assert (cfg.name, cfg.device_name, cfg.objects) == ('bacnet', 'bacnet', [])


@pytest.mark.parametrize(
  ('old', 'new', 'message'),
  [
    ('127.0.0.1/8:47808', '0.0.0.0/0:47808', 'not 0.0.0.0'),
    ('127.0.0.1/8:47808', '127.0.0.1:47808', 'ip/prefix'),
    ('127.0.0.1/8:47808', '127.0.0.1/8:70000', 'port out of range'),
    ('<endpoint>127.0.0.1/8:47808</endpoint>', '', 'endpoint element missing'),
    ('mode="server"', 'mode="client"', 'only supports server mode'),
    ('<instance>3003</instance>', '<instance>4194303</instance>', 'device instance'),
    ('<instance>3003</instance>', '', 'device instance'),
    ('<name>AHU-1</name>', '<name>AHU-1</name><location>roof</location>', 'unknown setting: location'),
    ('<object type="analog-input">', '<object type="accumulator">', 'object type'),
    ('<tag>ahu.supply_temp</tag>', '', 'tag element missing'),
    ('<tag>ahu.supply_temp</tag>', '<tag>ahu.supply_temp</tag><scaling>2</scaling>', 'unknown setting: scaling'),
    ('<tag>ahu.supply_temp</tag>', '<tag>ahu.supply_temp</tag><commandable>true</commandable>', 'input object'),
    ('<tag>ahu.damper_pct</tag>', '<tag>ahu.damper_pct</tag><commandable>false</commandable>', 'always commandable'),
    ('<tag>ahu.runtime_hours</tag>', '<tag>ahu.runtime_hours</tag><override-tag>x</override-tag>', 'not commandable'),
    ('<instance>2</instance>\n      <tag>ahu.runtime_hours</tag>', '<instance>two</instance><tag>ahu.runtime_hours</tag>', 'must be an integer'),
    ('<instance>2</instance>\n      <tag>ahu.runtime_hours</tag>', '<instance>1</instance><tag>ahu.runtime_hours</tag>', 'more than one analog-value object with instance 1'),
    ('<name>AHU1.SAT</name>', '<name>AHU1.DMPR</name>', 'more than one object named AHU1.DMPR'),
    ('<name>AHU1.SAT</name>', '<name>AHU-1</name>', 'more than one object named AHU-1'),
    ('<min>50</min>', '<min>90</min>', 'min is greater than max'),
    ('<min>50</min>', '<min>low</min>', 'must be a number'),
    ('<name>AHU1.ALM</name>', '<name>AHU1.ALM</name><units>percent</units>', 'only analog objects'),
    ('<name>AHU1.ALM</name>', '<name>AHU1.ALM</name><cov-increment>1</cov-increment>', 'only analog objects'),
    ('<cov-increment>0.5</cov-increment>', '<cov-increment>-1</cov-increment>', '0 or more'),
    ('<name>AHU1.ALM</name>', '<name>AHU1.ALM</name><state>on</state>', 'only a multi-state-value'),
    ('<state>occupied</state>\n      <state>unoccupied</state>\n      <state>standby</state>', '', 'needs one state element'),
    ('<stale-after>0</stale-after>', '<stale-after>-1</stale-after>', '0 or more'),
  ],
)
def test_a_bad_config_is_refused(old: str, new: str, message: str):
  with pytest.raises(bacnet.ConfigError, match=message):
    config(**{old: new})


def test_a_config_without_a_bacnet_element_is_refused():
  with pytest.raises(bacnet.ConfigError, match='no bacnet element'):
    config('<ot-sim><cpu/></ot-sim>')


def test_units_must_be_a_bacnet_engineering_unit():
  async def test() -> None:
    server = bacnet.Server(config(**{'<units>percent</units>': '<units>furlongs</units>'}))

    with pytest.raises(bacnet.ConfigError, match='analog-output,1 for bacnet-server'):
      server.build()

  asyncio.run(test())


def test_a_bad_config_exits_without_a_restart(tmp_path, capsys, monkeypatch):
  '''The CPU module restarts a module that dies, unless it exits with 101.'''

  path = tmp_path / 'config.xml'
  path.write_text('<ot-sim><cpu/></ot-sim>')

  for argv in (['ot-sim-bacnet-module', str(path)], ['ot-sim-bacnet-module', str(tmp_path / 'missing.xml')], ['ot-sim-bacnet-module']):
    monkeypatch.setattr(sys, 'argv', argv)

    with pytest.raises(SystemExit) as ex:
      bacnet.main()

    assert ex.value.code == bacnet.EXIT_NO_RESTART

  out = capsys.readouterr().out

  assert '[bacnet] [ERROR] no bacnet element in the config' in out
  assert 'no config file provided' in out


# ---------------------------------------------------------------- message bus


def test_only_status_messages_carry_values():
  assert bacnet.status_points(json.loads(status({'a.b': 1, 'c.d': 2.5}))) == [('a.b', 1.0), ('c.d', 2.5)]

  update = envelope.new_update_envelope('x', {'updates': [{'tag': 't', 'value': 1.0, 'ts': 0}]})

  assert bacnet.status_points(update) == []
  assert bacnet.status_points('not an envelope') == []
  assert bacnet.status_points({'kind': 'Status', 'contents': {'measurements': [{'tag': 't', 'value': None}]}}) == []


def test_a_value_helics_has_not_received_yet_is_not_a_measurement():
  '''Until a federate publishes a value, HELICS hands the I/O module -1e49 for
  it, and the I/O module puts that on the message bus. A BACnet Real cannot hold
  it, so the object keeps its value instead of failing every read.'''

  async def test(bench: Lab) -> None:
    await bench.pub.send_multipart((b'RUNTIME', status({'ahu.supply_temp': -1e49})))
    await bench.status({'ahu.alarm': 0}) # this later message has arrived, so the first one has too

    (point,) = bench.server.points['ahu.supply_temp']

    assert point.seen is None
    assert await bench.read('analog-input,1') == 0.0

    await bench.status({'ahu.supply_temp': 55.5})
    assert await bench.read('analog-input,1') == 55.5

  run(test)


def test_the_module_pushes_its_metrics():
  async def test(bench: Lab) -> None:
    await bench.status({'ahu.fan': 0})
    assert await bench.write('binary-output,1', 'active') is None
    assert await bench.write('binary-input,1', 'active') == DENIED

    while True:
      frames = await asyncio.wait_for(bench.pull.recv_multipart(), 5)

      if frames[0] == b'HEALTH':
        break

    env = json.loads(frames[1])

    assert env['kind'] == 'Metric' and env['metadata']['sender'] == 'bacnet-server'

    metrics = {m['name']: m for m in env['contents']['metrics']}

    assert metrics['bacnet-server_write_count']['value'] == 1
    assert metrics['bacnet-server_write_error_count']['value'] == 1
    assert metrics['bacnet-server_update_count']['value'] == 1
    assert metrics['bacnet-server_status_count']['value'] >= 1
    assert all(m['kind'] == 'Counter' and m['desc'] for m in metrics.values())

  run(test)


# ---------------------------------------------------------------- reads


def test_present_value_follows_the_message_bus():
  async def test(bench: Lab) -> None:
    await bench.status({
      'ahu.supply_temp': 55.5,
      'ahu.fan_status':  1,
      'ahu.alarm':       1,
      'ahu.mode':        2,
      'ahu.setpoint':    68,
      'some-other.tag':  5,
    })

    assert await bench.read('analog-input,1') == 55.5
    assert str(await bench.read('binary-input,1')) == 'active'
    assert str(await bench.read('binary-value,1')) == 'active'
    assert await bench.read('multi-state-value,1') == 2
    assert await bench.read('analog-value,1') == 68.0
    assert str(await bench.read('analog-input,1', 'reliability')) == 'no-fault-detected'
    assert list(await bench.read('analog-input,1', 'status-flags')) == [0, 0, 0, 0]

    await bench.status({'ahu.supply_temp': 61.25, 'ahu.fan_status': 0})

    assert await bench.read('analog-input,1') == 61.25
    assert str(await bench.read('binary-input,1')) == 'inactive'

  run(test)


def test_every_object_of_a_tag_follows_it():
  async def test(bench: Lab) -> None:
    await bench.status({'ahu.fan': 1})

    assert str(await bench.read('binary-output,1')) == 'active'
    assert str(await bench.read('binary-input,2')) == 'active'

  run(test)


def test_the_descriptive_properties_a_client_browses():
  async def test(bench: Lab) -> None:
    from bacpypes3.primitivedata import ObjectIdentifier

    assert await bench.read(DEVICE, 'object-name') == 'AHU-1'
    assert await bench.read(DEVICE, 'vendor-name') == 'OT-sim'
    assert await bench.read('analog-input,1', 'object-name') == 'AHU1.SAT'
    assert await bench.read('analog-input,1', 'description') == 'Supply air temperature'
    assert str(await bench.read('analog-input,1', 'units')) == 'degrees-fahrenheit'
    assert str(await bench.read('analog-value,2', 'units')) == 'no-units'
    assert await bench.read('multi-state-value,1', 'number-of-states') == 3
    assert list(await bench.read('multi-state-value,1', 'state-text')) == ['occupied', 'unoccupied', 'standby']
    assert await bench.read('analog-value,1', 'max-pres-value') == 80.0

    listed = {(str(kind), instance) for kind, instance in await bench.read(DEVICE, 'object-list')}

    assert listed == {
      ('device', 3003), ('network-port', 1), ('analog-input', 1), ('analog-output', 1), ('analog-value', 1),
      ('analog-value', 2), ('binary-input', 1), ('binary-input', 2), ('binary-output', 1), ('binary-value', 1),
      ('multi-state-value', 1),
    }

    await bench.status({'ahu.supply_temp': 57.0})

    rows = await bench.client.read_property_multiple(
      bench.address,
      [ObjectIdentifier('analog-input,1'), ['present-value', 'object-name'], ObjectIdentifier('binary-output,1'), ['present-value']],
    )

    assert [(str(o[0]), str(p), v if not hasattr(v, 'attr') else str(v)) for o, p, _, v in rows] == [
      ('analog-input', 'present-value', 57.0),
      ('analog-input', 'object-name', 'AHU1.SAT'),
      ('binary-output', 'present-value', 'inactive'),
    ]

  run(test)


def test_a_cov_subscriber_is_told_when_a_value_changes():
  async def test(bench: Lab) -> None:
    from bacpypes3.primitivedata import ObjectIdentifier

    await bench.status({'ahu.supply_temp': 55.0, 'ahu.fan_status': 0})

    seen = []

    async def subscribe(obj: str) -> None:
      async with bench.client.change_of_value(bench.address, ObjectIdentifier(obj), None, True, 60) as cov:
        while True:
          prop, value = await cov.get_value()

          if str(prop) == 'present-value':
            seen.append((obj, value if isinstance(value, float) else str(value)))

    tasks = [asyncio.create_task(subscribe(obj)) for obj in ('analog-input,1', 'binary-input,1')]

    await asyncio.sleep(0.5)
    await bench.status({'ahu.supply_temp': 55.25}) # less than the object's cov-increment
    await bench.status({'ahu.supply_temp': 60.0, 'ahu.fan_status': 1})
    await asyncio.sleep(0.5)

    for task in tasks:
      task.cancel()

    await asyncio.gather(*tasks, return_exceptions=True)

    # each subscription starts with the value at the time, then gets the changes
    assert sorted(seen, key=lambda s: s[0]) == [
      ('analog-input,1', 55.0), ('analog-input,1', 60.0), ('binary-input,1', 'inactive'), ('binary-input,1', 'active'),
    ]

  run(test)


def test_nothing_is_stale_unless_the_config_asks():
  async def test(bench: Lab) -> None:
    assert str(await bench.read('analog-input,1', 'reliability')) == 'no-fault-detected'
    assert list(await bench.read('analog-input,1', 'status-flags')) == [0, 0, 0, 0]

  run(test)


def test_a_tag_that_goes_quiet_reports_a_fault():
  async def test(bench: Lab) -> None:
    await bench.status({'ahu.supply_temp': 55.5})
    assert (await bench.read('analog-input,1', 'status-flags'))[1] == 0

    await asyncio.sleep(1.0) # stale-after is 0.3 seconds

    assert str(await bench.read('analog-input,1', 'reliability')) == 'communication-failure'
    assert (await bench.read('analog-input,1', 'status-flags'))[1] == 1
    assert await bench.read('analog-input,1') == 55.5 # the last value stays

    # A tag that has never had a value is stale too.
    assert str(await bench.read('binary-value,1', 'reliability')) == 'communication-failure'

    await bench.status({'ahu.supply_temp': 56.0})
    assert (await bench.read('analog-input,1', 'status-flags'))[1] == 0

  run(test, stale=0.3)


# ---------------------------------------------------------------- writes


def test_a_command_holds_until_it_is_relinquished():
  async def test(bench: Lab) -> None:
    from bacpypes3.primitivedata import Null

    await bench.status({'ahu.fan': 0})
    assert str(await bench.read('binary-output,1')) == 'inactive'

    assert await bench.write('binary-output,1', 'active', priority=16) is None
    assert await bench.update() == {'ahu.fan': 1.0}
    assert str(await bench.read('binary-output,1')) == 'active'
    assert (await bench.read('binary-output,1', 'current-command-priority')).unsigned == 16

    # The tag has not followed the command (yet): the command still wins...
    await bench.status({'ahu.fan': 0})
    assert str(await bench.read('binary-output,1')) == 'active'
    # ... while an object of the same tag that takes no commands shows the tag.
    assert str(await bench.read('binary-input,2')) == 'inactive'

    # A higher priority takes over, and giving it up falls back to priority 16.
    assert await bench.write('binary-output,1', 'inactive', priority=8) is None
    assert await bench.update() == {'ahu.fan': 0.0}
    assert await bench.write('binary-output,1', Null(()), priority=8) is None
    assert await bench.update() == {'ahu.fan': 1.0}

    # The tag echoes the command, which does not change the relinquish default.
    await bench.status({'ahu.fan': 1})
    assert str(await bench.read('binary-output,1', 'relinquish-default')) == 'inactive'

    # Relinquishing the last command puts the relinquish default in force.
    assert await bench.write('binary-output,1', Null(()), priority=16) is None
    assert await bench.update() == {'ahu.fan': 0.0}
    assert str(await bench.read('binary-output,1')) == 'inactive'
    assert (await bench.read('binary-output,1', 'current-command-priority')).unsigned is None

    # With nothing commanded, present-value follows the tag again.
    await bench.status({'ahu.fan': 1})
    assert str(await bench.read('binary-output,1')) == 'active'
    assert str(await bench.read('binary-output,1', 'relinquish-default')) == 'active'

  run(test)


def test_an_analog_command_keeps_its_limits():
  async def test(bench: Lab) -> None:
    await bench.status({'ahu.damper_pct': 20})

    assert await bench.write('analog-output,1', 75.0, priority=8) is None
    assert await bench.update() == {'ahu.damper_pct': 75.0}
    assert await bench.read('analog-output,1') == 75.0

    assert await bench.write('analog-output,1', 140.0, priority=8) == RANGE
    assert await bench.write('analog-output,1', -1.0) == RANGE
    assert await bench.read('analog-output,1') == 75.0

    await bench.no_update()

  run(test)


def test_an_override_tag_says_whether_a_command_is_in_force():
  '''What a simulation that runs the point itself needs to know: when to follow
  the command, and when to take the point back.'''

  async def test(bench: Lab) -> None:
    from bacpypes3.primitivedata import Null

    await bench.status({'ahu.setpoint': 68})

    assert await bench.write('analog-value,1', 72.0) is None # no priority means 16
    assert await bench.update() == {'ahu.setpoint': 72.0, 'ahu.setpoint.override': 1.0}

    await bench.status({'ahu.setpoint': 68})
    assert await bench.read('analog-value,1') == 72.0

    assert await bench.write('analog-value,1', 95.0) == RANGE

    assert await bench.write('analog-value,1', Null(()), priority=16) is None
    assert await bench.update() == {'ahu.setpoint': 68.0, 'ahu.setpoint.override': 0.0}
    assert await bench.read('analog-value,1') == 68.0

  run(test)


def test_a_multi_state_write_must_name_a_state():
  async def test(bench: Lab) -> None:
    from bacpypes3.primitivedata import Null

    await bench.status({'ahu.mode': 2})

    assert await bench.write('multi-state-value,1', 3) is None
    assert await bench.update() == {'ahu.mode': 3.0}

    assert await bench.write('multi-state-value,1', 7) == RANGE
    assert await bench.write('multi-state-value,1', 0) == RANGE
    assert await bench.read('multi-state-value,1') == 3

    assert await bench.write('multi-state-value,1', Null(()), priority=16) is None
    assert await bench.update() == {'ahu.mode': 2.0}
    assert await bench.read('multi-state-value,1') == 2

  run(test)


def test_a_null_without_a_priority_is_refused():
  async def test(bench: Lab) -> None:
    from bacpypes3.apdu          import WritePropertyRequest
    from bacpypes3.apdu          import ErrorRejectAbortNack
    from bacpypes3.constructeddata import Any as BACnetAny
    from bacpypes3.primitivedata import Null, ObjectIdentifier

    request = WritePropertyRequest(
      objectIdentifier   = ObjectIdentifier('binary-output,1'),
      propertyIdentifier = 'present-value',
      propertyValue      = BACnetAny(Null(())),
      destination        = bench.address,
    )

    with pytest.raises(ErrorRejectAbortNack):
      await bench.client.request(request)

    await bench.no_update()

  run(test)


def test_everything_else_refuses_a_write():
  '''BACpypes3 on its own accepts all of these.'''

  async def test(bench: Lab) -> None:
    await bench.status({'ahu.supply_temp': 55.5, 'ahu.alarm': 1})

    assert await bench.write('analog-input,1', 5.0) == DENIED
    assert await bench.write('binary-input,1', 'active') == DENIED
    assert await bench.write('binary-value,1', 'inactive') == DENIED
    assert await bench.write('analog-value,2', 1.0) == DENIED
    assert await bench.write('analog-input,1', 'changed', prop='object-name') == DENIED
    assert await bench.write('analog-value,1', 'changed', prop='description') == DENIED
    assert await bench.write('binary-output,1', 'active', prop='relinquish-default') == DENIED
    assert await bench.write('analog-output,1', True, prop='out-of-service') == DENIED
    assert await bench.write(DEVICE, 'changed', prop='object-name') == DENIED
    assert await bench.write('network-port,1', 'changed', prop='object-name') == DENIED

    assert await bench.read('analog-input,1') == 55.5
    assert str(await bench.read('binary-value,1')) == 'active'
    assert await bench.read(DEVICE, 'object-name') == 'AHU-1'

    await bench.no_update()

  run(test)


def test_write_property_multiple_takes_the_same_path():
  async def test(bench: Lab) -> None:
    from bacpypes3.apdu            import ErrorRejectAbortNack, WriteAccessSpecification, WritePropertyMultipleRequest
    from bacpypes3.basetypes       import PropertyValue
    from bacpypes3.constructeddata import Any as BACnetAny
    from bacpypes3.primitivedata   import ObjectIdentifier, Real

    def request(obj: str, value: float) -> WritePropertyMultipleRequest:
      prop = PropertyValue(propertyIdentifier='present-value', value=BACnetAny(Real(value)), priority=8)
      spec = WriteAccessSpecification(objectIdentifier=ObjectIdentifier(obj), listOfProperties=[prop])

      return WritePropertyMultipleRequest(listOfWriteAccessSpecs=[spec], destination=bench.address)

    await bench.status({'ahu.damper_pct': 20, 'ahu.supply_temp': 55.5})

    await bench.client.request(request('analog-output,1', 60.0))
    assert await bench.update() == {'ahu.damper_pct': 60.0}

    with pytest.raises(ErrorRejectAbortNack):
      await bench.client.request(request('analog-input,1', 1.0))

    assert await bench.read('analog-input,1') == 55.5

    await bench.no_update()

  run(test)


# ---------------------------------------------------------------- discovery


def test_who_is_gets_an_i_am():
  async def test(bench: Lab) -> None:
    (i_am,) = await bench.client.who_is(address=bench.address)

    assert i_am.iAmDeviceIdentifier[1] == 3003
    assert await bench.client.who_is(3004, 3010, address=bench.address) == []

  run(test)


def test_a_who_is_forwarded_by_a_bbmd_gets_a_unicast_i_am():
  '''A Who-Is from another subnet arrives as a Forwarded-NPDU that the local
  BBMD sends to the subnet's directed broadcast address. The device has to hear
  it there (which is why the endpoint needs its prefix) and answer the sender.'''

  async def test(bench: Lab) -> None:
    loop = asyncio.get_running_loop()

    origin = socket.socket(socket.AF_INET, socket.SOCK_DGRAM) # the client on the other subnet
    origin.bind(('127.0.0.3', 0))
    origin.setblocking(False)

    bbmd = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
    bbmd.bind(('127.0.0.4', 0))
    bbmd.setsockopt(socket.SOL_SOCKET, socket.SO_BROADCAST, 1)

    try:
      who_is = bytes([0x01, 0x20, 0xFF, 0xFF, 0x00, 0xFF, 0x10, 0x08]) # global broadcast Who-Is
      source = socket.inet_aton('127.0.0.3') + struct.pack('>H', origin.getsockname()[1])
      frame  = bytes([0x81, 0x04]) + struct.pack('>H', 4 + 6 + len(who_is)) + source + who_is

      bbmd.sendto(frame, ('127.255.255.255', bench.port))

      reply, sender = await asyncio.wait_for(loop.sock_recvfrom(origin, 1500), 3)
    finally:
      origin.close()
      bbmd.close()

    assert sender == ('127.0.0.1', bench.port)
    assert reply[:2] == bytes([0x81, 0x0A]) # Original-Unicast-NPDU

    # an unconfirmed I-Am, then the device object identifier (device 3003)
    marker = reply.index(bytes([0x10, 0x00, 0xC4]))
    assert struct.unpack('>I', reply[marker + 3 : marker + 7])[0] == (8 << 22) | 3003

  run(test)
