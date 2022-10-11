from __future__ import annotations

import json, logging, requests, signal, socket, sys, threading, time, typing

import otsim.msgbus.envelope as envelope
import xml.etree.ElementTree as ET

from otsim.msgbus.envelope   import Envelope
from otsim.msgbus.subscriber import Subscriber


class GroundTruth:
  def __init__(self: GroundTruth, pub: str, el: ET.Element):
    self.name     = el.get('name', default='ot-sim-ground-truth')
    self.hostname = socket.gethostname()

    self.elastic    = 'http://localhost:9200'
    self.index_base = 'ot-sim'

    elastic = el.find('elastic')

    if elastic:
      self.elastic    = elastic.findtext('endpoint',        default=self.elastic)
      self.index_base = elastic.findtext('index-base-name', default=self.index_base)

    pub_endpoint    = el.findtext('pub-endpoint', default=pub)
    self.subscriber = Subscriber(pub_endpoint)

    self.subscriber.add_status_handler(self.handle_msgbus_status)

    self.__ensure_index_template()


  def start(self: GroundTruth):
    self.subscriber.start('RUNTIME')


  def stop(self: GroundTruth):
    self.subscriber.stop()


  def handle_msgbus_status(self: GroundTruth, env: Envelope):
    status = envelope.status_from_envelope(env)

    if status:
      headers = {'Content-Type': 'application/json'}
      index   = time.strftime(f'{self.index_base}-%Y.%m.%d', time.localtime())

      for point in status.get('measurements', []):
        ts = point['ts']

        if not ts:
          # milliseconds since epoch
          ts = time.time_ns() // 1000000

        doc = {
          '@timestamp': ts,
          'source':     self.hostname,
          'field':      point['tag'],
          'value':      point['value'],
        }

        resp = requests.post(f'{self.elastic}/{index}/_doc', data=json.dumps(doc), headers=headers)

        if not resp.ok:
          print(f'ERROR: sending data to Elastic - (HTTP Status {resp.status_code}) -- {resp.text}')


  def __ensure_index_template(self: GroundTruth):
    headers  = {'Content-Type': 'application/json'}
    template = {
      'index_patterns': [f'{self.index_base}-*'],
      'template': {
        'mappings': {
          'properties': {
            '@timestamp': {'type': 'date'},
            'source':     {'type': 'keyword'}, # don't analyze
            'field':      {'type': 'text'},    # analize
            'value':      {'type': 'double'}
          }
        }
      }
    }

    resp = requests.put(f'{self.elastic}/_index_template/{self.index_base}', data=json.dumps(template), headers=headers)

    if not resp.ok:
      print(f'ERROR: creating index template in Elastic - (HTTP Status {resp.status_code}) -- {resp.text}')


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
  else:
    pub  = 'tcp://127.0.0.1:5678'

  modules: typing.List[GroundTruth] = []

  for gt in root.findall('ground-truth'):
    module = GroundTruth(pub, gt)
    module.start()

    modules.append(module)
  
  waiter = threading.Event()

  def handler(*_):
    waiter.set()

  signal.signal(signal.SIGINT, handler)
  waiter.wait()

  for module in modules:
    module.stop()
