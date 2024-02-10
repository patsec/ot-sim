package server

import (
	"context"
	"net/http"

	"github.com/beevik/etree"
	"github.com/patsec/ot-sim/sep2/schema"
)

type SEP2Server struct {
	name     string
	endpoint string

	devices     map[string]*schema.EndDevice
	aggregators map[string][]string
}

func New(name string) *SEP2Server {
	return &SEP2Server{
		name:        name,
		devices:     make(map[string]*schema.EndDevice),
		aggregators: make(map[string][]string),
	}
}

func (this SEP2Server) Name() string {
	return this.name
}

func (this *SEP2Server) Configure(e *etree.Element) error {
	for _, child := range e.ChildElements() {
		switch child.Tag {
		case "endpoint":
			this.endpoint = child.Text()
		case "device":
			device := child.Text()

			if attr := child.SelectAttr("aggregator"); attr != nil {
				this.aggregators[attr.Value] = append(this.aggregators[attr.Value], device)
			} else {
				this.devices[device] = nil
			}
		}
	}

	return nil
}

func (this *SEP2Server) Run(ctx context.Context, pubEndpoint, pullEndpoint string) error {
	sep := http.NewServeMux()
	sep.Handle("GET /dcap", ErrorHandler(handleDeviceCapability))
	sep.Handle("GET /edev", ErrorHandler(this.handleGetEndDeviceList))
	sep.Handle("POST /edev", ErrorHandler(this.handlePostEndDeviceList))
	sep.Handle("GET /edev/{dev}", ErrorHandler(this.handleGetEndDevice))
	sep.Handle("GET /tm", ErrorHandler(this.handleGetTime))

	http.DefaultServeMux.Handle("/sep2/", http.StripPrefix("/sep2", sep))

	http.DefaultServeMux.HandleFunc("/", func(w http.ResponseWriter, r *http.Request) {
		w.Write([]byte("try GET /sep2/dcap"))
	})

	go http.ListenAndServe(this.endpoint, nil)

	return nil
}
