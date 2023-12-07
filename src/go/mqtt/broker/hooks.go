package broker

import (
	"bytes"
	"strconv"

	mochi "github.com/mochi-mqtt/server/v2"
	"github.com/mochi-mqtt/server/v2/packets"
	"github.com/patsec/ot-sim/msgbus"
)

type PublishToMsgBus struct {
	mochi.HookBase

	name string

	pusher *msgbus.Pusher
	topics map[string]string

	log func(string, ...any)
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

	this.log("[DEBUG] topic: %s -- payload: %s", p.TopicName, string(p.Payload))

	if tag, ok := this.topics[p.TopicName]; ok {
		var points []msgbus.Point

		value, err := strconv.ParseFloat(string(p.Payload), 64)
		if err != nil {
			this.log("[ERROR] parsing payload for topic %s to float64: %v", p.TopicName, err)
			return
		}

		points = append(points, msgbus.Point{Tag: tag, Value: value})

		env, err := msgbus.NewEnvelope(this.name, msgbus.Status{Measurements: points})
		if err != nil {
			this.log("[ERROR] creating status message: %v", err)
			return
		}

		if err := this.pusher.Push("RUNTIME", env); err != nil {
			this.log("[ERROR] sending status message: %v", err)
		}
	}
}
