from __future__ import annotations

import json, threading, time, typing

import otsim.msgbus.envelope as envelope

from otsim.msgbus.envelope import Metric, MetricKind
from otsim.msgbus.pusher   import Pusher

class MetricsPusher:
  def __init__(self: MetricsPusher):
    self.running = False
    self.metrics: typing.Dict[str, Metric] = {}

  def start(self: MetricsPusher, pusher: Pusher, name: str) -> None:
    self.running = True

    self.thread = threading.Thread(target=self.__run, args=(pusher, name,))
    self.thread.start()

  def stop(self: MetricsPusher) -> None:
    self.running = False
    self.thread.join()

  def new_metric(self: MetricsPusher, kind: MetricKind, name: str, desc: str) -> None:
    self.metrics[name] = {'kind': kind.value, 'name': name, 'desc': desc, 'value': 0.0}

  def incr_metric(self: MetricsPusher, name: str) -> None:
    if name in self.metrics:
      metric = self.metrics[name]
      metric['value'] += 1.0
      self.metrics[name] = metric

  def incr_metric_by(self: MetricsPusher, name: str, val: int) -> None:
    if name in self.metrics:
      metric = self.metrics[name]
      metric['value'] += float(val)
      self.metrics[name] = metric

  def set_metric(self: MetricsPusher, name: str, val: float) -> None:
    if name in self.metrics:
      metric = self.metrics[name]
      metric['value'] = val
      self.metrics[name] = metric

  def __run(self: MetricsPusher, pusher: Pusher, name: str) -> None:
    prefix = name + "_"

    while self.running:
      updates: typing.List[Metric] = []

      for metric in self.metrics.values():
        copy = metric

        if not copy['name'].startswith(prefix):
          copy['name'] = prefix + copy['name']

        updates.append(copy)

      if len(updates) > 0:
        env = envelope.new_metric_envelope(name, {'metrics': updates})
        pusher.push('HEALTH', env)

      time.sleep(5)