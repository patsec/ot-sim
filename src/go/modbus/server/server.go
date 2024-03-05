package server

import (
	"context"
	"errors"
	"fmt"
	"strconv"
	"strings"
	"sync"
	"time"

	mbutil "github.com/patsec/ot-sim/modbus/util"
	"github.com/patsec/ot-sim/msgbus"

	"actshad.dev/mbserver"
	"github.com/beevik/etree"
	"github.com/goburrow/serial"
)

type ModbusServer struct {
	pullEndpoint string
	pubEndpoint  string

	pusher  *msgbus.Pusher
	metrics *msgbus.MetricsPusher

	name     string
	id       int
	endpoint string
	serial   *serial.Config

	registers map[string]map[int]*mbutil.Register
	tags      map[string]float64

	tagsMu sync.RWMutex
}

func New(name string) *ModbusServer {
	return &ModbusServer{
		name:      name,
		id:        1,
		registers: make(map[string]map[int]*mbutil.Register),
		tags:      make(map[string]float64),
		metrics:   msgbus.NewMetricsPusher(),
	}
}

func (this ModbusServer) Name() string {
	return this.name
}

func (this *ModbusServer) Configure(e *etree.Element) error {
	for _, child := range e.ChildElements() {
		switch child.Tag {
		case "pull-endpoint":
			this.pullEndpoint = child.Text()
		case "pub-endpoint":
			this.pubEndpoint = child.Text()
		case "id":
			var err error

			this.id, err = strconv.Atoi(child.Text())
			if err != nil {
				return fmt.Errorf("invalid unit ID '%s' provided: %w", child.Text(), err)
			}
		case "endpoint":
			this.endpoint = child.Text()
		case "serial":
			this.serial = &serial.Config{
				Address:  "/dev/ttyS0",
				BaudRate: 115200,
				DataBits: 8,
				StopBits: 1,
				Parity:   "N",
				Timeout:  0,
			}

			for _, child := range child.ChildElements() {
				switch child.Tag {
				case "device":
					this.serial.Address = child.Text()
				case "baud-rate":
					var err error

					this.serial.BaudRate, err = strconv.Atoi(child.Text())
					if err != nil {
						return fmt.Errorf("invalid baud rate '%s' provided: %w", child.Text(), err)
					}
				case "data-bits":
					var err error

					this.serial.DataBits, err = strconv.Atoi(child.Text())
					if err != nil {
						return fmt.Errorf("invalid data bits '%s' provided: %w", child.Text(), err)
					}
				case "stop-bits":
					var err error

					this.serial.StopBits, err = strconv.Atoi(child.Text())
					if err != nil {
						return fmt.Errorf("invalid stop bits '%s' provided: %w", child.Text(), err)
					}
				case "parity":
					if strings.EqualFold(child.Text(), "none") {
						this.serial.Parity = "N"
					} else if strings.EqualFold(child.Text(), "even") {
						this.serial.Parity = "E"
					} else if strings.EqualFold(child.Text(), "odd") {
						this.serial.Parity = "O"
					} else {
						return fmt.Errorf("invalid parity '%s' provided", child.Text())
					}
				case "timeout":
					var err error

					this.serial.Timeout, err = time.ParseDuration(child.Text())
					if err != nil {
						return fmt.Errorf("invalid timeout '%s' provided: %w", child.Text(), err)
					}
				}
			}
		case "register":
			var (
				reg = new(mbutil.Register)
				err error
			)

			a := child.SelectAttr("type")
			if a == nil {
				return fmt.Errorf("type attribute missing from register for %s", this.name)
			}

			reg.Type = a.Value

			if reg.Type == "input" || reg.Type == "holding" {
				reg.DataType = child.SelectAttrValue("data-type", "uint16")
			}

			e := child.SelectElement("address")
			if e == nil {
				return fmt.Errorf("address element missing from register for %s", this.name)
			}

			reg.Addr, err = strconv.Atoi(e.Text())
			if err != nil {
				return fmt.Errorf("unable to convert register address '%s' for %s", e.Text(), this.name)
			}

			e = child.SelectElement("tag")
			if e == nil {
				return fmt.Errorf("tag element missing from register for %s", this.name)
			}

			reg.Tag = e.Text()

			e = child.SelectElement("scaling")
			if e != nil {
				if reg.DataType == "float" {
					this.log("[WARN] scaling value ignored for registers using 'float' data types")
				} else {
					reg.Scaling, _ = strconv.Atoi(e.Text())
				}
			}

			if err := reg.Init(); err != nil {
				return fmt.Errorf("validating register for %s: %w", this.name, err)
			}

			registers, ok := this.registers[reg.Type]
			if !ok {
				registers = make(map[int]*mbutil.Register)
			}

			registers[reg.Addr] = reg
			this.tags[reg.Tag] = 0

			this.registers[reg.Type] = registers
		}
	}

	return nil
}

func (this *ModbusServer) Run(ctx context.Context, pubEndpoint, pullEndpoint string) error {
	// Use ZeroMQ PUB endpoint specified in `modbus` config block if provided.
	if this.pubEndpoint != "" {
		pubEndpoint = this.pubEndpoint
	}

	// Use ZeroMQ PULL endpoint specified in `modbus` config block if provided.
	if this.pullEndpoint != "" {
		pullEndpoint = this.pullEndpoint
	}

	this.pusher = msgbus.MustNewPusher(pullEndpoint)
	subscriber := msgbus.MustNewSubscriber(pubEndpoint)

	subscriber.AddStatusHandler(this.handleMsgBusStatus)
	subscriber.Start("RUNTIME")

	this.metrics.NewMetric(msgbus.METRIC_COUNTER, "status_count", "number of status messages processed")
	this.metrics.NewMetric(msgbus.METRIC_COUNTER, "coil_writes_count", "number of coil writes processed")
	this.metrics.NewMetric(msgbus.METRIC_COUNTER, "holding_writes_count", "number of holding writes processed")
	this.metrics.Start(this.pusher, this.name)

	server := mbserver.NewServer()

	server.RegisterContextFunctionHandler(1, this.readCoilRegisters)
	server.RegisterContextFunctionHandler(2, this.readDiscreteRegisters)
	server.RegisterContextFunctionHandler(3, this.readHoldingRegisters)
	server.RegisterContextFunctionHandler(4, this.readInputRegisters)
	server.RegisterContextFunctionHandler(5, this.writeCoilRegister)
	server.RegisterContextFunctionHandler(6, this.writeHoldingRegister)
	server.RegisterContextFunctionHandler(15, this.writeCoilRegisters)
	server.RegisterContextFunctionHandler(16, this.writeHoldingRegisters)

	go func() {
		<-ctx.Done()

		server.Close()
		subscriber.Stop()
	}()

	if this.endpoint != "" {
		if err := server.ListenTCP(this.endpoint); err != nil {
			return fmt.Errorf("listening for TCP connections on %s: %w", this.endpoint, err)
		}
	}

	if this.serial != nil {
		if err := server.ListenRTU(this.serial); err != nil {
			return fmt.Errorf("listening for RTU connections on %s: %w", this.serial.Address, err)
		}
	}

	return nil
}

func (this *ModbusServer) handleMsgBusStatus(env msgbus.Envelope) {
	if env.Sender() == this.name {
		return
	}

	this.metrics.IncrMetric("status_count")

	status, err := env.Status()
	if err != nil {
		if errors.Is(err, msgbus.ErrKindNotStatus) {
			return
		}

		this.log("[ERROR] getting status message from envelope: %v", err)
	}

	this.tagsMu.Lock()

	for _, point := range status.Measurements {
		this.log("setting tag %s to value %f", point.Tag, point.Value)
		this.tags[point.Tag] = point.Value
	}

	this.tagsMu.Unlock()
}

func (this ModbusServer) log(format string, a ...any) {
	fmt.Printf("[%s] %s\n", this.name, fmt.Sprintf(format, a...))
}
