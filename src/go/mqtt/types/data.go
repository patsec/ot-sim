package types

import (
	"bytes"
	"fmt"
	"text/template"
)

type Data struct {
	Epoch     int64
	Timestamp string
	Client    string
	Topic     string
	Value     any
}

func (this Data) Execute(tmpl *template.Template) (string, error) {
	var buf bytes.Buffer

	if err := tmpl.Execute(&buf, this); err != nil {
		return "", fmt.Errorf("executing template: %w", err)
	}

	return buf.String(), nil
}
