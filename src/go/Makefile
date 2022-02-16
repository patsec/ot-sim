SHELL := /bin/bash

# Default version number to git commit hash if not set.
VER     := $(or $(VER),$(shell git log -1 --format="%h"))
COMMIT  := $(or $(COMMIT),$(shell git log -1 --format="%h - %ae"))
DATE    := $(shell date -u)
VERSION := $(VER) (commit $(COMMIT)) $(DATE)

THISFILE := $(lastword $(MAKEFILE_LIST))
THISDIR  := $(shell dirname $(realpath $(THISFILE)))
GOBIN    := $(THISDIR)/bin

# Prepend this repo's bin directory to our path since we'll want to
# install some build tools there for use during the build process.
PATH := $(GOBIN):$(PATH)

# Export GOBIN env variable so `go install` picks it up correctly.
export GOBIN

MSGBUS_SOURCES := $(shell find msgbus \( -name '*.go' \))

clean:
	$(RM) bin/*

all: bin/ot-sim-cpu-module bin/ot-sim-logic-module bin/ot-sim-modbus-module

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

.PHONY: dev-install
dev-install: bin/ot-sim-cpu-module bin/ot-sim-logic-module bin/ot-sim-modbus-module
	sudo cp bin/ot-sim-cpu-module /usr/local/bin/ot-sim-cpu-module
	sudo cp bin/ot-sim-logic-module /usr/local/bin/ot-sim-logic-module
	sudo cp bin/ot-sim-modbus-module /usr/local/bin/ot-sim-modbus-module

.PHONY: install
install: GOBIN=/usr/local/bin
install:
	go install -a -ldflags="-s -w" -trimpath ./...
ifneq ($(shell which upx),)
	upx $(GOBIN)/ot-sim-cpu-module
	upx $(GOBIN)/ot-sim-logic-module
	upx $(GOBIN)/ot-sim-modbus-module
endif
