package common

import (
	"bytes"
	"encoding/binary"
	"fmt"
	"math"
)

type Register struct {
	DataType string
	Name     string
	Model    int

	Count   int
	Scaling float64

	Tag           string
	ScaleRegister string

	InternalValue  float64
	InternalString string

	Raw []byte // used for storing raw bytes until SF is known
}

func (this *Register) Init() error {
	switch this.DataType {
	case "acc16", "bitfield16", "enum16", "int16", "pad", "sunssf", "uint16":
		this.Count = 1
	case "acc32", "bitfield32", "enum32", "float32", "int32", "uint32":
		this.Count = 2
	case "acc64":
		this.Count = 4
	case "string8":
		this.Count = 8
	case "string16":
		this.Count = 16
	default:
		return fmt.Errorf("unknown data type %s", this.DataType)
	}

	this.Scaling = math.Abs(this.Scaling)

	return nil
}

func (this Register) Bytes(value any, scaling float64) ([]byte, error) {
	fmt.Printf("getting bytes for %s\n", this.Name)

	if value == nil {
		switch this.DataType {
		case "string8", "string16":
			value = this.InternalString
		default:
			value = this.InternalValue
		}
	}

	if scaling == 0 {
		scaling = this.Scaling
	}

	var v any

	switch this.DataType {
	case "acc16", "uint16":
		v = uint16(value.(float64) * math.Pow(10, -scaling))
	case "acc32", "uint32":
		v = uint32(value.(float64) * math.Pow(10, -scaling))
	case "acc64":
		v = uint64(value.(float64) * math.Pow(10, -scaling))
	case "bitfield16", "enum16":
		v = uint16(value.(float64))
	case "bitfield32", "enum32":
		v = uint32(value.(float64))
	case "float32":
		v = float32(value.(float64))
	case "int16":
		v = int16(value.(float64) * math.Pow(10, -scaling))
	case "int32":
		v = int32(value.(float64) * math.Pow(10, -scaling))
	case "string8":
		var buf [16]byte

		copy(buf[:], value.(string))

		v = buf[:]
	case "string16":
		var buf [32]byte

		copy(buf[:], value.(string))

		v = buf[:]
	case "pad", "sunssf":
		v = int16(value.(float64))
	}

	buf := new(bytes.Buffer)

	if err := binary.Write(buf, binary.BigEndian, v); err != nil {
		return nil, fmt.Errorf("writing bytes for %s: %w", this.DataType, err)
	}

	return buf.Bytes(), nil
}

func (this Register) Value(data []byte, scaling float64) (float64, error) {
	if data == nil {
		return this.InternalValue, nil
	}

	if scaling == 0 {
		scaling = this.Scaling
	}

	buf := bytes.NewReader(data)

	switch this.DataType {
	case "acc16", "uint16":
		var value uint16

		if err := binary.Read(buf, binary.BigEndian, &value); err != nil {
			return 0, err
		}

		return float64(value) * math.Pow(10, scaling), nil
	case "acc32", "uint32":
		var value uint32

		if err := binary.Read(buf, binary.BigEndian, &value); err != nil {
			return 0, err
		}

		return float64(value) * math.Pow(10, scaling), nil
	case "acc64":
		var value uint64

		if err := binary.Read(buf, binary.BigEndian, &value); err != nil {
			return 0, err
		}

		return float64(value) * math.Pow(10, scaling), nil
	case "bitfield16", "enum16":
		var value uint16

		if err := binary.Read(buf, binary.BigEndian, &value); err != nil {
			return 0, err
		}

		return float64(value), nil
	case "bitfield32", "enum32":
		var value uint32

		if err := binary.Read(buf, binary.BigEndian, &value); err != nil {
			return 0, err
		}

		return float64(value), nil
	case "float32":
		var value float32

		if err := binary.Read(buf, binary.BigEndian, &value); err != nil {
			return 0, err
		}

		return float64(value), nil
	case "int16":
		var value int16

		if err := binary.Read(buf, binary.BigEndian, &value); err != nil {
			return 0, err
		}

		return float64(value) * math.Pow(10, scaling), nil
	case "int32":
		var value int32

		if err := binary.Read(buf, binary.BigEndian, &value); err != nil {
			return 0, err
		}

		return float64(value) * math.Pow(10, scaling), nil
	case "pad", "sunssf":
		var value int16

		if err := binary.Read(buf, binary.BigEndian, &value); err != nil {
			return 0, err
		}

		return float64(value), nil
	default:
		return 0, fmt.Errorf("%s is not a value register", this.Name)
	}
}

func (this Register) String(data []byte) (string, error) {
	if data == nil {
		return this.InternalString, nil
	}

	switch this.DataType {
	case "string8", "string16":
		return string(data), nil
	default:
		return "", fmt.Errorf("not a string register")
	}
}
