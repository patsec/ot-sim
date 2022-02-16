import typing

from pydnp3 import opendnp3

class Point(typing.NamedTuple):
  address: int
  tag: str
  svariation: typing.Any
  evariation: typing.Any
  output: bool
  sbo: bool
  clazz: opendnp3.PointClass
  deadband: float

Points = typing.Dict[int, Point]
