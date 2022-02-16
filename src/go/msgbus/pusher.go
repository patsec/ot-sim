package msgbus

import (
	"encoding/json"
	"fmt"

	zmq "github.com/pebbe/zmq4"
)

type Pusher struct {
	ctx    *zmq.Context
	socket *zmq.Socket
}

func MustNewPusher(endpoint string) *Pusher {
	push, err := NewPusher(endpoint)
	if err != nil {
		panic(err)
	}

	return push
}

func NewPusher(endpoint string) (*Pusher, error) {
	ctx, err := zmq.NewContext()
	if err != nil {
		return nil, fmt.Errorf("creating ZMQ context: %w", err)
	}

	socket, err := ctx.NewSocket(zmq.PUSH)
	if err != nil {
		return nil, fmt.Errorf("creating ZMQ PUSH socket: %w", err)
	}

	if err := socket.Connect(endpoint); err != nil {
		return nil, fmt.Errorf("connecting ZMQ PUSH socket to %s: %w", endpoint, err)
	}

	if err := socket.SetLinger(0); err != nil {
		return nil, fmt.Errorf("setting ZMQ PUSH socket linger: %w", err)
	}

	return &Pusher{ctx: ctx, socket: socket}, nil
}

func (this Pusher) Push(topic string, env Envelope) error {
	body, err := json.Marshal(env)
	if err != nil {
		return fmt.Errorf("marshaling Envelope for topic %s: %w", topic, err)
	}

	if _, err := this.socket.SendMessage(topic, string(body)); err != nil {
		return fmt.Errorf("sending Envelope to topic %s: %w", topic, err)
	}

	return nil
}

func (this Pusher) PushString(topic, format string, a ...any) error {
	msg := fmt.Sprintf(format, a...)

	if _, err := this.socket.SendMessage(topic, msg); err != nil {
		return fmt.Errorf("sending string to topic %s: %w", topic, err)
	}

	return nil
}
