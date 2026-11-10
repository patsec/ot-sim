'''End-to-end test of the BACnet module on device 2.

Reads a value the OpenDSS federate publishes, then opens a switch with a BACnet
command and gives the command up again, each time waiting for the federate's
own state of the switch to come back through a second object of the same tag.
'''

import asyncio, sys

from bacpypes3.app           import Application
from bacpypes3.argparse      import SimpleArgumentParser
from bacpypes3.pdu           import Address
from bacpypes3.primitivedata import Null, ObjectIdentifier


DEVICE = Address('127.0.0.1:47808')


async def read(app, obj, prop = 'present-value'):
  return await app.read_property(DEVICE, ObjectIdentifier(obj), prop)


async def wait_for(app, obj, expected, timeout = 30):
  '''Poll present-value of `obj` until it is `expected`.'''

  value = None

  for _ in range(timeout):
    value = str(await read(app, obj))

    if value == expected:
      return

    await asyncio.sleep(1)

  raise AssertionError(f'{obj} is {value} after {timeout}s (expected {expected})')


async def main():
  args = ['--name', 'e2e-client', '--instance', '999', '--address', '127.0.0.2/8:47809']
  app  = Application.from_args(SimpleArgumentParser().parse_args(args))

  try:
    devices = await app.who_is(address=DEVICE)
    assert [d.iAmDeviceIdentifier[1] for d in devices] == [2], 'no I-Am from device 2'

    kW = await read(app, 'analog-input,0')
    print(f'kW:     {kW}')
    assert 1.0 <= kW <= 5.0, 'kW value is not from the OpenDSS federate'

    await wait_for(app, 'binary-input,0', 'active')
    print('closed: 1')

    # Open the switch. The binary input shows the federate's state of it.
    await app.write_property(DEVICE, ObjectIdentifier('binary-output,0'), 'present-value', 'inactive', priority=8)
    await wait_for(app, 'binary-input,0', 'inactive')
    print('closed: 0')

    # Relinquish the command, which puts back the value from before it.
    await app.write_property(DEVICE, ObjectIdentifier('binary-output,0'), 'present-value', Null(()), priority=8)
    await wait_for(app, 'binary-input,0', 'active')
    print('closed: 1')
  finally:
    app.close()


if __name__ == '__main__':
  try:
    asyncio.run(asyncio.wait_for(main(), 120))
  except Exception as ex:
    print(f'FAILED: {type(ex).__name__}: {ex}')
    sys.exit(1)

  print('PASSED')
