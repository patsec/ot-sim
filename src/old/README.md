# Old Code

This is all old code that I don't necessarily want to get rid of yet because
it's got some good examples of how to do things like use pydnp3, use JSON and
XML in C, etc.

## Noteworthy

* The pydnp3 master implementation is lacking because pydnp3 doesn't provide a
  way to get at the values gathered from scheduled class scans.
* The pydnp3 code does not use the `msgbus` package the other Python code uses.
* The C implementation of the `ot-sim-io-module` is fully functional, but it
  currently expects `<publication>` and `<subscription>` elements to be wrapped
  in `<publications>` and `<subscriptions>` parent elements. The Python
  implementation of the `ot-sim-io-module` does not look for the parent elements.
