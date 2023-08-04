package msgbus

import (
	"fmt"

	zmq "github.com/pebbe/zmq4"
)

type (
	StatusHandler      func(Envelope)
	UpdateHandler      func(Envelope)
	HealthCheckHandler func(Envelope)
)

type Subscriber struct {
	ctx    *zmq.Context
	socket *zmq.Socket

	running bool

	statusHandlers      []StatusHandler
	updateHandlers      []UpdateHandler
	healthCheckHandlers []HealthCheckHandler
}

func MustNewSubscriber(endpoint string) *Subscriber {
	sub, err := NewSubscriber(endpoint)
	if err != nil {
		panic(err)
	}

	return sub
}

func NewSubscriber(endpoint string) (*Subscriber, error) {
	ctx, err := zmq.NewContext()
	if err != nil {
		return nil, fmt.Errorf("creating ZMQ context: %w", err)
	}

	socket, err := ctx.NewSocket(zmq.SUB)
	if err != nil {
		return nil, fmt.Errorf("creating ZMQ SUB socket: %w", err)
	}

	if err := socket.Connect(endpoint); err != nil {
		return nil, fmt.Errorf("connecting ZMQ SUB socket to %s: %w", endpoint, err)
	}

	if err := socket.SetLinger(0); err != nil {
		return nil, fmt.Errorf("setting ZMQ SUB socket linger: %w", err)
	}

	return &Subscriber{ctx: ctx, socket: socket}, nil
}

func (this *Subscriber) AddStatusHandler(handler StatusHandler) {
	this.statusHandlers = append(this.statusHandlers, handler)
}

func (this *Subscriber) AddUpdateHandler(handler UpdateHandler) {
	this.updateHandlers = append(this.updateHandlers, handler)
}

func (this *Subscriber) AddHealthCheckHandler(handler HealthCheckHandler) {
	this.healthCheckHandlers = append(this.healthCheckHandlers, handler)
}

func (this Subscriber) Start(topic string) {
	this.running = true
	go this.run(topic)
}

func (this Subscriber) Stop() {
	this.running = false

	this.socket.Close()
	this.ctx.Term()
}

func (this Subscriber) run(topic string) {
	this.socket.SetSubscribe(topic)

	for this.running {
		msg, err := this.socket.RecvMessage(0)
		if err != nil {
			fmt.Printf("[ERROR] reading from ZMQ SUB socket: %v\n", err)
			continue
		}

		// This shouldn't ever really happen...
		if msg[0] != topic {
			continue
		}

		env, err := ParseEnvelope([]byte(msg[1]))
		if err != nil {
			fmt.Printf("[ERROR] creating envelope from message: %v\n", err)
			continue
		}

		switch env.Kind {
		case EnvelopeKind(ENVELOPE_STATUS):
			for _, handler := range this.statusHandlers {
				handler(env)
			}
		case EnvelopeKind(ENVELOPE_UPDATE):
			for _, handler := range this.updateHandlers {
				handler(env)
			}
		case EnvelopeKind(ENVELOPE_HEALTHCHECK):
			for _, handler := range this.healthCheckHandlers {
				handler(env)
			}
		}
	}
}
