package broker

import (
	"context"
	"encoding/json"
	"errors"
	"fmt"

	"github.com/patsec/ot-sim/msgbus"

	"github.com/beevik/etree"
	mochi "github.com/mochi-mqtt/server/v2"
	"github.com/mochi-mqtt/server/v2/hooks/auth"
	"github.com/mochi-mqtt/server/v2/listeners"
)

/*
<intercom mode="broker">
	<endpoint>127.0.0.1:1883</endpoint>
</intercom>
*/

type IntercomBroker struct {
	pubEndpoint  string
	pullEndpoint string

	name     string
	endpoint string

	server *mochi.Server
}

func New(name string) *IntercomBroker {
	return &IntercomBroker{name: name}
}

func (this IntercomBroker) Name() string {
	return this.name
}

func (this *IntercomBroker) Configure(e *etree.Element) error {
	for _, child := range e.ChildElements() {
		switch child.Tag {
		case "pub-endpoint":
			this.pubEndpoint = child.Text()
		case "pull-endpoint":
			this.pullEndpoint = child.Text()
		case "endpoint":
			this.endpoint = child.Text()
		}
	}

	return nil
}

func (this *IntercomBroker) Run(ctx context.Context, pubEndpoint, pullEndpoint string) error {
	// Use ZeroMQ PUB endpoint specified in `intercom` config block if provided.
	if this.pubEndpoint != "" {
		pubEndpoint = this.pubEndpoint
	}

	// Use ZeroMQ PULL endpoint specified in `intercom` config block if provided.
	if this.pullEndpoint != "" {
		pullEndpoint = this.pullEndpoint
	}

	if this.endpoint == "" {
		return fmt.Errorf("no Intercom broker listener endpoint provided")
	}

	msgBusHook := &PublishToMsgBus{
		name:   this.name,
		pusher: msgbus.MustNewPusher(pullEndpoint),
		log:    this.log,
	}

	subscriber := msgbus.MustNewSubscriber(pubEndpoint)
	subscriber.AddUpdateHandler(this.handleMsgBusEnvelope)
	subscriber.Start("RUNTIME")

	this.server = mochi.New(&mochi.Options{InlineClient: true})
	this.server.AddHook(new(auth.AllowHook), nil)
	this.server.AddHook(msgBusHook, nil)

	l := listeners.NewTCP("t0", this.endpoint, nil)

	if err := this.server.AddListener(l); err != nil {
		return fmt.Errorf("adding TCP listener to Intercom broker: %w", err)
	}

	go func() {
		if err := this.server.Serve(); err != nil {
			this.log("[ERROR] serving Intercom broker: %v", err)
		}
	}()

	go func() {
		<-ctx.Done()
		this.server.Close()
	}()

	return nil
}

func (this *IntercomBroker) handleMsgBusEnvelope(env msgbus.Envelope) {
	if env.Sender() == this.name {
		return
	}

	switch env.Kind {
	case msgbus.ENVELOPE_STATUS:
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

		this.server.Publish(topic, payload, false, 0)
	case msgbus.ENVELOPE_UPDATE:
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

		this.server.Publish(topic, payload, false, 0)
	}
}

func (this IntercomBroker) log(format string, a ...any) {
	fmt.Printf("[%s] %s\n", this.name, fmt.Sprintf(format, a...))
}
