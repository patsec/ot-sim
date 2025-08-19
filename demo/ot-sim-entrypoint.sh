#!/bin/bash

sed -i s/localhost/broker/g /usr/local/src/ot-sim/config/single-device/device.xml

exec "$@"
