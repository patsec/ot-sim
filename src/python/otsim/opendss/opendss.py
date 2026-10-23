from __future__ import annotations

import json, signal, sys, threading, time, typing

import xml.etree.ElementTree as ET
import opendssdirect         as dss

from collections import defaultdict

from otsim.helics_helper   import DataType, Endpoint, HelicsFederate, Publication, Subscription
from otsim.msgbus.metrics  import MetricsPusher
from otsim.msgbus.pusher   import Pusher


class OpenDSS(HelicsFederate):
  def get_tag(key: str, tag: str) -> str:
    if tag: return tag

    tag = key.split('/')

    if len(tag) > 1:
      return tag[1]
    else:
      return tag[0]


  def get_type(typ: str) -> DataType:
    if typ == 'boolean':
      return DataType.boolean
    elif typ == 'double':
      return DataType.double
    elif typ == 'complex':
      return DataType.complex
    else:
      return None


  def __init__(self: OpenDSS, pull: str, el: ET.Element):
    self.name = el.get('name', default='ot-sim-opendss')

    broker    = el.findtext('broker-endpoint',    default='127.0.0.1')
    log_level = el.findtext('federate-log-level', default='SUMMARY')

    HelicsFederate.federate_info_core_init_string = f'--federates=1 --broker={broker} --loglevel={log_level}'
    HelicsFederate.federate_info_log_level        = log_level

    name = el.findtext('federate-name', default=self.name)

    HelicsFederate.federate_name = name

    start = el.findtext('start-time', default=1)
    end   = el.findtext('end-time',   default=3600)

    HelicsFederate.start_time = int(start)
    HelicsFederate.end_time   = int(end)

    real_time = el.findtext('real-time', default='true')

    HelicsFederate.federate_info_real_time = real_time.lower() in ['true', 'yes', '1']

    for sub in el.findall('subscription'):
      key = sub.findtext('key')
      typ = OpenDSS.get_type(sub.findtext('type'))
      # tag = OpenDSS.get_tag(key, s.findtext('tag'))

      HelicsFederate.subscriptions.append(Subscription(key, typ))

    for pub in el.findall('publication'):
      key = pub.findtext('key')
      typ = OpenDSS.get_type(pub.findtext('type'))
      # tag = OpenDSS.get_tag(key, p.findtext('tag'))

      HelicsFederate.publications.append(Publication(key, typ))

    ep = el.find('endpoint')

    if ep:
      HelicsFederate.endpoints = [Endpoint(ep.get('name', default='updates'))]
    else:
      HelicsFederate.endpoints = [Endpoint("updates")]

    HelicsFederate.__init__(self, module_name=self.name)

    case = el.find('case')

    case_file = case.findtext('file')
    dss.run_command(f'Redirect {case_file}')

    mode = case.findtext('mode', default='daily')
    dss.run_command(f'set mode={mode}') # load profile mode

    # initial solve
    dss.Solution.StepSize(0)
    dss.Solution.Solve()

    step_size = case.findtext('step-size', default=60)
    dss.Solution.StepSize(int(step_size)) # load profile interval

    pull_endpoint = el.findtext('pull-endpoint', default=pull)

    self.pusher  = Pusher(pull_endpoint)
    self.metrics = MetricsPusher()

    # TODO: track some metrics
    # self.metrics.new_metric(MetricKind.COUNTER, 'status_count',            'number of OT-sim status messages generated')
    # self.metrics.new_metric(MetricKind.COUNTER, 'update_count',            'number of OT-sim update messages processed')
    # self.metrics.new_metric(MetricKind.COUNTER, 'helics_sub_count',        'number of HELICS subscriptions processed')
    # self.metrics.new_metric(MetricKind.COUNTER, 'helics_pub_update_count', 'number of HELICS publication-based updates generated')


  def start(self: OpenDSS):
    # this already runs in a thread
    self.metrics.start(self.pusher, self.name)

    # run HELICS helper in a thread
    threading.Thread(target=self.run, daemon=True).start()


  def stop(self: OpenDSS):
    self.metrics.stop()


  def action_post_request_time(self):
    time.sleep(1)
    dss.Solution.Solve()


  def action_subscriptions(self: OpenDSS, data: typing.Dict, ts: int):
    pass


  def action_publications(self: OpenDSS, data: typing.Dict, ts: int):
    for topic in data.keys():
      # topic = line-650632.kW
      # kind  = line
      # name  = 650632
      # field = kW

      name, field = topic.split('.', 1)
      kind, name  = name.split('-', 1)

      if kind == 'bus':
        dss.Circuit.SetActiveBus(name)

        if field == 'voltage':
          data[topic] = dss.Bus.puVmagAngle()[0]
          continue
        if field == 'kV':
          data[topic] = complex(dss.Bus.Voltages()[0], dss.Bus.Voltages()[1])
          continue
      elif kind == 'load':
        dss.Circuit.SetActiveClass(b'load')
        dss.Circuit.SetActiveElement(name)

        phases   = dss.CktElement.NumPhases()
        powers   = dss.CktElement.Powers()
        currents = dss.CktElement.CurrentsMagAng()

        # remove any trailing zeros
        powers   = powers[:2*phases]
        currents = currents[:2*phases]

        real_or_mag = tuple(currents[0:2*phases:2])

        active   = 0.0
        reactive = 0.0
        current  = 0.0

        if phases == 1:
          active   = powers[0]
          reactive = powers[1]
          current  = real_or_mag[0]
        elif phases in [2, 3]:
          p = tuple(powers[0:2*phases:2])
          q = tuple(powers[1:2*phases+1:2])

          active   = sum(p)
          reactive = sum(q)
          current  = sum(real_or_mag)

        if field == 'active_power':
          data[topic] = active
          continue
        if field == 'reactive_power':
          data[topic] = reactive
          continue
        if field == 'current':
          data[topic] = current
          continue

        bus = dss.CktElement.BusNames()[0]
        dss.Circuit.SetActiveBus(bus)

        nodes = dss.Bus.NumNodes()
        pu    = dss.Bus.puVmagAngle()

        real_or_mag = tuple(pu[0:2*nodes:2])

        voltage = 0.0

        if nodes == 1 or phases == 1:
          voltage = real_or_mag[0]
        else:
          voltage = sum(real_or_mag) / len(real_or_mag)

        if field == 'voltage':
          data[topic] = voltage
          continue

      elif kind == 'line':
        dss.Circuit.SetActiveClass(b'line')
        dss.Circuit.SetActiveElement(name)

        powers = dss.CktElement.Powers()
        S = sum([complex(x,y) for x, y in zip(powers[0:6], powers[1:6])])

        if field == 'kW':
          data[topic] = S.real / 1000.0
          continue
        if field == 'kVAR':
          data[topic] = S.imag / 1000.0
          continue
        if field == 'kVA':
          data[topic] = S / 1000.0
          continue
        if field == 'closed':
          data[topic] = not dss.CktElement.IsOpen(0, 0)
          continue

      elif kind == 'switch':
        dss.Circuit.SetActiveClass('line')
        dss.Circuit.SetActiveElement(name)

        if field == 'closed':
          data[topic] = not dss.CktElement.IsOpen(0, 0)
          continue
      elif kind == 'regulator':
        dss.Circuit.SetActiveClass('regcontrol')
        dss.Circuit.SetActiveElement(name)

        if field == 'vreg':
          data[topic] = dss.RegControls.ForwardVreg()


  def action_endpoints_send(self: OpenDSS, endpoints: typing.Dict, ts: int):
    pass


  def action_endpoints_recv(self: OpenDSS, endpoints: typing.Dict, ts: int):
    for msg in endpoints.get('updates', []):
      updates = json.loads(msg.data)

      for update in updates:
        topic = update.get('tag',   None)
        value = update.get('value', None)

        assert topic is not None
        assert value is not None

        name, field = topic.split('.', 1)
        kind, name  = name.split('-', 1)

        if kind == 'switch':
          dss.Circuit.SetActiveClass("line")
          dss.Circuit.SetActiveElement(name)

          if field == 'closed':
            if value is True or value != 0:
              dss.CktElement.Close(0, 0)
            elif value is False or value == 0:
              dss.CktElement.Open(0, 0)
        elif kind == 'line':
          dss.Circuit.SetActiveClass("line")
          dss.Circuit.SetActiveElement(name)

          if field == 'closed':
            if value is True or value != 0:
              dss.CktElement.Close(0, 0)
            elif value is False or value == 0:
              dss.CktElement.Open(0, 0)
        elif kind == 'regulator':
          dss.Circuit.SetActiveClass('regcontrol')
          dss.Circuit.SetActiveElement(name)

          if field == 'vreg':
            dss.RegControls.ForwardVreg(value)
        elif kind == 'bus':
          dss.Circuit.SetActiveClass("Vsource")
          dss.Circuit.SetActiveElement("source")

          dss.Vsources.BasekV(value[0] * 115)


def main():
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

  devices: typing.List[OpenDSS] = []

  for opendss in root.findall('opendss'):
    device = OpenDSS(pull, opendss)
    device.start()

    devices.append(device)
  
  waiter = threading.Event()

  def handler(*_):
    waiter.set()

  signal.signal(signal.SIGINT, handler)
  waiter.wait()

  for device in devices:
    device.stop()
