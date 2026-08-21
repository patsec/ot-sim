import calendar
import time
from dataclasses import dataclass
from datetime import datetime, timedelta
from enum import IntEnum
from pathlib import Path
from typing import Union, Any, List, Dict

PathStr = Union[Path, str]
StrPath = PathStr
TimeType = int
TimeOffsetType = int
Lfdi = str

SEP_XML = "application/sep+xml"


def format_time(dt_obj: datetime, is_local: bool = False) -> TimeType:
    """ Return a proper IEEE2030_5 TimeType object for the dt_obj passed in.
            From IEEE 2030.5 spec:
                TimeType Object (Int64)
                    Time is a signed 64 bit value representing the number of seconds
                    since 0 hours, 0 minutes, 0 seconds, on the 1st of January, 1970,
                    in UTC, not counting leap seconds.
        :param dt_obj: Datetime object to convert to IEEE2030_5 TimeType object.
        :param is_local: dt_obj is in UTC or Local time. Default to UTC time.
        :return: Time XSD object
        :raises: If utc_dt_obj is not UTC
    """

    if dt_obj.tzinfo is None:
        raise Exception("IEEE 2030.5 times should be timezone aware UTC or local")

    if dt_obj.utcoffset() != timedelta(0) and not is_local:
        raise Exception("IEEE 2030.5 TimeType should be based on UTC")

    if is_local:
        return TimeType(int(time.mktime(dt_obj.timetuple())))
    else:
        return TimeType(int(calendar.timegm(dt_obj.timetuple())))


class DERControlType(IntEnum):
    # Control modes supported by the DER. Bit
    # positions SHALL be defined as follows:
    # 0 - Charge mode
    chargeMode = 0
    # 1 - Discharge mode
    dischargeMode = 1
    # 2 - opModConnect (Connect / Disconnect -
    # implies galvanic isolation)
    opModConnect = 2
    # 3 - opModEnergize (Energize / De-Energize)
    opModEnergize = 3
    # 4 - opModFixedPFAbsorbW (Fixed Power
    opModFixedPFAbsorb = 4
    # Factor Setpoint when absorbing active # power)
    # 5 - opModFixedPFInjectW (Fixed Power
    opModFixedPFInject = 5
    # Factor Setpoint when injecting active power)
    # 6 - opModFixedVar (Reactive Power Setpoint)
    opModFixedVar = 6
    # 7 - opModFixedW (Charge / Discharge Setpoint)
    opModFixedW = 7
    # 8 - opModFreqDroop (Frequency-Watt Parameterized Mode)
    opModFreqDroop = 8
    # 9 - opModFreqWatt (Frequency-Watt Curve Mode)
    opModFreqWatt = 9
    # 10 - opModHFRTMayTrip (High Frequency Ride Through, May Trip Mode)
    opModHFRTMayTrip = 10
    # 11 - opModHFRTMustTrip (High Frequency Ride Through, Must Trip Mode)
    opModHFRTMustTrip = 11
    # 12 - opModHVRTMayTrip (High Voltage Ride Through, May Trip Mode)
    opModHVRTMayTrip = 12
    # 13 - opModHVRTMomentaryCessation (High Voltage Ride Through, Momentary
    # Cessation Mode)
    opModHVRTMomentaryCessation = 13
    # 14 - opModHVRTMustTrip (High Voltage Ride Through, Must Trip Mode)
    opModHVRTMustTrip = 14
    # 15 - opModLFRTMayTrip (Low Frequency Ride Through)
    opModLFRTMayTrip = 15
