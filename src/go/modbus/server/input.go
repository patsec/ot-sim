package server

import (
	"context"
	"encoding/binary"

	"actshad.dev/mbserver"
)

func (this *ModbusServer) readInputRegisters(_ context.Context, f mbserver.Framer) ([]byte, *mbserver.Exception) {
	if this.registers["input"] == nil {
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
		reg, ok := this.registers["input"][addr]
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
