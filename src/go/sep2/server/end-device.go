package server

import (
	"encoding/xml"
	"fmt"
	"io"
	"net/http"

	"github.com/patsec/ot-sim/sep2/schema"
)

func (this SEP2Server) handleGetEndDevice(w http.ResponseWriter, r *http.Request) error {
	device := r.Header.Get("X-SEP2-Device")

	if device == "" {
		return NewWebError(fmt.Errorf("missing device name")).SetMessageToError()
	}

	if val := r.PathValue("dev"); device != val {
		return NewWebError(fmt.Errorf("device %s asked for EndDevice %s", device, val)).SetStatus(http.StatusForbidden)
	}

	dev, ok := this.devices[device]
	if !ok {
		return NewWebError(fmt.Errorf("unauthorized device")).SetStatus(http.StatusForbidden)
	}

	if dev == nil {
		return NewWebError(fmt.Errorf("device not registered")).SetMessageToError()
	}

	body, err := xml.Marshal(dev)
	if err != nil {
		err := fmt.Errorf("marshaling EndDevice: %w", err)
		return NewWebError(err).SetStatus(http.StatusInternalServerError)
	}

	w.Write(body)

	return nil
}

func (this SEP2Server) handleGetEndDeviceList(w http.ResponseWriter, r *http.Request) error {
	device := r.Header.Get("X-SEP2-Device")

	if device == "" {
		return NewWebError(fmt.Errorf("missing device name")).SetMessageToError()
	}

	edev := schema.EndDeviceList{
		Href:         r.RequestURI,
		Subscribable: 0,
	}

	_, ok := this.aggregators[device]
	if ok {
		// TODO: aggregator
		return NewWebError(fmt.Errorf("aggregator not implemented")).SetStatus(http.StatusNotImplemented).SetMessageToError()
	} else {
		dev, ok := this.devices[device]
		if ok && dev != nil {
			edev.EndDevice = append(edev.EndDevice, *dev)
			edev.Results = 1
			edev.All = 1
		}
	}

	body, err := xml.Marshal(edev)
	if err != nil {
		err := fmt.Errorf("marshaling EndDeviceList: %w", err)
		return NewWebError(err).SetStatus(http.StatusInternalServerError)
	}

	w.Write(body)

	return nil
}

func (this *SEP2Server) handlePostEndDeviceList(w http.ResponseWriter, r *http.Request) error {
	device := r.Header.Get("X-SEP2-Device")

	if device == "" {
		return NewWebError(fmt.Errorf("missing device name")).SetMessageToError()
	}

	_, ok := this.aggregators[device]
	if ok {
		// TODO: aggregator
		return NewWebError(fmt.Errorf("aggregator not implemented")).SetStatus(http.StatusNotImplemented).SetMessageToError()
	} else {
		dev, ok := this.devices[device]
		if !ok {
			return NewWebError(fmt.Errorf("unauthorized device")).SetStatus(http.StatusForbidden)
		}

		if dev != nil {
			return NewWebError(fmt.Errorf("device already registered")).SetMessageToError()
		}
	}

	body, err := io.ReadAll(r.Body)
	if err != nil {
		return NewWebError(err).SetMessage("no EndDevice data provided")
	}

	var dev *schema.EndDevice

	if err := xml.Unmarshal(body, &dev); err != nil {
		return NewWebError(err).SetMessage("invalid EndDevice data provided")
	}

	this.devices[device] = dev

	w.Header().Add("Location", fmt.Sprintf("/sep2/edev/%s", device))
	w.WriteHeader(http.StatusCreated)

	return nil
}
