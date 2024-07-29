package client

import (
	"fmt"

	"actshad.dev/modbus"
	"github.com/patsec/ot-sim/sunspec/common"
)

func confirmIdentifier(c modbus.Client) error {
	r := common.IdentifierRegister

	body, err := c.ReadHoldingRegisters(40000, uint16(r.Count))
	if err != nil {
		return fmt.Errorf("reading identifier: %w", err)
	}

	identifier, err := r.Value(body, 0.0)
	if err != nil {
		return fmt.Errorf("parsing identifier: %w", err)
	}

	if identifier != common.SunSpecIdentifier {
		return fmt.Errorf("invalid identifier provided by remote device")
	}

	return nil
}

func nextModel(c modbus.Client, a int) (int, int, error) {
	r := common.Register{DataType: "uint16"}

	if err := r.Init(); err != nil {
		return 0, 0, fmt.Errorf("initializing generic model register %d: %w", a, err)
	}

	// read model and length at same time
	d, err := c.ReadHoldingRegisters(uint16(a), 2)
	if err != nil {
		return 0, 0, fmt.Errorf("reading model ID and length: %w", err)
	}

	m, err := r.Value(d[0:2], 0.0)
	if err != nil {
		return 0, 0, fmt.Errorf("parsing model ID: %w", err)
	}

	l, err := r.Value(d[2:4], 0.0)
	if err != nil {
		return 0, 0, fmt.Errorf("parsing model length: %w", err)
	}

	return int(m), int(l), nil
}

func modelData(c modbus.Client, m, a, l int) (map[string]*common.Register, error) {
	regs := make(map[string]*common.Register)

	d, err := c.ReadHoldingRegisters(uint16(a), uint16(l))
	if err != nil {
		return nil, fmt.Errorf("reading Model %d data: %w", m, err)
	}

	s, err := common.GetModelSchema(m)
	if err != nil {
		return nil, fmt.Errorf("getting Model %d schema: %w", m, err)
	}

	// track position of current model data array
	var pos int

	for i, p := range s.Group.Points {
		if i < 2 {
			continue
		}

		dt := string(p.Type)
		if dt == string(common.PointTypeString) {
			dt = fmt.Sprintf("string%d", p.Size)
		}

		r := &common.Register{
			DataType: dt,
			Name:     p.Name,
			Model:    m,
		}

		switch sf := p.Sf.(type) {
		case nil:
			// noop
		case int:
			r.Scaling = float64(sf)
		case string:
			r.ScaleRegister = sf
		default:
			return nil, fmt.Errorf("unknown type when parsing scaling factor for %s", p.Name)
		}

		if err := r.Init(); err != nil {
			return nil, fmt.Errorf("initializing %s register: %w", p.Name, err)
		}

		r.Raw = d[pos : pos+(p.Size*2)]

		regs[r.Name] = r

		pos += p.Size * 2
	}

	return regs, nil
}
