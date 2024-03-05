package server

import (
	"context"
	"encoding/binary"

	"actshad.dev/mbserver"
	"github.com/patsec/ot-sim/msgbus"
)

func (this *ModbusServer) readHoldingRegisters(_ context.Context, f mbserver.Framer) ([]byte, *mbserver.Exception) {
	if this.registers["holding"] == nil {
		return nil, &mbserver.IllegalDataAddress
	}

	var (
		data  = f.GetData()
		start = int(binary.BigEndian.Uint16(data[0:2]))
		count = int(binary.BigEndian.Uint16(data[2:4]))
		size  int
	)

	data = nil

	for addr := start; addr < start+count; {
		reg, ok := this.registers["holding"][addr]
		if !ok {
			return nil, &mbserver.IllegalDataAddress
		}

		this.tagsMu.RLock()
		buf, err := reg.Bytes(this.tags[reg.Tag])
		this.tagsMu.RUnlock()

		if err != nil {
			return nil, &mbserver.SlaveDeviceFailure
		}

		size = size + (reg.Count * 2)
		data = append(data, buf...)

		addr = addr + reg.Count
	}

	return append([]byte{byte(size)}, data...), &mbserver.Success
}

func (this *ModbusServer) writeHoldingRegister(_ context.Context, f mbserver.Framer) ([]byte, *mbserver.Exception) {
	if this.registers["holding"] == nil {
		return nil, &mbserver.IllegalDataAddress
	}

	var (
		data = f.GetData()
		addr = int(binary.BigEndian.Uint16(data[0:2]))
	)

	reg, ok := this.registers["holding"][addr]
	if !ok {
		return nil, &mbserver.IllegalDataAddress
	}

	value, err := reg.Value(data[2:4])
	if err != nil {
		return nil, &mbserver.IllegalDataValue
	}

	this.tagsMu.Lock()
	this.tags[reg.Tag] = value
	this.tagsMu.Unlock()

	this.log("updating tag %s --> %f", reg.Tag, value)

	updates := []msgbus.Point{{Tag: reg.Tag, Value: value}}

	env, err := msgbus.NewEnvelope(this.name, msgbus.Update{Updates: updates})
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

func (this *ModbusServer) writeHoldingRegisters(_ context.Context, f mbserver.Framer) ([]byte, *mbserver.Exception) {
	if this.registers["holding"] == nil {
		return nil, &mbserver.IllegalDataAddress
	}

	var (
		data  = f.GetData()
		start = int(binary.BigEndian.Uint16(data[0:2]))
		count = int(binary.BigEndian.Uint16(data[2:4]))

		// beginning of data to be written starts at offset 5
		begin   = 5
		updates []msgbus.Point
	)

	for addr := start; addr < start+count; {
		reg, ok := this.registers["holding"][addr]
		if !ok {
			return nil, &mbserver.IllegalDataAddress
		}

		end := begin + (reg.Count * 2) // each holding register is 2 bytes long

		value, err := reg.Value(data[begin:end])
		if err != nil {
			return nil, &mbserver.IllegalDataValue
		}

		this.tagsMu.Lock()
		this.tags[reg.Tag] = value
		this.tagsMu.Unlock()

		this.log("updating tag %s --> %f", reg.Tag, value)

		updates = append(updates, msgbus.Point{Tag: reg.Tag, Value: value})

		begin = end
		addr = addr + reg.Count
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

	this.metrics.IncrMetricBy("holding_writes_count", count)

	return data[0:4], &mbserver.Success
}
