package msgbus

import (
	"encoding/json"
	"fmt"
)

type (
	EnvelopeKind     string
	EnvelopeMetadata map[string]string
)

type Envelope struct {
	Version  string           `json:"version"`
	Kind     EnvelopeKind     `json:"kind"`
	Metadata EnvelopeMetadata `json:"metadata"`
	Contents json.RawMessage  `json:"contents"`
}

func NewEnvelope(data []byte) (Envelope, error) {
	var env Envelope

	if err := json.Unmarshal(data, &env); err != nil {
		return env, fmt.Errorf("unmarshaling envelope: %w", err)
	}

	return env, nil
}

func (this Envelope) Sender() string {
	if this.Metadata == nil {
		return ""
	}

	if sender, ok := this.Metadata["sender"]; ok {
		return sender
	}

	return ""
}
