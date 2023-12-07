package broker

import (
	"bytes"
	"context"
	"crypto/tls"
	"encoding/binary"
	"errors"
	"fmt"
	"strings"

	"github.com/patsec/ot-sim/mqtt/types"
	"github.com/patsec/ot-sim/msgbus"

	"github.com/beevik/etree"
	mochi "github.com/mochi-mqtt/server/v2"
	"github.com/mochi-mqtt/server/v2/hooks/auth"
	"github.com/mochi-mqtt/server/v2/listeners"
)

/*
<mqtt mode="broker">
	<!-- broker can listen on multiple interfaces (just TCP for now) -->
	<endpoint>
		<address>127.0.0.1:1883</address>
	</endpoint>
	<endpoint>
		<address>10.11.12.13:8883</address>
		<tls>
			<ca>/etc/ot-sim/root.pem</ca>
			<key>/etc/ot-sim/broker.key</key>
			<certificate>/etc/ot-sim/broker.crt</certificate>
		</tls>
	</endpoint>
	<endpoint>127.0.0.1:1883</endpoint> <!-- alternative way to specify -->
	<topic tag="foo.bar">foo/bar</topic>
</mqtt>
*/

type MQTTBroker struct {
	pubEndpoint  string
	pullEndpoint string

	endpoints []types.Endpoint

	name       string
	topicToTag map[string]string
	tagToTopic map[string]string

	server *mochi.Server
}

func New(name string) *MQTTBroker {
	return &MQTTBroker{
		name:       name,
		topicToTag: make(map[string]string),
		tagToTopic: make(map[string]string),
	}
}

func (this MQTTBroker) Name() string {
	return this.name
}

func (this *MQTTBroker) Configure(e *etree.Element) error {
	for _, child := range e.ChildElements() {
		switch child.Tag {
		case "pull-endpoint":
			this.pullEndpoint = child.Text()
		case "endpoint":
			var endpoint types.Endpoint

			if len(child.ChildElements()) == 0 {
				endpoint.Address = child.Text()
			} else {
				for _, child := range child.ChildElements() {
					switch child.Tag {
					case "address":
						endpoint.Address = child.Text()
					case "tls":
						for _, child := range child.ChildElements() {
							switch child.Tag {
							case "ca":
								endpoint.CAPath = child.Text()
							case "key":
								endpoint.KeyPath = child.Text()
							case "certificate":
								endpoint.CertPath = child.Text()
							}
						}
					}
				}
			}

			this.endpoints = append(this.endpoints, endpoint)
		case "topic":
			var (
				topic = child.Text()
				tag   = child.SelectAttrValue("tag", strings.ReplaceAll(topic, "/", "."))
			)

			this.topicToTag[topic] = tag
			this.tagToTopic[tag] = topic
		}
	}

	for idx, endpoint := range this.endpoints {
		if err := endpoint.Validate(); err != nil {
			return fmt.Errorf("validating endpoint: %w", err)
		}

		this.endpoints[idx] = endpoint
	}

	return nil
}

func (this *MQTTBroker) Run(ctx context.Context, pubEndpoint, pullEndpoint string) error {
	// Use ZeroMQ PUB endpoint specified in `mqtt` config block if provided.
	if this.pubEndpoint != "" {
		pubEndpoint = this.pubEndpoint
	}

	// Use ZeroMQ PULL endpoint specified in `mqtt` config block if provided.
	if this.pullEndpoint != "" {
		pullEndpoint = this.pullEndpoint
	}

	if len(this.endpoints) == 0 {
		return fmt.Errorf("no MQTT broker listener endpoints provided")
	}

	msgBusHook := &PublishToMsgBus{
		name:   this.name,
		pusher: msgbus.MustNewPusher(pullEndpoint),
		topics: this.topicToTag,
		log:    this.log,
	}

	subscriber := msgbus.MustNewSubscriber(pubEndpoint)
	subscriber.AddUpdateHandler(this.handleMsgBusUpdate)
	subscriber.Start("RUNTIME")

	this.server = mochi.New(&mochi.Options{InlineClient: true})
	this.server.AddHook(new(auth.AllowHook), nil)
	this.server.AddHook(msgBusHook, nil)

	for i, endpoint := range this.endpoints {
		var config *listeners.Config

		if !endpoint.Insecure {
			config = &listeners.Config{
				TLSConfig: &tls.Config{
					RootCAs:      endpoint.Roots,
					Certificates: []tls.Certificate{endpoint.Cert},
				},
			}
		}

		l := listeners.NewTCP(fmt.Sprintf("t%d", i), endpoint.Address, config)

		if err := this.server.AddListener(l); err != nil {
			return fmt.Errorf("adding TCP listener to MQTT broker: %w", err)
		}
	}

	go func() {
		if err := this.server.Serve(); err != nil {
			this.log("[ERROR] serving MQTT broker: %v", err)
		}
	}()

	go func() {
		<-ctx.Done()
		this.server.Close()
	}()

	return nil
}

func (this *MQTTBroker) handleMsgBusUpdate(env msgbus.Envelope) {
	if env.Sender() == this.name {
		return
	}

	update, err := env.Update()
	if err != nil {
		if !errors.Is(err, msgbus.ErrKindNotUpdate) {
			this.log("[ERROR] getting Update message from envelope: %v", err)
		}

		return
	}

	for _, point := range update.Updates {
		this.log("[DEBUG] received update for tag %s (value: %f)", point.Tag, point.Value)

		if topic, ok := this.tagToTopic[point.Tag]; ok {
			var buf bytes.Buffer
			if err := binary.Write(&buf, binary.BigEndian, point.Value); err != nil {
				this.log("[ERROR] converting value %f for tag %s to bytes: %v", point.Value, point.Tag, err)
			}

			this.log("[DEBUG] publishing value %f to topic %s", point.Value, topic)

			this.server.Publish(topic, buf.Bytes(), false, 0)
		}
	}
}

func (this MQTTBroker) log(format string, a ...any) {
	fmt.Printf("[%s] %s\n", this.name, fmt.Sprintf(format, a...))
}
