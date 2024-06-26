# Operational Technology (OT) Simulator

![Tests](https://github.com/patsec/ot-sim/actions/workflows/e2e.yml/badge.svg)
![Docker](https://github.com/patsec/ot-sim/actions/workflows/docker.yml/badge.svg)
[![Docs Badge](https://img.shields.io/badge/docs-reference-blue.svg)](https://ot-sim.patsec.dev)

OT-sim is a set of modules that run simulated OT devices in VMs or containers.
It allows researchers to represent a physical system, at scale, in a
co-simulation environment for specific or system-wide testing and evaluation
without impacting a real-world system. Over time, our goal is to include
additional protocol support and hardware-in-the-loop capability.

The purpose of OT-sim is to support co-simulation of OT-based modules and
related infrastructure for a variety of research purposes. To that end, all of
the modules run as separate processes and communicate with each other using a
common message bus to make up a device. The message bus then resembles the
backplane for communication from a variety of modules as seen in real-world
environments. However, in OT-sim, it is the communication environment where all
modules interact with the simulation environment &mdash; management signals, as
well as production messages, are passed over the message bus.

The current set of OT-sim modules are categorized as CPU, protocol &mdash;
Modbus and DNP3 are currently supported &mdash; I/O &mdash; acting as a
[HELICS](https://helics.org) federate, and logic. OT devices can include devices
such as PLCs, protection relays, IEDs, RTUs, FEPs, etc. These devices are
virtual representations and “walk the walk” when it comes to protocol
communication.

The message bus utilizes ZeroMQ's PUB/SUB protocol to allow connected modules to
publish messages for other modules to process. ZeroMQ allows the abstraction of
the medium in which the message bus sends messages over, supporting IPC and Unix
sockets for modules running in the same host and IP for modules running across
distributed hosts. OT-sim supports a wide variety of programming languages; it
is possible for modules to be written in their developer's language of choice.
In addition, since each module runs as its own process, it is possible to have
an OT-sim device comprised of modules written in different programming
languages.

In most cases, the combination of modules formed to create an OT-sim device will
all run together as separate processes on the same host &mdash; e.g., within the
same VM or container. However, the design of the message bus supports modules
running across multiple hosts &mdash; e.g., using an IP-based ZeroMQ socket in
place of an IPC- or file-based socket.

[HELICS](https://helics.org/) is used as the primary co-simulation platform for
simulating physical processes that OT-sim devices would monitor and control, so
to support this the I/O module acts as a HELICS federate to facilitate the
exchange of data with other HELICS federates. As an analogy, the data exchanged
between HELICS federates can be compared to the 4-20mA current loop process
control signals between sensors, actuators, and controllers in actual processes.

The logic module facilitates the use of custom logic provided at runtime via the
device configuration file, therefore avoiding the need for custom compiled
modules for different logic scripts. Custom logic is a set of simple
mathematical or boolean expressions that is parsed, compiled, and evaluated
against variables present in the logic module or values from other modules.

The CPU module is required as part of any OT-sim device and will process the
device configuration file and configure and deploy the additional modules
accordingly, as well as collocate logs generated by all other modules.

## Building

### Requirements

- `Debian-based Linux` (recommend Ubuntu 20.04 LTS or greater)
- `golang` v1.21 or greater
- `build-essential`
- `cmake` v3.11 or greater
- `libboost-dev`
- `libczmq-dev`
- `libxml2-dev`
- `libzmq5-dev`
- `pkg-config`
- `python3-dev`
- `python3-pip`

#### Install apt Packages
```
sudo apt update && sudo apt install \
  build-essential cmake libboost-dev libczmq-dev libxml2-dev libzmq5-dev pkg-config python3-dev python3-pip
```

#### Install Golang
```
wget -O go.tgz https://golang.org/dl/go1.21.8.linux-amd64.tar.gz \
  && sudo tar -C /usr/local -xzf go.tgz && rm go.tgz \
  && sudo ln -s /usr/local/go/bin/* /usr/local/bin
```

### Install OT-sim

Install the OT-sim C, C++, and Golang modules.

```
cmake -S . -B build && sudo cmake --build build -j$(nproc) --target install && sudo ldconfig \
  && sudo make -C src/go install
```

Install the OT-sim Python modules. This step will also install the Python HELICS
code, on which some of the OT-sim Python modules depend.

```
sudo python3 -m pip install src/python
```

### Optional Installations

#### Hivemind

```
wget -O hivemind.gz https://github.com/DarthSim/hivemind/releases/download/v1.1.0/hivemind-v1.1.0-linux-amd64.gz \
  && gunzip hivemind.gz \
  && sudo mv hivemind /usr/local/bin/hivemind \
  && sudo chmod +x /usr/local/bin/hivemind
```

#### Overmind

```
wget -O overmind.gz https://github.com/DarthSim/overmind/releases/download/v2.2.2/overmind-v2.2.2-linux-amd64.gz \
  && gunzip overmind.gz \
  && sudo mv overmind /usr/local/bin/overmind \
  && chmod +x /usr/local/bin/overmind
```

#### mbpoll

```
apt install -y mbpoll
```

#### OpenDSS

```
python3 -m pip install opendssdirect.py~=0.8.4
```

> NOTE: the version of `opendssdirect.py` to install may depend on which OS
> version is being used.

> NOTE: the example below uses OpenDSS to run a well-known IEEE 13 Bus power
> system test case. Some of the test case files are stored in this repo using
> git LFS, so in order to run the test case git LFS must be installed and `git
> lfs pull` must be run from the root of this repo in order to populate the test
> case files.

## Running an Example

If you have a Procfile-compatible tool, such as [Overmind](#overmind) or
[Hivemind](#hivemind), you can use it to run the Procfile in the root directory.

For a complete example, you will need something like [mbpoll](#mbpoll) to
interact with the Modbus module that is run as part of the example.

From one terminal or `tmux` pane, run the following:

```
overmind start -D -f Procfile.single
```

In a separate terminal or `tmux` pane, run the following to read from holding
register `30000` to get the kW line flow value.

`mbpoll` example:

```
> mbpoll -0 -1 -t 3 -r 30000 -c 1 -p 5502 localhost

-- Polling slave 1...
[30000]:        192
```

Register `30000` is configured to be scaled by a factor of two, so given the
above example the actual value in the experiment is 1.85kW.

You can trip the line feeding the bus by doing a coil write with a value of `0`
to address `0`. When you read a second time, the holding register `30000` will
update to `0`.

`mbpoll` example:

```
> mbpoll -0 -1 -t 0 -r 0 -p 5502 localhost 0

Written 1 references.

> mbpoll -0 -1 -t 3 -r 30000 -c 1 -p 5502 localhost

-- Polling slave 1...
[30000]:        0
```

### DNP3 Example with Docker

> NOTE: The latest version of the Docker image built with the provided
> Dockerfile is based on Debian Bookworm. Currently, `pydnp3` fails to build on
> Debian Bookworm and it's highly unlikely that will ever be fixed. The
> following instructions have been left here for legacy reasons. If users wish
> to run the following DNP3 example, they will need to use a different DNP3
> client or modify the Dockerfile. The last OS version known by the authors to
> work with `pydnp3` is Ubuntu 20.04.

First, build the Docker image.

```
docker build -t ot-sim .
```

Then start a container running all the modules, including the HELICS I/O module
and the DNP3 module, in outstation mode.

```
docker run -it --rm --name ot-test ot-sim hivemind Procfile.single
```

> You can also run a multi-device configuration, where one device acts as a DNP3
> outstation to the Modbus client gateway by using `Procfile.multi` instead of
> `Procfile.single` above. In this configuration, when you send the DNP3 CROB
> command below, it is translated to a Modbus message and sent along to the
> second device to modify the HELICS I/O module.

Next, from another terminal or `tmux` pane exec into the container and execute
the test DNP3 master, which will do a Class 0 scan, then send a CROB command to
trip a line in the OpenDSS HELICS federate that is also running, then do another
Class 0 scan.

```
docker exec -it ot-test sh -c "cd testing/dnp3 && python3 master.py"
```

## COPYRIGHT

Copyright (C) 2021-2022 Patria Security, LLC

## LICENSE

This software is distributed under the [GNU General Public License
v3](https://www.gnu.org/licenses/gpl-3.0.en.html). See
[COPYING](https://github.com/patsec/ot-sim/blob/main/COPYING) for more
information.
