package cpu

import (
	"errors"
	"fmt"
	"net/http"
	"regexp"

	"github.com/patsec/ot-sim/msgbus"

	"github.com/prometheus/client_golang/prometheus"
	"github.com/prometheus/client_golang/prometheus/promauto"
	"github.com/prometheus/client_golang/prometheus/promhttp"
)

var (
	counters = make(map[string]prometheus.Counter)
	gauges   = make(map[string]prometheus.Gauge)

	nameRegex = regexp.MustCompile(`-|:|\.`)
)

func metricsHandler(topic, msg string) error {
	env, err := msgbus.ParseEnvelope([]byte(msg))
	if err != nil {
		return fmt.Errorf("creating new envelope: %w", err)
	}

	metrics, err := env.Metrics()
	if err != nil {
		if errors.Is(err, msgbus.ErrKindNotMetric) {
			return nil
		}

		return fmt.Errorf("getting metrics from envelope: %w", err)
	}

	for _, metric := range metrics.Updates {
		switch metric.Kind {
		case msgbus.METRIC_COUNTER:
			counter, ok := counters[metric.Name]
			if !ok {
				counter = promauto.NewCounter(prometheus.CounterOpts{
					Name: nameRegex.ReplaceAllString(metric.Name, "_"),
					Help: metric.Desc,
				})

				counters[metric.Name] = counter
			}

			counter.Add(metric.Value)
		case msgbus.METRIC_GAUGE:
			gauge, ok := gauges[metric.Name]
			if !ok {
				gauge = promauto.NewGauge(prometheus.GaugeOpts{
					Name: nameRegex.ReplaceAllString(metric.Name, "_"),
					Help: metric.Desc,
				})

				gauges[metric.Name] = gauge
			}

			gauge.Set(metric.Value)
		}
	}

	return nil
}

func init() {
	mux := http.NewServeMux()
	mux.Handle("/metrics", promhttp.Handler())

	server := http.Server{Addr: ":9100", Handler: mux}
	go server.ListenAndServe()
}
