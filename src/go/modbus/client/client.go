package client

import (
	"context"
	"errors"
	"fmt"
	"strconv"
	"strings"
	"time"

	mbutil "github.com/patsec/ot-sim/modbus/util"
	"github.com/patsec/ot-sim/msgbus"

	"actshad.dev/modbus"
	"github.com/beevik/etree"
	"github.com/goburrow/serial"
)

type ModbusClient struct {
	pullEndpoint string
	pubEndpoint  string

	pusher *msgbus.Pusher
	client modbus.Client

	name     string
	id       int
	endpoint string
	serial   *serial.Config
	period   time.Duration

	registers map[string]*mbutil.Register
}

func New(name string) *ModbusClient {
	return &ModbusClient{
		name:      name,
		id:        1,
		period:    5 * time.Second,
		registers: make(map[string]*mbutil.Register),
	}
}

func (this ModbusClient) Name() string {
	return this.name
}

func (this *ModbusClient) Configure(e *etree.Element) error {
	for _, child := range e.ChildElements() {
		switch child.Tag {
		case "pull-endpoint":
			this.pullEndpoint = child.Text()
		case "pub-endpoint":
			this.pubEndpoint = child.Text()
		case "unit-id":
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
				Timeout:  5 * time.Second,
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
		case "period":
			var err error

			this.period, err = time.ParseDuration(child.Text())
			if err != nil {
				return fmt.Errorf("invalid period '%s' provided for %s", child.Text(), this.name)
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

			this.registers[reg.Tag] = reg
		}
	}

	return nil
}

func (this *ModbusClient) Run(ctx context.Context, pubEndpoint, pullEndpoint string) error {
	if _, err := this.getEndpoint(); err != nil {
		return err
	}

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

	subscriber.AddUpdateHandler(this.handleMsgBusUpdate)
	subscriber.Start("RUNTIME")

	var handler modbus.ClientHandler

	if this.endpoint != "" {
		handler = modbus.NewTCPClientHandler(this.endpoint)
		handler.(*modbus.TCPClientHandler).SlaveId = byte(this.id)
	} else if this.serial != nil {
		handler = modbus.NewRTUClientHandler(this.serial.Address)
		handler.(*modbus.RTUClientHandler).Config = *this.serial
		handler.(*modbus.RTUClientHandler).SlaveId = byte(this.id)
	} else {
		// This should never happen given the first set of if-statements in this
		// function.
		panic("missing endpoint or serial configuration")
	}

	this.client = modbus.NewClient(handler)

	go func() {
		endpoint, _ := this.getEndpoint()

		for {
			select {
			case <-ctx.Done():
				return
			case <-time.After(this.period):
				var points []msgbus.Point

				// TODO: optimize this so all registers of the same kind that are
				// consecutive can be read at once.

				for tag, reg := range this.registers {
					var (
						data []byte
						err  error
					)

					switch reg.Type {
					case "coil":
						data, err = this.client.ReadCoils(uint16(reg.Addr), 1)
						if err != nil {
							this.log("[ERROR] reading coil %d from %s: %v", reg.Addr, endpoint, err)
							continue
						}
					case "discrete":
						data, err = this.client.ReadDiscreteInputs(uint16(reg.Addr), 1)
						if err != nil {
							this.log("[ERROR] reading discrete input %d from %s: %v", reg.Addr, endpoint, err)
							continue
						}
					case "input":
						data, err = this.client.ReadInputRegisters(uint16(reg.Addr), uint16(reg.Count))
						if err != nil {
							this.log("[ERROR] reading input %d from %s: %v", reg.Addr, endpoint, err)
							continue
						}
					case "holding":
						data, err = this.client.ReadHoldingRegisters(uint16(reg.Addr), uint16(reg.Count))
						if err != nil {
							this.log("[ERROR] reading holding %d from %s: %v", reg.Addr, endpoint, err)
							continue
						}
					}

					value, err := reg.Value(data)
					if err != nil {
						this.log("[ERROR] getting register value: %v", err)
						continue
					}

					points = append(points, msgbus.Point{Tag: tag, Value: value})
				}

				if len(points) > 0 {
					points = append(points, msgbus.Point{Tag: fmt.Sprintf("%s.connected", this.name), Value: 1.0})
				} else {
					points = append(points, msgbus.Point{Tag: fmt.Sprintf("%s.connected", this.name), Value: 0.0})
					this.log("[ERROR] no measurements read from %s", endpoint)
				}

				env, err := msgbus.NewEnvelope(this.name, msgbus.Status{Measurements: points})
				if err != nil {
					this.log("[ERROR] creating status message: %v", err)
					continue
				}

				if err := this.pusher.Push("RUNTIME", env); err != nil {
					this.log("[ERROR] sending status message: %v", err)
				}
			}
		}
	}()

	go func() {
		<-ctx.Done()
		subscriber.Stop()
	}()

	return nil
}

func (this *ModbusClient) handleMsgBusUpdate(env msgbus.Envelope) {
	if env.Sender() == this.name {
		return
	}

	update, err := env.Update()
	if err != nil {
		if !errors.Is(err, msgbus.ErrKindNotUpdate) {
			this.log("[ERROR] getting update message from envelope: %v", err)
		}

		return
	}

	endpoint, _ := this.getEndpoint()

	for _, point := range update.Updates {
		if register, ok := this.registers[point.Tag]; ok {
			switch register.Type {
			case "coil":
				value := point.Value

				if value != 0 {
					value = 65280 // 0xFF00, per Modbus spec
				}

				if _, err := this.client.WriteSingleCoil(uint16(register.Addr), uint16(value)); err != nil {
					this.log("[ERROR] writing to coil %d at %s: %v", register.Addr, endpoint, err)
				}

				this.log("writing coil %d at %s --> %t", register.Addr, endpoint, uint16(value) != 0)
			case "holding":
				data, err := register.Bytes(point.Value)
				if err != nil {
					this.log("[ERROR] converting register value to bytes: %v", err)
					continue
				}

				if _, err := this.client.WriteMultipleRegisters(uint16(register.Addr), uint16(register.Count), data); err != nil {
					this.log("[ERROR] writing to holding %d at %s: %v", register.Addr, endpoint, err)
				}

				this.log("writing holding %d at %s --> %d", register.Addr, endpoint, int(register.Scaled(point.Value)))
			}
		}
	}
}

func (this ModbusClient) getEndpoint() (string, error) {
	if this.endpoint != "" && this.serial != nil {
		return "", fmt.Errorf("cannot provide both endpoint and serial configuration options")
	}

	if this.endpoint != "" {
		return this.endpoint, nil
	}

	if this.serial != nil {
		return this.serial.Address, nil
	}

	return "", fmt.Errorf("must provide either endpoint or serial configuration option")
}

func (this ModbusClient) log(format string, a ...any) {
	fmt.Printf("[%s] %s\n", this.name, fmt.Sprintf(format, a...))
}
