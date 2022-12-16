package client

import (
	"bytes"
	"context"
	"encoding/binary"
	"errors"
	"fmt"
	"math"
	"strconv"
	"time"

	mbutil "github.com/patsec/ot-sim/modbus/util"
	"github.com/patsec/ot-sim/msgbus"
	"github.com/patsec/ot-sim/util"

	"actshad.dev/modbus"
	"github.com/beevik/etree"
)

var validRegisterTypes = []string{"coil", "discrete", "input", "holding"}

type register struct {
	typ     string
	addr    int
	scaling int
}

type ModbusClient struct {
	pullEndpoint string
	pubEndpoint  string

	pusher *msgbus.Pusher
	client modbus.Client

	name     string
	period   time.Duration
	endpoint string

	registers map[string]register
}

func New(name string) *ModbusClient {
	return &ModbusClient{
		name:      name,
		period:    5 * time.Second,
		registers: make(map[string]register),
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
		case "endpoint":
			this.endpoint = child.Text()
		case "period":
			var err error

			this.period, err = time.ParseDuration(child.Text())
			if err != nil {
				return fmt.Errorf("invalid period '%s' provided for %s", child.Text(), this.name)
			}
		case "register":
			var (
				reg register
				err error
			)

			t := child.SelectAttr("type")
			if t == nil {
				return fmt.Errorf("type attribute missing from register for %s", this.name)
			}

			reg.typ = t.Value

			if !util.SliceContains(validRegisterTypes, reg.typ) {
				return fmt.Errorf("invalid register type '%s' provided for %s", reg.typ, this.name)
			}

			e := child.SelectElement("address")
			if e == nil {
				return fmt.Errorf("address element missing from register for %s", this.name)
			}

			reg.addr, err = strconv.Atoi(e.Text())
			if err != nil {
				return fmt.Errorf("unable to convert register address '%s' for %s", e.Text(), this.name)
			}

			e = child.SelectElement("tag")
			if e == nil {
				return fmt.Errorf("tag element missing from register for %s", this.name)
			}

			tag := e.Text()

			e = child.SelectElement("scaling")
			if e != nil {
				reg.scaling, _ = strconv.Atoi(e.Text())
			}

			this.registers[tag] = reg
		}
	}

	return nil
}

func (this *ModbusClient) Run(ctx context.Context, pubEndpoint, pullEndpoint string) error {
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

	this.client = modbus.TCPClient(this.endpoint)

	go func() {
		for {
			select {
			case <-ctx.Done():
				return
			case <-time.After(this.period):
				var points []msgbus.Point

				// TODO: optimize this so all registers of the same kind that are
				// consecutive can be read at once.

				for tag, reg := range this.registers {
					switch reg.typ {
					case "coil":
						data, err := this.client.ReadCoils(uint16(reg.addr), 1)
						if err != nil {
							this.log("[ERROR] reading coil %d from %s: %v", reg.addr, this.endpoint, err)
							continue
						}

						coils := mbutil.BytesToBits(data)

						points = append(points, msgbus.Point{Tag: tag, Value: float64(coils[0])})
					case "discrete":
						data, err := this.client.ReadDiscreteInputs(uint16(reg.addr), 1)
						if err != nil {
							this.log("[ERROR] reading discrete input %d from %s: %v", reg.addr, this.endpoint, err)
							continue
						}

						discretes := mbutil.BytesToBits(data)

						points = append(points, msgbus.Point{Tag: tag, Value: float64(discretes[0])})
					case "input":
						data, err := this.client.ReadInputRegisters(uint16(reg.addr), 1)
						if err != nil {
							this.log("[ERROR] reading input %d from %s: %v", reg.addr, this.endpoint, err)
							continue
						}

						var (
							buf = bytes.NewReader(data)
							val int16
						)

						if err := binary.Read(buf, binary.BigEndian, &val); err != nil {
							this.log("[ERROR] parsing input %d from %s: %v", reg.addr, this.endpoint, err)
							continue
						}

						scaled := float64(val) * math.Pow(10, -float64(reg.scaling))

						points = append(points, msgbus.Point{Tag: tag, Value: scaled})
					case "holding":
						data, err := this.client.ReadHoldingRegisters(uint16(reg.addr), 1)
						if err != nil {
							this.log("[ERROR] reading holding %d from %s: %v", reg.addr, this.endpoint, err)
							continue
						}

						var (
							buf = bytes.NewReader(data)
							val int16
						)

						if err := binary.Read(buf, binary.BigEndian, &val); err != nil {
							this.log("[ERROR] parsing holding %d from %s: %v", reg.addr, this.endpoint, err)
							continue
						}

						scaled := float64(val) * math.Pow(10, -float64(reg.scaling))

						points = append(points, msgbus.Point{Tag: tag, Value: scaled})
					}
				}

				if len(points) > 0 {
					env, err := msgbus.NewStatusEnvelope(this.name, msgbus.Status{Measurements: points})
					if err != nil {
						this.log("[ERROR] creating status message: %v", err)
						continue
					}

					if err := this.pusher.Push("RUNTIME", env); err != nil {
						this.log("[ERROR] sending status message: %v", err)
					}
				} else {
					this.log("[ERROR] no measurements read from %s", this.endpoint)
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

	for _, point := range update.Updates {
		if register, ok := this.registers[point.Tag]; ok {
			switch register.typ {
			case "coil":
				value := point.Value

				if value != 0 {
					value = 65280 // 0xFF00, per Modbus spec
				}

				if _, err := this.client.WriteSingleCoil(uint16(register.addr), uint16(value)); err != nil {
					this.log("[ERROR] writing to coil %d at %s: %v", register.addr, this.endpoint, err)
				}

				this.log("writing coil %d at %s --> %t", register.addr, this.endpoint, uint16(value) != 0)
			case "holding":
				scaled := point.Value * math.Pow(10, float64(register.scaling))

				if _, err := this.client.WriteSingleRegister(uint16(register.addr), uint16(scaled)); err != nil {
					this.log("[ERROR] writing to holding %d at %s: %v", register.addr, this.endpoint, err)
				}

				this.log("writing holding %d at %s --> %d", register.addr, this.endpoint, uint16(scaled))
			}
		}
	}
}

func (this ModbusClient) log(format string, a ...any) {
	fmt.Printf("[%s] %s\n", this.name, fmt.Sprintf(format, a...))
}
