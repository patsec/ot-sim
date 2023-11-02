# -*- coding: utf-8 -*-
import enum, uuid

from collections import OrderedDict, defaultdict
from typing      import NamedTuple, List, Dict

import helics as h


class HelicsException(h.HelicsException):
    pass


class DataType(enum.Enum):
    string         = h.helics_data_type_string
    double         = h.helics_data_type_double
    int            = h.helics_data_type_int
    complex        = h.helics_data_type_complex
    vector         = h.helics_data_type_vector
    complex_vector = h.helics_data_type_complex_vector
    named_point    = h.helics_data_type_named_point
    boolean        = h.helics_data_type_boolean
    time           = h.helics_data_type_time
    raw            = h.helics_data_type_raw
    any            = h.helics_data_type_any


class Subscription(NamedTuple):
    name: str
    type: DataType


class Publication(NamedTuple):
    name: str
    type: DataType


class GlobalPublication(NamedTuple):
    name: str
    type: DataType


class Endpoint(NamedTuple):
    name: str
    type: str = ""


class GlobalEndpoint(NamedTuple):
    name: str
    type: str = ""


class Values(NamedTuple):
    send: Dict = {}
    recv: Dict = {}


class Message(NamedTuple):
    data:                 str
    time:                 float = 0
    source:               str   = ""
    destination:          str   = ""
    original_source:      str   = ""
    original_destination: str   = ""


class Messages(NamedTuple):
    send: Dict[str, List[Message]] = defaultdict(list)
    recv: Dict[str, List[Message]] = defaultdict(list)


class HelicsFederate(object):
    publications:  List[Publication]  = []
    subscriptions: List[Subscription] = []
    endpoints:     List[Endpoint]     = []

    federate_info_core_name:        str = ""
    federate_info_core_init_string: str = "--federates=1"
    federate_info_core_type:        str = "zmq"

    federate_info_time_delta: float = 0.01
    federate_info_real_time:  bool  = True
    federate_info_log_level:  str   = "none"

    federate_name: str = ""

    start_time: int = -1
    end_time:   int = -1
    step_time:  int =  1

    def __init__(self, **kwargs):
        self.current_time = -1

        self.values   = Values()
        self.messages = Messages()

        self._publications  = OrderedDict()
        self._subscriptions = OrderedDict()
        self._endpoints     = OrderedDict()

        # Allow federate name to be set at runtime.
        if 'name' in kwargs:
            self.federate_name = kwargs['name']

        # Allow federate init string to be set at runtime, ensuring it includes
        # the `--federates` option.
        if 'init_string' in kwargs:
            if '--federates' in kwargs['init_string']:
                self.federate_info_core_init_string = kwargs['init_string']
            else:
                self.federate_info_core_init_string += f" {kwargs['init_string']}"

        # Allow federate start/end/step time to be set at runtime.
        if 'start_time' in kwargs:
            self.start_time = kwargs['start_time']
        if 'end_time' in kwargs:
            self.end_time = kwargs['end_time']
        if 'step_time' in kwargs:
            self.step_time = kwargs['step_time']

        if 'module_name' in kwargs:
            self.module_name = kwargs['module_name']
        else:
            self.module_name = self.federate_name

        self.log(f'initializing HELICS version: {h.helicsGetVersion()}', helper=True)

        self._setup_federate()

    def _setup_federate(self):
        if self.federate_name == "":
            self.federate_name = str(uuid.uuid4())

        self.log(f'setting up federate {self.federate_name}', helper=True)

        self._setup_federate_info()

        self._federate = h.helicsCreateCombinationFederate(
            self.federate_name, self._federate_info
        )

        self._setup_publications()
        self._setup_subscriptions()
        self._setup_endpoints()

    def _setup_federate_info(self):
        self._federate_info = h.helicsCreateFederateInfo()

        if self.federate_info_core_name == "":
            self.federate_info_core_name = str(uuid.uuid4())

        self.log(f'setting federate core name to {self.federate_info_core_name}', helper=True)

        h.helicsFederateInfoSetCoreName(
            self._federate_info,
            self.federate_info_core_name
        )

        self.log(f'setting federate core type to {self.federate_info_core_type}', helper=True)

        h.helicsFederateInfoSetCoreTypeFromString(
            self._federate_info,
            self.federate_info_core_type
        )

        self.log(f'setting federate core init string to {self.federate_info_core_init_string}', helper=True)

        h.helicsFederateInfoSetCoreInitString(
            self._federate_info,
            self.federate_info_core_init_string
        )

        self.log(f'setting federate time delta to {self.federate_info_time_delta}', helper=True)

        h.helicsFederateInfoSetTimeProperty(
            self._federate_info,
            h.helics_property_time_delta,
            self.federate_info_time_delta,
        )

        self.log(f'setting federate real time to {self.federate_info_real_time}', helper=True)

        h.helicsFederateInfoSetFlagOption(
            self._federate_info,
            h.helics_flag_realtime,
            self.federate_info_real_time
        )

        log_level: int = 0

        if self.federate_info_log_level.lower() == 'none':
            log_level = h.helics_log_level_no_print
        elif self.federate_info_log_level.lower() == 'no_print':
            log_level = h.helics_log_level_no_print
        elif self.federate_info_log_level.lower() == 'error':
            log_level = h.helics_log_level_error
        elif self.federate_info_log_level.lower() == 'profiling':
            log_level = h.helics_log_level_profiling
        elif self.federate_info_log_level.lower() == 'warning':
            log_level = h.helics_log_level_warning
        elif self.federate_info_log_level.lower() == 'summary':
            log_level = h.helics_log_level_summary
        elif self.federate_info_log_level.lower() == 'connections':
            log_level = h.helics_log_level_connections
        elif self.federate_info_log_level.lower() == 'interfaces':
            log_level = h.helics_log_level_interfaces
        elif self.federate_info_log_level.lower() == 'timing':
            log_level = h.helics_log_level_timing
        elif self.federate_info_log_level.lower() == 'data':
            log_level = h.helics_log_level_data
        elif self.federate_info_log_level.lower() == 'debug':
            log_level = h.helics_log_level_debug
        elif self.federate_info_log_level.lower() == 'trace':
            log_level = h.helics_log_level_trace

        self.log(f'setting federate log level to {log_level}', helper=True)

        h.helicsFederateInfoSetIntegerProperty(
            self._federate_info,
            h.helics_property_int_log_level,
            log_level
        )

    def _setup_publications(self):
        for p in self.publications:
            self.log(f'setting up publication {p}', helper=True)

            if isinstance(p, GlobalPublication):
                self._publications[p] = h.helicsFederateRegisterGlobalPublication(
                    self._federate, p.name, p.type.value, ""
                )
            elif isinstance(p, Publication):
                # TODO: Should we use federate_name as a prefix for the key in the dictionary here?
                self._publications[p] = h.helicsFederateRegisterPublication(
                    self._federate, p.name, p.type.value, ""
                )

    def _setup_subscriptions(self):
        for s in self.subscriptions:
            self.log(f'setting up subscription {s}', helper=True)

            self._subscriptions[s] = h.helicsFederateRegisterSubscription(
                self._federate, s.name, ""
            )

    def _setup_endpoints(self):
        for e in self.endpoints:
            self.log(f'setting up endpoint {e}', helper=True)

            if isinstance(e, GlobalEndpoint):
                self._endpoints[e] = h.helicsFederateRegisterGlobalEndpoint(
                    self._federate, e.name, e.type
                )
            elif isinstance(e, Endpoint):
                self._endpoints[e] = h.helicsFederateRegisterEndpoint(
                    self._federate, e.name, e.type
                )

    def log(self, msg, helper=False):
        if helper:
            print(f'[{self.module_name}::helics_helper] {msg}', flush=True)
        else:
            print(f'[{self.module_name}] {msg}', flush=True)

    def enter_execution_mode(self):
        self.log('entering execution mode', helper=True)

        h.helicsFederateEnterExecutingMode(self._federate)

        self.log('entered execution mode', helper=True)

    def run(self):
        # Ensure publish/subscribe topics exist in the relevant dictionaries
        # prior to calling `action_publications` and `action_subscriptions`.
        for p in self._publications.keys():
            if p.name not in self.values.send:
                self.values.send[p.name] = None

        for s in self._subscriptions.keys():
            if s.name not in self.values.recv:
                self.values.recv[s.name] = None

        self.enter_execution_mode()

        granted_time = -1

        for t in range(self.start_time, self.end_time + self.step_time, self.step_time):
            while granted_time < t:
                self.log(f'requesting time {t}', helper=True)

                granted_time = h.helicsFederateRequestTime(self._federate, t)

                self.log(f'granted time {granted_time}', helper=True)

                self.current_time = granted_time

            self.action_pre_request_time()

            if len(self.publications) > 0:
                self.log('calling user-defined action_publications', helper=True)

                # User defined action
                self.action_publications(self.values.send, self.current_time)

            self._publish()

            if len(self.endpoints) > 0:
                self.log('calling user-defined action_endpoints_send', helper=True)

                # User defined action
                self.action_endpoints_send(self.messages.send, self.current_time)

            self._endpoints_send()
            self._endpoints_recv()

            if len(self.endpoints) > 0:
                self.log('calling user-defined action_endpoints_recv', helper=True)

                # User defined action
                self.action_endpoints_recv(self.messages.recv, self.current_time)

            self._subscribe()

            if len(self.subscriptions) > 0:
                self.log('calling user-defined action_subscriptions', helper=True)

                # User defined action
                self.action_subscriptions(self.values.recv, self.current_time)

            self.action_post_request_time()

        while granted_time < self.end_time:
            self.log(f'requesting time {self.end_time}', helper=True)

            granted_time = h.helicsFederateRequestTime(self._federate, self.end_time)

            self.log(f'granted time {granted_time}', helper=True)

            self.current_time = granted_time

        self.cleanup()

    def action_pre_request_time(self):
        pass

    def action_post_request_time(self):
        pass

    def action_endpoints_send(self, endpoints, current_time):
        raise NotImplementedError("Subclass and implement this function")

    def action_endpoints_recv(self, endpoints, current_time):
        raise NotImplementedError("Subclass and implement this function")

    def action_publications(self, publication_data, current_time):
        raise NotImplementedError("Subclass and implement this function")

    def action_subscriptions(self, subscription_data, current_time):
        raise NotImplementedError("Subclass and implement this function")

    def _endpoints_send(self):
        for e, endp in self._endpoints.items():
            if e.name in self.messages.send.keys():
                for _message in self.messages.send[e.name]:
                    self.log(f'sending raw event {_message.data} to {_message.destination} at time {_message.time}', helper=True)

                    h.helicsEndpointSendEventRaw(
                        endp, _message.destination, _message.data, _message.time
                    )

        self.messages.send.clear()

    def _endpoints_recv(self):
        self.messages.recv.clear()

        for e, endp in self._endpoints.items():
            if h.helicsEndpointHasMessage(endp) != 0:
                msg = h.helicsEndpointGetMessage(endp)

                message = Message(
                    data=msg.data,
                    original_destination=msg.original_dest,
                    original_source=msg.original_source,
                    destination=e.name,
                    source=msg.source,
                    time=msg.time,
                )

                self.messages.recv[e.name].append(message)
            else:
                self.log(f'endpoint {e} has no message', helper=True)

    def _publish(self):
        for p, pub in self._publications.items():
            d = self.values.send[p.name]

            if d == None: continue

            self.log(
                f'publishing value {repr(d)} of type {p.type} to topic {p.name} at time {self.current_time}',
                helper=True
            )

            if p.type == DataType.string:
                h.helicsPublicationPublishString(pub, d)
            elif p.type == DataType.boolean:
                h.helicsPublicationPublishBoolean(pub, d)
            elif p.type == DataType.complex:
                h.helicsPublicationPublishComplex(pub, d.real, d.imag)
            elif p.type == DataType.double:
                h.helicsPublicationPublishDouble(pub, d)
            elif p.type == DataType.int:
                h.helicsPublicationPublishInteger(pub, d)
            elif p.type == DataType.time:
                h.helicsPublicationPublishTime(pub, d)
            elif p.type == DataType.vector:
                h.helicsPublicationPublishVector(pub, d)
            elif p.type == DataType.named_point:
                h.helicsPublicationPublishNamedPoint(pub, d)
            elif p.type == DataType.raw:
                h.helicsPublicationPublishRaw(pub, d)
            else:
                raise HelicsException(
                    "Unknown type of data for publication {}".format(p)
                )

    def _subscribe(self):
        for s, sub in self._subscriptions.items():
            if s.type == DataType.boolean:
                d = h.helicsInputGetBoolean(sub)
            elif s.type == DataType.string:
                d = h.helicsInputGetString(sub)
            elif s.type == DataType.complex:
                d = h.helicsInputGetComplex(sub)
            elif s.type == DataType.double:
                d = h.helicsInputGetDouble(sub)
            elif s.type == DataType.int:
                d = h.helicsInputGetInteger(sub)
            elif s.type == DataType.named_point:
                d = h.helicsInputGetNamedPoint(sub)
            elif s.type == DataType.time:
                d = h.helicsInputGetTime(sub)
            elif s.type == DataType.vector:
                d = h.helicsInputGetVector(sub)
            else:
                raise HelicsException(
                    "Unknown type of data for subscription {}".format(s)
                )

            self.values.recv[s.name] = d

            self.log(
                f'got subscribed value {self.values.recv[s.name]} of type {s.type} for topic {s.name} at time {self.current_time}',
                helper=True
            )

    def cleanup(self):
        self.log('starting cleanup', helper=True)

        h.helicsFederateFinalize(self._federate)

        self.log('finished federate Finalize', helper=True)

        h.helicsFederateInfoFree(self._federate)

        self.log('finished federate InfoFree', helper=True)

        h.helicsFederateFree(self._federate_info)

        self.log('finished federate Free', helper=True)

        h.helicsCloseLibrary()

        self.log('finished cleanup', helper=True)
