package util

import (
	"fmt"
	"testing"
)

func TestInputRegister(t *testing.T) {
	data := []byte{0x00, 0x00, 0xbb, 0x80}

	reg := new(Register)
	reg.Type = "input"
	reg.DataType = "uint32"
	reg.Scaling = 2

	if err := reg.Init(); err != nil {
		t.Log(err)
		t.FailNow()
	}

	val, err := reg.Value(data)
	if err != nil {
		t.Log(err)
		t.FailNow()
	}

	if val != 480 {
		t.FailNow()
	}
}

func TestCoilRegister(t *testing.T) {
	reg := new(Register)
	reg.Type = "coil"

	if err := reg.Init(); err != nil {
		t.Log(err)
		t.FailNow()
	}

	val, err := reg.Bytes(1)
	if err != nil {
		t.Log(err)
		t.FailNow()
	}

	expected := []byte{0xff, 0x00}

	if len(val) != len(expected) {
		t.FailNow()
	}

	for i, e := range val {
		if e != expected[i] {
			t.FailNow()
		}
	}
}

func TestServerRegister(t *testing.T) {
	reg := new(Register)
	reg.Type = "input"
	reg.DataType = "uint32"
	reg.Scaling = 2

	if err := reg.Init(); err != nil {
		t.Log(err)
		t.FailNow()
	}

	data, err := reg.Bytes(480)
	if err != nil {
		t.Log(err)
		t.FailNow()
	}

	expected := []byte{0x00, 0x00, 0xbb, 0x80}

	for i, e := range data {
		if e != expected[i] {
			t.FailNow()
		}
	}

	val, err := reg.Value(data)
	if err != nil {
		t.Log(err)
		t.FailNow()
	}

	if val != 480 {
		t.FailNow()
	}
}

func ExampleRegister() {
	reg := new(Register)
	reg.Type = "input"
	reg.DataType = "uint32"
	reg.Scaling = 2

	if err := reg.Init(); err != nil {
		fmt.Println(err)
		return
	}

	data, err := reg.Bytes(480)
	if err != nil {
		fmt.Println(err)
		return
	}

	fmt.Printf("%x\n", data)
	// Output: 0000bb80
}
