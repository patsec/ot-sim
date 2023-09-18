package msgbus

import (
	"encoding/json"
	"fmt"
)

type (
	ConfirmationResults map[string]any
	ConfirmationErrors  map[string]string
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

func (Status) Kind() EnvelopeKind {
	return ENVELOPE_STATUS
}

type Update struct {
	Updates   []Point `json:"updates"`
	Recipient string  `json:"recipient"`
	Confirm   string  `json:"confirm"`
}

func (Update) Kind() EnvelopeKind {
	return ENVELOPE_UPDATE
}

type Confirmation struct {
	Confirm string              `json:"confirm"`
	Results ConfirmationResults `json:"results"`
	Errors  ConfirmationErrors  `json:"errors"`
}

func (Confirmation) Kind() EnvelopeKind {
	return ENVELOPE_CONFIRMATION
}

type IEnvelope interface {
	Status | Update | ModuleControl | Confirmation
	Kind() EnvelopeKind
}

func NewEnvelope[T IEnvelope](sender string, contents T) (Envelope, error) {
	var env Envelope

	raw, err := json.Marshal(contents)
	if err != nil {
		return env, fmt.Errorf("marshaling envelope contents: %w", err)
	}

	env = Envelope{
		Version: "v1",
		Kind:    contents.Kind(),
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
