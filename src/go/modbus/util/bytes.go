package util

import (
	"math/bits"
)

func BytesToBits(data []byte) []int {
	result := make([]int, len(data)*8)

	for i, d := range data {
		// switch from LSB to MSB
		d = bits.Reverse8(d)

		for j := 0; j < 8; j++ {
			idx := i*8 + j

			if bits.LeadingZeros8(d) == 0 {
				result[idx] = 1
			} else {
				result[idx] = 0
			}

			d = d << 1
		}
	}

	return result
}

func BitsToBytes(bits []int) []byte {
	var (
		count = len(bits)
		size  = count / 8
	)

	if (count % 8) != 0 {
		size++
	}

	var (
		data = make([]byte, size)
		idx  = 0
	)

	for _, b := range bits {
		if b != 0 {
			shift := uint(idx) % 8
			data[idx/8] |= byte(1 << shift)
		}

		idx++
	}

	return data
}
