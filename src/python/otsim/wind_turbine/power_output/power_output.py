from __future__ import annotations

import logging, signal, sys, threading, time, typing

import numpy  as np
import pandas as pd

import otsim.msgbus.envelope as envelope
import xml.etree.ElementTree as ET

from otsim.msgbus.envelope   import Envelope
from otsim.msgbus.pusher     import Pusher
from otsim.msgbus.subscriber import Subscriber

from windpowerlib import ModelChain, WindTurbine


class PowerOutput:
  def __init__(self: PowerOutput, pub: str, pull: str, el: ET.Element):
    self.name = el.get('name', default='ot-sim-wind-turbine-power-output')

    model = {
      'turbine_type': el.findtext('turbine-type', default='E-126/4200'),
      'hub_height':   float(el.findtext('hub-height', default='135')),
    }

    self.turbine = WindTurbine(**model)
    self.chain   = ModelChain(self.turbine)

    self.roughness  = float(el.findtext('roughness-length', default='0.15'))
    self.target_mw  = el.findtext('target-mw', default=None)
    self.conditions = None
    self.emer_stop  = 0 # not in emergency stop mode

    if self.target_mw:
      assert(self.turbine.nominal_power)
      self.target_mw = (self.turbine.nominal_power / 1e6) / self.target_mw

    # don't mark as initialized until output is non-zero
    self.initialized = False

    weather_data = el.find('weather-data')
    assert(weather_data)

    self.weather_data_columns = ['roughness_length']
    self.weather_data_heights = [0]
    self.weather_data_tags    = []

    for c in weather_data.findall('column'):
      self.weather_data_columns.append(c.get('name'))
      self.weather_data_heights.append(float(c.get('height')))
      self.weather_data_tags.append(c.text)

    self.cut_in_tag    = el.findtext('./tags/cut-in',         default='turbine.cut-in')
    self.cut_out_tag   = el.findtext('./tags/cut-out',        default='turbine.cut-out')
    self.output_tag    = el.findtext('./tags/output',         default='turbine.mw-output')
    self.emer_stop_tag = el.findtext('./tags/emergency-stop', default='turbine.emergency-stop')

    pub_endpoint  = el.findtext('pub-endpoint',  default=pub)
    pull_endpoint = el.findtext('pull-endpoint', default=pull)

    self.subscriber = Subscriber(pub_endpoint)
    self.pusher     = Pusher(pull_endpoint)

    self.subscriber.add_status_handler(self.handle_msgbus_status)
    self.subscriber.add_update_handler(self.handle_msgbus_update)


  def start(self: PowerOutput):
    self.subscriber.start('RUNTIME')
    threading.Thread(target=self.run, daemon=True).start()


  def stop(self: PowerOutput):
    self.subscriber.stop()


  def run(self: PowerOutput):
    # Examine turbine power curve to determine cut-in and cut-out speeds.
    cut_in  = 0.0
    cut_out = self.turbine.power_curve.tail(1).values[0][0]

    for r in self.turbine.power_curve.itertuples():
      if r.value:
        cut_in = r.wind_speed
        break

    while True:
      # Periodically publish status containing turbine power curve cut-in and
      # cut-out wind speeds for other modules to use if needed.
      points = [
        {'tag': self.cut_in_tag,  'value': cut_in,  'ts': 0},
        {'tag': self.cut_out_tag, 'value': cut_out, 'ts': 0},
      ]

      env = envelope.new_status_envelope(self.name, {'measurements': points})
      self.pusher.push('RUNTIME', env)

      time.sleep(10)


  def handle_msgbus_status(self: PowerOutput, env: Envelope):
    status = envelope.status_from_envelope(env)

    if status:
      if not self.conditions:
        self.conditions = [self.roughness] + [None] * len(self.weather_data_tags)

      for point in status.get('measurements', []):
        tag = point['tag']

        if tag not in self.weather_data_tags:
          continue

        # Add 1 to index to account for roughness length always being the first
        # entry in the conditions array.
        idx = self.weather_data_tags.index(tag) + 1

        if idx > 0:
          self.conditions[idx] = point['value']

      if not None in self.conditions:
        self.update_power_output()


  def handle_msgbus_update(self: PowerOutput, env: Envelope):
    update = envelope.update_from_envelope(env)

    if update:
      for point in update['updates']:
        if point['tag'] == self.emer_stop_tag:
          self.emer_stop = point['value']
          break

      if self.emer_stop:
        self.update_power_output()


  def update_power_output(self: PowerOutput):
    output = 0.0

    if not self.emer_stop:
      weather = pd.DataFrame(
        [self.conditions],
        index=pd.date_range('1/1/2022', periods=1, freq='H'),
        columns=[np.array(self.weather_data_columns), np.array(self.weather_data_heights)],
      )

      output = self.chain.run_model(weather).power_output.values[0]
      output = output / 1e6 # W --> MW

      if self.target_mw:
        output = output / self.target_mw

    # `output` could be zero for a while if this module starts up before the
    # anemometer module does.
    if not self.initialized and output:
      self.initialized = True

    # Prevent initial values of zero from messing up any physical system
    # simulator the device this module is part of is connected to.
    if self.initialized:
      points = [{'tag': self.output_tag, 'value': output, 'ts': 0}]

      env = envelope.new_status_envelope(self.name, {'measurements': points})
      self.pusher.push('RUNTIME', env)

      env = envelope.new_update_envelope(self.name, {'updates': points})
      self.pusher.push('RUNTIME', env)

    self.conditions = None


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

  modules: typing.List[PowerOutput] = []

  for wp in root.findall('./wind-turbine/power-output'):
    module = PowerOutput(pub, pull, wp)
    module.start()

    modules.append(module)

  waiter = threading.Event()

  def handler(*_):
    waiter.set()

  signal.signal(signal.SIGINT, handler)
  waiter.wait()

  for module in modules:
    module.stop()
