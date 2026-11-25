package msgbus

import (
	"sync"
	"testing"
	"time"

	zmq "github.com/pebbe/zmq4"
)

// Modules push runtime messages and metrics from different goroutines, so a
// Pusher must keep each message's frames together.
func TestPushFromGoroutines(t *testing.T) {
	pull, err := zmq.NewSocket(zmq.PULL)
	if err != nil {
		t.Fatal(err)
	}

	defer pull.Close()

	if err := pull.Bind("tcp://127.0.0.1:*"); err != nil {
		t.Fatal(err)
	}

	// Fail instead of hanging if a message never arrives.
	if err := pull.SetRcvtimeo(10 * time.Second); err != nil {
		t.Fatal(err)
	}

	endpoint, _ := pull.GetLastEndpoint()

	pusher, err := NewPusher(endpoint)
	if err != nil {
		t.Fatal(err)
	}

	points := make([]Point, 1000)
	for i := range points {
		points[i] = Point{Tag: "tag", Value: float64(i)}
	}

	status, _ := NewEnvelope("test", Status{Measurements: points})
	metrics, _ := NewMetricEnvelope("test", Metrics{Updates: []Metric{{Kind: METRIC_COUNTER, Name: "count"}}})

	const count = 2000

	var wg sync.WaitGroup

	for topic, env := range map[string]Envelope{"RUNTIME": status, "HEALTH": metrics} {
		wg.Add(1)

		go func(topic string, env Envelope) {
			defer wg.Done()

			for i := 0; i < count; i++ {
				if err := pusher.Push(topic, env); err != nil {
					t.Error(err)
					return
				}
			}
		}(topic, env)
	}

	kinds := map[string]EnvelopeKind{"RUNTIME": ENVELOPE_STATUS, "HEALTH": ENVELOPE_METRIC}

	for i := 0; i < 2*count; i++ {
		msg, err := pull.RecvMessage(0)
		if err != nil {
			t.Fatal(err)
		}

		if len(msg) != 2 {
			t.Fatalf("message %d has %d frames", i, len(msg))
		}

		env, err := ParseEnvelope([]byte(msg[1]))
		if err != nil {
			t.Fatalf("message %d on topic %s: %v", i, msg[0], err)
		}

		if env.Kind != kinds[msg[0]] {
			t.Fatalf("message %d on topic %s is a %s message", i, msg[0], env.Kind)
		}
	}

	wg.Wait()
}
