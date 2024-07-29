package common

var SunSpecIdentifier = float64(1400204883) // SunS

var IdentifierRegister = Register{
	DataType:      "uint32",
	Name:          "SunSpec_Identifier",
	InternalValue: SunSpecIdentifier,
}

var EndRegister = Register{
	DataType:      "uint16",
	Name:          "SunSpec_EndRegister",
	InternalValue: 65535,
}

var EndRegisterLength = Register{
	DataType:      "uint16",
	Name:          "SunSpec_EndRegister_Length",
	InternalValue: 0,
}

type Models struct {
	Order    []int
	Settings map[int]ModelSettings
}

type ModelSettings struct {
	Model     int
	StartAddr int
	Length    int
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
