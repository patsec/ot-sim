# OT-sim SunSpec Module

Specify list of model IDs to include in SunSpec device.

Use existing Modbus server and client implementations with support for different
holding register data types.

  * will need to add support for string types that are used by SunSpec

Client requests should be for groups of holding registers that map to entire
SunSpec model point groups. On the server side, this means parsing through the
point IDs and grabbing configured static values or values from OT-sim tags to
stuff into the response.

What will be different between client and server config options?

  * hopefully nothing?

How do we go about mapping certain SunSpec model points (correct term?) to
OT-sim tags?

  * map point IDs to OT-sim tag or static value?
  * have default static values for point IDs that can be overwritten?

How do we want to specify point scalings?

  * map scaling values to SF values?
  * have deault scaling values for SF's that can be overwritten?

## Server

* If XML config element for SunSpec point is a string:
  * if schema for point says it's type string:
    * static value for point
  * if schema for point says it's type other than string:
    * OT-sim tag to get value from
* If XML config element for SunSpec point is a number:
  * if schema for point says it's type string:
    * ERROR
  * if schema for point says it's type other than string:
    * static value for point

XML config element values will always be strings. Should we try to parse as
number, catch error, and assume string on error, or should we use an XML element
attribute to denote static values? I vote first option... less typing in config.

## TODO

* [x] Build out initial Model 1 (static data)
* [ ] Add OT-sim msg bus status and update handlers
* [ ] Figure out how to handle scaling config