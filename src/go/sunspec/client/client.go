package client

import (
	"context"
	"fmt"
	"strconv"

	otsim "github.com/patsec/ot-sim"
	"github.com/patsec/ot-sim/sunspec/common"

	"actshad.dev/modbus"
	"github.com/beevik/etree"
)

type Factory struct{}

func (Factory) NewModule(e *etree.Element) (otsim.Module, error) {
	name := e.SelectAttrValue("name", "sunspec")
	return New(name), nil
}

type SunSpecClient struct {
	pullEndpoint string
	pubEndpoint  string

	name     string
	id       int
	endpoint string

	models    []int
	registers map[int]common.Register
}

func New(name string) *SunSpecClient {
	return &SunSpecClient{
		name:      name,
		registers: make(map[int]common.Register),
	}
}

func (this SunSpecClient) Name() string {
	return this.name
}

func (this *SunSpecClient) Configure(e *etree.Element) error {
	for _, child := range e.ChildElements() {
		switch child.Tag {
		case "pull-endpoint":
			this.pullEndpoint = child.Text()
		case "pub-endpoint":
			this.pubEndpoint = child.Text()
		case "endpoint":
			this.endpoint = child.Text()
		case "model":
			attr := child.SelectAttr("id")
			if attr == nil {
				return fmt.Errorf("missing 'id' attribute for SunSpec model")
			}

			id, err := strconv.Atoi(attr.Value)
			if err != nil {
				return fmt.Errorf("parsing model ID %s: %w", attr.Value, err)
			}

			this.models = append(this.models, id)
		}
	}

	return nil
}

func (this *SunSpecClient) Run(ctx context.Context, pubEndpoint, pullEndpoint string) error {
	var handler modbus.ClientHandler

	handler = modbus.NewTCPClientHandler(this.endpoint)
	handler.(*modbus.TCPClientHandler).SlaveId = byte(this.id)

	client := modbus.NewClient(handler)

	r := common.IdentifierRegister

	data, err := client.ReadHoldingRegisters(40000, uint16(r.Count))
	if err != nil {
		return fmt.Errorf("reading identifier from SunSpec device %s: %w", this.endpoint, err)
	}

	value, err := r.Value(data)
	if err != nil {
		return fmt.Errorf("parsing identifier from SunSpec device %s: %w", this.endpoint, err)
	}

	if value != common.SunSpecIdentifier {
		return fmt.Errorf("invalid identifier provided by remote device")
	}

	// start addr at 40002 after well known identifier
	addr := 40002

	r = common.Register{DataType: "uint16"}

	if err := r.Init(); err != nil {
		return fmt.Errorf("initializing generic model register %d: %w", addr, err)
	}

	data, err = client.ReadHoldingRegisters(uint16(addr), uint16(r.Count))
	if err != nil {
		return fmt.Errorf("reading model ID from SunSpec device %s: %w", this.endpoint, err)
	}

	value, err = r.Value(data)
	if err != nil {
		return fmt.Errorf("parsing model ID from SunSpec device %s: %w", this.endpoint, err)
	}

	if value != 1 {
		return fmt.Errorf("remote SunSpec device missing required Model 1")
	}

	addr += r.Count

	data, err = client.ReadHoldingRegisters(uint16(addr), uint16(r.Count))
	if err != nil {
		return fmt.Errorf("reading model 1 length from SunSpec device %s: %w", this.endpoint, err)
	}

	value, err = r.Value(data)
	if err != nil {
		return fmt.Errorf("parsing model 1 length from SunSpec device %s: %w", this.endpoint, err)
	}

	addr += r.Count

	data, err = client.ReadHoldingRegisters(uint16(addr), uint16(value))
	if err != nil {
		return fmt.Errorf("reading rest of model 1 data from SunSpec device %s: %w", this.endpoint, err)
	}

	model, err := common.GetModelSchema(1)
	if err != nil {
		return fmt.Errorf("getting model schema: %w", err)
	}

	// track position of current model data array
	var pos int

	for idx, point := range model.Group.Points {
		if idx < 2 {
			continue
		}

		dt := string(point.Type)
		if dt == string(common.PointTypeString) {
			dt = fmt.Sprintf("string%d", point.Size)
		}

		r := common.Register{
			DataType: dt,
			Tag:      point.Name,
		}

		if err := r.Init(); err != nil {
			return fmt.Errorf("initializing register %d: %w", addr, err)
		}

		bytes := data[pos : pos+(point.Size*2)]

		if point.Type == common.PointTypeString {
			value, err := r.String(bytes)
			if err != nil {
				return fmt.Errorf("parsing string value for SunSpec point: %w", err)
			}

			fmt.Printf("%s - %s\n", point.Name, value)
		} else {
			value, err := r.Value(bytes)
			if err != nil {
				return fmt.Errorf("parsing value for SunSpec point: %w", err)
			}

			fmt.Printf("%s - %f\n", point.Name, value)
		}

		pos += point.Size * 2
	}

	return nil
}

func (this SunSpecClient) log(format string, a ...any) {
	fmt.Printf("[%s] %s\n", this.name, fmt.Sprintf(format, a...))
}
