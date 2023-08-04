from __future__ import annotations

import json, logging, signal, sys, threading, typing

import otsim.msgbus.envelope as envelope
import xml.etree.ElementTree as ET

from otsim.msgbus.envelope   import Envelope
from otsim.msgbus.subscriber import Subscriber

import RPi.GPIO as GPIO


class RPiGPIO:
  def __init__(self: RPiGPIO, pub: str, pull: str, el: ET.Element):
    # map tags --> pin number
    self.outputs: typing.Dict[str, int] = {}

    self.name = el.get('name', default='ot-sim-rpi-gpio')
    mode      = el.get('mode', default='BOARD')

    if mode.upper() == 'BOARD':
      GPIO.setmode(GPIO.BOARD)
    elif mode.upper() == 'BCM':
      GPIO.setmode(GPIO.BCM)
    else:
      print(f"unknown GPIO mode '{mode}' - defaulting to 'BOARD' mode")
      GPIO.setmode(GPIO.BOARD)

    pub_endpoint    = el.findtext('pub-endpoint', default=pub)
    self.subscriber = Subscriber(pub_endpoint)

    for o in el.findall('output'):
      pin = int(o.get('pin'))
      tag = o.findtext('tag')

      self.outputs[tag] = pin
      GPIO.setup(pin, GPIO.OUT)

    self.subscriber.add_update_handler(self.handle_msgbus_update)


  def start(self: RPiGPIO):
    self.subscriber.start('RUNTIME')


  def stop(self: RPiGPIO):
    GPIO.cleanup()
    self.subscriber.stop()


  def handle_msgbus_update(self: RPiGPIO, env: Envelope):
    update = envelope.update_from_envelope(env)

    if update:
      for point in update['updates']:
        tag = point['tag']

        if tag in self.outputs:
          GPIO.output(self.outputs[tag], point['value'])


def main():
  logging.basicConfig(level=logging.ERROR)

  if len(sys.argv) < 2:
    print('no config file provided')
    sys.exit(1)

  tree = ET.parse(sys.argv[1])

  root = tree.getroot()
  assert root.tag == 'ot-sim'

  mb = root.find('message-bus')

  if mb:
    pub  = mb.findtext('pub-endpoint')
    pull = mb.findtext('pull-endpoint')
  else:
    pub  = 'tcp://127.0.0.1:5678'
    pull = 'tcp://127.0.0.1:1234'

  devices: typing.List[RPiGPIO] = []

  for gpio in root.findall('rpi-gpio'):
    device = RPiGPIO(pub, pull, gpio)
    device.start()

    devices.append(device)
  
  waiter = threading.Event()

  def handler(*_):
    waiter.set()

  signal.signal(signal.SIGINT, handler)
  waiter.wait()

  for device in devices:
    device.stop()
