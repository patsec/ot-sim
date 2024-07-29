# Intercom Module

> TODO: improve upon this README

The `intercom` module uses MQTT to extend an OT-sim device's message bus data
to other OT-sim devices. This is helpful when, for example, multiple other
OT-sim devices that are being used as HMI servers need to talk to the same
remote OT-sim device. This could be done using DNP3, for example, but the DNP3
module only supports a single client connection. Using the `intercom` module
also cuts down on configuration requirements since it simply mirrors all tags
automatically.

## TODO

* [ ] Update client and broker implementations to default to the following settings.

```
  <intercom mode="broker">
    <endpoint>:1883</endpoint>
    <publish>
      <status>true</status>
      <update>false</update>
    </publish>
    <subscribe>
      <status>false</status>
      <update>true</update>
    </subscribe>
  </intercom>
```

```
  <intercom mode="client">
    <endpoint>tcp://10.2.2.2:1883</endpoint>
    <publish>
      <status>false</status>
      <update>true</update>
    </publish>
    <subscribe>
      <status>true</status>
      <update>false</update>
    </subscribe>
  </intercom>
```
