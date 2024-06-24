package client

import (
	"context"
	"encoding/json"
	"errors"
	"fmt"
	"net/url"
	"strconv"
	"strings"

	"github.com/patsec/ot-sim/msgbus"

	"github.com/beevik/etree"
	"github.com/eclipse/paho.golang/autopaho"
	"github.com/eclipse/paho.golang/paho"
)

/*
<intercom mode="client">
	<endpoint>broker.example.com:1883</endpoint>
	<client-id>ot-sim-jitp-test</client-id>
	<publish>
		<status>true</status>
		<update>true</update>
	</publish>
	<subscribe>
		<status>true</status>
		<update>true</update>
	</subscribe>
</mqtt>
*/

type IntercomClient struct {
	pubEndpoint  string
	pullEndpoint string

	id       string
	name     string
	endpoint string

	pubStatus bool
	pubUpdate bool
	subStatus bool
	subUpdate bool

	client *autopaho.ConnectionManager
	pusher *msgbus.Pusher
}

func New(name string) *IntercomClient {
	return &IntercomClient{
		id:   name,
		name: name,

		// default to pub/sub all messages
		pubStatus: true,
		pubUpdate: true,
		subStatus: true,
		subUpdate: true,
	}
}

func (this IntercomClient) Name() string {
	return this.name
}

func (this *IntercomClient) Configure(e *etree.Element) error {
	for _, child := range e.ChildElements() {
		switch child.Tag {
		case "pub-endpoint":
			this.pubEndpoint = child.Text()
		case "endpoint":
			this.endpoint = child.Text()
		case "client-id":
			this.id = child.Text()
		case "publish":
			for _, child := range child.ChildElements() {
				val, _ := strconv.ParseBool(child.Text())

				switch child.Tag {
				case "status":
					this.pubStatus = val
				case "update":
					this.pubUpdate = val
				}
			}
		case "subscribe":
			for _, child := range child.ChildElements() {
				val, _ := strconv.ParseBool(child.Text())

				switch child.Tag {
				case "status":
					this.subStatus = val
				case "update":
					this.subUpdate = val
				}
			}
		}
	}

	return nil
}

func (this *IntercomClient) Run(ctx context.Context, pubEndpoint, pullEndpoint string) error {
	// Use ZeroMQ PUB endpoint specified in `intercom` config block if provided.
	if this.pubEndpoint != "" {
		pubEndpoint = this.pubEndpoint
	}

	// Use ZeroMQ PULL endpoint specified in `intercom` config block if provided.
	if this.pullEndpoint != "" {
		pullEndpoint = this.pullEndpoint
	}

	if this.endpoint == "" {
		return fmt.Errorf("no Intercom broker endpoint provided")
	}

	this.pusher = msgbus.MustNewPusher(pullEndpoint)

	subscriber := msgbus.MustNewSubscriber(pubEndpoint)
	subscriber.AddStatusHandler(this.handleMsgBusEnvelope(ctx))
	subscriber.AddUpdateHandler(this.handleMsgBusEnvelope(ctx))
	subscriber.Start("RUNTIME")

	endpoint, err := url.Parse(this.endpoint)
	if err != nil {
		return fmt.Errorf("parsing Intercom broker endpoint as URL: %w", err)
	}

	config := autopaho.ClientConfig{
		ServerUrls:                    []*url.URL{endpoint},
		KeepAlive:                     20,
		CleanStartOnInitialConnection: false,
		SessionExpiryInterval:         60,
		OnConnectionUp: func(mgr *autopaho.ConnectionManager, _ *paho.Connack) {
			this.log("[DEBUG] Intercom client connection up")

			var subs []paho.SubscribeOptions

			if this.subStatus {
				subs = append(subs, paho.SubscribeOptions{Topic: "devices/+/status", QoS: 0, NoLocal: true})
			}

			if this.subUpdate {
				subs = append(subs, paho.SubscribeOptions{Topic: "devices/+/update", QoS: 0, NoLocal: true})
			}

			if _, err := mgr.Subscribe(ctx, &paho.Subscribe{Subscriptions: subs}); err != nil {
				this.log("[ERROR] unable to subscribe to Intercom status and update topics: %v", err)
			}

			this.log("[DEBUG] subscribed to Intercom status and update topics")
		},
		OnConnectError: func(err error) { this.log("[ERROR] unable to connect to Intercom broker: %v", err) },
		ClientConfig: paho.ClientConfig{
			ClientID: this.id,
			// OnPublishReceived is a slice of functions that will be called when a message is received.
			// You can write the function(s) yourself or use the supplied Router
			OnPublishReceived: []func(paho.PublishReceived) (bool, error){this.handleIntercom},
			OnClientError:     func(err error) { this.log("[ERROR] client error: %v", err) },
			OnServerDisconnect: func(d *paho.Disconnect) {
				if d.Properties != nil {
					this.log("[WARN] server requested disconnect: %s", d.Properties.ReasonString)
				} else {
					this.log("[WARN] server requested disconnect: reason code %d", d.ReasonCode)
				}
			},
		},
	}

	this.client, err = autopaho.NewConnection(ctx, config)
	if err != nil {
		return fmt.Errorf("creating new connection to Intercom broker: %w", err)
	}

	if err = this.client.AwaitConnection(ctx); err != nil {
		return fmt.Errorf("waiting for new connection to Intercom broker: %w", err)
	}

	return nil
}

func (this *IntercomClient) handleIntercom(pub paho.PublishReceived) (bool, error) {
	if strings.HasSuffix(pub.Packet.Topic, "/status") {
		var points []msgbus.Point

		if err := json.Unmarshal(pub.Packet.Payload, &points); err != nil {
			return false, fmt.Errorf("unmarshaling status points: %w", err)
		}

		env, err := msgbus.NewEnvelope(this.name, msgbus.Status{Measurements: points})
		if err != nil {
			return false, fmt.Errorf("creating status message: %w", err)
		}

		if err := this.pusher.Push("RUNTIME", env); err != nil {
			return false, fmt.Errorf("sending status message: %w", err)
		}
	}

	if strings.HasSuffix(pub.Packet.Topic, "/update") {
		var points []msgbus.Point

		if err := json.Unmarshal(pub.Packet.Payload, &points); err != nil {
			return false, fmt.Errorf("unmarshaling update points: %w", err)
		}

		env, err := msgbus.NewEnvelope(this.name, msgbus.Update{Updates: points})
		if err != nil {
			return false, fmt.Errorf("creating update message: %w", err)
		}

		if err := this.pusher.Push("RUNTIME", env); err != nil {
			return false, fmt.Errorf("sending update message: %w", err)
		}
	}

	return true, nil
}

func (this *IntercomClient) handleMsgBusEnvelope(ctx context.Context) func(msgbus.Envelope) {
	return func(env msgbus.Envelope) {
		if env.Sender() == this.name {
			return
		}

		switch env.Kind {
		case msgbus.ENVELOPE_STATUS:
			if !this.pubStatus {
				return
			}

			status, err := env.Status()
			if err != nil {
				if !errors.Is(err, msgbus.ErrKindNotStatus) {
					this.log("[ERROR] getting Status message from envelope: %v", err)
				}

				return
			}

			topic := fmt.Sprintf("devices/%s/status", this.name)

			payload, err := json.Marshal(status.Measurements)
			if err != nil {
				this.log("[ERROR] marshaling measurements from envelope: %v", err)
				return
			}

			this.log("[DEBUG] publishing %d points to topic %s", len(status.Measurements), topic)

			queue := &autopaho.QueuePublish{Publish: &paho.Publish{Topic: topic, QoS: 0, Payload: payload}}
			this.client.PublishViaQueue(ctx, queue)
		case msgbus.ENVELOPE_UPDATE:
			if !this.pubUpdate {
				return
			}

			update, err := env.Update()
			if err != nil {
				if !errors.Is(err, msgbus.ErrKindNotUpdate) {
					this.log("[ERROR] getting Update message from envelope: %v", err)
				}

				return
			}

			topic := fmt.Sprintf("devices/%s/update", this.name)

			payload, err := json.Marshal(update.Updates)
			if err != nil {
				this.log("[ERROR] marshaling updates from envelope: %v", err)
				return
			}

			this.log("[DEBUG] publishing %d points to topic %s", len(update.Updates), topic)

			queue := &autopaho.QueuePublish{Publish: &paho.Publish{Topic: topic, QoS: 0, Payload: payload}}
			this.client.PublishViaQueue(ctx, queue)
		}
	}
}

func (this IntercomClient) log(format string, a ...any) {
	fmt.Printf("[%s] %s\n", this.name, fmt.Sprintf(format, a...))
}
