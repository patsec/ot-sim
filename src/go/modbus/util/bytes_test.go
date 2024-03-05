package util

import "fmt"

func ExampleBytesToBits() {
	data := []byte{0xcd, 0x01}
	bits := BytesToBits(data)

	fmt.Printf("%v\n", bits)
	// Output: [1 0 1 1 0 0 1 1 1 0 0 0 0 0 0 0]
}

func ExampleBitsToBytes() {
	bits := []int{1, 0, 1, 1, 0, 0, 1, 1, 1, 0}
	data := BitsToBytes(bits)

	fmt.Printf("%x\n", data)
	// Output: cd01
}
