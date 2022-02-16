package msgbus

import (
	"encoding/json"
	"fmt"
)

const (
	ENVELOPE_HEALTHCHECK EnvelopeKind = "HealthCheck"
)

var (
	ErrKindNotHealthCheck error = fmt.Errorf("not a HealthCheck message")
)

type HealthCheck struct {
	State string `json:"state"`
}

func NewHealthCheckEnvelope(sender string, hc HealthCheck) (Envelope, error) {
	var env Envelope

	raw, err := json.Marshal(hc)
	if err != nil {
		return env, fmt.Errorf("marshaling HealthCheck envelope contents: %w", err)
	}

	env = Envelope{
		Version: "v1",
		Kind:    ENVELOPE_HEALTHCHECK,
		Metadata: map[string]string{
			"sender": sender,
		},
		Contents: raw,
	}

	return env, nil
}

func (this Envelope) HealthCheck() (HealthCheck, error) {
	var hc HealthCheck

	if this.Kind != ENVELOPE_HEALTHCHECK {
		return hc, ErrKindNotHealthCheck
	}

	if err := json.Unmarshal(this.Contents, &hc); err != nil {
		return hc, fmt.Errorf("unmarshaling HealthCheck message: %w", err)
	}

	return hc, nil
}
