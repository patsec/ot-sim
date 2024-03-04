SHELL := /bin/bash

# default install prefix
prefix := /usr/local

# Default version number to git commit hash if not set.
# VER     := $(or $(VER),$(shell git log -1 --format="%h"))
# COMMIT  := $(or $(COMMIT),$(shell git log -1 --format="%h - %ae"))
# DATE    := $(shell date -u)
# VERSION := $(VER) (commit $(COMMIT)) $(DATE)

MSGBUS_SOURCES := $(shell find msgbus \( -name '*.go' \))

clean:
	$(RM) bin/*

all: bin/ot-sim-cpu-module bin/ot-sim-logic-module bin/ot-sim-modbus-module bin/ot-sim-mqtt-module bin/ot-sim-node-red-module bin/ot-sim-tailscale-module bin/ot-sim-telnet-module

CPU_SOURCES := $(shell find cpu \( -name '*.go' \))

bin/ot-sim-cpu-module: $(CPU_SOURCES) $(MSGBUS_SOURCES)
	mkdir -p bin
	GOOS=linux go build -a -ldflags="-s -w" -trimpath -o bin/ot-sim-cpu-module cmd/ot-sim-cpu-module/main.go

LOGIC_SOURCES := $(shell find logic \( -name '*.go' \))

bin/ot-sim-logic-module: $(LOGIC_SOURCES) $(MSGBUS_SOURCES)
	mkdir -p bin
	GOOS=linux go build -a -ldflags="-s -w" -trimpath -o bin/ot-sim-logic-module cmd/ot-sim-logic-module/main.go

MODBUS_SOURCES := $(shell find modbus \( -name '*.go' \))

bin/ot-sim-modbus-module: $(MODBUS_SOURCES) $(MSGBUS_SOURCES)
	mkdir -p bin
	GOOS=linux go build -a -ldflags="-s -w" -trimpath -o bin/ot-sim-modbus-module cmd/ot-sim-modbus-module/main.go

MQTT_SOURCES := $(shell find mqtt \( -name '*.go' \))

bin/ot-sim-mqtt-module: $(MQTT_SOURCES) $(MSGBUS_SOURCES)
	mkdir -p bin
	GOOS=linux go build -a -ldflags="-s -w" -trimpath -o bin/ot-sim-mqtt-module cmd/ot-sim-mqtt-module/main.go

NODERED_SOURCES := $(shell find nodered \( -name '*.go' -o -name '*.tmpl' \))

bin/ot-sim-node-red-module: $(NODERED_SOURCES)
	mkdir -p bin
	GOOS=linux go build -a -ldflags="-s -w" -trimpath -o bin/ot-sim-node-red-module cmd/ot-sim-node-red-module/main.go

TAILSCALE_SOURCES := $(shell find tailscale \( -name '*.go' \))

bin/ot-sim-tailscale-module: $(TAILSCALE_SOURCES) $(MSGBUS_SOURCES)
	mkdir -p bin
	GOOS=linux go build -a -ldflags="-s -w" -trimpath -o bin/ot-sim-tailscale-module cmd/ot-sim-tailscale-module/main.go

TELNET_SOURCES := $(shell find telnet \( -name '*.go' \))

bin/ot-sim-telnet-module: $(TELNET_SOURCES) $(MSGBUS_SOURCES)
	mkdir -p bin
	GOOS=linux go build -a -ldflags="-s -w" -trimpath -o bin/ot-sim-telnet-module cmd/ot-sim-telnet-module/main.go

.PHONY: install
install: bin/ot-sim-cpu-module bin/ot-sim-logic-module bin/ot-sim-modbus-module bin/ot-sim-mqtt-module bin/ot-sim-node-red-module bin/ot-sim-tailscale-module bin/ot-sim-telnet-module
	cp bin/ot-sim-cpu-module       $(prefix)/bin/ot-sim-cpu-module
	cp bin/ot-sim-logic-module     $(prefix)/bin/ot-sim-logic-module
	cp bin/ot-sim-modbus-module    $(prefix)/bin/ot-sim-modbus-module
	cp bin/ot-sim-mqtt-module      $(prefix)/bin/ot-sim-mqtt-module
	cp bin/ot-sim-node-red-module  $(prefix)/bin/ot-sim-node-red-module
	cp bin/ot-sim-tailscale-module $(prefix)/bin/ot-sim-tailscale-module
	cp bin/ot-sim-telnet-module    $(prefix)/bin/ot-sim-telnet-module
