# Raspberry Pi GPIO Module

This OT-sim module leverages the [RPi.GPIO](https://pypi.org/project/RPi.GPIO/)
Python module to associate OT-sim tags with input and output pins.

Please note the following about this module:

1. This module will only work when OT-sim is run on a Raspberry Pi. To avoid
long compilation times, the suggested way to do so is to run OT-sim on a
Raspberry Pi using the OT-sim Docker image, which is a multi-architecture build.
1. Each input and output is used to setup the corresponding GPIO channel,
defined by the `pin` assignment.
1. This module only supports boolean inputs and outputs (`PWM` is not
supported).
1. This module only subscribes to `update` messages from the OT-sim message bus,
and only publishes `status` messages to the OT-sim message bus. GPIO inputs are
mapped to input tags, and output tags are mapped to GPIO outputs.
1. On exit, `GPIO.cleanup()` is called, which will set all used channels back to
inputs with no pull up/down. This may cause anything physically connected to the
channels to be affected.

## Example Configuration

```
<rpi-gpio name="rpi-hil" mode="board"> <!-- mode can be "board" or "bcm" -->
  <period>1</period> <!-- how often, in seconds, to read GPIO inputs; defaults to 5 -->
  <output pin="11">
    <tag>led-11</tag>
  </output>
  <output pin="13">
    <tag>relay-1-3</tag>
  </output>
  <input pin="17">
    <tag>switch-17</tag>
  </input>
</rpi-gpio>
```