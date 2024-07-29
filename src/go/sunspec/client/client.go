package client

import (
	"context"
	"fmt"
	"strings"
	"time"

	otsim "github.com/patsec/ot-sim"
	"github.com/patsec/ot-sim/msgbus"
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
	period   time.Duration

	pusher *msgbus.Pusher
	client modbus.Client

	models    *common.Models
	registers map[int]*common.Register
	points    map[string]*common.Register
}

func New(name string) *SunSpecClient {
	return &SunSpecClient{
		name:      name,
		period:    5 * time.Second,
		models:    &common.Models{Settings: make(map[int]common.ModelSettings)},
		registers: make(map[int]*common.Register),
		points:    make(map[string]*common.Register),
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
		case "period":
			var err error

			this.period, err = time.ParseDuration(child.Text())
			if err != nil {
				return fmt.Errorf("invalid period '%s' provided for %s", child.Text(), this.name)
			}
		}
	}

	return nil
}

func (this *SunSpecClient) Run(ctx context.Context, pubEndpoint, pullEndpoint string) error {
	// Use ZeroMQ PUB endpoint specified in `sunspec` config block if provided.
	if this.pubEndpoint != "" {
		pubEndpoint = this.pubEndpoint
	}

	// Use ZeroMQ PULL endpoint specified in `sunspec` config block if provided.
	if this.pullEndpoint != "" {
		pullEndpoint = this.pullEndpoint
	}

	this.pusher = msgbus.MustNewPusher(pullEndpoint)

	var handler modbus.ClientHandler

	handler = modbus.NewTCPClientHandler(this.endpoint)
	handler.(*modbus.TCPClientHandler).SlaveId = byte(this.id)

	this.client = modbus.NewClient(handler)

	if err := confirmIdentifier(this.client); err != nil {
		return fmt.Errorf("confirming SunSpec identifier from SunSpec device %s: %w", this.endpoint, err)
	}

	var (
		// start addr at 40002 after well known identifier
		addr   = 40002
		model1 bool
	)

	for {
		id, length, err := nextModel(this.client, addr)
		if err != nil {
			return fmt.Errorf("getting next from SunSpec device %s: %w", this.endpoint, err)
		}

		if !model1 {
			if id == 1 {
				model1 = true
			} else {
				return fmt.Errorf("remote SunSpec device missing required Model 1")
			}
		}

		if id == int(common.EndRegister.InternalValue) {
			break
		}

		// model id and length are 2 words long
		addr += 2

		// TODO: don't add model 1 once we're done testing
		this.models.Order = append(this.models.Order, id)
		this.models.Settings[id] = common.ModelSettings{StartAddr: addr, Length: length}

		data, err := modelData(this.client, id, addr, length)
		if err != nil {
			return fmt.Errorf("reading Model %d data from SunSpec device %s: %w", id, this.endpoint, err)
		}

		if err := this.process(data); err != nil {
			return fmt.Errorf("processing Model %d data: %w", id, err)
		}

		addr += length
	}

	go func() {
		for {
			select {
			case <-ctx.Done():
				return
			case <-time.After(this.period):
				for _, id := range this.models.Order {
					var (
						addr   = this.models.Settings[id].StartAddr
						length = this.models.Settings[id].Length
					)

					data, err := modelData(this.client, id, addr, length)
					if err != nil {
						this.log("reading Model %d data from SunSpec device %s: %v", id, this.endpoint, err)
					}

					if err := this.process(data); err != nil {
						this.log("[ERROR] processing Model %d data: %v", id, err)
					}
				}
			}
		}
	}()

	for _, id := range this.models.Order {
		model, err := common.GetModelSchema(id)
		if err != nil {
			return fmt.Errorf("getting schema for model %d: %w", id, err)
		}

		for idx, point := range model.Group.Points {
			if idx < 2 {
				continue
			}

			r := this.points[point.Name]

			if strings.HasPrefix(r.DataType, "string") {
				value, err := r.String(r.Raw)
				if err != nil {
					return fmt.Errorf("parsing string value for SunSpec point: %w", err)
				}

				fmt.Printf("%s - %s\n", r.Name, value)
			} else {
				scaling := 0.0

				if r.ScaleRegister != "" {
					p, ok := this.points[r.ScaleRegister]
					if !ok {
						this.log("[ERROR] scaling factor %s does not exist", r.ScaleRegister)
						continue
					}

					scaling = p.InternalValue
				}

				value, err := r.Value(r.Raw, scaling)
				if err != nil {
					return fmt.Errorf("parsing value for SunSpec point: %w", err)
				}

				fmt.Printf("%s - %f\n", r.Name, value)
			}
		}
	}

	return nil
}

func (this SunSpecClient) process(data map[string]*common.Register) error {
	var points []msgbus.Point

	for name, reg := range data {
		this.points[name] = reg

		if !strings.HasPrefix(reg.DataType, "string") {
			if reg.DataType == "pad" || reg.DataType == "sunssf" {
				continue
			}

			scaling := 0.0

			if reg.ScaleRegister != "" {
				p, ok := this.points[reg.ScaleRegister]
				if !ok {
					this.log("[ERROR] scaling factor %s does not exist", reg.ScaleRegister)
					continue
				}

				scaling = p.InternalValue
			}

			value, err := reg.Value(reg.Raw, scaling)
			if err != nil {
				this.log("[ERROR] parsing value for SunSpec point: %v", err)
				continue
			}

			points = append(points, msgbus.Point{Tag: name, Value: value})
		}
	}

	if len(points) > 0 {
		points = append(points, msgbus.Point{Tag: fmt.Sprintf("%s.connected", this.name), Value: 1.0})
	} else {
		points = append(points, msgbus.Point{Tag: fmt.Sprintf("%s.connected", this.name), Value: 0.0})
		this.log("[ERROR] no measurements read from %s", this.endpoint)
	}

	env, err := msgbus.NewEnvelope(this.name, msgbus.Status{Measurements: points})
	if err != nil {
		return fmt.Errorf("creating status message: %w", err)
	}

	if err := this.pusher.Push("RUNTIME", env); err != nil {
		return fmt.Errorf("sending status message: %w", err)
	}

	return nil
}

func (this SunSpecClient) log(format string, a ...any) {
	fmt.Printf("[%s] %s\n", this.name, fmt.Sprintf(format, a...))
}
