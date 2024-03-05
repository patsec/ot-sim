package server

import (
	"context"
	"encoding/binary"

	"actshad.dev/mbserver"
)

func (this *ModbusServer) readDiscreteRegisters(_ context.Context, f mbserver.Framer) ([]byte, *mbserver.Exception) {
	if this.registers["discrete"] == nil {
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
		reg, ok := this.registers["discrete"][addr]
		if !ok {
			return nil, &mbserver.IllegalDataAddress
		}

		this.tagsMu.RLock()
		value := this.tags[reg.Tag]
		this.tagsMu.RUnlock()

		if value != 0 {
			shift := uint(idx) % 8
			data[idx/8] |= byte(1 << shift)
		}

		idx++
	}

	return append([]byte{byte(size)}, data...), &mbserver.Success
}
