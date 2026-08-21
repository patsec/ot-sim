from dataclasses import dataclass

from dataclasses_json import dataclass_json


@dataclass_json
@dataclass
class DataTree:
    index: int
    dc: dataclass
    href: str
