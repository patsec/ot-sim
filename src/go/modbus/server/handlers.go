package server

import (
	"bytes"
	"context"
	"encoding/binary"
	"math"

	"github.com/patsec/ot-sim/msgbus"

	"actshad.dev/mbserver"
)

func (this *ModbusServer) readCoils() mbserver.ContextFunctionHandler {
	return this.readBits(this.registers["coil"], this.tags)
}

func (this *ModbusServer) readDiscretes() mbserver.ContextFunctionHandler {
	return this.readBits(this.registers["discrete"], this.tags)
}

func (this *ModbusServer) readInputs() mbserver.ContextFunctionHandler {
	return this.readRegisters(this.registers["input"], this.tags)
}

func (this *ModbusServer) readHoldings() mbserver.ContextFunctionHandler {
	return this.readRegisters(this.registers["holding"], this.tags)
}

func (this *ModbusServer) writeCoil(ctx context.Context, f mbserver.Framer) ([]byte, *mbserver.Exception) {
	if this.registers["coil"] == nil {
		return nil, &mbserver.IllegalDataAddress
	}

	var (
		data = f.GetData()
		addr = int(binary.BigEndian.Uint16(data[0:2]))
		val  = binary.BigEndian.Uint16(data[2:4])
	)

	if val != 0 {
		val = 65280 // 0xFF00, per Modbus spec
	}

	reg, ok := this.registers["coil"][addr]
	if !ok {
		return nil, &mbserver.IllegalDataAddress
	}

	value := float64(val)

	this.tagsMu.Lock()
	this.tags[reg.tag] = value
	this.tagsMu.Unlock()

	this.log("updating tag %s --> %t", reg.tag, value != 0)

	updates := []msgbus.Point{{Tag: reg.tag, Value: value}}

	env, err := msgbus.NewUpdateEnvelope(this.name, msgbus.Update{Updates: updates})
	if err != nil {
		this.log("[ERROR] creating new update message: %v", err)
		return nil, &mbserver.SlaveDeviceFailure
	}

	if err := this.pusher.Push("RUNTIME", env); err != nil {
		this.log("[ERROR] sending update message: %v", err)
		return nil, &mbserver.SlaveDeviceFailure
	}

	this.metrics.IncrMetric("coil_writes_count")

	return data[0:4], &mbserver.Success
}

func (this *ModbusServer) writeCoils(ctx context.Context, f mbserver.Framer) ([]byte, *mbserver.Exception) {
	if this.registers["coil"] == nil {
		return nil, &mbserver.IllegalDataAddress
	}

	var (
		data  = f.GetData()
		start = int(binary.BigEndian.Uint16(data[0:2]))
		count = int(binary.BigEndian.Uint16(data[2:4]))
	)

	bits := 0

	// beginning of data to be written starts at offset 5
	for i, v := range data[5:] {
		for pos := uint(0); pos < 8; pos++ {
			idx := start + (i * 8) + int(pos)

			reg, ok := this.registers["coil"][idx]
			if !ok {
				return nil, &mbserver.IllegalDataAddress
			}

			value := float64((v >> pos) & 0x01)

			this.tagsMu.Lock()
			this.tags[reg.tag] = value
			this.tagsMu.Unlock()

			this.log("updating tag %s --> %t", reg.tag, value != 0)

			updates := []msgbus.Point{{Tag: reg.tag, Value: value}}

			env, err := msgbus.NewUpdateEnvelope(this.name, msgbus.Update{Updates: updates})
			if err != nil {
				this.log("[ERROR] creating new update message: %v", err)
				return nil, &mbserver.SlaveDeviceFailure
			}

			if err := this.pusher.Push("RUNTIME", env); err != nil {
				this.log("[ERROR] sending update message: %v", err)
				return nil, &mbserver.SlaveDeviceFailure
			}

			bits++

			if bits >= count {
				break
			}
		}

		if bits >= count {
			break
		}
	}

	this.metrics.IncrMetricBy("coil_writes_count", count)

	return data[0:4], &mbserver.Success
}

func (this *ModbusServer) writeHolding(ctx context.Context, f mbserver.Framer) ([]byte, *mbserver.Exception) {
	if this.registers["holding"] == nil {
		return nil, &mbserver.IllegalDataAddress
	}

	var (
		data = f.GetData()
		addr = int(binary.BigEndian.Uint16(data[0:2]))
		val  = binary.BigEndian.Uint16(data[2:4])
	)

	reg, ok := this.registers["holding"][addr]
	if !ok {
		return nil, &mbserver.IllegalDataAddress
	}

	value := float64(val) * math.Pow(10, float64(reg.scaling))

	this.tagsMu.Lock()
	this.tags[reg.tag] = value
	this.tagsMu.Unlock()

	this.log("updating tag %s --> %f", reg.tag, value)

	updates := []msgbus.Point{{Tag: reg.tag, Value: value}}

	env, err := msgbus.NewUpdateEnvelope(this.name, msgbus.Update{Updates: updates})
	if err != nil {
		this.log("[ERROR] creating new update message: %v", err)
		return nil, &mbserver.SlaveDeviceFailure
	}

	if err := this.pusher.Push("RUNTIME", env); err != nil {
		this.log("[ERROR] sending update message: %v", err)
		return nil, &mbserver.SlaveDeviceFailure
	}

	this.metrics.IncrMetric("holding_writes_count")

	return data[0:4], &mbserver.Success
}

func (this *ModbusServer) writeHoldings(ctx context.Context, f mbserver.Framer) ([]byte, *mbserver.Exception) {
	if this.registers["holding"] == nil {
		return nil, &mbserver.IllegalDataAddress
	}

	var (
		data  = f.GetData()
		start = int(binary.BigEndian.Uint16(data[0:2]))
		count = int(binary.BigEndian.Uint16(data[2:4]))
	)

	idx := 5 // beginning of data to be written starts at offset 5

	for i := start; i < start+count; i++ {
		end := idx + 2 // each holding register is 2 bytes long
		byt := data[idx:end]

		var (
			val int16
			buf = bytes.NewReader(byt)
		)

		if err := binary.Read(buf, binary.BigEndian, &val); err != nil {
			this.log("[ERROR] reading value: %v", err)
			return nil, &mbserver.SlaveDeviceFailure
		}

		reg, ok := this.registers["holding"][i]
		if !ok {
			return nil, &mbserver.IllegalDataAddress
		}

		value := float64(val) * math.Pow(10, float64(reg.scaling))

		this.tagsMu.Lock()
		this.tags[reg.tag] = value
		this.tagsMu.Unlock()

		this.log("updating tag %s --> %f", reg.tag, value)

		updates := []msgbus.Point{{Tag: reg.tag, Value: value}}

		env, err := msgbus.NewUpdateEnvelope(this.name, msgbus.Update{Updates: updates})
		if err != nil {
			this.log("[ERROR] creating new update message: %v", err)
			return nil, &mbserver.SlaveDeviceFailure
		}

		if err := this.pusher.Push("RUNTIME", env); err != nil {
			this.log("[ERROR] sending update message: %v", err)
			return nil, &mbserver.SlaveDeviceFailure
		}

		idx = end
	}

	this.metrics.IncrMetricBy("holding_writes_count", count)

	return data[0:4], &mbserver.Success
}

func (this *ModbusServer) readBits(registers map[int]register, tags map[string]float64) mbserver.ContextFunctionHandler {
	return func(ctx context.Context, f mbserver.Framer) ([]byte, *mbserver.Exception) {
		if registers == nil {
			return nil, &mbserver.IllegalDataAddress
		}

		var (
			data  = f.GetData()
			start = int(binary.BigEndian.Uint16(data[0:2]))
			count = int(binary.BigEndian.Uint16(data[2:4]))
		)

		size := count / 8

		if (count % 8) != 0 {
			size++
		}

		data = make([]byte, size)
		idx := 0

		for addr := start; addr < start+count; addr++ {
			reg, ok := registers[addr]
			if !ok {
				return nil, &mbserver.IllegalDataAddress
			}

			this.tagsMu.RLock()
			value := tags[reg.tag]
			this.tagsMu.RUnlock()

			if value != 0 {
				shift := uint(idx) % 8
				data[idx/8] |= byte(1 << shift)
			}

			idx++
		}

		return append([]byte{byte(size)}, data...), &mbserver.Success
	}
}

func (this *ModbusServer) readRegisters(registers map[int]register, tags map[string]float64) mbserver.ContextFunctionHandler {
	return func(ctx context.Context, f mbserver.Framer) ([]byte, *mbserver.Exception) {
		if registers == nil {
			return nil, &mbserver.IllegalDataAddress
		}

		var (
			data  = f.GetData()
			start = int(binary.BigEndian.Uint16(data[0:2]))
			count = int(binary.BigEndian.Uint16(data[2:4]))
		)

		data = []byte{}

		for addr := start; addr < start+count; addr++ {
			reg, ok := registers[addr]
			if !ok {
				return nil, &mbserver.IllegalDataAddress
			}

			this.tagsMu.RLock()
			value := int16(tags[reg.tag] * math.Pow(10, -float64(reg.scaling)))
			this.tagsMu.RUnlock()

			buf := new(bytes.Buffer)

			if err := binary.Write(buf, binary.BigEndian, value); err != nil {
				return nil, &mbserver.SlaveDeviceFailure
			}

			data = append(data, buf.Bytes()...)
		}

		return append([]byte{byte(count * 2)}, data...), &mbserver.Success
	}
}
