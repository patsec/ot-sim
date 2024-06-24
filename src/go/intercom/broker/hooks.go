package broker

import (
	"bytes"
	"encoding/json"
	"strings"

	mochi "github.com/mochi-mqtt/server/v2"
	"github.com/mochi-mqtt/server/v2/packets"
	"github.com/patsec/ot-sim/msgbus"
)

type PublishToMsgBus struct {
	mochi.HookBase

	name   string
	pusher *msgbus.Pusher
	log    func(string, ...any)

	subStatus bool
	subUpdate bool
}

func (this *PublishToMsgBus) ID() string {
	return "publish-to-ot-sim-msg-bus"
}

func (this *PublishToMsgBus) Provides(b byte) bool {
	return bytes.Contains([]byte{
		mochi.OnPublished,
	}, []byte{b})
}

func (this *PublishToMsgBus) OnPublished(c *mochi.Client, p packets.Packet) {
	if c.ID == "inline" {
		return
	}

	if strings.HasSuffix(p.TopicName, "/status") {
		if !this.subStatus {
			return
		}

		this.log("[DEBUG] topic: %s -- payload: %s", p.TopicName, string(p.Payload))

		var points []msgbus.Point

		if err := json.Unmarshal(p.Payload, &points); err != nil {
			this.log("[ERROR] unmarshaling status points: %v", err)
			return
		}

		env, err := msgbus.NewEnvelope(this.name, msgbus.Status{Measurements: points})
		if err != nil {
			this.log("[ERROR] creating status message: %v", err)
			return
		}

		if err := this.pusher.Push("RUNTIME", env); err != nil {
			this.log("[ERROR] sending status message: %v", err)
			return
		}
	}

	if strings.HasSuffix(p.TopicName, "/update") {
		if !this.subUpdate {
			return
		}

		this.log("[DEBUG] topic: %s -- payload: %s", p.TopicName, string(p.Payload))

		var points []msgbus.Point

		if err := json.Unmarshal(p.Payload, &points); err != nil {
			this.log("[ERROR] unmarshaling update points: %v", err)
			return
		}

		env, err := msgbus.NewEnvelope(this.name, msgbus.Update{Updates: points})
		if err != nil {
			this.log("[ERROR] creating update message: %v", err)
			return
		}

		if err := this.pusher.Push("RUNTIME", env); err != nil {
			this.log("[ERROR] sending update message: %v", err)
			return
		}
	}
}
