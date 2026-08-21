"""Minimal IEEE 2030.5 protocol model profile used by this repository.

This module intentionally models a focused subset of IEEE 2030.5 resources
that are required by the current standalone scheduler + device flow.
"""

from __future__ import annotations

from dataclasses import asdict, dataclass, field
from typing import Any, Dict, List, Optional


def _pick(payload: Dict[str, Any], *keys: str, default: Any = None) -> Any:
    for key in keys:
        if key in payload:
            return payload[key]
    return default


@dataclass
class DeviceIdentity:
    sfdi: str
    lfdi: str
    pin_code: str
    categories: List[str] = field(default_factory=list)
    enabled: bool = True


@dataclass
class DerCapability:
    rtg_w: float
    rtg_va: float
    max_charge_rate_w: Optional[float] = None
    max_discharge_rate_w: Optional[float] = None
    supported_modes: List[str] = field(default_factory=list)


@dataclass
class DerControlBase:
    op_mod_connect: Optional[bool] = None
    op_mod_energize: Optional[bool] = None
    op_mod_fixed_w: Optional[float] = None
    op_mod_fixed_pf_absorb_w: Optional[float] = None
    op_mod_fixed_pf_inject_w: Optional[float] = None
    set_grad_w: Optional[float] = None
    set_es_delay: Optional[int] = None

    @staticmethod
    def from_api_payload(payload: Dict[str, Any]) -> "DerControlBase":
        return DerControlBase(
            op_mod_connect=_pick(payload, "opModConnect", "op_mod_connect"),
            op_mod_energize=_pick(payload, "opModEnergize", "op_mod_energize"),
            op_mod_fixed_w=_pick(payload, "opModFixedW", "op_mod_fixed_w"),
            op_mod_fixed_pf_absorb_w=_pick(payload, "opModFixedPFAbsorbW",
                                           "op_mod_fixed_pf_absorb_w"),
            op_mod_fixed_pf_inject_w=_pick(payload, "opModFixedPFInjectW",
                                           "op_mod_fixed_pf_inject_w"),
            set_grad_w=_pick(payload, "setGradW", "set_grad_w"),
            set_es_delay=_pick(payload, "setESDelay", "set_es_delay"),
        )

    def to_api_payload(self) -> Dict[str, Any]:
        payload = {
            "opModConnect": self.op_mod_connect,
            "opModEnergize": self.op_mod_energize,
            "opModFixedW": self.op_mod_fixed_w,
            "opModFixedPFAbsorbW": self.op_mod_fixed_pf_absorb_w,
            "opModFixedPFInjectW": self.op_mod_fixed_pf_inject_w,
            "setGradW": self.set_grad_w,
            "setESDelay": self.set_es_delay,
        }
        return {k: v for k, v in payload.items() if v is not None}


@dataclass
class TimeWindow:
    start_time_utc: str
    duration_sec: int

    @staticmethod
    def from_api_payload(payload: Dict[str, Any]) -> "TimeWindow":
        return TimeWindow(start_time_utc=_pick(payload, "start", "start_time_utc"),
                          duration_sec=int(_pick(payload, "durationSec", "duration_sec")))

    def to_api_payload(self) -> Dict[str, Any]:
        return {"start": self.start_time_utc, "durationSec": self.duration_sec}


@dataclass
class DerControlEvent:
    event_id: str
    interval: TimeWindow
    control_base: DerControlBase

    @staticmethod
    def from_api_payload(payload: Dict[str, Any]) -> "DerControlEvent":
        interval = TimeWindow.from_api_payload(payload["interval"])
        control = DerControlBase.from_api_payload(_pick(payload, "DERControlBase", "control_base",
                                                        default={}))
        return DerControlEvent(event_id=_pick(payload, "eventId", "event_id"),
                               interval=interval,
                               control_base=control)

    def to_api_payload(self) -> Dict[str, Any]:
        return {
            "eventId": self.event_id,
            "interval": self.interval.to_api_payload(),
            "DERControlBase": self.control_base.to_api_payload(),
        }


@dataclass
class DerProgramModel:
    program_id: str
    default_control: DerControlBase
    events: List[DerControlEvent] = field(default_factory=list)

    @staticmethod
    def from_api_payload(payload: Dict[str, Any]) -> "DerProgramModel":
        container = payload.get("program", payload)
        raw_default = _pick(container, "defaultControl", "default_control", default={})
        raw_events = _pick(container, "events", default=[])
        return DerProgramModel(
            program_id=_pick(container, "programId", "program_id"),
            default_control=DerControlBase.from_api_payload(raw_default),
            events=[DerControlEvent.from_api_payload(x) for x in raw_events],
        )

    def to_api_derp_response(self) -> Dict[str, Any]:
        return {
            "program": {
                "programId": self.program_id,
                "defaultControl": self.default_control.to_api_payload(),
            }
        }

    def to_api_derc_response(self) -> Dict[str, Any]:
        return {"controls": [event.to_api_payload() for event in self.events]}


@dataclass
class MirrorUsageSample:
    sfdi: str
    ts_utc: str
    soc_pct: Optional[float] = None
    p_w: Optional[float] = None
    q_var: Optional[float] = None
    status: Optional[str] = None

    @staticmethod
    def from_api_payload(payload: Dict[str, Any]) -> "MirrorUsageSample":
        return MirrorUsageSample(
            sfdi=payload["sfdi"],
            ts_utc=_pick(payload, "ts", "ts_utc"),
            soc_pct=_pick(payload, "socPct", "soc_pct"),
            p_w=_pick(payload, "pW", "p_w"),
            q_var=_pick(payload, "qVar", "q_var"),
            status=payload.get("status"),
        )

    def to_api_payload(self) -> Dict[str, Any]:
        payload = {
            "sfdi": self.sfdi,
            "ts": self.ts_utc,
            "socPct": self.soc_pct,
            "pW": self.p_w,
            "qVar": self.q_var,
            "status": self.status,
        }
        return {k: v for k, v in payload.items() if v is not None}


def model_to_dict(model: Any) -> Dict[str, Any]:
    return asdict(model)


def create_peak_hours_sample_program() -> DerProgramModel:
    default = DerControlBase(op_mod_connect=True,
                             op_mod_energize=True,
                             op_mod_fixed_pf_inject_w=0.98,
                             op_mod_fixed_pf_absorb_w=0.98,
                             set_grad_w=1500,
                             set_es_delay=0)
    events = [
        DerControlEvent(
            event_id="pre-peak-ramp-up",
            interval=TimeWindow(start_time_utc="2026-04-24T16:30:00Z", duration_sec=1800),
            control_base=DerControlBase(op_mod_connect=True,
                                        op_mod_energize=True,
                                        op_mod_fixed_w=25000,
                                        set_grad_w=1200),
        ),
        DerControlEvent(
            event_id="peak-hold",
            interval=TimeWindow(start_time_utc="2026-04-24T17:00:00Z", duration_sec=7200),
            control_base=DerControlBase(op_mod_fixed_w=40000, set_grad_w=0),
        ),
        DerControlEvent(
            event_id="post-peak-ramp-down",
            interval=TimeWindow(start_time_utc="2026-04-24T19:00:00Z", duration_sec=1800),
            control_base=DerControlBase(op_mod_fixed_w=5000, set_grad_w=1200),
        ),
    ]
    return DerProgramModel(program_id="peak-hours-v1", default_control=default, events=events)


def create_sample_mup() -> MirrorUsageSample:
    return MirrorUsageSample(sfdi="111222333444",
                             ts_utc="2026-04-24T17:15:00Z",
                             soc_pct=62.5,
                             p_w=18750,
                             q_var=1500,
                             status="ON_GRID")
