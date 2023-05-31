import json, os, time

import opendssdirect as dss

from otsim.helics_helper import DataType, Endpoint, HelicsFederate, Publication, Subscription


current_directory = os.path.realpath(os.path.dirname(__file__))


class OpenDSSFederate(HelicsFederate):
    HelicsFederate.federate_name = "OpenDSS"
    HelicsFederate.federate_info_core_init_string = "--federates=1 --broker=127.0.0.1"

    HelicsFederate.start_time = 1
    HelicsFederate.end_time   = 3600

    HelicsFederate.publications = [
        Publication("load-671.voltage",        DataType.double),
        Publication("load-671.current",        DataType.double),
        Publication("load-671.active_power",   DataType.double),
        Publication("load-671.reactive_power", DataType.double),
        Publication("load-645.voltage",        DataType.double),
        Publication("load-645.current",        DataType.double),
        Publication("load-645.active_power",   DataType.double),
        Publication("load-645.reactive_power", DataType.double),
        Publication("load-646.voltage",        DataType.double),
        Publication("load-646.current",        DataType.double),
        Publication("load-646.active_power",   DataType.double),
        Publication("load-646.reactive_power", DataType.double),
        Publication("load-692.voltage",        DataType.double),
        Publication("load-692.current",        DataType.double),
        Publication("load-692.active_power",   DataType.double),
        Publication("load-692.reactive_power", DataType.double),
        Publication("line-650632.kW",          DataType.double),
        Publication("line-650632.kVAR",        DataType.double),
        Publication("line-650632.kVA",         DataType.complex),
        Publication("line-650632.closed",      DataType.boolean),
        Publication("bus-692.voltage",         DataType.double),
        Publication("bus-RG60.voltage",        DataType.double),
        Publication("switch-671692.closed",    DataType.boolean),
        Publication("regulator-Reg1.vreg",     DataType.double),
    ]

#   HelicsFederate.subscriptions = [
#       Subscription("ot-sim-io/line-650632.closed", DataType.boolean),
#   ]

    # Message federate endpoint to receive updates.
    HelicsFederate.endpoints = [
        Endpoint("updates"),
    ]

    def __init__(self, *args, **kwargs):
        HelicsFederate.__init__(self)

        case = os.path.join(current_directory, "data", "IEEE13", "IEEE13Nodeckt.dss")

        dss.run_command(f'Redirect {case}')
        dss.run_command('set mode=daily') # daily profile configured for loads

        # initial solve
        dss.Solution.StepSize(0)
        dss.Solution.Solve()

        dss.Solution.StepSize(60) # load profile is 1m interval


    def action_post_request_time(self):
        time.sleep(1)
        dss.Solution.Solve()


    def action_subscriptions(self, data, ts):
        for k, v in data.items():
            _, topic = k.split('/', 1)

            name, field = topic.split('.', 1)
            kind, name  = name.split('-', 1)

            if kind == 'switch':
                dss.Circuit.SetActiveClass("line")
                dss.Circuit.SetActiveElement(name)

                if field == 'closed':
                    if v is True or v != 0:
                        dss.CktElement.Close(0, 0)
                    elif v is False or v == 0:
                        dss.CktElement.Open(0, 0)
            elif kind == 'line':
                dss.Circuit.SetActiveClass("line")
                dss.Circuit.SetActiveElement(name)

                if field == 'closed':
                    if v is True or v != 0:
                        dss.CktElement.Close(0, 0)
                    elif v is False or v == 0:
                        dss.CktElement.Open(0, 0)
            elif kind == 'regulator':
                dss.Circuit.SetActiveClass('regcontrol')
                dss.Circuit.SetActiveElement(name)

                if field == 'vreg':
                    dss.RegControls.ForwardVreg(v)
            elif kind == 'bus':
                dss.Circuit.SetActiveClass("Vsource")
                dss.Circuit.SetActiveElement("source")

                dss.Vsources.BasekV(v[0] * 115)


    def action_publications(self, data, ts):
        for topic in data.keys():
            # topic = line-650632.kW
            # kind  = line
            # name  = 650632
            # field = kW

            name, field = topic.split('.', 1)
            kind, name  = name.split('-', 1)

            if kind == 'bus':
                dss.Circuit.SetActiveBus(name)

                if field == 'voltage':
                    # Voltage magnitude
                    data[topic] = dss.Bus.puVmagAngle()[0]
                    continue
                if field == 'kV':
                    data[topic] = complex(dss.Bus.Voltages()[0], dss.Bus.Voltages()[1])
                    continue
            elif kind == 'load':
                dss.Circuit.SetActiveClass(b'load')
                dss.Circuit.SetActiveElement(name)

                phases   = dss.CktElement.NumPhases()
                powers   = dss.CktElement.Powers()
                currents = dss.CktElement.CurrentsMagAng()

                # remove any trailing zeros
                powers   = powers[:2*phases]
                currents = currents[:2*phases]

                real_or_mag = tuple(currents[0:2*phases:2])

                active   = 0.0
                reactive = 0.0
                current  = 0.0

                if phases == 1:
                    active   = powers[0]
                    reactive = powers[1]
                    current  = real_or_mag[0]
                elif phases in [2, 3]:
                    p = tuple(powers[0:2*phases:2])
                    q = tuple(powers[1:2*phases+1:2])

                    active   = sum(p)
                    reactive = sum(q)
                    current  = sum(real_or_mag)

                if field == 'active_power':
                    data[topic] = active
                    continue
                if field == 'reactive_power':
                    data[topic] = reactive
                    continue
                if field == 'current':
                    data[topic] = current
                    continue

                bus = dss.CktElement.BusNames()[0]
                dss.Circuit.SetActiveBus(bus)

                nodes = dss.Bus.NumNodes()
                pu    = dss.Bus.puVmagAngle()

                real_or_mag = tuple(pu[0:2*nodes:2])

                voltage = 0.0

                if nodes == 1 or phases == 1:
                    voltage = real_or_mag[0]
                else:
                    voltage = sum(real_or_mag) / len(real_or_mag)

                if field == 'voltage':
                    data[topic] = voltage
                    continue

            elif kind == 'line':
                dss.Circuit.SetActiveClass(b'line')
                dss.Circuit.SetActiveElement(name)

                powers = dss.CktElement.Powers()
                S = sum([complex(x,y) for x, y in zip(powers[0:6], powers[1:6])])

                if field == 'kW':
                    data[topic] = S.real / 1000.0
                    continue
                if field == 'kVAR':
                    data[topic] = S.imag / 1000.0
                    continue
                if field == 'kVA':
                    data[topic] = S / 1000.0
                    continue
                if field == 'closed':
                    data[topic] = not dss.CktElement.IsOpen(0, 0)
                    continue

            elif kind == 'switch':
                dss.Circuit.SetActiveClass('line')
                dss.Circuit.SetActiveElement(name)

                if field == 'closed':
                    data[topic] = not dss.CktElement.IsOpen(0, 0)
                    continue
            elif kind == 'regulator':
                dss.Circuit.SetActiveClass('regcontrol')
                dss.Circuit.SetActiveElement(name)

                if field == 'vreg':
                    data[topic] = dss.RegControls.ForwardVreg()


    def action_endpoints_send(self, endpoints, ts):
        pass


    def action_endpoints_recv(self, endpoints, ts):
        for msg in endpoints.get('updates', []):
            updates = json.loads(msg.data)

            for update in updates:
                topic = update.get('tag',   None)
                value = update.get('value', None)

                assert topic is not None
                assert value is not None

                name, field = topic.split('.', 1)
                kind, name  = name.split('-', 1)

                if kind == 'switch':
                    dss.Circuit.SetActiveClass("line")
                    dss.Circuit.SetActiveElement(name)

                    if field == 'closed':
                        if value is True or value != 0:
                            dss.CktElement.Close(0, 0)
                        elif value is False or value == 0:
                            dss.CktElement.Open(0, 0)
                elif kind == 'line':
                    dss.Circuit.SetActiveClass("line")
                    dss.Circuit.SetActiveElement(name)

                    if field == 'closed':
                        if value is True or value != 0:
                            dss.CktElement.Close(0, 0)
                        elif value is False or value == 0:
                            dss.CktElement.Open(0, 0)
                elif kind == 'regulator':
                    dss.Circuit.SetActiveClass('regcontrol')
                    dss.Circuit.SetActiveElement(name)

                    if field == 'vreg':
                        dss.RegControls.ForwardVreg(value)
                elif kind == 'bus':
                    dss.Circuit.SetActiveClass("Vsource")
                    dss.Circuit.SetActiveElement("source")

                    dss.Vsources.BasekV(value[0] * 115)


if __name__ == "__main__":
    import logging

    FORMAT = "%(asctime)s [%(name)s:%(lineno)d] %(levelname)s: %(message)s"
    logging.basicConfig(level=logging.ERROR, format=FORMAT)

    federate1 = OpenDSSFederate()
    federate1.run()
