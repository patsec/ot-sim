package msgbus

import (
	"encoding/json"
	"fmt"
	"strings"
	"sync"
	"time"
)

type (
	MetricKind string
)

const (
	ENVELOPE_METRIC EnvelopeKind = "Metric"

	METRIC_COUNTER MetricKind = "Counter"
	METRIC_GAUGE   MetricKind = "Gauge"
)

var (
	ErrKindNotMetric error = fmt.Errorf("not a Metric message")
)

type MetricsPusher struct {
	sync.RWMutex

	metrics map[string]Metric
}

func NewMetricsPusher() *MetricsPusher {
	return &MetricsPusher{
		metrics: make(map[string]Metric),
	}
}

func (this *MetricsPusher) Start(pusher *Pusher, name string) {
	prefix := name + "_"

	go func() {
		for range time.Tick(5 * time.Second) {
			var updates []Metric

			this.RLock()

			for _, metric := range this.metrics {
				copy := metric

				if !strings.HasPrefix(copy.Name, prefix) {
					copy.Name = prefix + copy.Name
				}

				updates = append(updates, copy)
			}

			this.RUnlock()

			if len(updates) > 0 {
				env, err := NewMetricEnvelope(name, Metrics{Updates: updates})
				if err != nil {
					pusher.PushString("LOG", "[%s] [ERROR] %v", name, err)
					continue
				}

				if err := pusher.Push("HEALTH", env); err != nil {
					pusher.PushString("LOG", "[%s] [ERROR] %v", name, err)
				}
			}
		}
	}()
}

func (this *MetricsPusher) NewMetric(kind MetricKind, name, desc string) {
	this.Lock()
	defer this.Unlock()

	metric := Metric{
		Kind: kind,
		Name: name,
		Desc: desc,
	}

	this.metrics[name] = metric
}

func (this *MetricsPusher) IncrMetric(name string) {
	this.Lock()
	defer this.Unlock()

	if metric, ok := this.metrics[name]; ok {
		metric.Value += 1.0
		this.metrics[name] = metric
	}
}

func (this *MetricsPusher) IncrMetricBy(name string, val int) {
	this.Lock()
	defer this.Unlock()

	if metric, ok := this.metrics[name]; ok {
		metric.Value += float64(val)
		this.metrics[name] = metric
	}
}

func (this *MetricsPusher) SetMetric(name string, val float64) {
	this.Lock()
	defer this.Unlock()

	if metric, ok := this.metrics[name]; ok {
		metric.Value = val
		this.metrics[name] = metric
	}
}

type Metric struct {
	Kind  MetricKind `json:"kind"`
	Name  string     `json:"name"`
	Desc  string     `json:"desc"`
	Value float64    `json:"value"`
}

type Metrics struct {
	Updates []Metric `json:"metrics"`
}

func NewMetricEnvelope(sender string, metrics Metrics) (Envelope, error) {
	var env Envelope

	raw, err := json.Marshal(metrics)
	if err != nil {
		return env, fmt.Errorf("marshaling Metric envelope contents: %w", err)
	}

	env = Envelope{
		Version: "v1",
		Kind:    ENVELOPE_METRIC,
		Metadata: map[string]string{
			"sender": sender,
		},
		Contents: raw,
	}

	return env, nil
}

func (this Envelope) Metrics() (Metrics, error) {
	var metrics Metrics

	if this.Kind != ENVELOPE_METRIC {
		return metrics, ErrKindNotMetric
	}

	if err := json.Unmarshal(this.Contents, &metrics); err != nil {
		return metrics, fmt.Errorf("unmarshaling Metric message: %w", err)
	}

	return metrics, nil
}
