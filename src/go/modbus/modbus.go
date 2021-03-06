package modbus

import (
	"fmt"
	"strings"

	otsim "github.com/patsec/ot-sim"
	"github.com/patsec/ot-sim/modbus/client"
	"github.com/patsec/ot-sim/modbus/server"

	"github.com/beevik/etree"
)

type Factory struct{}

func (Factory) NewModule(e *etree.Element) (otsim.Module, error) {
	mode := e.SelectAttrValue("mode", "server")

	switch strings.ToLower(mode) {
	case "server":
		name := e.SelectAttrValue("name", "modbus-server")
		return server.New(name), nil
	case "client":
		name := e.SelectAttrValue("name", "modbus-client")
		return client.New(name), nil
	}

	return nil, fmt.Errorf("unknown mode '%s' provided for Modbus module", mode)
}

func init() {
	otsim.AddModuleFactory("modbus", new(Factory))
}
