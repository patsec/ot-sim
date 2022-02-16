from __future__ import annotations

import signal, sys, threading, time, zmq
import xml.etree.ElementTree as ET

import variations

from client     import Client
from logger     import Logger
from master     import Master, MasterConfig
from outstation import Outstation, OutstationConfig
from point      import Point
from server     import Server

def parse_points(el: ET.Element, dev: Master | Outstation) -> None:
  for r in el.findall('input'):
    typ = r.get('type')

    assert typ in ['binary', 'analog']

    address = int(r.findtext('address'))
    tag = r.findtext('tag')

    assert tag

    clvar = r.findtext('class', 'Class1')
    clazz = variations.point_classes.get(clvar, None)

    assert clazz

    if typ == 'binary':
      sgvar = r.findtext('sgvar', default='Group1Var2')
      svariation = variations.static_binary_variations.get(sgvar, None)

      assert svariation

      egvar = r.findtext('egvar', default='Group2Var2')
      evariation = variations.event_binary_variations.get(egvar, None)

      assert evariation

      point = Point(address, tag, svariation, evariation, False, False, clazz, 0.0)
      dev.add_binary(point)
    elif typ == 'analog':
      sgvar = r.findtext('sgvar', default='Group30Var6')
      svariation = variations.static_analog_variations.get(sgvar, None)

      assert svariation

      egvar = r.findtext('egvar', default='Group32Var6')
      evariation = variations.event_analog_variations.get(egvar, None)

      assert evariation

      point = Point(address, tag, svariation, evariation, False, False, clazz, 0.0)
      dev.add_analog(point)

  for r in el.findall('output'):
    typ = r.get('type')

    assert typ in ['binary', 'analog']

    address = int(r.findtext('address'))
    tag = r.findtext('tag')

    assert tag

    sbo = r.findtext('sbo', default='false') == 'true'
    output = True

    clvar = r.findtext('class', 'Class1')
    clazz = variations.point_classes.get(clvar, None)

    assert clazz

    if typ == 'binary':
      sgvar = r.findtext('sgvar', default='Group10Var2')
      svariation = variations.static_binary_variations.get(sgvar, None)

      assert svariation

      egvar = r.findtext('egvar', default='Group11Var2')
      evariation = variations.event_binary_variations.get(egvar, None)

      assert evariation

      point = Point(address, tag, svariation, evariation, output, sbo, clazz, 0.0)
      dev.add_binary(point)
    elif typ == 'analog':
      sgvar = r.findtext('sgvar', default='Group40Var4')
      svariation = variations.static_binary_variations.get(sgvar, None)

      assert svariation

      egvar = r.findtext('egvar', default='Group42Var6')
      evariation = variations.event_binary_variations.get(egvar, None)

      assert evariation

      point = Point(address, tag, svariation, evariation, output, sbo, clazz, 0.0)
      dev.add_analog(point)

def main(config: str):
  tree = ET.parse(config)
  root = tree.getroot()

  assert root.tag == 'ot-sim'

  el = root.find('message-bus')

  assert el

  pub_endpoint  = el.findtext('pub-endpoint',  default='tcp://127.0.0.1:5678')
  pull_endpoint = el.findtext('pull-endpoint', default='tcp://127.0.0.1:1234')

  context    = zmq.Context()
  subscriber = context.socket(zmq.SUB)
  pusher     = context.socket(zmq.PUSH)

  subscriber.connect(pub_endpoint)
  subscriber.setsockopt(zmq.SUBSCRIBE, b'RUNTIME')

  pusher.connect(pull_endpoint)

  for el in root.findall('dnp3'):
    mode = el.get('mode')

    if mode == 'client':
      cid = el.get('name', default='dnp3-client')
      endpoint = el.findtext('endpoint', default='tcp://127.0.0.1:20000')

      client = Client(Logger(cid, pusher))
      client.init_client(cid, endpoint)

      for m in el.findall('master'):
        mid = m.get('name', default='dnp3-master')
        local = m.findtext('local-address')
        remote: str | int

        if local:
          remote = m.findtext('remote-address')
        else:
          local = m.findtext('address')
          remote = 1024

        master = client.add_master(mid, int(local), int(remote), pusher)

        parse_points(m, master)

      client.start(subscriber)
    elif mode == 'server':
      sid = el.get('name', default='dnp3-server')
      endpoint = el.findtext('endpoint', default='tcp://127.0.0.1:20000')
      cold = el.findtext('cold-restart-delay', default='180')

      server = Server(Logger(sid, pusher), int(cold))
      server.init_server(sid, endpoint)

      for o in el.findall('outstation'):
        oid   = o.get('name', default='dnp3-outstation')
        warm  = o.findtext('warm-restart-delay', default='30')
        local = o.findtext('local-address')
        remote: str | int

        if local:
          remote = o.findtext('remote-address')
        else:
          local = o.findtext('address')
          remote = 1

        outstation = server.add_outstation(oid, int(local), int(remote), int(warm), pusher)

        parse_points(o, outstation)

      server.start(subscriber)

if __name__ == '__main__':
  if len(sys.argv) > 2:
    time.sleep(int(sys.argv[2]))

  main(sys.argv[1])

  waiter = threading.Event()

  def handler(signum, frame):
    waiter.set()

  signal.signal(signal.SIGINT, handler)
  waiter.wait()