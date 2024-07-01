package server

import (
	"context"
	"encoding/binary"
	"fmt"
	"strconv"
	"strings"

	otsim "github.com/patsec/ot-sim"
	"github.com/patsec/ot-sim/sunspec/common"

	"actshad.dev/mbserver"
	"github.com/beevik/etree"
)

type Factory struct{}

func (Factory) NewModule(e *etree.Element) (otsim.Module, error) {
	name := e.SelectAttrValue("name", "sunspec")
	return New(name), nil
}

type SunSpecServer struct {
	pullEndpoint string
	pubEndpoint  string

	name     string
	endpoint string

	registers map[int]common.Register
	tags      map[string]any
}

func New(name string) *SunSpecServer {
	return &SunSpecServer{
		name:      name,
		registers: make(map[int]common.Register),
		tags:      make(map[string]any),
	}
}

func (this SunSpecServer) Name() string {
	return this.name
}

func (this *SunSpecServer) Configure(e *etree.Element) error {
	this.registers[40000] = common.IdentifierRegister

	var (
		addr   = 40002 // start addr at 40002 after well known identifier
		points = make(map[string]int)
		model1 bool
	)

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

			model, err := common.GetModelSchema(id)
			if err != nil {
				return fmt.Errorf("getting schema for model %d: %w", id, err)
			}

			for _, p := range model.Group.Points {
				dt := string(p.Type)
				if dt == string(common.PointTypeString) {
					dt = fmt.Sprintf("string%d", p.Size)
				}

				r := common.Register{
					DataType: dt,
					Tag:      p.Name,
				}

				if p.Name == "ID" {
					r.InternalValue = float64(id)
				}

				if p.Name == "L" {
					r.InternalValue = float64(common.GetModelLength(model))
				}

				if p.Type == common.PointTypePad {
					r.InternalValue = 0
				}

				if err := r.Init(); err != nil {
					return fmt.Errorf("initializing register %d: %w", addr, err)
				}

				this.registers[addr] = r
				points[p.Name] = addr

				addr += p.Size
			}

			for _, child := range child.ChildElements() {
				addr, ok := points[child.Tag]
				if !ok {
					return fmt.Errorf("no SunSpec point named %s in model %d", child.Tag, id)
				}

				r := this.registers[addr]

				if strings.HasPrefix(r.DataType, "string") {
					r.InternalString = child.Text()
				} else {
					val, err := strconv.ParseFloat(child.Text(), 64)
					if err != nil {
						return fmt.Errorf("parsing value %s for point %s in model %d: %w", child.Text(), child.Tag, id, err)
					}

					r.InternalValue = val
				}

				this.registers[addr] = r
			}

			if id == 1 {
				model1 = true
			}
		}
	}

	this.registers[addr] = common.EndRegister
	this.registers[addr+1] = common.EndRegisterLength

	if !model1 {
		return fmt.Errorf("SunSpec Model 1 not configured as first model")
	}

	return nil
}

func (this *SunSpecServer) Run(ctx context.Context, pubEndpoint, pullEndpoint string) error {
	server := mbserver.NewServer()

	server.RegisterContextFunctionHandler(3, this.readHoldingRegisters)

	go func() {
		<-ctx.Done()
		server.Close()
	}()

	if this.endpoint != "" {
		if err := server.ListenTCP(this.endpoint); err != nil {
			return fmt.Errorf("listening for TCP connections on %s: %w", this.endpoint, err)
		}
	}

	return nil
}

func (this SunSpecServer) readHoldingRegisters(_ context.Context, f mbserver.Framer) ([]byte, *mbserver.Exception) {
	var (
		data  = f.GetData()
		start = int(binary.BigEndian.Uint16(data[0:2]))
		count = int(binary.BigEndian.Uint16(data[2:4]))
		size  int
	)

	data = nil

	for addr := start; addr < start+count; {
		reg, ok := this.registers[addr]
		if !ok {
			return nil, &mbserver.IllegalDataAddress
		}

		buf, err := reg.Bytes(this.tags[reg.Tag])

		if err != nil {
			return nil, &mbserver.SlaveDeviceFailure
		}

		size = size + (reg.Count * 2)
		data = append(data, buf...)

		addr = addr + reg.Count
	}

	return append([]byte{byte(size)}, data...), &mbserver.Success
}

func init() {
	otsim.AddModuleFactory("sunspec", new(Factory))
}
