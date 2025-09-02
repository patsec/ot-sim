package common

import (
	"embed"
	"encoding/json"
	"fmt"
)

//go:embed models/json/*
var schemas embed.FS

func GetModelSchema(id int) (SchemaJson, error) {
	var model SchemaJson

	schema, err := schemas.ReadFile(fmt.Sprintf("models/json/model_%d.json", id))
	if err != nil {
		return model, fmt.Errorf("reading model %d schema: %w", id, err)
	}

	if err := json.Unmarshal(schema, &model); err != nil {
		return model, fmt.Errorf("unmarshaling model %d schema: %w", id, err)
	}

	return model, nil
}

func GetModelLength(model SchemaJson) int {
	var length int

	for idx, point := range model.Group.Points {
		if idx < 2 {
			continue
		}

		length += point.Size
	}

	return length
}
