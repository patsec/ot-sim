package sep2

import (
	"fmt"
	"strings"

	otsim "github.com/patsec/ot-sim"
	"github.com/patsec/ot-sim/sep2/client"
	"github.com/patsec/ot-sim/sep2/server"

	"github.com/beevik/etree"
)

type Factory struct{}

func (Factory) NewModule(e *etree.Element) (otsim.Module, error) {
	mode := e.SelectAttrValue("mode", "server")

	switch strings.ToLower(mode) {
	case "server":
		name := e.SelectAttrValue("name", "2030.5-server")
		return server.New(name), nil
	case "client":
		name := e.SelectAttrValue("name", "2030.5-client")
		return client.New(name), nil
	}

	return nil, fmt.Errorf("unknown mode '%s' provided for SEP2 (2030.5) module", mode)
}

func init() {
	otsim.AddModuleFactory("sep2", new(Factory))
}
