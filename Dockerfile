FROM golang:1.20-bookworm AS gobuild

ENV TZ=Etc/UTC
RUN ln -snf /usr/share/zoneinfo/$TZ /etc/localtime && echo $TZ > /etc/timezone

RUN apt update && apt install -y \
  libzmq5-dev \
  make \
  pkg-config

ADD .git /usr/local/src/ot-sim/.git

ADD src/go /usr/local/src/ot-sim/src/go
RUN make -C /usr/local/src/ot-sim/src/go install

FROM python:3.11-bookworm as pybuild

ADD .git /usr/local/src/ot-sim/.git

ADD src/python /usr/local/src/ot-sim/src/python
RUN python3 -m pip install /usr/local/src/ot-sim/src/python

FROM debian:bookworm AS build

ENV TZ=Etc/UTC
RUN ln -snf /usr/share/zoneinfo/$TZ /etc/localtime && echo $TZ > /etc/timezone

RUN apt update && apt install -y \
    build-essential \
    cmake \
    git \
    libboost-dev \
    libczmq-dev \
    libxml2-dev \
    libzmq3-dev \
    pkg-config \
    python3-dev \
    python3-pip \
    wget

ADD .git /usr/local/src/ot-sim/.git

ADD CMakeLists.txt /usr/local/src/ot-sim/CMakeLists.txt
ADD src/c          /usr/local/src/ot-sim/src/c
ADD src/c++        /usr/local/src/ot-sim/src/c++
RUN cmake -S /usr/local/src/ot-sim -B /usr/local/src/ot-sim/build \
  && cmake --build /usr/local/src/ot-sim/build -j $(nproc) --target install

FROM debian:bookworm AS prod

ENV TZ=Etc/UTC
RUN ln -snf /usr/share/zoneinfo/$TZ /etc/localtime && echo $TZ > /etc/timezone

RUN apt update && apt install -y \
  bash-completion curl git tmux tree vim wget xz-utils \
  libczmq4 libsodium23 libxml2 libzmq5 python3-pip

WORKDIR /root

ADD install-node-red.sh .

# needed by nod-red install script
ARG TARGETARCH
RUN /root/install-node-red.sh \
  && rm /root/install-node-red.sh

ADD ./src/js/node-red /root/.node-red/nodes/ot-sim

COPY --from=gobuild /usr/local /usr/local
COPY --from=pybuild /usr/local /usr/local
COPY --from=build   /usr/local /usr/local

RUN ldconfig

WORKDIR /

FROM debian:bookworm AS test

ENV TZ=Etc/UTC
RUN ln -snf /usr/share/zoneinfo/$TZ /etc/localtime && echo $TZ > /etc/timezone

RUN apt update && apt install -y \
  bash-completion curl git mbpoll tmux tree vim wget xz-utils \
  build-essential cmake libczmq4 libsodium23 libxml2 libzmq5 python3-dev python3-pip

RUN wget -O hivemind.gz https://github.com/DarthSim/hivemind/releases/download/v1.1.0/hivemind-v1.1.0-linux-amd64.gz \
  && gunzip --stdout hivemind.gz > /usr/local/bin/hivemind \
  && chmod +x /usr/local/bin/hivemind \
  && rm hivemind.gz

RUN wget -O overmind.gz https://github.com/DarthSim/overmind/releases/download/v2.2.2/overmind-v2.2.2-linux-amd64.gz \
  && gunzip --stdout overmind.gz > /usr/local/bin/overmind \
  && chmod +x /usr/local/bin/overmind \
  && rm overmind.gz

WORKDIR /root

ADD install-node-red.sh .

# needed by nod-red install script
ARG TARGETARCH
RUN /root/install-node-red.sh \
  && rm /root/install-node-red.sh

ADD ./src/js/node-red /root/.node-red/nodes/ot-sim

COPY --from=gobuild /usr/local /usr/local
COPY --from=pybuild /usr/local /usr/local
COPY --from=build   /usr/local /usr/local

RUN python3 -m pip install --break-system-packages opendssdirect.py~=0.8.4

RUN ldconfig

ADD . /usr/local/src/ot-sim
WORKDIR /usr/local/src/ot-sim
