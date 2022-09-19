from __future__ import annotations

import json, logging, signal, sys, threading, typing

import otsim.msgbus.envelope as envelope
import xml.etree.ElementTree as ET

from collections import defaultdict

from otsim.helics_helper     import DataType, Endpoint, HelicsFederate, Message, Publication, Subscription
from otsim.msgbus.envelope   import Envelope, MetricKind, Point
from otsim.msgbus.metrics    import MetricsPusher
from otsim.msgbus.pusher     import Pusher
from otsim.msgbus.subscriber import Subscriber


class IO(HelicsFederate):
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
    else:
      return None


  def __init__(self: IO, pub: str, pull: str, el: ET.Element):
    # mutex to protect access to updated values dictionary
    self.mutex = threading.Lock()
    # map updated tags --> values
    self.updated: typing.Dict[str, float] = {}

    # map HELICS topics --> ot-sim tags
    self.tags: typing.Dict[str, str] = {}
    # map ot-sim tags -- HELICS topics
    self.keys: typing.Dict[str, str] = {}
    # map HELICS topics --> HELICS pub
    self.pubs: typing.Dict[str, Publication] = {}
    # map HELICS topics --> HELICS sub
    self.subs: typing.Dict[str, Subscription] = {}
    # map ot-sim tags --> HELICS topic and endpoint
    self.eps: typing.Dict[str, typing.Dict] = {}

    self.name = el.get('name', default='ot-sim-io')

    broker    = el.findtext('broker-endpoint',    default='127.0.0.1')
    name      = el.findtext('federate-name',      default=self.name)
    log_level = el.findtext('federate-log-level', default='SUMMARY')

    HelicsFederate.federate_info_core_init_string = f'--federates=1 --broker={broker} --loglevel={log_level}'
    HelicsFederate.federate_name = name

    start = el.findtext('start-time', default=1)
    end   = el.findtext('end-time',   default=3600)

    HelicsFederate.start_time = int(start)
    HelicsFederate.end_time   = int(end)

    for s in el.findall('subscription'):
      key = s.findtext('key')
      typ = IO.get_type(s.findtext('type'))
      tag = IO.get_tag(key, s.findtext('tag'))

      s = Subscription(key, typ)

      HelicsFederate.subscriptions.append(s)

      self.tags[key] = tag
      self.keys[tag] = key
      self.subs[key] = s

    for p in el.findall('publication'):
      key = p.findtext('key')
      typ = IO.get_type(p.findtext('type'))
      tag = IO.get_tag(key, p.findtext('tag'))

      p = Publication(key, typ)

      HelicsFederate.publications.append(p)

      self.tags[key] = tag
      self.keys[tag] = key
      self.pubs[key] = p

    need_endpoint = False

    for e in el.findall('endpoint'):
      need_endpoint = True

      endpoint_name = e.get('name')
      assert endpoint_name

      for t in e.findall('tag'):
        tag = t.text
        key = t.get(key, default=tag)

        self.eps[tag] = {'topic': key, 'endpoint': endpoint_name}

    if need_endpoint:
      # We only need a single endpoint in this HELICS federate to send updates
      # over. The name doesn't really matter since we'll never be receiving
      # updates, just sending them.
      HelicsFederate.endpoints.append(Endpoint("updates"))

    HelicsFederate.__init__(self)

    pub_endpoint  = el.findtext('pub-endpoint',  default=pub)
    pull_endpoint = el.findtext('pull-endpoint', default=pull)

    self.subscriber = Subscriber(pub_endpoint)
    self.pusher     = Pusher(pull_endpoint)
    self.metrics    = MetricsPusher()

    self.subscriber.add_update_handler(self.handle_msgbus_update)
    self.metrics.new_metric(MetricKind.COUNTER, 'status_count',            'number of OT-sim status messages generated')
    self.metrics.new_metric(MetricKind.COUNTER, 'update_count',            'number of OT-sim update messages processed')
    self.metrics.new_metric(MetricKind.COUNTER, 'helics_sub_count',        'number of HELICS subscriptions processed')
    self.metrics.new_metric(MetricKind.COUNTER, 'helics_pub_update_count', 'number of HELICS publication-based updates generated')


  def start(self: IO):
    # these already run in a thread
    self.subscriber.start('RUNTIME')
    self.metrics.start(self.pusher, self.name)

    # run HELICS helper in a thread
    threading.Thread(target=self.run, daemon=True).start()


  def stop(self: IO):
    self.subscriber.stop()
    self.metrics.stop()


  def handle_msgbus_update(self: IO, env: Envelope):
    update = envelope.update_from_envelope(env)

    if update:
      self.metrics.incr_metric('update_count')

      with self.mutex:
        for point in update['updates']:
          self.updated[point['tag']] = point['value']


  def action_subscriptions(self: IO, data: typing.Dict, _):
    '''published subcribed data as status message to message bus'''

    points: typing.List[Point] = []

    for k, v in data.items():
      tag = self.tags[k]

      if not tag: continue

      points.append({'tag': tag, 'value': float(v), 'ts': 0})
      self.metrics.incr_metric('helics_sub_count')

    if len(points) > 0:
      env = envelope.new_status_envelope(self.name, {'measurements': points})
      self.pusher.push('RUNTIME', env)
      self.metrics.incr_metric('status_count')


  def action_publications(self: IO, data: typing.Dict, _):
    '''publish any update messages that have come in to HELICS'''

    with self.mutex:
      for k in data.keys():
        tag = self.tags[k]

        if tag and tag in self.updated:
          pub   = self.pubs[k]
          value = self.updated[tag]

          print(f'[{self.name}] updating federate topic {k} to {value}')

          if pub.type == DataType.boolean:
            data[k] = bool(value)
          elif pub.type == DataType.double:
            data[k] = float(value)
          else:
            continue

          self.metrics.incr_metric('helics_pub_update_count')

      self.updated.clear()


  def action_endpoints_send(self: IO, endpoints: typing.Dict, ts: int):
    '''send any update messages that have come in to HELICS'''

    with self.mutex:
      # key is endpoint name, value is list of dictionaries representing updates
      data = defaultdict(list)

      for tag in self.updated:
        if tag in self.eps:
          ep = self.eps[tag]

          endpoint = ep['endpoint']
          key      = ep['topic']
          value    = self.updated[tag]

          print(f'[{self.name}] updating federate topic {key} to {value}')

          data[endpoint].append({'tag': key, 'value': value})

      for endpoint, data in data.items():
        endpoints['updates'].append(Message(destination=endpoint, time=ts, data=json.dumps(data)))

      self.updated.clear()


  def action_endpoints_recv(self: IO, endpoints: typing.Dict, ts: int):
    pass


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

  devices: typing.List[IO] = []

  for io in root.findall('io'):
    device = IO(pub, pull, io)
    device.start()

    devices.append(device)
  
  waiter = threading.Event()

  def handler(*_):
    waiter.set()

  signal.signal(signal.SIGINT, handler)
  waiter.wait()

  for device in devices:
    device.stop()
