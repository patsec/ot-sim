#!/bin/bash

echo "TARGET ARCHITECTURE: ${TARGETARCH}"

wget -O installer.sh \
  https://raw.githubusercontent.com/node-red/linux-installers/master/deb/update-nodejs-and-nodered

if [[ ${TARGETARCH} = arm* ]]; then
  echo "BUILDING FOR ARM"

  bash ./installer.sh --confirm-root --confirm-install --no-init \
    && rm installer.sh

  apt install -y libzmq3-dev

  pushd /root/.node-red

  npm install zeromq --zmq-shared

  apt purge -y libzmq3-dev && apt autoremove -y
else
  echo "NOT BUILDING FOR ARM"

  bash ./installer.sh --confirm-root --confirm-install --no-init --skip-pi \
    && rm installer.sh

  pushd /root/.node-red

  npm install zeromq
fi

npm install \
  node-red-dashboard \
  node-red-contrib-modbus \
  @node-red-contrib-themes/theme-collection

popd
