from __future__ import annotations

import logging, signal, sys, threading, time, typing

import otsim.msgbus.envelope as envelope
import xml.etree.ElementTree as ET

from otsim.msgbus.envelope   import Envelope, Point
from otsim.msgbus.pusher     import Pusher
from otsim.msgbus.subscriber import Subscriber

from pycomm3 import BOOL, LogixDriver


class EthernetIP:
  def __init__(self: EthernetIP, pub: str, pull: str, el: ET.Element):
    # map EthernetIP tags --> ot-sim tags (alias)
    self.tags: typing.Dict[str, str] = {}
    # map ot-sim tags (alias) --> EthernetIP tags
    self.alias: typing.Dict[str, str] = {}

    self.monitoring = True

    self.name = el.get('name', default='ot-sim-ethernet-ip')

    pub_endpoint  = el.findtext('pub-endpoint', default=pub)
    pull_endpoint = el.findtext('pull-endpoint', default=pull)

    self.subscriber = Subscriber(pub_endpoint)
    self.pusher     = Pusher(pull_endpoint)

    # EthernetIP address of target CIP device
    self.outstation = el.findtext('outstation')
    # how often to poll target for tag values
    self.period = float(el.findtext('period', default=5))

    for t in el.findall('tag'):
      tag   = t.text
      alias = t.get('alias', default=tag)

      self.tags[tag]    = alias
      self.alias[alias] = tag


  def start(self: EthernetIP):
    self.subscriber.start('RUNTIME')

    target = LogixDriver(self.outstation)

    while True:
      try:
        target.open()
        break
      except Exception as e:
        print(f"unable to connect to '{self.outstation}': {e} - retrying in 2 seconds")
        time.sleep(2)

    tags = [t['tag_name'] for t in target.get_tag_list()]
    target.close()

    if not self.tags:
      for tag in tags:
        self.tags[tag]  = tag
        self.alias[tag] = tag
    else:
      for tag in self.tags:
        if tag in tags:
          print(f"confirmed tag '{tag}' found on device '{self.outstation}'")
        else:
          print(f"tag '{tag}' not found on device '{self.outstation}' - deleting")
          del(self.tags[tag])

    # run EthernetIP target monitor in a thread
    self.monitor_thread = threading.Thread(target=self.monitor, daemon=True)
    self.monitor_thread.start()


  def stop(self: EthernetIP):
    self.monitoring = False
    self.monitor_thread.join(self.period)

    self.subscriber.stop()


  def monitor(self: EthernetIP):
    if not self.tags:
      return

    with LogixDriver(self.outstation) as target:
      while self.monitoring:
        points: typing.List[Point] = []

        for tag, alias in self.tags.items():
          val = target.read(tag)

          if val.error:
            print(f"error reading tag '{tag}' from device '{self.outstation}': {val.error}")
            continue

          if isinstance(val.value, dict):
            val = val.value.get('Data', 0.0)
          elif val.type == BOOL:
            val = 1.0 if val.value else 0.0
          else:
            val = float(val.value)

          points.append({'tag': alias, 'value': val, 'ts': 0})

        env = envelope.new_status_envelope(self.name, {'measurements': points})
        self.pusher.push('RUNTIME', env)

        time.sleep(self.period)


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

  devices: typing.List[EthernetIP] = []

  for enip in root.findall('ethernet-ip'):
    device = EthernetIP(pub, pull, enip)
    device.start()

    devices.append(device)
  
  waiter = threading.Event()

  def handler(*_):
    waiter.set()

  signal.signal(signal.SIGINT, handler)
  waiter.wait()

  for device in devices:
    device.stop()