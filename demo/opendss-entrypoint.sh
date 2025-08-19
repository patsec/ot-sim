#!/bin/bash

sed -i s/127.0.0.1/broker/g /usr/local/src/ot-sim/testing/e2e/helics/opendss-federate.py

exec "$@"
