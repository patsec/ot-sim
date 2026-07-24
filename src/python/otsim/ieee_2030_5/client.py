
import sys, typing
import xml.etree.ElementTree as ET

import threading
import json
import re
import ssl
import subprocess
import sys
from dataclasses import replace
from http.client import HTTPConnection, HTTPSConnection
from pathlib import Path
from typing import Any, Dict, List, Optional, Set, Tuple
from otsim.ieee_2030_5.client_helper.client import IEEE2030_5_Client
from otsim.ieee_2030_5.client_helper.models.enums import DeviceCategoryType
from otsim.ieee_2030_5.constants import TypeConstants

import otsim.ieee_2030_5.client_helper.models as m
import time

from otsim.msgbus import envelope
from otsim.msgbus.envelope import Envelope
from otsim.msgbus.pusher import Pusher
from otsim.msgbus.subscriber import Subscriber

class IEEE20305Client():
    EXPECTED_WRITABLE_CONTROL_TAGS = {
        'pv:setpoint',
        'storage:setchargedischargerate',
    }
    
    def __init__(self, pub: str, pull: str, el: ET.Element, config_dir: Optional[Path] = None):
        self.name = el.get('name', default='ot-sim-20305-client')
        
        self.pub = pub
        self.pull = pull
        
        self.device_id = el.findtext('device-id')
        
        self.cert_dir = Path(el.findtext('certificate-directory'))
        self.server_address = el.findtext('server-address')
        self.server_port = el.findtext('server-port')
        self.config_dir = config_dir or Path.cwd()
        
        self.site_name = el.findtext('site-name')
        self.device_name = el.findtext('device-name') or None
        self.polling_rate = int(el.findtext('polling-rate-seconds'))
        self.control_setpoint_tag = el.findtext('control-setpoint-tag', default='der.active-power-setpoint')
        
        self.device_categories = []
        for cat in el.findall('category'):
            if cat.text:
                self.device_categories.append(cat.text)

        self.device_category_bitmap_hex: Optional[str] = self._build_device_category_bitmap()
        self.device_category_bitmap_int: Optional[int] = (
            int(self.device_category_bitmap_hex, 16)
            if self.device_category_bitmap_hex
            else None
        )
            
        
        self.readings = []
        self.readings_by_tag: Dict[str, Dict[str, Any]] = {}
        self.local_state: Dict[str, int] = {}
        for i, elm in enumerate(el.findall('reading')):
            tag = elm.findtext('tag')
            local_id = (i + 1).to_bytes(2, 'big')
            multiplier_text = elm.findtext('power-of-ten-multiplier')
            multiplier = int(multiplier_text) if multiplier_text else 0
            reading = {
                "description": elm.findtext('description'),
                "type": elm.findtext('reading-type'),
                "tag": tag,
                "mrid": "",
                "local_id": local_id,
                "power_of_ten_multiplier": multiplier,
            }
            self.readings.append(reading)
            self.readings_by_tag[self._normalize_tag(tag)] = reading
            self.local_state[self._normalize_tag(tag)] = 0

        self.allowed_control_setpoint_tags = set(self.EXPECTED_WRITABLE_CONTROL_TAGS)
        self.control_setpoint_tags = self._resolve_control_setpoint_tags(el)

        self._acknowledged_control_mrid: Optional[bytes] = None
        self._resolved_device_href: Optional[str] = None
        self._logged_missing_setpoint_tags = False
        self._curve_cache: Dict[str, Any] = {}
        self._ramp_state_by_tag: Dict[str, Dict[str, float]] = {}

        self.subscriber = Subscriber(pub)
        self.pusher = Pusher(pull)
        
        self.running = False
        self.log("adding msgbus update and status listeners")
        self.subscriber.add_update_handler(self.listen_msgbus_updates)
        self.subscriber.add_status_handler(self.listen_msgbus_status)
        
    def log(self, msg):
        print(f'[IEEE 2030.5 Client] {msg}', flush=True)

    @staticmethod
    def _normalize_tag(tag: str) -> str:
        return (tag or '').replace('_', '-').strip().lower()

    def _resolve_control_setpoint_tags(self, el: ET.Element) -> List[str]:
        self.log(
            'Allowed writable control setpoints: '
            + ', '.join(sorted(self.allowed_control_setpoint_tags))
        )
        configured: List[str] = []
        for node in el.findall('control-setpoint-tag'):
            if not node.text:
                continue
            configured.extend([part.strip() for part in node.text.split(',') if part.strip()])

        if not configured and self.control_setpoint_tag:
            configured = [self.control_setpoint_tag]

        reading_tag_lookup: Dict[str, str] = {
            self._normalize_tag(reading.get('tag', '')): (reading.get('tag') or '')
            for reading in self.readings
            if reading.get('tag')
        }

        resolved: List[str] = []
        seen: Set[str] = set()

        def _try_add(tag: str) -> None:
            normalized = self._normalize_tag(tag)
            if not normalized or normalized in seen:
                return
            if self.allowed_control_setpoint_tags and normalized not in self.allowed_control_setpoint_tags:
                return
            if normalized not in reading_tag_lookup:
                return
            seen.add(normalized)
            resolved.append(reading_tag_lookup[normalized])

        for tag in configured:
            _try_add(tag)

        # Auto-select writable tags from readings when explicit config did not resolve.
        if not resolved and self.allowed_control_setpoint_tags:
            for normalized, original in reading_tag_lookup.items():
                if normalized in self.allowed_control_setpoint_tags and normalized not in seen:
                    seen.add(normalized)
                    resolved.append(original)

        if resolved:
            self.log(f'Control setpoint publish tags: {", ".join(resolved)}')
        else:
            self.log('No valid writable control setpoint tags resolved; control outputs will not be published')

        return resolved

    def _reading_type_for_name(self, name: str, multiplier: int = 0):
        base_type = None
        match name:
            case "active-power":
                base_type = TypeConstants.ACTIVE_POWER
            case "reactive-power":
                base_type = TypeConstants.REACTIVE_POWER
            case "apparent-power":
                base_type = TypeConstants.APPARENT_POWER
            case "voltage":
                base_type = TypeConstants.VOLTAGE
            case "current":
                base_type = TypeConstants.CURRENT
            case "frequency":
                base_type = TypeConstants.FREQUENCY
            case "energy-exported":
                base_type = TypeConstants.ENERGY_EXPORTED
            case "percentage":
                base_type = TypeConstants.PERCENTAGE
            case _:
                self.log(f"Unsupported reading type '{name}', defaulting to ACTIVE_POWER")
                base_type = TypeConstants.ACTIVE_POWER
        
        return replace(base_type, powerOfTenMultiplier=multiplier)

    @staticmethod
    def _scale_curve_axis(value: Any, multiplier: Any) -> Optional[float]:
        if value is None:
            return None
        try:
            numeric = float(value)
            power = int(multiplier or 0)
            return numeric * (10 ** power)
        except (TypeError, ValueError):
            return None

    def _lookup_measurement(self, reading_type: str, output_tag: Optional[str]) -> Optional[float]:
        desired_domain = None
        if output_tag and ':' in output_tag:
            desired_domain = output_tag.split(':', 1)[0].strip().lower()

        preferred = None
        fallback = None
        for reading in self.readings:
            if reading.get('type') != reading_type:
                continue
            raw_tag = reading.get('tag') or ''
            normalized_tag = self._normalize_tag(raw_tag)
            raw_value = self.local_state.get(normalized_tag)
            if raw_value is None:
                continue
            scaled = self._scale_curve_axis(raw_value, reading.get('power_of_ten_multiplier', 0))
            if scaled is None:
                continue
            fallback = scaled
            if desired_domain and raw_tag.lower().startswith(f'{desired_domain}:'):
                preferred = scaled
                break

        return preferred if preferred is not None else fallback

    @staticmethod
    def _piecewise_linear(points: List[Tuple[float, float]], x_value: float) -> Optional[float]:
        if not points:
            return None

        ordered = sorted(points, key=lambda item: item[0])
        if x_value <= ordered[0][0]:
            return ordered[0][1]
        if x_value >= ordered[-1][0]:
            return ordered[-1][1]

        for idx in range(1, len(ordered)):
            x0, y0 = ordered[idx - 1]
            x1, y1 = ordered[idx]
            if x0 <= x_value <= x1:
                if abs(x1 - x0) < 1e-12:
                    return y1
                ratio = (x_value - x0) / (x1 - x0)
                return y0 + ((y1 - y0) * ratio)
        return ordered[-1][1]

    @staticmethod
    def _percent_to_control_hundredths(percent_value: float) -> float:
        # opModFixedW/MaxLimW values are represented in hundredths-of-percent.
        if abs(percent_value) <= 100.0:
            return percent_value * 100.0
        return percent_value

    def _curve_target_setpoint_w(self, control_base: Any, output_tag: Optional[str]) -> Optional[float]:
        # Active power oriented curve modes supported by this client path.
        curve_modes: List[Tuple[str, str]] = [
            ('opModVoltWatt', 'voltage'),
            ('opModFreqWatt', 'frequency'),
        ]

        for curve_field, reading_type in curve_modes:
            curve_link = getattr(control_base, curve_field, None)
            if curve_link is None:
                continue

            curve = self._get_curve_from_link(curve_link)
            if curve is None:
                continue

            measured_x = self._lookup_measurement(reading_type=reading_type, output_tag=output_tag)
            if measured_x is None:
                continue

            if reading_type == 'voltage':
                v_ref = getattr(curve, 'vRef', None)
                if isinstance(v_ref, int) and v_ref > 0:
                    measured_x = (measured_x / float(v_ref)) * 100.0

            x_multiplier = getattr(curve, 'xMultiplier', 0)
            y_multiplier = getattr(curve, 'yMultiplier', 0)
            points: List[Tuple[float, float]] = []
            for point in (getattr(curve, 'CurveData', []) or []):
                x_scaled = self._scale_curve_axis(getattr(point, 'xvalue', None), x_multiplier)
                y_scaled = self._scale_curve_axis(getattr(point, 'yvalue', None), y_multiplier)
                if x_scaled is None or y_scaled is None:
                    continue
                points.append((x_scaled, y_scaled))

            if not points:
                continue

            y_output = self._piecewise_linear(points, measured_x)
            if y_output is None:
                continue
            return self._percent_to_control_hundredths(y_output)

        return None

    def _extract_control_setpoint_w(self, selected_control: Any, output_tag: Optional[str] = None) -> Optional[float]:
        control_base = getattr(selected_control, 'DERControlBase', None)
        if control_base is None:
            return None

        fixed_w = getattr(control_base, 'opModFixedW', None)
        if fixed_w is not None:
            return float(fixed_w)

        max_lim_w = getattr(control_base, 'opModMaxLimW', None)
        if max_lim_w is not None:
            return float(max_lim_w)

        target_w = getattr(control_base, 'opModTargetW', None)
        if target_w is not None:
            value = getattr(target_w, 'value', None)
            multiplier = getattr(target_w, 'multiplier', 0) or 0
            if value is not None:
                return float(value) * (10 ** multiplier)

        # If direct power setpoints are absent, evaluate active-power curves.
        curve_target = self._curve_target_setpoint_w(control_base, output_tag=output_tag)
        if curve_target is not None:
            return curve_target

        return None

    @staticmethod
    def _control_identifier(control: Any) -> str:
        mrid = getattr(control, 'mRID', None)
        if isinstance(mrid, (bytes, bytearray)):
            return mrid.hex()
        if mrid:
            return str(mrid)
        href = getattr(control, 'href', None)
        return str(href or 'default')

    def _get_curve_from_link(self, curve_link: Any) -> Optional[Any]:
        href = getattr(curve_link, 'href', None)
        if not href:
            return None
        if href in self._curve_cache:
            return self._curve_cache[href]
        try:
            curve = self.client.request(href)
            self._curve_cache[href] = curve
            return curve
        except Exception as exc:
            self.log(f'Unable to fetch DERCurve {href}: {exc}')
            return None

    def _find_curve_ramp_seconds(self, control_base: Any, increasing: bool) -> Optional[float]:
        if control_base is None:
            return None

        curve_fields = (
            'opModVoltWatt',
            'opModWattVar',
            'opModFreqWatt',
            'opModVoltVar',
            'opModWattPF',
        )

        for field_name in curve_fields:
            curve_link = getattr(control_base, field_name, None)
            if curve_link is None:
                continue
            curve = self._get_curve_from_link(curve_link)
            if curve is None:
                continue

            preferred = getattr(curve, 'rampIncTms' if increasing else 'rampDecTms', None)
            if isinstance(preferred, int) and preferred > 0:
                return preferred / 100.0

            fallback = getattr(curve, 'rampPT1Tms', None)
            if isinstance(fallback, int) and fallback > 0:
                return fallback / 100.0

        return None

    @staticmethod
    def _derive_gradient_ramp_seconds(from_value: float, to_value: float, set_grad_w: Any) -> Optional[float]:
        if set_grad_w is None:
            return None
        try:
            grad = float(set_grad_w)
        except (TypeError, ValueError):
            return None
        if grad <= 0:
            return None

        # setGradW is in hundredths-of-percent per second and opModFixedW is
        # represented in hundredths-of-percent, so delta/grad yields seconds.
        delta = abs(to_value - from_value)
        return delta / grad

    def _resolve_transition_seconds(
        self,
        selected_control: Any,
        default_control: Any,
        from_value: float,
        to_value: float,
    ) -> float:
        if abs(to_value - from_value) < 1e-9:
            return 0.0

        control_base = getattr(selected_control, 'DERControlBase', None)

        ramp_tms = getattr(control_base, 'rampTms', None)
        if isinstance(ramp_tms, int) and ramp_tms > 0:
            return ramp_tms / 100.0

        curve_seconds = self._find_curve_ramp_seconds(
            control_base,
            increasing=(to_value >= from_value),
        )
        if curve_seconds is not None:
            return curve_seconds

        for source in (selected_control, default_control):
            service_ramp = getattr(source, 'setESRampTms', None)
            if isinstance(service_ramp, int) and service_ramp > 0:
                return service_ramp / 100.0

            grad_seconds = self._derive_gradient_ramp_seconds(
                from_value=from_value,
                to_value=to_value,
                set_grad_w=getattr(source, 'setGradW', None),
            )
            if grad_seconds is not None:
                return grad_seconds

            soft_grad_seconds = self._derive_gradient_ramp_seconds(
                from_value=from_value,
                to_value=to_value,
                set_grad_w=getattr(source, 'setSoftGradW', None),
            )
            if soft_grad_seconds is not None:
                return soft_grad_seconds

        return 0.0

    def _ramped_setpoint_value(
        self,
        tag: str,
        selected_control: Any,
        default_control: Any,
        target_value: float,
        now_epoch: int,
    ) -> float:
        key = self._normalize_tag(tag)
        state = self._ramp_state_by_tag.get(key)
        control_id = self._control_identifier(selected_control)

        if state is None:
            start_value = float(self.local_state.get(key, target_value))
            transition_seconds = self._resolve_transition_seconds(
                selected_control=selected_control,
                default_control=default_control,
                from_value=start_value,
                to_value=target_value,
            )
            state = {
                'control_id': control_id,
                'start': start_value,
                'target': target_value,
                'start_ts': float(now_epoch),
                'transition_seconds': transition_seconds,
                'last_value': start_value,
            }
            self._ramp_state_by_tag[key] = state

        changed_instruction = (
            state.get('control_id') != control_id
            or abs(float(state.get('target', 0.0)) - target_value) > 1e-9
        )

        if changed_instruction:
            current_value = float(state.get('last_value', state.get('target', target_value)))
            transition_seconds = self._resolve_transition_seconds(
                selected_control=selected_control,
                default_control=default_control,
                from_value=current_value,
                to_value=target_value,
            )
            state.update({
                'control_id': control_id,
                'start': current_value,
                'target': target_value,
                'start_ts': float(now_epoch),
                'transition_seconds': transition_seconds,
            })

        transition_seconds = float(state.get('transition_seconds', 0.0) or 0.0)
        start_value = float(state.get('start', target_value))
        end_value = float(state.get('target', target_value))

        if transition_seconds <= 0.0:
            value = end_value
        else:
            elapsed = max(0.0, float(now_epoch) - float(state.get('start_ts', now_epoch)))
            ratio = min(1.0, elapsed / transition_seconds)
            value = start_value + ((end_value - start_value) * ratio)

        state['last_value'] = value
        self.local_state[key] = int(round(value))
        return value

    def _build_device_category_bitmap(self) -> Optional[str]:
        """Convert device category names to a 4-byte hex bitmap.
        
        Returns hex string like '00800000' for COMBINED_PV_AND_STORAGE (bit 23),
        or None if no categories are configured.
        """
        if not self.device_categories:
            return None
        
        bitmap = 0
        for cat_name in self.device_categories:
            try:
                cat_enum = DeviceCategoryType[cat_name]
                bitmap |= (1 << cat_enum.value)
                self.log(f"Added category {cat_name} (bit {cat_enum.value})")
            except KeyError:
                self.log(f"WARNING: Unknown device category '{cat_name}'")
        
        # Convert to 4-byte big-endian hex
        hex_bitmap = format(bitmap, '08x')
        self.log(f"Device category bitmap: {hex_bitmap}")
        return hex_bitmap

    def generate_private_key(self, device_id: str, output_dir: Path) -> Path:
        """Generate an EC (prime256v1) private key and return its path."""
        key_file = output_dir / f"{device_id}.key"
        if key_file.exists():
            self.log(f"Private key already exists: {str(key_file)}")
            return key_file

        self.log(f"Generating private key for {device_id}")
        result = subprocess.run(
            ["openssl", "ecparam", "-genkey", "-name", "prime256v1", "-out", str(key_file)],
            capture_output=True, text=True,
        )
        if result.returncode != 0:
            raise RuntimeError(f"openssl ecparam failed: {result.stderr}")
        self.log(f"  -> {str(key_file)}")
        return key_file

    def generate_csr(self, device_id: str, key_file: Path, output_dir: Path) -> Path:
        """Generate a CSR with *device_id* as the Common Name."""
        csr_file = output_dir / f"{device_id}.csr"
        if csr_file.exists():
            self.log(f"CSR already exists: {str(csr_file)}")
            return csr_file

        self.log(f"Generating CSR for {device_id}")
        result = subprocess.run(
            ["openssl", "req", "-new", "-key", str(key_file),
            "-out", str(csr_file), "-subj", f"/CN={device_id}"],
            capture_output=True, text=True,
        )
        if result.returncode != 0:
            raise RuntimeError(f"openssl req failed: {result.stderr}")
        self.log(f"  -> {str(csr_file)}")
        return csr_file

    def submit_csr(self, device_id: str, csr_file: Path,
                server: str, port: int,
                use_https: bool = False) -> dict:
        """POST the CSR to ``/api/csr/submit`` and return the JSON response.

        The returned dict has at least:
        ``certificate``, ``ca_certificate``, ``lfdi``, ``sfdi``.
        """
        self.log(f"Submitting CSR to {server}:{port}")
        payload = json.dumps({"device_id": device_id, "csr": csr_file.read_text()})

        if use_https:
            ctx = ssl.create_default_context()
            ctx.check_hostname = False
            ctx.verify_mode = ssl.CERT_NONE
            conn = HTTPSConnection(server, port, context=ctx)
        else:
            conn = HTTPConnection(server, port)
        while True:
            try:
                conn.request(
                    "POST", "/api/csr/submit",
                    body=payload,
                    headers={"Content-Type": "application/json",
                            "Content-Length": str(len(payload))},
                )
                resp = conn.getresponse()
                body = resp.read().decode("utf-8")

                if resp.status != 200:
                    raise RuntimeError(f"Server returned {resp.status}: {body}")

                data = json.loads(body)
                if not data.get("success"):
                    raise RuntimeError(f"Server error: {data.get('error')}")
                self.log(f"Certificate signed - LFDI={data['lfdi']} SFDI={data['sfdi']}")
                return data
            except Exception:
                continue
            finally:
                conn.close()

    def save_certificates(self, device_id: str, cert_data: dict,
                        output_dir: Path) -> Tuple[Path, Path]:
        """Write signed cert and CA cert to *output_dir*."""
        cert_file = output_dir / f"{device_id}.crt"
        ca_file = output_dir / "ca.crt"

        cert_file.write_text(cert_data["certificate"])
        ca_file.write_text(cert_data["ca_certificate"])

        self.log(f"Certificate saved: {cert_file}")
        self.log(f"CA cert saved:     {ca_file}")
        return cert_file, ca_file

    def _ssl_ctx(self, cert_file: Path, key_file: Path, ca_file: Path,
                include_client_cert: bool = True) -> ssl.SSLContext:
        ctx = ssl.SSLContext(ssl.PROTOCOL_TLS_CLIENT)
        ctx.check_hostname = False
        ctx.verify_mode = ssl.CERT_OPTIONAL
        ctx.load_verify_locations(cafile=str(ca_file))
        if include_client_cert:
            ctx.load_cert_chain(certfile=str(cert_file), keyfile=str(key_file))
        return ctx

    def _is_certificate_unknown(self, exc: ssl.SSLError) -> bool:
        return "CERTIFICATE_UNKNOWN" in str(exc).upper()

    def register_device(self, cert_file: Path, key_file: Path, ca_file: Path,
                        server: str, port: int, sfdi: int,
                        name: Optional[str] = None) -> str:
        """POST an ``<EndDevice>`` to ``/api/register`` (DMZ).

        Returns the ``Location`` header value (e.g. ``/edev/0``).
        """
        self.log(f"Registering device via DMZ on {server}:{port}")
        
        # Build the registration XML with device category bitmap
        dev_cat_hex = self.device_category_bitmap_hex
        if dev_cat_hex:
            body = (
                '<EndDevice xmlns="urn:ieee:std:2030.5:ns">'
                f"<sFDI>{sfdi}</sFDI>"
                f"<deviceCategory>{dev_cat_hex}</deviceCategory>"
                "</EndDevice>"
            )
        else:
            body = (
                '<EndDevice xmlns="urn:ieee:std:2030.5:ns">'
                f"<sFDI>{sfdi}</sFDI>"
                "</EndDevice>"
            )

        # Build query string — site and optional friendly name
        params: list[str] = []
        if self.site_name:
            params.append(f"site={self.site_name}")
        if name:
            import urllib.parse
            params.append(f"name={urllib.parse.quote(name, safe='')}")
        path = "/api/register" + ("?" + "&".join(params) if params else "")

        for include_client_cert in (True, False):
            ctx = self._ssl_ctx(cert_file, key_file, ca_file, include_client_cert=include_client_cert)
            conn = HTTPSConnection(server, port, context=ctx)
            try:
                conn.request(
                    "POST", path,
                    body=body,
                    headers={"Content-Type": "application/sep+xml",
                            "Content-Length": str(len(body))},
                )
                resp = conn.getresponse()
                resp_body = resp.read().decode("utf-8")

                if resp.status not in (200, 201):
                    raise RuntimeError(f"Registration failed ({resp.status}): {resp_body}")

                location = resp.headers.get("Location", "")
                self.log(f"Device registered - Location: {location}")
                return location
            except ssl.SSLError as exc:
                if include_client_cert and self._is_certificate_unknown(exc):
                    self.log("Server rejected the presented client cert during DMZ registration; retrying without a client cert")
                    continue
                raise
            finally:
                conn.close()

        raise RuntimeError("Registration failed without a usable TLS mode")

    def verify_registration(self, cert_file: Path, key_file: Path, ca_file: Path,
                            server: str, port: int,
                            device_href: str,
                            pin: Optional[int] = None) -> Tuple[bool, Optional[int]]:
        """Fetch the Registration resource and check the PIN.

        Returns ``(verified, server_pin)``.
        """
        self.log("Verifying registration")
        href_candidates = [f"{device_href}/rg"]
        if "_" in device_href:
            href_candidates.append(f"{device_href}_rg")

        for reg_href in href_candidates:
            for include_client_cert in (True, False):
                ctx = self._ssl_ctx(cert_file, key_file, ca_file, include_client_cert=include_client_cert)
                conn = HTTPSConnection(server, port, context=ctx)
                try:
                    conn.request("GET", reg_href)
                    resp = conn.getresponse()
                    reg_data = resp.read().decode("utf-8")

                    if resp.status != 200:
                        continue

                    match = re.search(r"<pIN>(\d+)</pIN>", reg_data)
                    server_pin: Optional[int] = int(match.group(1)) if match else None

                    if server_pin is None:
                        self.log("<pIN> not found in registration response")
                        return False, None

                    if pin is not None:
                        ok = server_pin == pin
                        if ok:
                            self.log("PIN verified")
                        else:
                            self.log(f"PIN mismatch - expected {pin}, got {server_pin}")
                        return ok, server_pin

                    self.log(f"Server-assigned PIN: {server_pin}")
                    return True, server_pin
                except ssl.SSLError as exc:
                    if include_client_cert and self._is_certificate_unknown(exc):
                        self.log("Server rejected the presented client cert while verifying registration; retrying without a client cert")
                        continue
                    raise
                finally:
                    conn.close()

        self.log("Could not fetch registration")
        return False, None
    
    def initialize_client(self):
        # 1. Create key
        self.key = self.generate_private_key(self.device_id, self.cert_dir)
        # 2. Create CSR
        self.csr_file = self.generate_csr(self.device_id, self.key, self.cert_dir)
        # 3. Submit CSR to server
        cert_data = self.submit_csr(self.device_id, self.csr_file, self.server_address, self.server_port, True)
        # 4. Save certs
        self.cert, self.ca_cert = self.save_certificates(self.device_id, cert_data, self.cert_dir)
        # 5. Register via DMZ
        self.device_url = self.register_device(
            self.cert, self.key, self.ca_cert,
            self.server_address, self.server_port,
            cert_data["sfdi"],
            name=self.device_name,
        )
        # 6. Verify registration
        verified, self.pin = self.verify_registration(self.cert, self.key, self.ca_cert, self.server_address, self.server_port, self.device_url)
        
        self.lfdi = cert_data["lfdi"]
        self.sfdi = cert_data["sfdi"]
        
        self.log(f"""\nSUMMARY
Device ID:                   {self.device_id}
LFDI:                        {self.lfdi}
SFDI:                        {self.sfdi}
Key file:                    {self.key}
CSR file:                    {self.csr_file}
Certificate file:            {self.cert}
Certificate Authority file:  {self.ca_cert}
Device URL:                  {self.device_url}
Server PIN:                  {self.pin}
Verified?                    {verified}
Full certificate data:\n{cert_data}\nEND SUMMARY\n""")
        
        # Create client & query device capability, required before using client
        client = IEEE2030_5_Client(
            cafile=self.ca_cert,
            server_hostname=self.server_address,
            keyfile=self.key,
            certfile=self.cert,
            server_ssl_port=self.server_port,
            debug=True
        )
        client.device_capability()
        
        return client
        
    def new_uuid(self, client):
        return client.new_uuid().replace("-", "")

    def build_mirror_usage_points(self, client):
        mup_mrid = self.new_uuid(client)
        mirror_readings = []
        for (i, reading) in enumerate(self.readings):
            
            mRID = self.new_uuid(client)
            reading_type = self._reading_type_for_name(reading["type"], reading.get("power_of_ten_multiplier", 0))
                    
            mirror_readings.append(
                m.MirrorMeterReading(
                    mRID=mRID,
                    lastUpdateTime=int(time.time()),
                    # Use the configured tag as canonical signal identity so
                    # the dashboard can render per-signal cards/charts.
                    description=reading["tag"] or reading["description"],
                    Reading=m.Reading(localID=reading["local_id"], value=0),
                    ReadingType=reading_type
                )
            )
            
            reading["mrid"] = mRID
            self.readings[i] = reading
        
        mup = m.MirrorUsagePoint(mRID=mup_mrid,
                                 deviceLFDI=self.lfdi,
                                 MirrorMeterReading=mirror_readings)
        status, mup_href = client.create_mirror_usage_point(mup)
        assert status in (200, 201), f"MUP creation failed with status {status}"

        return (mup_mrid, mup_href)

    def build_batched_mirror_usage_point(self):
        now_epoch = int(time.time())
        mirror_readings = []
        for reading in self.readings:
            normalized_tag = self._normalize_tag(reading['tag'])
            value = int(self.local_state.get(normalized_tag, 0))
            mirror_readings.append(
                m.MirrorMeterReading(
                    lastUpdateTime=now_epoch,
                    description=reading['tag'] or reading['description'],
                    Reading=m.Reading(localID=reading['local_id'], value=value),
                    ReadingType=self._reading_type_for_name(reading['type'], reading.get('power_of_ten_multiplier', 0)),
                )
            )

        return m.MirrorUsagePoint(
            deviceLFDI=self.lfdi,
            postRate=self.polling_rate,
            MirrorMeterReading=mirror_readings,
        )

    def connect_to_existing_mirror_usage_points(self, client):
        pass

    def _resolve_device_href(self) -> str:
        # Prefer the registration Location if it is valid.
        if getattr(self, 'device_url', None):
            try:
                device = self.client.end_device_by_href(self.device_url)
                if getattr(device, 'FunctionSetAssignmentsListLink', None):
                    return self.device_url
            except Exception:
                pass

        # Discover from EndDeviceList and match on SFDI when possible.
        try:
            end_device_list = self.client.end_devices()
            end_devices = list(getattr(end_device_list, 'EndDevice', []) or [])
            my_sfdi = str(getattr(self, 'sfdi', ''))

            for end_device in end_devices:
                href = getattr(end_device, 'href', None)
                if href and str(getattr(end_device, 'sFDI', '')) == my_sfdi:
                    return href
        except Exception:
            pass

        # Our EndDevice is not on the server (e.g. after a server restart that
        # wiped in-memory state). Re-register so the server creates it again.
        self.log('EndDevice not found on server — re-registering…')
        try:
            self.device_url = self.register_device(
                self.cert, self.key, self.ca_cert,
                self.server_address, self.server_port,
                self.sfdi,
            name=self.device_name,
        )
            self.log(f'Re-registered EndDevice at {self.device_url}')
            # Rebuild MUP so the server maps telemetry to the new EndDevice entry.
            self.mup_mrid, self.mup_href = self.build_mirror_usage_points(self.client)
            self.log(f'Rebuilt MUP: mrid={self.mup_mrid}, href={self.mup_href}')
            return self.device_url
        except Exception as exc:
            self.log(f'Re-registration failed: {exc}')

        raise RuntimeError('Unable to resolve an EndDevice href for control polling')
    
    def listen_20305(self):
        while self.running:
            try:
                now_epoch = int(time.time())
                if not self._resolved_device_href:
                    self._resolved_device_href = self._resolve_device_href()

                device = self.client.end_device_by_href(self._resolved_device_href)

                controls_with_primacy, default_control = self.client.der_controls_across_programs(device)
                selected_control = self.client.select_active_control(
                    controls=None,
                    default_control=default_control,
                    current_time=now_epoch,
                    device_category_bitmap=self.device_category_bitmap_int,
                    controls_with_primacy=controls_with_primacy,
                )

                if selected_control is not None:
                    ctrl_mrid = getattr(selected_control, 'mRID', None)
                    reply_to = getattr(selected_control, 'replyTo', None)
                    rr = getattr(selected_control, 'responseRequired', b'\x00')
                    requires_start_ack = isinstance(rr, bytes) and len(rr) > 0 and (rr[-1] & 0x02)
                    if (
                        ctrl_mrid and reply_to and requires_start_ack
                        and ctrl_mrid != self._acknowledged_control_mrid
                    ):
                        self.client.post_event_response(reply_to, ctrl_mrid, self.lfdi, status=2)
                        self._acknowledged_control_mrid = ctrl_mrid

                    if self.control_setpoint_tags:
                        update_points = []
                        for tag in self.control_setpoint_tags:
                            setpoint = self._extract_control_setpoint_w(selected_control, output_tag=tag)
                            if setpoint is None:
                                continue
                            update_points.append({
                                'tag': tag,
                                'value': self._ramped_setpoint_value(
                                    tag=tag,
                                    selected_control=selected_control,
                                    default_control=default_control,
                                    target_value=setpoint,
                                    now_epoch=now_epoch,
                                ),
                                'ts': now_epoch,
                            })

                        if update_points:
                            runtime_update = envelope.new_update_envelope(self.name, {'updates': update_points})
                            self.pusher.push('RUNTIME', runtime_update)
                        elif not self._logged_missing_setpoint_tags:
                            self.log('Skipping control output publish because selected control has no publishable setpoint value')
                            self._logged_missing_setpoint_tags = True
                    elif not self._logged_missing_setpoint_tags:
                            self.log('Skipping control output publish because no allowed setpoint tags are configured')
                            self._logged_missing_setpoint_tags = True

                telemetry = self.build_batched_mirror_usage_point()
                status, mup_href = self.client.create_mirror_usage_point(telemetry)
                if status == 403:
                    # Server rejected telemetry — our EndDevice registration was
                    # lost (e.g. server restart). Re-register on the next loop.
                    self.log("MUP rejected with 403 — EndDevice not enrolled; forcing re-registration.")
                    self._resolved_device_href = None
                    self.device_url = None
                elif status in (200, 201) and mup_href:
                    self.mup_href = mup_href
            except Exception as e:
                # Reset resolved href on device-shape or missing-resource failures.
                if 'FunctionSetAssignmentsListLink' in str(e) or '404' in str(e):
                    self._resolved_device_href = None
                import traceback
                self.log(f'2030.5 poll error: {e}')
                self.log(f'Exception type: {type(e).__name__}')
                self.log(traceback.format_exc())

            time.sleep(self.polling_rate)
    
    # On update received from zmq
    def listen_msgbus_updates(self, env: Envelope):
        update = envelope.update_from_envelope(env)
        if not update:
            return
        self._apply_points(update['updates'])

    # On status received from zmq
    def listen_msgbus_status(self, env: Envelope):
        status = envelope.status_from_envelope(env)
        if not status:
            return
        self._apply_points(status['measurements'])

    def _apply_points(self, points) -> None:
        for point in points:
            normalized_tag = self._normalize_tag(point.get('tag'))
            reading = self.readings_by_tag.get(normalized_tag)
            if reading is None:
                continue
            try:
                value = int(round(float(point.get('value', 0))))
            except (TypeError, ValueError):
                continue
            self.local_state[normalized_tag] = value


    def start(self):
        self.subscriber.start('RUNTIME')

        self.client = self.initialize_client()
        
        self.mup_mrid, self.mup_href = self.build_mirror_usage_points(self.client)
        self.log(f"mrid {self.mup_mrid}, mup {self.mup_href}")
        
        
        self.running = True
        self.poll_thread = threading.Thread(target=self.listen_20305, daemon=True)
        self.poll_thread.start()
    
    def stop(self):
        self.running = False
        self.poll_thread.join(self.polling_rate)
        self.subscriber.stop()

def main():
    if len(sys.argv) < 2:
        print('no config file provided')
        sys.exit(1)

    config_path = Path(sys.argv[1]).resolve()
    tree = ET.parse(config_path)
    root = tree.getroot()
    assert root.tag == 'ot-sim'

    mb = root.find('message-bus')

    if mb:
        pub  = mb.findtext('pub-endpoint')
        pull = mb.findtext('pull-endpoint')
    else:
        pub  = 'tcp://127.0.0.1:5678'
        pull = 'tcp://127.0.0.1:1234'

    devices: typing.List[IEEE20305Client] = []
    
    for client in root.findall('ieee20305-client'):
        device = IEEE20305Client(pub, pull, client, config_dir=config_path.parent)
        device.start()
        devices.append(device)

    waiter = threading.Event()

    def handler(*_):
        waiter.set()

    waiter.wait()

    for device in devices:
        device.stop()
