package common

var SunSpecIdentifier = float64(1400204883)

var IdentifierRegister = Register{
	DataType:      "uint32",
	InternalValue: SunSpecIdentifier,
}

var EndRegister = Register{
	DataType:      "uint16",
	InternalValue: 65535,
}

var EndRegisterLength = Register{
	DataType:      "uint16",
	InternalValue: 0,
}

func init() {
	if err := IdentifierRegister.Init(); err != nil {
		panic(err)
	}

	if err := EndRegister.Init(); err != nil {
		panic(err)
	}

	if err := EndRegisterLength.Init(); err != nil {
		panic(err)
	}
}
