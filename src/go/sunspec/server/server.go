package server

import (
	"context"
	"encoding/binary"
	"errors"
	"fmt"
	"strconv"
	"strings"
	"sync"

	otsim "github.com/patsec/ot-sim"
	"github.com/patsec/ot-sim/msgbus"
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
	sync.RWMutex // used to protect registers, points, and tags maps

	pullEndpoint string
	pubEndpoint  string

	pusher *msgbus.Pusher

	name     string
	endpoint string

	registers map[int]*common.Register
	points    map[string]*common.Register
	tags      map[string]any
}

func New(name string) *SunSpecServer {
	return &SunSpecServer{
		name:      name,
		registers: make(map[int]*common.Register),
		points:    make(map[string]*common.Register),
		tags:      make(map[string]any),
	}
}

func (this SunSpecServer) Name() string {
	return this.name
}

func (this *SunSpecServer) Configure(e *etree.Element) error {
	this.registers[40000] = &common.IdentifierRegister

	var (
		addr   = 40002 // start addr at 40002 after well known identifier
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

				r := &common.Register{
					DataType: dt,
					Name:     p.Name,
					Model:    id,
				}

				if p.Name == "ID" {
					r.InternalValue = float64(id)
				}

				if p.Name == "L" {
					r.InternalValue = float64(common.GetModelLength(model))
				}

				/* // shouldn't need this since zero value for InternalValue is already 0.0
				if p.Type == common.PointTypePad {
					r.InternalValue = 0
				}
				*/

				if p.Sf != nil {
					switch sf := p.Sf.(type) {
					case int:
						r.Scaling = float64(sf)
					case float64:
						r.Scaling = sf
					case string:
						r.ScaleRegister = sf
					default:
						return fmt.Errorf("unknown type when parsing scaling factor for %s", p.Name)
					}
				}

				if err := r.Init(); err != nil {
					return fmt.Errorf("initializing register %d: %w", addr, err)
				}

				this.registers[addr] = r
				this.points[p.Name] = r

				addr += p.Size
			}

			for _, child := range child.ChildElements() {
				r, ok := this.points[child.Tag]
				if !ok {
					return fmt.Errorf("no SunSpec point named %s in model %d", child.Tag, id)
				}

				if strings.HasPrefix(r.DataType, "string") {
					r.InternalString = child.Text()
				} else {
					val, err := strconv.ParseFloat(child.Text(), 64)
					if err == nil {
						r.InternalValue = val
					} else {
						this.tags[child.Text()] = 0.0
						r.Tag = child.Text()
					}
				}

				this.registers[addr] = r
			}

			if id == 1 {
				model1 = true
			}
		}
	}

	this.registers[addr] = &common.EndRegister
	this.registers[addr+1] = &common.EndRegisterLength

	if !model1 {
		return fmt.Errorf("SunSpec Model 1 not configured as first model")
	}

	return nil
}

func (this *SunSpecServer) Run(ctx context.Context, pubEndpoint, pullEndpoint string) error {
	// Use ZeroMQ PUB endpoint specified in `sunspec` config block if provided.
	if this.pubEndpoint != "" {
		pubEndpoint = this.pubEndpoint
	}

	// Use ZeroMQ PULL endpoint specified in `sunspec` config block if provided.
	if this.pullEndpoint != "" {
		pullEndpoint = this.pullEndpoint
	}

	this.pusher = msgbus.MustNewPusher(pullEndpoint)
	subscriber := msgbus.MustNewSubscriber(pubEndpoint)

	subscriber.AddStatusHandler(this.handleMsgBusStatus)
	subscriber.Start("RUNTIME")

	server := mbserver.NewServer()

	server.RegisterContextFunctionHandler(3, this.readHoldingRegisters)
	server.RegisterContextFunctionHandler(16, this.writeHoldingRegisters)

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

func (this *SunSpecServer) readHoldingRegisters(_ context.Context, f mbserver.Framer) ([]byte, *mbserver.Exception) {
	var (
		data  = f.GetData()
		start = int(binary.BigEndian.Uint16(data[0:2]))
		count = int(binary.BigEndian.Uint16(data[2:4]))
		size  int
	)

	data = nil

	for addr := start; addr < start+count; {
		this.RLock()
		r, ok := this.registers[addr]
		this.RUnlock()

		if !ok {
			this.log("[ERROR] register %d does not exist", addr)
			return nil, &mbserver.IllegalDataAddress
		}

		scaling := 0.0

		if r.ScaleRegister != "" {
			this.RLock()
			p, ok := this.points[r.ScaleRegister]
			this.RUnlock()

			if !ok {
				this.log("[ERROR] scaling factor %s does not exist", r.ScaleRegister)
				return nil, &mbserver.SlaveDeviceFailure
			}

			scaling = p.InternalValue
		}

		this.RLock()
		v := this.tags[r.Tag]
		this.RUnlock()

		buf, err := r.Bytes(v, scaling)

		if err != nil {
			this.log("[ERROR] converting register %d value to data: %v", addr, err)
			return nil, &mbserver.SlaveDeviceFailure
		}

		size = size + (r.Count * 2)
		data = append(data, buf...)

		addr = addr + r.Count
	}

	return append([]byte{byte(size)}, data...), &mbserver.Success
}

func (this *SunSpecServer) writeHoldingRegisters(_ context.Context, f mbserver.Framer) ([]byte, *mbserver.Exception) {
	var (
		data  = f.GetData()
		start = int(binary.BigEndian.Uint16(data[0:2]))
		count = int(binary.BigEndian.Uint16(data[2:4]))
	)

	// offset after start register and register count
	b := 5

	var updates []msgbus.Point

	for addr := start; addr < start+count; {
		this.RLock()
		r, ok := this.registers[addr]
		this.RUnlock()

		if !ok {
			this.log("[ERROR] register %d does not exist", addr)
			return []byte{}, &mbserver.IllegalDataAddress
		}

		scaling := 0.0

		if r.ScaleRegister != "" {
			this.RLock()
			p, ok := this.points[r.ScaleRegister]
			this.RUnlock()

			if !ok {
				this.log("[ERROR] scaling factor %s does not exist", r.ScaleRegister)
				return nil, &mbserver.SlaveDeviceFailure
			}

			scaling = p.InternalValue
		}

		e := b + (r.Count * 2)
		d := data[b:e]

		v, err := r.Value(d, scaling)
		if err != nil {
			this.log("[ERROR] converting register %d data to value: %v", addr, err)
			return []byte{}, &mbserver.SlaveDeviceFailure
		}

		if r.Tag == "" {
			r.InternalValue = v

			this.Lock()
			this.registers[addr] = r
			this.Unlock()
		} else {
			updates = append(updates, msgbus.Point{Tag: r.Tag, Value: v})
		}
	}

	if len(updates) > 0 {
		env, err := msgbus.NewEnvelope(this.name, msgbus.Update{Updates: updates})
		if err != nil {
			this.log("[ERROR] creating new update message: %v", err)
			return nil, &mbserver.SlaveDeviceFailure
		}

		if err := this.pusher.Push("RUNTIME", env); err != nil {
			this.log("[ERROR] sending update message: %v", err)
			return nil, &mbserver.SlaveDeviceFailure
		}
	}

	return data[0:4], &mbserver.Success
}

func (this *SunSpecServer) handleMsgBusStatus(env msgbus.Envelope) {
	if env.Sender() == this.name {
		return
	}

	status, err := env.Status()
	if err != nil {
		if errors.Is(err, msgbus.ErrKindNotStatus) {
			return
		}

		this.log("[ERROR] getting status message from envelope: %v", err)
	}

	for _, point := range status.Measurements {
		this.RLock()
		_, ok := this.tags[point.Tag]
		this.RUnlock()

		if ok {
			this.log("setting tag %s to value %f", point.Tag, point.Value)

			this.Lock()
			this.tags[point.Tag] = point.Value
			this.Unlock()
		}
	}
}

func (this SunSpecServer) log(format string, a ...any) {
	fmt.Printf("[%s] %s\n", this.name, fmt.Sprintf(format, a...))
}

func init() {
	otsim.AddModuleFactory("sunspec", new(Factory))
}
