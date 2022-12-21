from __future__ import annotations

import csv, logging, signal, sys, threading, time, typing

import otsim.msgbus.envelope as envelope
import xml.etree.ElementTree as ET

from otsim.msgbus.envelope import Point
from otsim.msgbus.pusher   import Pusher


class Anemometer:
  def __init__(self: Anemometer, pull: str, el: ET.Element):
    self.name = el.get('name', default='ot-sim-wind-turbine-anemometer')

    self.data_path = el.findtext('data-path')

    weather_data = el.find('weather-data')
    assert(weather_data)

    self.weather_data_columns = []
    self.weather_data_tags    = []

    for c in weather_data.findall('column'):
      self.weather_data_columns.append(c.get('name'))
      self.weather_data_tags.append(c.text)

    pull_endpoint = el.findtext('pull-endpoint', default=pull)
    self.pusher   = Pusher(pull_endpoint)


  def start(self: Anemometer):
    threading.Thread(target=self.run, daemon=True).start()


  def stop(self: Anemometer):
    pass


  def run(self: Anemometer):
    ts   = 0
    rows = None

    with open(self.data_path, newline='') as f:
        reader = csv.DictReader(f)
        rows = list(reader)

    while True:
      points: typing.List[Point] = []

      for i, c in enumerate(self.weather_data_columns):
        tag   = self.weather_data_tags[i]
        value = rows[ts][c]

        points.append({'tag': tag, 'value': float(value), 'ts': 0})

      if len(points) > 0:
        env = envelope.new_status_envelope(self.name, {'measurements': points})
        self.pusher.push('RUNTIME', env)

      ts += 1
      time.sleep(1)


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
    pull = mb.findtext('pull-endpoint')
  else:
    pull = 'tcp://127.0.0.1:1234'

  modules: typing.List[Anemometer] = []

  for wp in root.findall('./wind-turbine/anemometer'):
    module = Anemometer(pull, wp)
    module.start()

    modules.append(module)

  waiter = threading.Event()

  def handler(*_):
    waiter.set()

  signal.signal(signal.SIGINT, handler)
  waiter.wait()

  for module in modules:
    module.stop()
