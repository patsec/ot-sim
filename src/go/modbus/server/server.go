package server

import (
	"context"
	"errors"
	"fmt"
	"strconv"
	"strings"
	"sync"
	"time"

	"github.com/patsec/ot-sim/msgbus"
	"github.com/patsec/ot-sim/util"

	"actshad.dev/mbserver"
	"github.com/beevik/etree"
	"github.com/goburrow/serial"
)

var validRegisterTypes = []string{"coil", "discrete", "input", "holding"}

type register struct {
	tag     string
	scaling int
}

type ModbusServer struct {
	pullEndpoint string
	pubEndpoint  string

	pusher  *msgbus.Pusher
	metrics *msgbus.MetricsPusher

	name     string
	id       int
	endpoint string
	serial   *serial.Config

	registers map[string]map[int]register
	tags      map[string]float64

	tagsMu sync.RWMutex
}

func New(name string) *ModbusServer {
	return &ModbusServer{
		name:      name,
		id:        1,
		registers: make(map[string]map[int]register),
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
			t := child.SelectAttr("type")
			if t == nil {
				continue
			}

			typ := t.Value

			if !util.SliceContains(validRegisterTypes, typ) {
				return fmt.Errorf("invalid register type '%s' provided for %s", typ, this.name)
			}

			registers, ok := this.registers[typ]
			if !ok {
				registers = make(map[int]register)
			}

			var reg register

			e := child.SelectElement("address")
			if e == nil {
				continue
			}

			addr, err := strconv.Atoi(e.Text())
			if err != nil {
				continue
			}

			e = child.SelectElement("tag")
			if t == nil {
				continue
			}

			reg.tag = e.Text()

			e = child.SelectElement("scaling")
			if e != nil {
				reg.scaling, _ = strconv.Atoi(e.Text())
			}

			registers[addr] = reg
			this.tags[reg.tag] = 0

			this.registers[typ] = registers
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

	server.RegisterContextFunctionHandler(1, this.readCoils())
	server.RegisterContextFunctionHandler(2, this.readDiscretes())
	server.RegisterContextFunctionHandler(3, this.readHoldings())
	server.RegisterContextFunctionHandler(4, this.readInputs())
	server.RegisterContextFunctionHandler(5, this.writeCoil)
	server.RegisterContextFunctionHandler(6, this.writeHolding)
	server.RegisterContextFunctionHandler(15, this.writeCoils)
	server.RegisterContextFunctionHandler(16, this.writeHoldings)

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
