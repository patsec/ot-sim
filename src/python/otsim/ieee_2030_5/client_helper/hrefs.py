from __future__ import annotations

from dataclasses import dataclass
from enum import Enum
from functools import lru_cache
from typing import Any, List, NamedTuple, Optional, Union

import client_helper.models as m

EDEV = "edev"
DCAP = "dcap"
UTP = "upt"
MUP = "mup"
DRP = "drp"
SDEV = "sdev"
MSG = "msg"
DER = "der"
CURVE = "dc"
RSPS = "rsps"
LOG = "log"
DERC = "derc"
DDERC = "dderc"
DERCA = "derca"
DERCURVE = "dc"
FSA = "fsa"
TIME = "tm"

DER_PROGRAM = "derp"
# DER Available
DER_AVAILABILITY = "dera"
# DER Status
DER_STATUS = "ders"
DER_CONTROL_ACTIVE = DERCA
DER_CAPABILITY = "dercap"
# Settings
DER_SETTINGS = "derg"
END_DEVICE_REGISTRATION = "rg"
END_DEVICE_STATUS = "dstat"
END_DEVICE_FSA = FSA
END_DEVICE_POWER_STATUS = "ps"
END_DEVICE_LOG_EVENT_LIST = "lel"
END_DEVICE_INFORMATION = "di"

DEFAULT_TIME_ROOT = f"/{TIME}"
DEFAULT_DCAP_ROOT = f"/{DCAP}"
DEFAULT_EDEV_ROOT = f"/{EDEV}"
DEFAULT_UPT_ROOT = f"/{UTP}"
DEFAULT_MUP_ROOT = f"/{MUP}"
DEFAULT_DRP_ROOT = f"/{DRP}"
DEFAULT_SELF_ROOT = f"/{SDEV}"
DEFAULT_MESSAGE_ROOT = f"/{MSG}"
DEFAULT_DER_ROOT = f"/{DER}"
DEFAULT_CURVE_ROOT = f"/{CURVE}"
DEFAULT_RSPS_ROOT = f"/{RSPS}"
DEFAULT_LOG_EVENT_ROOT = f"/{LOG}"
DEFAULT_FSA_ROOT = f"/{FSA}"
DEFAULT_DERP_ROOT = f"/{DER_PROGRAM}"
DEFAULT_DDERC_ROOT = f"/{DDERC}"

SEP = "_"
MATCH_REG = "[a-zA-Z0-9_]*"

# Used as a sentinal value when we only want the href of the root
NO_INDEX = -1


class HrefParser:

    def __init__(self, href: str):
        self.href = href
        self._split = href.split(SEP)

    def has_index(self) -> bool:
        """This function returns true if there is an index on the primary type.
        
        Ex: /edev_12_dstat has an index of 12 so this will return true
        Ex: /edev has no index so this will return false
        """
        return len(self._split) > 1

    def count(self) -> int:
        return len(self._split)

    def join(self, how_many: int) -> str:
        return SEP.join([str(x) for x in self._split[:how_many]])

    def startswith(self, value: str) -> bool:
        return self.href.startswith(value)

    def at(self, index: int) -> Union[str, int, None]:
        try:
            intvalue = int(self._split[index])
            return intvalue
        except ValueError:
            return self._split[index]
        except IndexError:
            return None


class HrefEventParser(HrefParser):

    @property
    def program_index(self) -> int:
        return int(self.at(1))

    @property
    def event_index(self) -> int:
        return int(self._split[-1])

    @property
    def events_href(self) -> str:
        return SEP.join(self._split[:-1])


class EndDeviceHref:

    def __init__(self, index: int = None, edev_href: str = None):
        if index is None and edev_href is None:
            raise ValueError("Must have either index or edev_href specified")

        if index is not None and edev_href is not None:
            raise ValueError("Cannot have both index and edev_href specified")

        self.index = index
        if edev_href is not None:
            self.index = int(edev_href.split(SEP)[1])

        self._root = SEP.join([DEFAULT_EDEV_ROOT, str(self.index)])

    @staticmethod
    def parse(href: str) -> EndDeviceHref:
        index = int(href.split(SEP)[1])
        return EndDeviceHref(index)

    def __str__(self) -> str:
        return SEP.join([DEFAULT_EDEV_ROOT, str(self.index)])

    @property
    def configuration(self) -> str:
        return SEP.join([DEFAULT_EDEV_ROOT, str(self.index), "cfg"])

    @property
    def der_list(self) -> str:
        return SEP.join([DEFAULT_EDEV_ROOT, str(self.index), DER])

    @property
    def device_information(self) -> str:
        return SEP.join([DEFAULT_EDEV_ROOT, str(self.index), END_DEVICE_INFORMATION])

    @property
    def device_status(self) -> str:
        return SEP.join([DEFAULT_EDEV_ROOT, str(self.index), END_DEVICE_STATUS])

    @property
    def power_status(self) -> str:
        return SEP.join([DEFAULT_EDEV_ROOT, str(self.index), END_DEVICE_POWER_STATUS])

    @property
    def registration(self) -> str:
        return SEP.join([DEFAULT_EDEV_ROOT, str(self.index), END_DEVICE_REGISTRATION])

    @property
    def function_set_assignments(self) -> str:
        return SEP.join([DEFAULT_EDEV_ROOT, str(self.index), END_DEVICE_FSA])

    @property
    def log_event_list(self) -> str:
        return SEP.join([DEFAULT_EDEV_ROOT, str(self.index), END_DEVICE_LOG_EVENT_LIST])

    def fill_hrefs(self, enddevice: Any) -> Any:
        """Populate canonical link objects on an EndDevice resource."""
        enddevice.href = self._root
        enddevice.RegistrationLink = m.RegistrationLink(href=self.registration)
        enddevice.DERListLink = m.DERListLink(href=self.der_list, all=0)
        enddevice.FunctionSetAssignmentsListLink = m.FunctionSetAssignmentsListLink(
            href=self.function_set_assignments,
            all=0,
        )
        enddevice.LogEventListLink = m.LogEventListLink(href=self.log_event_list, all=0)
        enddevice.DeviceInformationLink = m.DeviceInformationLink(href=self.device_information)
        enddevice.DeviceStatusLink = m.DeviceStatusLink(href=self.device_status)
        enddevice.PowerStatusLink = m.PowerStatusLink(href=self.power_status)
        return enddevice


# class DeviceCapabilityHref:

#     def __init__(self, index: int = None, enddevice: EndDeviceHref = None):
#         if index is None and enddevice is None:
#             raise ValueError(f"index and enddevice cannot both be None")
#         elif index is not None and enddevice is not None:
#             if enddevice.index != index:
#                 raise ValueError(f"index and enddevice.index must match if both are specified")

#         if enddevice is not None:
#             index = enddevice.index

#         self.index = index

#     def __str__(self) -> str:
#         return SEP.join([DEFAULT_DCAP_ROOT, str(self.index)])


class DERSubType(Enum):
    Capability = DER_CAPABILITY
    Settings = DER_SETTINGS
    Status = DER_STATUS
    Availability = DER_AVAILABILITY
    CurrentProgram = DER_PROGRAM
    None_Available = NO_INDEX

    @classmethod
    def parse(cls, value: str) -> DERSubType:
        if value == END_DEVICE_STATUS:
            return cls.Status
        return cls(value)


class FSASubType(Enum):
    DERProgram = "derp"


class DERProgramSubType(Enum):
    NoLink = 0
    ActiveDERControlListLink = 1
    DefaultDERControlLink = 2
    DERControlListLink = 3
    DERCurveListLink = 4
    DERControlReplyTo = 5
    DERControl = 6


class DERHref:

    def __init__(self, root: str) -> None:
        """Constructs a DERHref.
        
        The root should be a single instance not a list.  The properties
        on this object will be the href of the link to the resourse.
        """
        self.root = root

    @property
    def der_availability(self) -> str:
        return SEP.join([self.root, DER_AVAILABILITY])

    @property
    def der_status(self) -> str:
        return SEP.join([self.root, DER_STATUS])

    @property
    def der_capability(self) -> str:
        return SEP.join([self.root, DER_CAPABILITY])

    @property
    def der_settings(self) -> str:
        return SEP.join([self.root, DER_SETTINGS])

    @property
    def der_current_program(self) -> str:
        return SEP.join([self.root, DER_PROGRAM])

    def fill_hrefs(self, der: Any) -> Any:
        """Populate canonical link objects on a DER resource."""
        der.href = self.root
        der.DERAvailabilityLink = m.DERAvailabilityLink(href=self.der_availability)
        der.DERStatusLink = m.DERStatusLink(href=self.der_status)
        der.DERCapabilityLink = m.DERCapabilityLink(href=self.der_capability)
        der.DERSettingsLink = m.DERSettingsLink(href=self.der_settings)
        der.CurrentDERProgramLink = m.CurrentDERProgramLink(href=self.der_current_program)
        return der


class DeviceCapabilityHref:

    def __init__(self, end_device_index: str) -> None:
        self._end_device_index = end_device_index
        self.root = DEFAULT_DCAP_ROOT
        #SEP.join([DEFAULT_EDEV_ROOT, self._end_device_index, DER, DER_CAPABILITY])
        #m.DeviceCapability

    @property
    def enddevice_href(self) -> str:
        return DEFAULT_EDEV_ROOT

    @property
    def mirror_usage_point_href(self) -> str:
        return DEFAULT_MUP_ROOT

    @property
    def self_device_href(self) -> str:
        return DEFAULT_SELF_ROOT

    @property
    def time_href(self) -> str:
        return DEFAULT_TIME_ROOT

    @property
    def usage_point_href(self) -> str:
        return DEFAULT_UPT_ROOT

    def fill_hrefs(self, dcap: Any) -> Any:
        """Populate canonical discovery links on a DeviceCapability resource."""
        dcap.href = self.root
        dcap.EndDeviceListLink = m.EndDeviceListLink(href=self.enddevice_href, all=0)
        dcap.MirrorUsagePointListLink = m.MirrorUsagePointListLink(
            href=self.mirror_usage_point_href,
            all=0,
        )
        dcap.SelfDeviceLink = m.SelfDeviceLink(href=self.self_device_href)
        dcap.TimeLink = m.TimeLink(href=self.time_href)
        dcap.UsagePointListLink = m.UsagePointListLink(href=self.usage_point_href, all=0)
        return dcap


class DERProgramHref:

    def __init__(self, program_index: int) -> None:
        self._root = SEP.join([DEFAULT_DERP_ROOT, str(program_index)])

    @property
    def active_control_href(self) -> str:
        return SEP.join([self._root, DER_CONTROL_ACTIVE])

    @property
    def default_control_href(self) -> str:
        return SEP.join([self._root, DDERC])

    @property
    def der_control_list_href(self) -> str:
        return SEP.join([self._root, DERC])

    @property
    def der_curve_list_href(self) -> str:
        return SEP.join([self._root, CURVE])

    def fill_hrefs(self, program: Any) -> Any:
        """Populate canonical link objects on a DERProgram resource."""
        program.href = self._root
        program.ActiveDERControlListLink = m.ActiveDERControlListLink(
            href=self.active_control_href,
            all=0,
        )
        program.DefaultDERControlLink = m.DefaultDERControlLink(href=self.default_control_href)
        program.DERControlListLink = m.DERControlListLink(href=self.der_control_list_href, all=0)
        program.DERCurveListLink = m.DERCurveListLink(href=self.der_curve_list_href, all=0)
        return program


class DERProgramHrefOld(NamedTuple):
    root: str
    index: int
    derp_subtype: DERProgramSubType = DERProgramSubType.NoLink
    derp_subtype_index: int = NO_INDEX

    @staticmethod
    def parse(href: str) -> DERProgramHrefOld:
        parsed = href.split(SEP)
        if len(parsed) == 1:
            return DERProgramHrefOld(parsed[0], NO_INDEX)
        elif len(parsed) == 2:
            return DERProgramHrefOld(parsed[0], int(parsed[1]))
        else:
            mapped = dict(
                derc=DERProgramSubType.DERControlListLink,
                derca=DERProgramSubType.ActiveDERControlListLink,
                dderc=DERProgramSubType.DefaultDERControlLink,
            )
            if len(parsed) == 4:
                return DERProgramHrefOld(parsed[0], int(parsed[1]), mapped[parsed[2]],
                                         int(parsed[3]))
            return DERProgramHrefOld(parsed[0], int(parsed[1]), mapped[parsed[2]])


def der_program_parse(href: str) -> DERProgramHrefOld:
    return DERProgramHrefOld.parse(href)


def der_program_href(index: int = NO_INDEX,
                     sub: DERProgramSubType = DERProgramSubType.NoLink,
                     subindex: int = NO_INDEX) -> str:
    if index == NO_INDEX:
        return DEFAULT_DERP_ROOT

    if sub == DERProgramSubType.NoLink:
        return SEP.join([DEFAULT_DERP_ROOT, str(index)])

    if sub == DERProgramSubType.ActiveDERControlListLink:
        if subindex == NO_INDEX:
            return SEP.join([DEFAULT_DERP_ROOT, str(index), DER_CONTROL_ACTIVE])
        else:
            return SEP.join([DEFAULT_DERP_ROOT, str(index), DER_CONTROL_ACTIVE, str(subindex)])

    if sub == DERProgramSubType.DefaultDERControlLink:
        if subindex == NO_INDEX:
            return SEP.join([DEFAULT_DERP_ROOT, str(index), DDERC])
        else:
            return SEP.join([DEFAULT_DERP_ROOT, str(index), DDERC, str(subindex)])

    if sub == DERProgramSubType.DERCurveListLink:
        if subindex == NO_INDEX:
            return SEP.join([DEFAULT_DERP_ROOT, str(index), CURVE])
        else:
            return SEP.join([DEFAULT_DERP_ROOT, str(index), CURVE, str(subindex)])

    if sub == DERProgramSubType.DERControlListLink:
        if subindex == NO_INDEX:
            return SEP.join([DEFAULT_DERP_ROOT, str(index), DERC])
        else:
            return SEP.join([DEFAULT_DERP_ROOT, str(index), DERC, str(subindex)])

    if sub == DERProgramSubType.DERControlReplyTo:
        return DEFAULT_RSPS_ROOT


@lru_cache()
def get_server_config_href() -> str:
    return "/server/cfg"


@lru_cache()
def get_enddevice_list_href() -> str:
    return DEFAULT_EDEV_ROOT


@lru_cache()
def curve_href(index: int = NO_INDEX) -> str:
    if index == NO_INDEX:
        return DEFAULT_CURVE_ROOT

    return SEP.join([DEFAULT_CURVE_ROOT, str(index)])


@lru_cache()
def fsa_href(index: int = NO_INDEX, edev_index: int = NO_INDEX):
    if index == NO_INDEX and edev_index == NO_INDEX:
        return DEFAULT_FSA_ROOT
    elif index != NO_INDEX and edev_index == NO_INDEX:
        return SEP.join([DEFAULT_FSA_ROOT, str(index)])
    elif index == NO_INDEX and edev_index != NO_INDEX:
        return SEP.join([DEFAULT_EDEV_ROOT, str(edev_index), FSA])
    else:
        return SEP.join([DEFAULT_EDEV_ROOT, str(edev_index), FSA, str(index)])


def derp_href(edev_index: int, fsa_index: int) -> str:
    return SEP.join([DEFAULT_EDEV_ROOT, str(edev_index), FSA, str(fsa_index), DER_PROGRAM])


def der_href(index: int = NO_INDEX, fsa_index: int = NO_INDEX, edev_index: int = NO_INDEX):
    if index == NO_INDEX and fsa_index == NO_INDEX and edev_index == NO_INDEX:
        return DEFAULT_DER_ROOT
    elif index != NO_INDEX and fsa_index == NO_INDEX and edev_index == NO_INDEX:
        return SEP.join([DEFAULT_DER_ROOT, str(index)])
    elif index == NO_INDEX and fsa_index != NO_INDEX and edev_index == NO_INDEX:
        return SEP.join([DEFAULT_FSA_ROOT, str(fsa_index), DER_PROGRAM])
    elif edev_index != NO_INDEX and fsa_index == NO_INDEX and index == NO_INDEX:
        return SEP.join([DEFAULT_EDEV_ROOT, int(edev_index), FSA])
    elif edev_index != NO_INDEX and fsa_index != NO_INDEX and index == NO_INDEX:
        return SEP.join([DEFAULT_EDEV_ROOT, int(edev_index), FSA, int(fsa_index)])
    else:
        raise ValueError(f"index={index}, fsa_index={fsa_index}, edev_index={edev_index}")


def edev_der_href(edev_index: int, der_index: int = NO_INDEX) -> str:
    if der_index == NO_INDEX:
        return SEP.join([DEFAULT_EDEV_ROOT, str(edev_index), DER])
    return SEP.join([DEFAULT_EDEV_ROOT, str(edev_index), DER, str(der_index)])


class EDevSubType(Enum):
    None_Available = NO_INDEX
    Registration = END_DEVICE_REGISTRATION
    DeviceStatus = END_DEVICE_STATUS
    PowerStatus = END_DEVICE_POWER_STATUS
    FunctionSetAssignments = END_DEVICE_FSA
    LogEventList = END_DEVICE_LOG_EVENT_LIST
    DeviceInformation = END_DEVICE_INFORMATION
    DER = DER


@dataclass
class EdevHref:
    edev_index: int
    edev_subtype: EDevSubType = EDevSubType.None_Available
    edev_subtype_index: int = NO_INDEX
    edev_der_subtype: DERSubType = DERSubType.None_Available

    def __str__(self) -> str:
        value = "/edev"
        if self.edev_index != NO_INDEX:
            value = f"{value}{SEP}{self.edev_index}"

        if self.edev_subtype != EDevSubType.None_Available:
            value = f"{value}{SEP}{self.edev_subtype.value}"

        if self.edev_subtype_index != NO_INDEX:
            value = f"{value}{SEP}{self.edev_subtype_index}"

        if self.edev_der_subtype != DERSubType.None_Available:
            value = f"{value}{SEP}{self.edev_der_subtype.value}"

        return value

    def parse(path: str) -> EdevHref:
        split_pth = path.split(SEP)

        if split_pth[0] != EDEV and split_pth[0][1:] != EDEV:
            raise ValueError(f"Must start with {EDEV}")

        if len(split_pth) == 1:
            return EdevHref(NO_INDEX)
        elif len(split_pth) == 2:
            return EdevHref(int(split_pth[1]))
        elif len(split_pth) == 3:
            return EdevHref(int(split_pth[1]), edev_subtype=EDevSubType(split_pth[2]))
        elif len(split_pth) == 4:
            return EdevHref(int(split_pth[1]),
                            edev_subtype=EDevSubType(split_pth[2]),
                            edev_subtype_index=int(split_pth[3]))
        elif len(split_pth) == 5:
            return EdevHref(int(split_pth[1]),
                            edev_subtype=EDevSubType(split_pth[2]),
                            edev_subtype_index=int(split_pth[3]),
                            edev_der_subtype=DERSubType.parse(split_pth[4]))
        else:
            raise ValueError("Out of bounds parsing.")

    def __eq__(self, other: object) -> bool:
        return other.edev_index == self.edev_index and other.edev_subtype == self.edev_subtype, \
            other.edev_subtype_index == self.edev_subtype_index and other.edev_der_subtype == self.edev_der_subtype


class FSAHref(NamedTuple):
    fsa_index: NO_INDEX
    fsa_sub: FSASubType = None


def fsa_parse(path: str) -> FSAHref:
    split_pth = path.split(SEP)

    if len(split_pth) == 1:
        return FSAHref(NO_INDEX)
    elif len(split_pth) == 2:
        return FSAHref(int(split_pth[1]))
    elif len(split_pth) == 3:
        return FSAHref(int(split_pth[1]), fsa_sub=split_pth[2])

    raise ValueError("Invalid parsing path.")


def der_sub_href(edev_index: int, index: int = NO_INDEX, subtype: DERSubType = None):
    if subtype is None and index == NO_INDEX:
        return SEP.join([DEFAULT_EDEV_ROOT, str(edev_index), DER])
    elif subtype is None:
        return SEP.join([DEFAULT_EDEV_ROOT, str(edev_index), DER, str(index)])
    else:
        return SEP.join([DEFAULT_EDEV_ROOT, str(edev_index), DER, str(index), subtype.value])


@lru_cache()
def mirror_usage_point_href(mirror_usage_point_index: int = NO_INDEX):
    """Mirror Usage Point hrefs
    
       /mup
       /mup/{mirror_usage_point_index}
       
    
    """
    if mirror_usage_point_index == NO_INDEX:
        ret = DEFAULT_MUP_ROOT
    else:
        ret = SEP.join([DEFAULT_MUP_ROOT, str(mirror_usage_point_index)])

    return ret


class ParsedUsagePointHref:

    def __init__(self, href: str):
        self._href = href
        self._split = href.split(SEP)

    def last_list(self) -> str:
        """Assuming the parsed href has a reference to an item, return the container href.
        """
        if self._split[-1].isnumeric():
            return SEP.join(self._split[:-1])
        return self._href

    def has_usage_point_index(self) -> bool:
        return self.usage_point_index is not None

    def has_extra(self) -> bool:
        return self.has_meter_reading_list() or \
                self.has_reading_list() or \
                self.has_reading_set_list() or \
                self.has_reading_set_reading_list()

    def has_meter_reading_list(self) -> bool:
        try:
            if retval := self._split[2] == "mr":
                ...
            return retval
        except IndexError:
            return False

    def has_reading_type(self) -> bool:
        try:
            if retval := self._split[4] == "rt":
                ...
            return retval
        except IndexError:
            return False

    def has_reading_set_list(self) -> bool:
        try:
            if retval := self._split[4] == "rs":
                ...
            return retval
        except IndexError:
            return False

    def has_reading_set_reading_list(self) -> bool:
        try:
            if retval := self._split[6] == "r":
                ...
            return retval
        except IndexError:
            return False

    def has_reading_list(self) -> bool:
        try:
            if retval := self._split[4] == "r" or self._split[6] == "r":
                ...
            return retval
        except IndexError:
            return False

    @property
    def usage_point_index(self) -> Optional[int]:
        try:
            return int(self._split[1])
        except IndexError:
            pass
        return None

    @property
    def meter_reading_index(self) -> Optional[int]:
        try:
            return int(self._split[3])
        except IndexError:
            pass
        return None

    @property
    def reading_set_index(self) -> Optional[int]:
        try:
            if self._split[4] == "rs":
                return int(self._split[5])
        except IndexError:
            pass
        return None

    @property
    def reading_set_reading_index(self) -> Optional[int]:
        try:
            if self._split[6] == "r":
                return int(self._split[7])
        except IndexError:
            pass
        return None

    @property
    def reading_index(self) -> Optional[int]:
        try:
            if self._split[4] == "r":
                return int(self._split[5])
            elif self._split[6] == "r":
                return int(self._split[7])
        except IndexError:
            pass

        return None


class UsagePointHref:

    def __init__(self, href: str = None, root: str = '/upt'):
        self._href = href
        self._root = root

    def is_root(self) -> bool:
        return self._href == self._root

    def value(self) -> str:
        return self._root

    def usage_point(self, usage_point_index: int):
        return SEP.join([self._root, str(usage_point_index)])

    def meterreading_list(self, usage_point_index: int) -> str:
        return SEP.join([self._root, str(usage_point_index), "mr"])

    def meterreading(self, usage_point_index: int, meter_reading_index: int) -> str:
        return SEP.join([self.meterreading_list(usage_point_index), str(meter_reading_index)])

    def readingset_list(self, usage_point_index: int, meter_reading_index: int) -> str:
        return SEP.join([self.meterreading(usage_point_index, meter_reading_index), "rs"])

    def readingtype(self, usage_point_index: int, meter_reading_index: int) -> str:
        return SEP.join([self.meterreading(usage_point_index, meter_reading_index), "rt"])

    def readingset(self, usage_point_index: int, meter_reading_index: int,
                   reading_set_index: int) -> str:
        return SEP.join(
            [self.readingset_list(usage_point_index, meter_reading_index),
             str(reading_set_index)])

    def readingsetreading_list(self, usage_point_index: int, meter_reading_index: int,
                               reading_set_index: int):
        return SEP.join(
            [self.readingset(usage_point_index, meter_reading_index, reading_set_index), "r"])

    def readingsetreading(self, usage_point_index: int, meter_reading_index: int,
                          reading_set_index: int, reading_index: int):
        return SEP.join([
            self.readingsetreading_list(usage_point_index, meter_reading_index, reading_set_index),
            str(reading_index)
        ])

    def reading_list(self, usage_point_index: int, meter_reading_index: int) -> str:
        return SEP.join([self.meterreading(usage_point_index, meter_reading_index), "r"])

    def reading(self, usage_point_index: int, meter_reading_index: int, reading_index: int) -> str:
        return SEP.join(
            [self.reading_list(usage_point_index, meter_reading_index),
             str(reading_index)])


@dataclass
class MirrorUsagePointHref:
    mirror_usage_point_index: int = NO_INDEX
    meter_reading_list_index: int = NO_INDEX
    meter_reading_index: int = NO_INDEX
    reading_set_index: int = NO_INDEX
    reading_index: int = NO_INDEX

    @staticmethod
    def parse(href: str) -> MirrorUsagePointHref:
        items = href.split(SEP)
        if len(items) == 1:
            return MirrorUsagePointHref()

        if len(items) == 2:
            return MirrorUsagePointHref(items[1])


def usage_point_href(usage_point_index: int | str = NO_INDEX,
                     meter_reading_list: bool = False,
                     meter_reading_list_index: int = NO_INDEX,
                     meter_reading_index: int = NO_INDEX,
                     meter_reading_type: bool = False,
                     reading_set: bool = False,
                     reading_set_index: int = NO_INDEX,
                     reading_index: int = NO_INDEX):
    """Usage point hrefs 

       /upt
       /upt/{usage_point_index}
       /upt/{usage_point_index}/mr
       /upt/{usage_point_index}/mr/{meter_reading_index}
       /upt/{usage_point_index}/mr/{meter_reading_index}/rt
       /upt/{usage_point_index}/mr/{meter_reading_index}/rs
       /upt/{usage_point_index}/mr/{meter_reading_index}/rs/{reading_set_index}
       /upt/{usage_point_index}/mr/{meter_reading_index}/rs/{reading_set_index}/r
       /upt/{usage_point_index}/mr/{meter_reading_index}/rs/{reading_set_index}/r/{reading_index}
       
       

    """
    if isinstance(usage_point_index, str):
        base_upt = usage_point_index
    else:
        base_upt = DEFAULT_UPT_ROOT

    if usage_point_index == NO_INDEX:
        ret = base_upt
    else:
        if isinstance(usage_point_index, str):
            arr = [base_upt]
        else:
            arr = [DEFAULT_UPT_ROOT, str(usage_point_index)]

        if meter_reading_list:
            if meter_reading_list_index == NO_INDEX:
                arr.extend(["mr"])
            else:
                arr.extend(["mr", str(meter_reading_list_index)])

        ret = SEP.join(arr)
    return ret


def get_der_program_list(fsa_href: str) -> str:
    return SEP.join([fsa_href, "der"])


def get_dr_program_list(fsa_href: str) -> str:
    return SEP.join([fsa_href, "dr"])


def get_fsa_list_href(end_device_href: str) -> str:
    return SEP.join([end_device_href, "fsa"])


def get_response_set_href():
    return DEFAULT_RSPS_ROOT


@lru_cache()
def get_der_list_href(index: int) -> str:
    if index == NO_INDEX:
        ret = DEFAULT_DER_ROOT
    else:
        ret = SEP.join([DEFAULT_DER_ROOT, str(index)])
    return ret


@lru_cache()
def get_enddevice_href(edev_indx: int = NO_INDEX, subref: str = None) -> str:
    if edev_indx == NO_INDEX:
        ret = DEFAULT_EDEV_ROOT
    elif subref:
        ret = SEP.join([DEFAULT_EDEV_ROOT, f"{edev_indx}", f"{subref}"])
    else:
        ret = SEP.join([DEFAULT_EDEV_ROOT, f"{edev_indx}"])
    return ret


@lru_cache()
def registration_href(edev_index: int) -> str:
    return SEP.join([DEFAULT_EDEV_ROOT, str(edev_index), "rg"])


@lru_cache()
def get_configuration_href(edev_index: int) -> str:
    return get_enddevice_href(edev_index, "cfg")


@lru_cache()
def get_power_status_href(edev_index: int) -> str:
    return get_enddevice_href(edev_index, "ps")


@lru_cache()
def get_device_status(edev_index: int) -> str:
    return get_enddevice_href(edev_index, "ds")


@lru_cache()
def get_device_information(edev_index: int) -> str:
    return get_enddevice_href(edev_index, "di")


@lru_cache()
def get_time_href() -> str:
    # return f"{DEFAULT_DCAP_ROOT}{SEP}tm"
    return f"/tm"


@lru_cache()
def get_log_list_href(edev_index: int) -> str:
    return get_enddevice_href(edev_index, "lel")


@lru_cache()
def get_dcap_href() -> str:
    return f"{DEFAULT_DCAP_ROOT}"


def get_dderc_href() -> str:
    return SEP.join([DEFAULT_DER_ROOT, DDERC])


def get_derc_default_href(derp_index: int) -> str:
    return SEP.join([DEFAULT_DER_ROOT, DDERC, f"{derp_index}"])


def get_derc_href(index: int) -> str:
    """Return the DERControl href to the caller

    if NO_INDEX then don't include the index in the result.
    """
    if index == NO_INDEX:
        return SEP.join([DEFAULT_DER_ROOT, DERC])

    return SEP.join([DEFAULT_DER_ROOT, DERC, f"{index}"])


def get_program_href(index: int, subref: str = None):
    """Return the DERProgram href to the caller

    Args:
        index: if NO_INDEX then don't include the index in the result else use the index
        subref: used to specify a subsection in the program.
    """
    if index == NO_INDEX:
        ref = f"{DEFAULT_DERP_ROOT}"
    else:
        if subref is not None:
            ref = f"{DEFAULT_DERP_ROOT}{SEP}{index}{SEP}{subref}"
        else:
            ref = f"{DEFAULT_DERP_ROOT}{SEP}{index}"
    return ref


sdev: str = DEFAULT_SELF_ROOT

admin: str = "/admin"
uuid_gen: str = "/uuid"


def build_link(base_url: str, *suffix: Optional[str]):
    result = base_url
    if result.endswith("/"):
        result = result[:-1]

    if suffix:
        for p in suffix:
            if p is not None:
                if isinstance(p, str):
                    if p.startswith("/"):
                        result += f"{p}"
                    else:
                        result += f"/{p}"
                else:
                    result += f"/{p}"

    return result


def extend_url(base_url: str, index: Optional[int] = None, suffix: Optional[str] = None):
    result = base_url
    if index is not None:
        result += f"/{index}"
    if suffix:
        result += f"/{suffix}"

    return result
