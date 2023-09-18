package cpu

import (
	"context"
	"fmt"

	zmq "github.com/pebbe/zmq4"
)

type MsgBusHandler func(string, string) error

func MonitorMsgBusChannel(ctx context.Context, endpoint, topic string, handlers []MsgBusHandler, errors chan error) {
	sendErr := func(err error) {
		if errors != nil {
			errors <- err
		}
	}

	socket, err := zmq.NewSocket(zmq.SUB)
	if err != nil {
		sendErr(fmt.Errorf("creating new SUB socket: %w", err))
		return
	}

	if err := socket.Connect(endpoint); err != nil {
		sendErr(fmt.Errorf("connecting to publisher: %w", err))
		return
	}

	socket.SetSubscribe(topic)

	for {
		select {
		case <-ctx.Done():
			socket.Close()
			return
		default:
			msg, err := socket.RecvMessage(0)
			if err != nil {
				sendErr(fmt.Errorf("receiving message from publisher: %w", err))
				return
			}

			// This shouldn't ever really happen...
			if msg[0] != topic {
				continue
			}

			for _, handler := range handlers {
				if err := handler(topic, msg[1]); err != nil {
					sendErr(fmt.Errorf("running handler: %w", err))
					return
				}
			}
		}
	}
}
