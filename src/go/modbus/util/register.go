package util

import (
	"bytes"
	"encoding/binary"
	"fmt"
	"math"
	"slices"
)

var (
	validRegisterTypes = []string{"coil", "discrete", "input", "holding"}
	validDataTypes     = []string{"int16", "uint16", "int32", "uint32", "int64", "uint64", "float", "double"}
)

type Register struct {
	Type     string
	DataType string

	Addr    int
	Count   int
	Scaling int

	Tag string
}

func (this *Register) Init() error {
	if !slices.Contains(validRegisterTypes, this.Type) {
		return fmt.Errorf("invalid register type '%s' provided", this.Type)
	}

	if this.Type == "input" || this.Type == "holding" {
		if !slices.Contains(validDataTypes, this.DataType) {
			return fmt.Errorf("invalid register data type '%s' provided", this.DataType)
		}
	}

	switch this.DataType {
	case "int16", "uint16":
		this.Count = 1
	case "int32", "uint32":
		this.Count = 2
	case "int64", "uint64":
		this.Count = 4
	case "float":
		this.Count = 2
	case "double":
		this.Count = 4
	}

	// backwards compatibility
	this.Scaling = int(math.Abs(float64(this.Scaling)))

	return nil
}

func (this Register) Scaled(value float64) float64 {
	switch this.Type {
	case "coil", "discrete":
		return value
	case "input", "holding":
		switch this.DataType {
		case "int16", "uint16", "int32", "uint32", "int64", "uint64":
			return value * math.Pow(10, float64(this.Scaling))
		case "float", "double":
			return value
		}
	}

	return value
}

func (this Register) Bytes(value float64) ([]byte, error) {
	var v any

	switch this.Type {
	case "coil", "discrete":
		if value == 0 {
			v = uint16(0)
		} else {
			v = uint16(65280) // 0xFF00, per Modbus spec
		}

		buf := new(bytes.Buffer)

		if err := binary.Write(buf, binary.BigEndian, v); err != nil {
			return nil, fmt.Errorf("writing bytes for %s %d: %w", this.Type, this.Addr, err)
		}

		return buf.Bytes(), nil
	case "input", "holding":
		switch this.DataType {
		case "int16":
			v = int16(value * math.Pow(10, float64(this.Scaling)))
		case "uint16":
			v = uint16(value * math.Pow(10, float64(this.Scaling)))
		case "int32":
			v = int32(value * math.Pow(10, float64(this.Scaling)))
		case "uint32":
			v = uint32(value * math.Pow(10, float64(this.Scaling)))
		case "int64":
			v = int64(value * math.Pow(10, float64(this.Scaling)))
		case "uint64":
			v = uint64(value * math.Pow(10, float64(this.Scaling)))
		case "float":
			v = float32(value)
		case "double":
			v = float64(value)
		default:
			return nil, fmt.Errorf("invalid register data type %s for register %d", this.DataType, this.Addr)
		}

		buf := new(bytes.Buffer)

		if err := binary.Write(buf, binary.BigEndian, v); err != nil {
			return nil, fmt.Errorf("writing bytes for %s %d: %w", this.Type, this.Addr, err)
		}

		return buf.Bytes(), nil
	}

	return nil, fmt.Errorf("invalid register type %s for register %d", this.Type, this.Addr)
}

func (this Register) Value(data []byte) (float64, error) {
	switch this.Type {
	case "coil", "discrete":
		bits := BytesToBits(data)

		return float64(bits[0]), nil
	case "input", "holding":
		buf := bytes.NewReader(data)

		switch this.DataType {
		case "int16":
			var val int16

			if err := binary.Read(buf, binary.BigEndian, &val); err != nil {
				return 0, fmt.Errorf("reading int16 from %s %d: %w", this.Type, this.Addr, err)
			}

			return float64(val) * math.Pow(10, -float64(this.Scaling)), nil
		case "uint16":
			var val uint16

			if err := binary.Read(buf, binary.BigEndian, &val); err != nil {
				return 0, fmt.Errorf("reading uint16 from %s %d: %w", this.Type, this.Addr, err)
			}

			return float64(val) * math.Pow(10, -float64(this.Scaling)), nil
		case "int32":
			var val int32

			if err := binary.Read(buf, binary.BigEndian, &val); err != nil {
				return 0, fmt.Errorf("reading int32 from %s %d: %w", this.Type, this.Addr, err)
			}

			return float64(val) * math.Pow(10, -float64(this.Scaling)), nil
		case "uint32":
			var val uint32

			if err := binary.Read(buf, binary.BigEndian, &val); err != nil {
				return 0, fmt.Errorf("reading uint32 from %s %d: %w", this.Type, this.Addr, err)
			}

			return float64(val) * math.Pow(10, -float64(this.Scaling)), nil
		case "int64":
			var val int64

			if err := binary.Read(buf, binary.BigEndian, &val); err != nil {
				return 0, fmt.Errorf("reading int64 from %s %d: %w", this.Type, this.Addr, err)
			}

			return float64(val) * math.Pow(10, -float64(this.Scaling)), nil
		case "uint64":
			var val uint64

			if err := binary.Read(buf, binary.BigEndian, &val); err != nil {
				return 0, fmt.Errorf("reading uint64 from %s %d: %w", this.Type, this.Addr, err)
			}

			return float64(val) * math.Pow(10, -float64(this.Scaling)), nil
		case "float":
			var val float32

			if err := binary.Read(buf, binary.BigEndian, &val); err != nil {
				return 0, fmt.Errorf("reading float from %s %d: %w", this.Type, this.Addr, err)
			}

			return float64(val), nil
		case "double":
			var val float64

			if err := binary.Read(buf, binary.BigEndian, &val); err != nil {
				return 0, fmt.Errorf("reading double from %s %d: %w", this.Type, this.Addr, err)
			}

			return val, nil
		}

		return 0, fmt.Errorf("invalid register data type %s for register %d", this.DataType, this.Addr)
	}

	return 0, fmt.Errorf("invalid register type %s for register %d", this.Type, this.Addr)
}
