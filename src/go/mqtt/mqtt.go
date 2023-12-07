package mqtt

import (
	"fmt"
	"strings"

	otsim "github.com/patsec/ot-sim"
	"github.com/patsec/ot-sim/mqtt/broker"
	"github.com/patsec/ot-sim/mqtt/client"

	"github.com/beevik/etree"
)

type Factory struct{}

func (Factory) NewModule(e *etree.Element) (otsim.Module, error) {
	mode := e.SelectAttrValue("mode", "client")

	switch strings.ToLower(mode) {
	case "broker":
		name := e.SelectAttrValue("name", "mqtt-broker")
		return broker.New(name), nil
	case "client":
		name := e.SelectAttrValue("name", "mqtt-client")
		return client.New(name), nil
	}

	return nil, fmt.Errorf("unknown mode '%s' provided for MQTT module", mode)
}

func init() {
	otsim.AddModuleFactory("mqtt", new(Factory))
}
