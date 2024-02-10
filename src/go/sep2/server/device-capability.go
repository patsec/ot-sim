package server

import (
	"encoding/xml"
	"fmt"
	"net/http"

	"github.com/patsec/ot-sim/sep2/schema"
)

func handleDeviceCapability(w http.ResponseWriter, r *http.Request) error {
	dcap := schema.DeviceCapability{
		Href:              r.RequestURI,
		TimeLink:          schema.TimeLink{Href: "/sep2/tm"},
		EndDeviceListLink: schema.EndDeviceListLink{Href: "/sep2/edev"},
	}

	body, err := xml.Marshal(dcap)
	if err != nil {
		err := fmt.Errorf("marshaling DeviceCapability: %w", err)
		return NewWebError(err).SetStatus(http.StatusInternalServerError)
	}

	w.Write(body)

	return nil
}
