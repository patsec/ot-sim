package server

import (
	"encoding/xml"
	"fmt"
	"net/http"
	"time"

	"github.com/patsec/ot-sim/sep2/schema"
)

func (this SEP2Server) handleGetTime(w http.ResponseWriter, r *http.Request) error {
	device := r.Header.Get("X-SEP2-Device")

	if device == "" {
		return NewWebError(fmt.Errorf("missing device name")).SetMessageToError()
	}

	_, ok := this.devices[device]
	if !ok {
		return NewWebError(fmt.Errorf("unauthorized device")).SetStatus(http.StatusForbidden)
	}

	t := schema.Time{
		CurrentTime: time.Now().Unix(),
	}

	body, err := xml.Marshal(t)
	if err != nil {
		err := fmt.Errorf("marshaling Time: %w", err)
		return NewWebError(err).SetStatus(http.StatusInternalServerError)
	}

	w.Write(body)

	return nil
}
