# End-to-End Integration Testing

This project's end-to-end (e2e) testing constists of two separate OT-sim
devices; device 1 acting as a RTU/gateway device and device 2 acting as an
IED/leaf device connected to (virtual) sensors and actuators. To support the
test, a HELICS broker and OpenDSS federate are also included.

Device 1 consists of the following OT-sim modules:

* CPU (manages other modules; collects logs)
* DNP3 (acting as outstation)
* Modbus (acting as master)

Device 2 consists of the following OT-sim modules:

* CPU (manages other modules; collects logs)
* Logic (not really used; just present to demo)
* Modbus (acting as outstation)
* I/O (acting as HELICS federate)

The test consists of a DNP3 master
([ot-sim-e2e-dnp3-master](../../src/c++/cmd/ot-sim-e2e-dnp3-master)) sending a
series of DNP3 commands to device 1 to confirm that end-to-end communications
are working from the test master all the way to the HELICS co-simulation via the
I/O module on device 2.

The DNP3 commands sent are as follows:

* on-demand class 0 scan to get values being generated by OpenDSS federate
* direct operate command to change a value in the OpenDSS federate
* on-demand class 0 scan to confirm the value changed in the OpenDSS federate

The DNP3 commands are sent to device 1, which then have to be processed and sent
to device 2 via the Modbus client on device 1. Once received on device 2, they
have to be processed by the I/O module and sent to the OpenDSS federate via the
HELICS broker.

This e2e test effectively validates the following:

* The I/O module on device 2 is receiving updates from other HELICS federates
  and making them available to other modules on device 2 via the message bus.
* The Modbus module on device 2 is processing messages from the I/O module and
  making them available to Modbus master queries.
* The Modbus module on device 1 is periodically querying the Modbus module on
  device 2 and making the updates available to other modules on device 1.
* The DNP3 module on device 1 is processing messages from the Modbus module and
  making them available to DNP3 master scans.
* The DNP3 module on device 1 receives operate commands from DNP3 masters and
  pushes the appropriate messages to other modules on device 1.
* The Modbus module on device 1 receives updates from other modules on device 1
  and generates the appropriate write commands to be sent to the Modbus
  outstation on device 2.
* The Modbus module on device 2 receives write commands from Modbus masters and
  pushes the appropriate messages to other modules on device 2.
* The I/O module on device 2 receives updates from other modules on device 2 and
  publishes them appropriately to other federates within the HELICS
  co-simulation.
