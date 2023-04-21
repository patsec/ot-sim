module.exports = function(RED) {
  "use strict";
  var zmq = require('zeromq');

  function OTsimIn(config) {
    RED.nodes.createNode(this, config);

    this.tag     = config.tag;
    this.updates = config.updates;

    var node = this;

    node.endpoint = 'tcp://localhost:5678';

    if (RED.settings.otsim && RED.settings.otsim.pub) {
      node.endpoint = RED.settings.otsim.pub;
    }

    node.sock = zmq.socket('sub');

    node.sock.connect(node.endpoint);
    node.sock.subscribe('RUNTIME');

    node.status({fill: "green", shape: "ring", text: "subscribing"});

    node.sock.on('message', function(topic, msg) {
      msg = JSON.parse(msg.toString());

      if (msg.kind === 'Status') {
        for (const m of msg.contents.measurements) {
          if (m.tag === node.tag) {
            node.send({topic: node.tag, payload: m.value});
          }
        }
      }

      if (node.updates && msg.kind === 'Update') {
        for (const u of msg.contents.updates) {
          if (u.tag === node.tag) {
            node.send({payload: u.value});
          }
        }
      }
    });
  }

  RED.nodes.registerType("ot-sim in", OTsimIn);

  function OTsimOut(config) {
    RED.nodes.createNode(this, config);

    this.tag = config.tag;
    var node = this;

    node.endpoint = 'tcp://localhost:1234';

    if (RED.settings.otsim && RED.settings.otsim.pull) {
      node.endpoint = RED.settings.otsim.pull;
    }

    node.sock = zmq.socket('push');

    node.sock.connect(node.endpoint);
    node.sock.setsockopt(zmq.ZMQ_LINGER, 0);

    node.status({fill: "yellow", shape: "ring", text: "idle"});

    node.on('input', function(msg) {
      var value = parseFloat(msg.payload);

      if (isNaN(value)) {
        console.log('payload was not a valid floating point number');
        return;
      }

      var update = {
        version: 'v1',
        kind: 'Update',
        metadata: {
          sender: 'Node-Red'
        },
        contents: {
          updates: [
            {
              tag: node.tag,
              value: value
            }
          ]
        }
      }

      node.status({fill: "green", shape: "ring", text: "updating"});
      node.sock.send(['RUNTIME', JSON.stringify(update)])
      node.status({fill: "yellow", shape: "ring", text: "idle"});
    });
  }

  RED.nodes.registerType("ot-sim out", OTsimOut);
}
