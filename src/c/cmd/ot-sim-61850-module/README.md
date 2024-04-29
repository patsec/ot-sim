# IEC 61850 Module

## Initial Design Approach

Predefined OT devices
  * WTG main controller, yaw controller, blade controller, etc
  * Specific, hard-coded tags for each controller
  * Enabled via specific elements in OT-sim XML config

Alternatively, the WTG main controller could provide a 61400-25 IED via MMS
externally, and still use Modbus internally.

## Future Design Goals

Fully composable and configurable devices using relevant portions of CID files
in OT-sim XML config.
  * Shoot for fully dynamic creation of LDs, LNs, DOs, DAs, etc at runtime
  * May require precompilation step at runtime (like examples are now)

## Predefined Devices

### WTG Blade Controller

* LD:
* LN:
* DO:
* DA:

### Wind Turbine (WTUR)

iec61400-25-2{ed2.0}b.pdf - pg 22

Inherits from "Wind Power Plant Common" - pg 19

> Does WTUR need NamPlt, Beh, Health, and Mod since LLN0 already has it? What
> would it be specific to for WTUR vs LLN0? Maybe for a plant-wide logical
> device that has multiple WTUR nodes?
> Turns out it just needs Beh. The others are only mandatory in LLN0 for the
> root LD. A physical device may have multiple logical devices if it's acting as
> a concentrator, for example.

* WTUR
  * NamPlt - LPL (iec61850-7-3{ed2.1}en.pdf - pg 106)
    * vendor - VisString255
    * swRev - VisString255
    * configRev - VisString255
  * Beh - ENS (iec61850-7-3{ed2.1}en.pdf - pg 36)
    * stVal - EnumDA (of type BehaviourModeKind)
    * q - Quality
    * t - Timestamp
  * Health - ENS (iec61850-7-3{ed2.1}en.pdf - pg 36)
  * Mod - ENC (iec61850-7-3{ed2.1}en.pdf - pg 70)

## TODO

* [ ] map WTUR data attributes to OT-sim tags

Check data attribute type to know if conversion of OT-sim tag floating point
value needs to occur.

WTUR data attributes to map:
  * Beh_stVal (always enabled?)
  * TotWh_cntVal_actVal (set TotWh_cntVal_pulsQty to 1000 to represent kWh)
  * TurSt_st_stVal
  * W_mag_f (and W_mag_i - rounded integer)
  * TurOp_st_stVal (this is a control)

Attributes that will be controllable must be under an `Oper` data attribute (is
this a protocol spec thing or a libiec61850 implementation thing?). See
`server_example_control` example. I tried removing `Oper` DA and putting all sub
attributes under `Oper` DA under `SPCSO1` and the example client no longer
worked.

## Notes

If an attribute has a FC, then it needs to be named in the hierarchy. Otherwise
the attributes can be placed on the parent directly.