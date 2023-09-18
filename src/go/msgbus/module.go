package msgbus

import (
	"encoding/json"
	"fmt"
)

const (
	ENVELOPE_MODULE_CONTROL EnvelopeKind = "ModuleControl"
)

var (
	ErrKindNotModuleControl error = fmt.Errorf("not a ModuleControl message")
)

type ModuleControl struct {
	List    bool     `json:"list"`
	Enable  []string `json:"enable"`
	Disable []string `json:"disable"`

	Recipient string `json:"recipient"`
	Confirm   string `json:"confirm"`
}

func (ModuleControl) Kind() EnvelopeKind {
	return ENVELOPE_MODULE_CONTROL
}

func (this Envelope) ModuleControl() (ModuleControl, error) {
	var control ModuleControl

	if this.Kind != ENVELOPE_MODULE_CONTROL {
		return control, ErrKindNotModuleControl
	}

	if err := json.Unmarshal(this.Contents, &control); err != nil {
		return control, fmt.Errorf("unmarshaling ModuleControl message: %w", err)
	}

	return control, nil
}
