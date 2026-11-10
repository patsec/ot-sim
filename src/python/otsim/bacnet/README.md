# BACnet Module

This OT-sim module makes a device a BACnet/IP server. It uses
[BACpypes3](https://github.com/JoelBender/BACpypes3), and the CPU module starts
it like any other module:

```
<cpu>
  <module name="backplane">ot-sim-message-bus {{config_file}}</module>
  <module name="bacnet">ot-sim-bacnet-module {{config_file}}</module>
</cpu>
```

Each BACnet object follows one OT-sim tag:

* The module subscribes to `status` messages from the OT-sim message bus. A
  value for a tag becomes the present-value of every object configured with that
  tag.
* An accepted BACnet write is published to the OT-sim message bus as an `update`
  message for the object's tag.

Please note the following about this module:

1. It is a server only (`mode="server"`). There is no BACnet client.
1. It serves BACnet/IP only, and it is not a BBMD or a foreign device. To reach
it from another subnet, run a BBMD on its subnet.
1. The supported object types are `analog-input`, `analog-output`,
`analog-value`, `binary-input`, `binary-output`, `binary-value`, and
`multi-state-value`.
1. Clients can use Who-Is, ReadProperty, ReadPropertyMultiple, WriteProperty,
WritePropertyMultiple, and SubscribeCOV.
1. Only the present-value of a commandable object can be written. A write to
anything else, including the Device object, is answered with
`write-access-denied`.

## Example Configuration

```
<bacnet name="bacnet-server" mode="server">
  <endpoint>10.0.0.5/24:47808</endpoint>
  <device>
    <instance>1001</instance>
    <name>AHU-1</name>
  </device>
  <object type="analog-input">
    <instance>1</instance>
    <tag>ahu.supply_temp</tag>
    <name>AHU1.SAT</name>
    <description>Supply air temperature</description>
    <units>degreesFahrenheit</units>
  </object>
  <object type="analog-value">
    <instance>1</instance>
    <tag>ahu.setpoint</tag>
    <units>degreesFahrenheit</units>
    <commandable>true</commandable>
    <min>50</min>
    <max>80</max>
  </object>
  <object type="binary-output">
    <instance>1</instance>
    <tag>ahu.fan</tag>
  </object>
  <object type="multi-state-value">
    <instance>1</instance>
    <tag>ahu.mode</tag>
    <commandable>true</commandable>
    <state>occupied</state>
    <state>unoccupied</state>
    <state>standby</state>
  </object>
</bacnet>
```

## Settings

Settings of the `bacnet` element:

| Setting | Required | Meaning |
|---|---|---|
| `name` attribute | no | The name of the module on the message bus. Defaults to `bacnet`. |
| `mode` attribute | no | Must be `server`, which is the default. |
| `endpoint` | yes | The IPv4 address of the device's interface, its prefix length, and optionally the UDP port (default 47808), e.g. `10.0.0.5/24`. See [Endpoint](#endpoint). |
| `device` | yes | The Device object. Its `instance` (0 to 4194302) is required and must be unique on the BACnet network. `name` defaults to the module's name. `description`, `vendor-name`, and `model-name` are optional. |
| `object` | no | One BACnet object. See below. |
| `stale-after` | no | Seconds without a value before an object reports a fault. Off by default. See [Stale Values](#stale-values). |
| `pub-endpoint`, `pull-endpoint` | no | The message bus endpoints, when they differ from the `message-bus` element's. |

Settings of an `object` element:

| Setting | Required | Meaning |
|---|---|---|
| `type` attribute | yes | One of the supported object types. |
| `instance` | yes | The object's instance (0 to 4194302). It must be unique for its object type. |
| `tag` | yes | The OT-sim tag the object follows. More than one object can have the same tag. |
| `name` | no | The object's name, which must be unique in the device. Defaults to the tag. |
| `description` | no | The object's description. |
| `units` | no | Analog objects only. The name of a BACnet engineering unit as BACpypes3 spells it, e.g. `degreesFahrenheit`, `percent`, or `kilowatts`. Defaults to `noUnits`. |
| `cov-increment` | no | Analog objects only. How far present-value has to move before COV subscribers are notified. Defaults to 0.1. |
| `commandable` | no | Value objects only. `true` gives the object a priority array so that its present-value can be written. Outputs always have one, and inputs never do. |
| `min`, `max` | no | Analog objects only. A write of a value outside these limits is answered with `value-out-of-range`. |
| `state` | multi-state only | The text of one state. The first `state` element is state 1. |
| `override-tag` | no | Commandable objects only. See [Commands](#commands). |

## Values

Tags are numbers on the message bus, so present-values are converted as follows.

| Object types | Tag value to present-value | Present-value to tag value |
|---|---|---|
| analog | the value | the value |
| binary | 0 is `inactive`, anything else is `active` | `inactive` is 0, `active` is 1 |
| multi-state | the value rounded, and limited to the object's states | the state number |

Until the first value arrives for its tag, an object's present-value is 0,
`inactive`, or state 1.

A value that does not fit in a BACnet Real is ignored. HELICS provides -1e49 for
a subscription that no federate has published yet, and the I/O module passes it
on, so this is what an object's tag holds for the first moments of a
co-simulation.

## Commands

A commandable object has a priority array, like the outputs and setpoints of a
real controller.

* A write to present-value, at any priority, is a command. The module publishes
  the present-value that results from the priority array as an `update` message
  for the tag.
* While a command is in force, present-value is the commanded value, whatever
  the tag's `status` messages say. Add a second, non-commandable object with the
  same tag (e.g. a `binary-input` beside a `binary-output`) to let clients read
  the tag itself.
* While no command is in force, present-value is the object's relinquish
  default, and the relinquish default follows the tag.
* Writing NULL to a priority relinquishes that command. When it was the last
  one, present-value goes back to the relinquish default, which is the value the
  tag had before the first command, and the module publishes that value as an
  `update` message for the tag.

Some simulations run a point with their own logic and only want to be overridden
while a BACnet command is in force. Giving the old value back when the command is
relinquished is not enough for them. For these, set an `override-tag` on the
object. Every `update` message for the object's tag then also has the override
tag in it, with the value 1 while a command is in force and 0 once the last
command is relinquished:

```
<object type="binary-output">
  <instance>1</instance>
  <tag>ahu.fan</tag>
  <override-tag>ahu.fan.override</override-tag>
</object>
```

## Stale Values

With `stale-after` set to a number of seconds, an object whose tag has had no
value for that long reports a `reliability` of `communication-failure` and sets
the fault bit of its `status-flags`. This includes an object that has not had a
value yet. The last present-value stays readable, and the fault clears with the
next value.

It is off by default because a tag that is only ever written (one that nothing
publishes `status` messages for) would always be stale.

## Endpoint

The endpoint must be the address of one of the device's interfaces with its
prefix length. It cannot be `0.0.0.0`. BACpypes3 listens for broadcasts on the
directed broadcast address of the subnet, which it works out from the address
and the prefix, and that is the address a client's Who-Is (or the Who-Is a BBMD
forwards) is sent to.

If the address is not on an interface when the module starts, BACpypes3 keeps
trying to bind it until it is.

## Errors

A configuration error is logged and the module exits with status 101, which
tells the CPU module not to restart it.

## Metrics

| Metric | Meaning |
|---|---|
| `status_count` | number of status messages processed |
| `update_count` | number of update messages generated |
| `write_count` | number of BACnet writes accepted |
| `write_error_count` | number of BACnet writes refused |

## Testing

`testing/bacnet` has tests that run the module against a BACpypes3 client and a
stand-in for the message bus, all on the loopback interface:

```
python3 -m pip install pytest
python3 -m pytest testing/bacnet
```

The [end-to-end test](../../../../testing/e2e/README.md) also reads and commands
objects of the BACnet module on device 2.
