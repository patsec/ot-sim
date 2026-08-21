import enum, typing

class EnvelopeKind(enum.Enum):
  STATUS       = 'Status'
  UPDATE       = 'Update'
  CONFIRMATION = 'Confirmation'
  METRIC       = 'Metric'

class MetricKind(enum.Enum):
  COUNTER = 'Counter'
  GAUGE   = 'Gauge'

class Point(typing.TypedDict):
  tag: str
  value: float
  ts: int

class Status(typing.TypedDict):
  measurements: typing.List[Point]

class Update(typing.TypedDict):
  updates: typing.List[Point]
  recipient: str
  confirm: str

class Confirmation(typing.TypedDict):
  confirm: str
  errors: typing.Dict[str, str]

class Metric(typing.TypedDict):
  kind: str
  name: str
  desc: str
  value: float

class Metrics(typing.TypedDict):
  metrics: typing.List[Metric]

class Envelope(typing.TypedDict):
  version: str
  kind: EnvelopeKind
  metadata: typing.Dict[str, str]
  contents: str

def new_status_envelope(sender: str, status: Status) -> Envelope:
  env: Envelope = {
    'version': 'v1',
    'kind': EnvelopeKind.STATUS.value,
    'metadata': {'sender': sender},
    'contents': status,
  }

  return env

def new_update_envelope(sender: str, update: Update) -> Envelope:
  if 'recipient' not in update:
    update['recipient'] = ''

  if 'confirm' not in update:
    update['confirm'] = ''

  env: Envelope = {
    'version': 'v1',
    'kind': EnvelopeKind.UPDATE.value,
    'metadata': {'sender': sender},
    'contents': update,
  }

  return env

def new_confirmation_envelope(sender: str, conf: Confirmation) -> Envelope:
  env: Envelope = {
    'version': 'v1',
    'kind': EnvelopeKind.CONFIRMATION.value,
    'metadata': {'sender': sender},
    'contents': conf,
  }

  return env

def new_metric_envelope(sender: str, metrics: Metrics) -> Envelope:
  env: Envelope = {
    'version': 'v1',
    'kind': EnvelopeKind.METRIC.value,
    'metadata': {'sender': sender},
    'contents': metrics,
  }

  return env

def status_from_envelope(env: Envelope) -> Status:
  if env['kind'] != EnvelopeKind.STATUS.value:
    return None

  return env['contents']

def update_from_envelope(env: Envelope) -> Update:
  if env['kind'] != EnvelopeKind.UPDATE.value:
    return None

  return env['contents']

def confirmation_from_envelope(env: Envelope) -> Confirmation:
  if env['kind'] != EnvelopeKind.CONFIRMATION.value:
    return None

  return env['contents']