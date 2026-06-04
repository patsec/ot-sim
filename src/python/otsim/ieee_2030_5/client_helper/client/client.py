from __future__ import annotations

import atexit
import http
import logging
import ssl
import threading
import time
import xml.dom.minidom
from http.client import HTTPSConnection
from os import PathLike
from pathlib import Path
from threading import Timer
from typing import Dict, Optional, Tuple, Any

import werkzeug.middleware.lint
import xsdata

import client_helper.utils as utils
import client_helper.utils.tls_wrapper as tls

_log = logging.getLogger(__name__)
_log_req_resp = logging.getLogger(__name__ + ".request")


class IEEE2030_5_Client:
    clients: set[IEEE2030_5_Client] = set()

    # noinspection PyUnresolvedReferences
    def __init__(self,
                 cafile: PathLike,
                 server_hostname: str,
                 keyfile: PathLike,
                 certfile: PathLike,
                 server_ssl_port: Optional[int] = 443,
                 debug: bool = True):

        cafile = cafile if isinstance(cafile, PathLike) else Path(cafile)
        keyfile = keyfile if isinstance(keyfile, PathLike) else Path(keyfile)
        certfile = certfile if isinstance(certfile, PathLike) else Path(certfile)

        self._key = keyfile
        self._cert = certfile
        self._ca = cafile
        self._server_hostname = server_hostname
        self._server_ssl_port = server_ssl_port

        assert cafile.exists(), f"cafile doesn't exist ({cafile})"
        assert keyfile.exists(), f"keyfile doesn't exist ({keyfile})"
        assert certfile.exists(), f"certfile doesn't exist ({certfile})"

        self._using_client_cert = True
        self._ssl_context = self._build_ssl_context(include_client_cert=True)
        self._http_conn = self._new_http_conn(include_client_cert=True)
        self._device_cap: Optional[Any] = None
        self._mup: Optional[Any] = None
        self._upt: Optional[Any] = None
        self._edev: Optional[Any] = None
        self._end_devices: Optional[Any] = None
        self._fsa_list: Optional[Any] = None
        self._debug = debug
        self._dcap_poll_rate: int = 0
        self._dcap_timer: Optional[Timer] = None
        self._disconnect: bool = False
        self._tls = tls.OpensslWrapper

        IEEE2030_5_Client.clients.add(self)

    @property
    def http_conn(self) -> HTTPSConnection:
        if self._http_conn.sock is None:
            self._http_conn.connect()
        return self._http_conn

    def _build_ssl_context(self, include_client_cert: bool) -> ssl.SSLContext:
        ssl_context = ssl.SSLContext(ssl.PROTOCOL_TLS_CLIENT)
        ssl_context.check_hostname = False
        ssl_context.verify_mode = ssl.CERT_OPTIONAL
        ssl_context.load_verify_locations(cafile=self._ca)
        if include_client_cert:
            ssl_context.load_cert_chain(certfile=self._cert, keyfile=self._key)
        return ssl_context

    def _new_http_conn(self, include_client_cert: bool) -> HTTPSConnection:
        self._using_client_cert = include_client_cert
        self._ssl_context = self._build_ssl_context(include_client_cert=include_client_cert)
        return HTTPSConnection(host=self._server_hostname,
                               port=self._server_ssl_port,
                               context=self._ssl_context)

    def _retry_without_client_cert(self, exc: ssl.SSLError) -> bool:
        if not self._using_client_cert:
            return False
        if "CERTIFICATE_UNKNOWN" not in str(exc).upper():
            return False

        _log.warning("Server rejected the presented client cert; retrying without a client cert")
        self._http_conn.close()
        self._http_conn = self._new_http_conn(include_client_cert=False)
        return True

    def register_end_device(self) -> str:
        lfid = utils.get_lfdi_from_cert(self._cert)
        sfid = utils.get_sfdi_from_lfdi(lfid)
        response = self.__post__(dcap.EndDeviceListLink.href,
                                 data=f'<EndDevice xmlns="urn:ieee:std:2030.5:ns"><sFDI>{sfid}</sFDI></EndDevice>')
        print(response)

        if response.status in (200, 201):
            return response.headers.get("Location")

        raise werkzeug.exceptions.Forbidden()

    def get(self, href):
        return self.__get_request__(href)

    def is_end_device_registered(self, end_device: Any, pin: int) -> bool:
        reg = self.registration(end_device)
        return reg.pIN == pin

    def new_uuid(self, url: str = "/uuid") -> str:
        res = self.__get_request__(url)
        return res

    def end_devices(self) -> Any:
        if not self._device_cap:
            self.device_capability()

        self._end_devices = self.__get_request__(self._device_cap.EndDeviceListLink.href)
        return self._end_devices

    def end_device(self, index: Optional[int] = 0) -> Any:
        if not self._end_devices:
            self.end_devices()

        return self._end_devices.EndDevice[index]

    def self_device(self) -> Any:
        if not self._device_cap:
            self.device_capability()

        return self.__get_request__(self._device_cap.SelfDeviceLink.href)

    def function_set_assignment_list(self,
                                     edev_index: Optional[int] = 0
                                     ) -> Any:
        fsa_list = self.__get_request__(
            self.end_device(edev_index).FunctionSetAssignmentsListLink.href)
        return fsa_list

    def function_set_assignment(self,
                                edev_index: Optional[int] = 0,
                                fsa_index: Optional[int] = 0) -> Any:
        fsa_list = self.function_set_assignment_list(edev_index)
        return fsa_list.FunctionSetAssignments[fsa_index]

    def der_list(self, edev_index: Optional[int] = 0) -> Any:
        der_list = self.__get_request__(self.end_device(edev_index).DERListLink.href)
        return der_list

    def poll_timer(self, fn, args):
        if not self._disconnect:
            _log.debug(threading.currentThread().name)
            fn(args)
            threading.currentThread().join()

    def device_capability(self, url: str = "/dcap") -> Any:
        self._device_cap: Any = self.__get_request__(url)
        if self._device_cap.pollRate is not None:
            self._dcap_poll_rate = self._device_cap.pollRate
        else:
            self._dcap_poll_rate = 600

        _log.debug(f"devcap id {id(self._device_cap)}")
        _log.debug(threading.currentThread().name)
        _log.debug(f"DCAP: Poll rate: {self._dcap_poll_rate}")
        # self._dcap_timer = Timer(self._dcap_poll_rate, self.poll_timer, (self.device_capability, url))
        # self._dcap_timer.start()
        return self._device_cap

    def time(self) -> Any:
        timexml = self.__get_request__(self._device_cap.TimeLink.href)
        return timexml

    def der_program_list(self,
                         edev_index: Optional[int] = 0,
                         fsa_index: Optional[int] = 0) -> Any:
        fsa = self.function_set_assignment(edev_index, fsa_index)
        derp_list = self.__get_request__(fsa.DERProgramListLink.href)
        return derp_list

    def der_program(self,
                    edev_index: Optional[int] = 0,
                    fsa_index: Optional[int] = 0,
                    derp_index: Optional[int] = 0) -> Any:
        derp_list = self.der_program_list(edev_index, fsa_index)
        return derp_list.DERProgram[derp_index]

    def der_control_list(self,
                         edev_index: Optional[int] = 0,
                         fsa_index: Optional[int] = 0,
                         derp_index: Optional[int] = 0) -> Any:
        der_program = self.der_program(edev_index, fsa_index, derp_index)
        control_list_link = getattr(der_program, "DERControlListLink", None)
        if control_list_link is None or getattr(control_list_link, "href", None) is None:
            return None
        return self.__get_request__(control_list_link.href)

    def default_der_control(self,
                            edev_index: Optional[int] = 0,
                            fsa_index: Optional[int] = 0,
                            derp_index: Optional[int] = 0) -> Any:
        der_program = self.der_program(edev_index, fsa_index, derp_index)
        default_control_link = getattr(der_program, "DefaultDERControlLink", None)
        if default_control_link is None or getattr(default_control_link, "href", None) is None:
            return None
        return self.__get_request__(default_control_link.href)

    @staticmethod
    def select_active_control(controls: Any,
                              default_control: Any,
                              current_time: Optional[int] = None) -> Any:
        if current_time is None:
            current_time = int(time.time())

        control_items = []
        if controls is None:
            control_items = []
        elif isinstance(controls, list):
            control_items = controls
        else:
            control_items = getattr(controls, "DERControl", []) or []

        active_controls = []
        for control in control_items:
            status = getattr(getattr(control, "EventStatus", None), "currentStatus", None)
            if status in (2, 3, 4):
                continue

            interval = getattr(control, "interval", None)
            if interval is None:
                if status == 1:
                    active_controls.append(control)
                continue

            start = getattr(interval, "start", None)
            duration = getattr(interval, "duration", None)
            if start is None or duration is None:
                continue

            if start <= current_time < (start + duration):
                active_controls.append(control)

        if active_controls:
            active_controls.sort(key=lambda ctl: (
                getattr(ctl, "creationTime", 0) or 0,
                getattr(getattr(ctl, "interval", None), "start", 0) or 0,
            ), reverse=True)
            return active_controls[0]

        return default_control

    def mirror_usage_point_list(self) -> Any:
        self._mup = self.__get_request__(self._device_cap.MirrorUsagePointListLink.href)
        return self._mup

    def usage_point_list(self) -> Any:
        self._upt = self.__get_request__(self._device_cap.UsagePointListLink.href)
        return self._upt

    def registration(self, end_device: Any) -> Any:
        reg = self.__get_request__(end_device.RegistrationLink.href)
        return reg

    def timelink(self):
        if self._device_cap is None:
            raise ValueError("Request device capability first")
        return self.__get_request__(url=self._device_cap.TimeLink.href)

    def disconnect(self):
        self._disconnect = True
        if self._dcap_timer:
            self._dcap_timer.cancel()
        IEEE2030_5_Client.clients.remove(self)

    def request(self, endpoint: str, body: dict = None, method: str = "GET", headers: dict = None):

        if method.upper() == 'GET':
            return self.__get_request__(endpoint, body, headers=headers)

        if method.upper() == 'POST':
            print("Doing post")
            return self.__post__(endpoint, body, headers=headers)

    def create_mirror_usage_point(self, mirror_usage_point: Any) -> Tuple[int, str]:
        data = utils.dataclass_to_xml(mirror_usage_point)
        resp = self.__post__(self._device_cap.MirrorUsagePointListLink.href, data=data)
        return resp.status, resp.headers['Location']

    def create_mirror_meter_reading(self, mirror_usage_point_href: str,
                                    mirror_meter_reading: Any) -> Tuple[int, str]:
        data = utils.dataclass_to_xml(mirror_meter_reading)
        resp = self.__post__(mirror_usage_point_href, data=data)
        return resp.status, resp.headers['Location']

    def post(self, url: str, data: Any, headers: Optional[Dict[str, str]] = None):
        response = self.__post__(url, data, headers=headers)

    def __post__(self, url: str, data=None, headers: Optional[Dict[str, str]] = None):
        if not headers:
            headers = {'Content-Type': 'text/xml'}

        self.http_conn.request(method="POST", headers=headers, url=url, body=data)
        response = self._http_conn.getresponse()
        # response_data = response.read().decode("utf-8")

        return response

    def __get_request__(self, url: str, body=None, headers: dict = None):
        if headers is None:
            headers = {"Connection": "keep-alive", "keep-alive": "timeout=30, max=1000"}

        if self._debug:
            print(f"----> GET REQUEST")
            print(f"url: {url} body: {body}")
        try:
            self.http_conn.request(method="GET", url=url, body=body, headers=headers)
            response = self._http_conn.getresponse()
        except ssl.SSLError as exc:
            if not self._retry_without_client_cert(exc):
                raise
            self.http_conn.request(method="GET", url=url, body=body, headers=headers)
            response = self._http_conn.getresponse()
        response_data = response.read().decode("utf-8")
        print(response.headers)

        response_obj = None
        try:
            response_obj = utils.xml_to_dataclass(response_data)
            resp_xml = xml.dom.minidom.parseString(response_data)
            if resp_xml and self._debug:
                print(f"<---- GET RESPONSE")
                print(f"{response_data}")    # toprettyxml()}")

        except xsdata.exceptions.ParserError as ex:
            if self._debug:
                print(f"<---- GET RESPONSE")
                print(f"{response_data}")
            response_obj = response_data

        return response_obj

    def __close__(self):
        self._http_conn.close()
        self._ssl_context = None
        self._http_conn = None

    def put(self, url: str, data: Any, headers: Optional[Dict[str, str]] = None):
        response = self.__put__(url, data, headers=headers)

    def __put__(self, url: str, data: Any, headers: Optional[Dict[str, str]] = None):
        if not headers:
            headers = {'Content-Type': 'text/xml'}

        if self._debug:
            _log_req_resp.debug(f"----> PUT REQUEST\nurl: {url}\nbody: {data}")

        try:
            self.http_conn.request(method="PUT", headers=headers, url=url, body=data)
        except http.client.CannotSendRequest as ex:
            self.http_conn.close()
            _log.debug("Reconnecting to server")
            self.http_conn.request(method="PUT", headers=headers, url=url, body=data)
        except ssl.SSLError as exc:
            if not self._retry_without_client_cert(exc):
                raise
            self.http_conn.request(method="PUT", headers=headers, url=url, body=data)

        response = self._http_conn.getresponse()
        return response

    def __post__(self, url: str, data=None, headers: Optional[Dict[str, str]] = None):
        if not headers:
            headers = {'Content-Type': 'text/xml'}

        if self._debug:
            _log_req_resp.debug(f"----> POST REQUEST\nurl: {url}\nbody: {data}")

        try:
            self.http_conn.request(method="POST", headers=headers, url=url, body=data)
            response = self._http_conn.getresponse()
        except ssl.SSLError as exc:
            if not self._retry_without_client_cert(exc):
                raise
            self.http_conn.request(method="POST", headers=headers, url=url, body=data)
            response = self._http_conn.getresponse()
        response_data = response.read().decode("utf-8")
        # response_data = response.read().decode("utf-8")
        if response_data and self._debug:
            _log_req_resp.debug(f"<---- POST RESPONSE\n{response_data}")

        return response


# noinspection PyTypeChecker
def __release_clients__():
    for x in IEEE2030_5_Client.clients:
        x.__close__()
    IEEE2030_5_Client.clients = None


atexit.register(__release_clients__)

#
# ssl_context = ssl.create_default_context(cafile=str(SERVER_CA_CERT))
#
#
# con = HTTPSConnection("me.com", 8000,
#                       key_file=str(KEY_FILE),
#                       cert_file=str(CERT_FILE),
#                       context=ssl_context)
# con.request("GET", "/dcap")
# print(con.getresponse().read())
# con.close()

if __name__ == '__main__':
    SERVER_CA_CERT = Path("~/tls/certs/ca.pem").expanduser().resolve()
    KEY_FILE = Path("~/tls/private/dev1.pem").expanduser().resolve()
    CERT_FILE = Path("~/tls/certs/dev1.pem").expanduser().resolve()

    headers = {'Connection': 'Keep-Alive', 'Keep-Alive': "max=1000,timeout=30"}

    h = IEEE2030_5_Client(cafile=SERVER_CA_CERT,
                          server_hostname="127.0.0.1",
                          server_ssl_port=8443,
                          keyfile=KEY_FILE,
                          certfile=CERT_FILE,
                          debug=True)
    # h2 = IEEE2030_5_Client(cafile=SERVER_CA_CERT, server_hostname="me.com", ssl_port=8000,
    #                        keyfile=KEY_FILE, certfile=KEY_FILE)
    dcap = h.device_capability()
    end_devices = h.end_devices()

    if not end_devices.all > 0:
        print("registering end device.")
        ed_href = h.register_end_device()
    my_ed = h.end_devices()
    my_fsa = h.function_set_assignment()
    my_program = h.der_program()

    # ed = h.end_devices()[0]
    # resp = h.request("/dcap", headers=headers)
    # print(resp)
    # resp = h.request("/dcap", headers=headers)
    # print(resp)
    #dcap = h.device_capability()
    # get device list
    #dev_list = h.request(dcap.EndDeviceListLink.href).EndDevice

    #ed = h.request(dev_list[0].href)
    #print(ed)
    #
    # print(dcap.mirror_usage_point_list_link)
    # # print(h.request(dcap.mirror_usage_point_list_link.href))
    # print(h.request("/dcap", method="post"))

    # tl = h.timelink()
    #print(IEEE2030_5_Client.clients)
