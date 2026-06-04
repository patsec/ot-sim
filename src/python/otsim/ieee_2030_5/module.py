
"""
The cilent performs two primary actions:

- Initial registration (server must be available first)
    - generate an EC private key 
    - generate a CSR
    - submit the CSR to the server's API (/api/csr/submit) endpoint
    - receive a signed certificate + CA certificate from server
    - save certificate locally
    - register the device via /api/csr/submit endpoint
    - verify registration by fetching the /edev/<n>/rg and checking pin
- Enter & execute primary client loop
    - send / receive data with 2030.5 server
"""

import sys, typing
import xml.etree.ElementTree as ET

import OpenSSL
import threading
import json
import logging
import re
import ssl
import subprocess
import sys
from http.client import HTTPConnection, HTTPSConnection
from pathlib import Path
from typing import Optional, Tuple
from constants import TypeConstants
from client_helper.client import IEEE2030_5_Client

import client_helper.models as m
import time 

import msgbus.envelope as envelope
from msgbus.envelope import Envelope, Point
from msgbus.pusher import Pusher
from msgbus.subscriber import Subscriber

# HACK for generating random device names for testing
import datetime

class IEEE20305Client():
    
    def __init__(self, pub: str, pull: str, el: ET.Element):
        self.name = el.get('name', default='ot-sim-20305-client')
        
        self.pub = pub
        self.pull = pull
        
        # self.device_id = el.findtext('device-id')
        # HACK for generating random device names for testing
        self.device_id = str(hash(datetime.datetime.now()))
        
        self.cert_dir = Path(el.findtext('certificate-directory'))
        self.server_address = el.findtext('server-address')
        self.server_port = el.findtext('server-port')
        
        self.polling_rate = int(el.findtext('polling-rate-seconds'))
        
        self.readings = []
        for elm in el.findall('reading'):
            reading = {
                "description": elm.findtext('description'),
                "type": elm.findtext('reading-type'),
                "tag": elm.findtext('tag'),
                "mrid": "",
            }
            self.readings.append(reading)

        self.subscriber = Subscriber(pub)
        self.pusher = Pusher(pull)
        
        self.running = False
        self.subscriber.add_update_handler(self.listen_msgbus)
        
    def log(self, msg):
        print(f'[IEEE 2030.5 Client] {msg}', flush=True)

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
                        server: str, port: int, sfdi: int) -> str:
        """POST an ``<EndDevice>`` to ``/api/register`` (DMZ).

        Returns the ``Location`` header value (e.g. ``/edev/0``).
        """
        self.log(f"Registering device via DMZ on {server}:{port}")
        body = (
            '<EndDevice xmlns="urn:ieee:std:2030.5:ns">'
            f"<sFDI>{sfdi}</sFDI>"
            "</EndDevice>"
        )

        for include_client_cert in (True, False):
            ctx = self._ssl_ctx(cert_file, key_file, ca_file, include_client_cert=include_client_cert)
            conn = HTTPSConnection(server, port, context=ctx)
            try:
                conn.request(
                    "POST", "/api/register",
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
        self.device_url = self.register_device(self.cert, self.key, self.ca_cert, self.server_address, self.server_port, cert_data["sfdi"])
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
            reading_type = ""
            match reading["type"]:
                case "active-power":
                    reading_type = TypeConstants.ACTIVE_POWER
                case "reactive-power":
                    reading_type = TypeConstants.REACTIVE_POWER
                case "apparent-power":
                    reading_type = TypeConstants.APPARENT_POWER
                case "voltage":
                    reading_type = TypeConstants.VOLTAGE
                case "current":
                    reading_type = TypeConstants.CURRENT
                case "frequency":
                    reading_type = TypeConstants.FREQUENCY
                case "energy-exported":
                    reading_type = TypeConstants.ENERGY_EXPORTED
                case "percentage":
                    reading_type = TypeConstants.PERCENTAGE
                case _:
                    self.log(f"Type not supported")
                    sys.exit()
                    
            mirror_readings.append(
                m.MirrorMeterReading(
                    mRID=mRID,
                    description=reading["description"],
                    ReadingType=reading_type
                )
            )
            
            reading["mrid"] = mRID
            self.readings[i] = reading
            
        status, mup_href = client.create_mirror_usage_point(
            m.MirrorUsagePoint(
                mRID=mup_mrid,
                deviceLFDI=self.lfdi,
                MirrorMeterReading=mirror_readings
            )
        )

        assert status == 201, f"MUP creation failed with status {status}"
        
        return (mup_mrid, mup_href)

    def connect_to_existing_mirror_usage_points(self, client):
        pass
    
    def listen_20305(self):
        while self.running:
            try: 
                controls = self.client.der_control_list()
                if controls: 
                    self.log(controls)
                    points = [...]
                    env = envelope.new_update_envelope(self.name, {'updates': points})
                    self.pusher.push('RUNTIME', env)
            except Exception as e:
                self.log(f'2030.5 poll error: {e}')
            time.sleep(self.polling_rate)
    
    # On update received from zmq
    def listen_msgbus(self, env: Envelope):
        update = envelope.update_from_envelope(env)
        if not update:
            return
        for point in update['updates']:
            reading = next((r for r in self.readings if r['tag'] == point['tag']), None)
            if reading is None:
                continue
        mmr = m.MirrorMeterReading(
            mRID=reading['mrid'],
            Reading=m.Reading(value=int(point))
        )
        self.client.create_mirror_meter_reading(self.mup_href, mmr)
            

    def start(self):
        self.subscriber.start('RUNTIME')

        self.client = self.initialize_client()
        self.mup_mrid, self.mup_href = self.build_mirror_usage_points(self.client)
        
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

    tree = ET.parse(sys.argv[1])
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
        device = IEEE20305Client(pub, pull, client)
        device.start()
        devices.append(device)

    waiter = threading.Event()

    def handler(*_):
        waiter.set()

    waiter.wait()

    for device in devices:
        device.stop()

# HACK 
if __name__ == '__main__':
    main()