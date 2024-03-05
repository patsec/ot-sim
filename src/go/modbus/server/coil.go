package server

import (
	"context"
	"encoding/binary"

	mbutil "github.com/patsec/ot-sim/modbus/util"

	"actshad.dev/mbserver"
	"github.com/patsec/ot-sim/msgbus"
)

func (this *ModbusServer) readCoilRegisters(_ context.Context, f mbserver.Framer) ([]byte, *mbserver.Exception) {
	if this.registers["coil"] == nil {
		return nil, &mbserver.IllegalDataAddress
	}

	var (
		data  = f.GetData()
		start = int(binary.BigEndian.Uint16(data[0:2]))
		count = int(binary.BigEndian.Uint16(data[2:4]))
	)

	var bits []int

	for addr := start; addr < start+count; addr++ {
		reg, ok := this.registers["coil"][addr]
		if !ok {
			return nil, &mbserver.IllegalDataAddress
		}

		this.tagsMu.RLock()
		value := this.tags[reg.Tag]
		this.tagsMu.RUnlock()

		if value == 0 {
			bits = append(bits, 0)
		} else {
			bits = append(bits, 1)
		}
	}

	data = mbutil.BitsToBytes(bits)
	size := len(data)

	return append([]byte{byte(size)}, data...), &mbserver.Success
}

func (this *ModbusServer) writeCoilRegister(ctx context.Context, f mbserver.Framer) ([]byte, *mbserver.Exception) {
	if this.registers["coil"] == nil {
		return nil, &mbserver.IllegalDataAddress
	}

	var (
		data = f.GetData()
		addr = int(binary.BigEndian.Uint16(data[0:2]))
	)

	reg, ok := this.registers["coil"][addr]
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

	this.log("updating tag %s --> %t", reg.Tag, value != 0)

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

	this.metrics.IncrMetric("coil_writes_count")

	return data[0:4], &mbserver.Success
}

func (this *ModbusServer) writeCoilRegisters(ctx context.Context, f mbserver.Framer) ([]byte, *mbserver.Exception) {
	if this.registers["coil"] == nil {
		return nil, &mbserver.IllegalDataAddress
	}

	var (
		data  = f.GetData()
		start = int(binary.BigEndian.Uint16(data[0:2]))
		count = int(binary.BigEndian.Uint16(data[2:4]))

		// beginning of data to be written starts at offset 5
		bits    = mbutil.BytesToBits(data[5:])
		updates []msgbus.Point
	)

	for addr := start; addr < start+count; addr++ {
		reg, ok := this.registers["coil"][addr]
		if !ok {
			return nil, &mbserver.IllegalDataAddress
		}

		idx := addr - start
		val := float64(bits[idx])

		this.tagsMu.Lock()
		this.tags[reg.Tag] = val
		this.tagsMu.Unlock()

		this.log("updating tag %s --> %t", reg.Tag, val != 0)

		updates = append(updates, msgbus.Point{Tag: reg.Tag, Value: val})
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

	this.metrics.IncrMetricBy("coil_writes_count", count)

	return data[0:4], &mbserver.Success
}
