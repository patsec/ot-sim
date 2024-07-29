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
* [x] Add OT-sim msg bus status handler
      * do we need an update handler?
* [x] Figure out how to handle scaling config
* [ ] Support mapping OT-sim tag names client-side
* [ ] Support different scan rates for different models client-side
* [ ] Support writes client-side (subscribe to updates)

Server-side is "pretty much" done. Client side needs work 1) continuing to read
available models, and 2) mapping model points to OT-sim tags. The client doesn't
really need to know what models the server side is providing ahead of time since
it can query the server for that, but configuration-wise users will need to know
so they can assign tags to points. Alternatively, we could default to a
well-defined method of automatically mapping points to tags. This could be as
easy as the SunSpec point's name. If we find that point names are not unique
across all SunSpec models, we could prefix the name with the model number. We
can also do things like skip publishing scaling factors within OT-sim.
