package msgbus

import (
	"encoding/json"
	"fmt"
)

type (
	ConfirmationErrors map[string]string
)

const (
	ENVELOPE_STATUS       EnvelopeKind = "Status"
	ENVELOPE_UPDATE       EnvelopeKind = "Update"
	ENVELOPE_CONFIRMATION EnvelopeKind = "Confirmation"
)

var (
	ErrKindNotStatus       error = fmt.Errorf("not a Status message")
	ErrKindNotUpdate       error = fmt.Errorf("not an Update message")
	ErrKindNotConfirmation error = fmt.Errorf("not a Confirmation message")
)

type Point struct {
	Tag    string  `json:"tag"`
	Value  float64 `json:"value"`
	Tstamp uint64  `json:"ts"`
}

type Status struct {
	Measurements []Point `json:"measurements"`
}

type Update struct {
	Updates   []Point `json:"updates"`
	Recipient string  `json:"recipient"`
	Confirm   string  `json:"confirm"`
}

type Confirmation struct {
	Confirm string             `json:"confirm"`
	Errors  ConfirmationErrors `json:"errors"`
}

func NewStatusEnvelope(sender string, status Status) (Envelope, error) {
	var env Envelope

	raw, err := json.Marshal(status)
	if err != nil {
		return env, fmt.Errorf("marshaling Status envelope contents: %w", err)
	}

	env = Envelope{
		Version: "v1",
		Kind:    ENVELOPE_STATUS,
		Metadata: map[string]string{
			"sender": sender,
		},
		Contents: raw,
	}

	return env, nil
}

func NewUpdateEnvelope(sender string, update Update) (Envelope, error) {
	var env Envelope

	raw, err := json.Marshal(update)
	if err != nil {
		return env, fmt.Errorf("marshaling Update envelope contents: %w", err)
	}

	env = Envelope{
		Version: "v1",
		Kind:    ENVELOPE_UPDATE,
		Metadata: map[string]string{
			"sender": sender,
		},
		Contents: raw,
	}

	return env, nil
}

func NewConfirmationEnvelope(sender string, conf Confirmation) (Envelope, error) {
	var env Envelope

	raw, err := json.Marshal(conf)
	if err != nil {
		return env, fmt.Errorf("marshaling Confirmation envelope contents: %w", err)
	}

	env = Envelope{
		Version: "v1",
		Kind:    ENVELOPE_CONFIRMATION,
		Metadata: map[string]string{
			"sender": sender,
		},
		Contents: raw,
	}

	return env, nil
}

func (this Envelope) Status() (Status, error) {
	var status Status

	if this.Kind != ENVELOPE_STATUS {
		return status, ErrKindNotStatus
	}

	if err := json.Unmarshal(this.Contents, &status); err != nil {
		return status, fmt.Errorf("unmarshaling Status message: %w", err)
	}

	return status, nil
}

func (this Envelope) Update() (Update, error) {
	var update Update

	if this.Kind != ENVELOPE_UPDATE {
		return update, ErrKindNotUpdate
	}

	if err := json.Unmarshal(this.Contents, &update); err != nil {
		return update, fmt.Errorf("unmarshaling Update message: %w", err)
	}

	return update, nil
}

func (this Envelope) Conformation() (Confirmation, error) {
	var conf Confirmation

	if this.Kind != ENVELOPE_CONFIRMATION {
		return conf, ErrKindNotConfirmation
	}

	if err := json.Unmarshal(this.Contents, &conf); err != nil {
		return conf, fmt.Errorf("unmarshaling Confirmation message: %w", err)
	}

	return conf, nil
}
