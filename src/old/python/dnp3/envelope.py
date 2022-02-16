import enum, json, typing

class EnvelopeKind(enum.Enum):
  STATUS       = 'Status'
  UPDATE       = 'Update'
  CONFIRMATION = 'Confirmation'

class Envelope(typing.TypedDict):
  apiVersion: str
  kind: EnvelopeKind
  metadata: typing.Dict[str, str]
  contents: str

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

def new_update_envelope(sender: str, update: Update) -> Envelope:
  env: Envelope = {
    'apiVersion': 'v1',
    'kind': EnvelopeKind.UPDATE.value,
    'metadata': {'sender': sender},
    'contents': update,
  }

  return env

def status_from_envelope(env: Envelope) -> Status:
  if env['kind'] != EnvelopeKind.STATUS.value:
    return None

  return env['contents']