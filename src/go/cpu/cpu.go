package cpu

import (
	"context"
	"fmt"
	"strings"

	otsim "github.com/patsec/ot-sim"
	"github.com/patsec/ot-sim/util"

	"github.com/beevik/etree"
)

type Factory struct{}

func (Factory) NewModule(e *etree.Element) (otsim.Module, error) {
	name := e.SelectAttrValue("name", "cpu")
	return New(name), nil
}

type CPU struct {
	name string

	pubEndpoint  string
	pullEndpoint string

	apiEndpoint string
	apiCACert   string
	apiCert     string
	apiKey      string

	modules map[string]string
}

func New(name string) *CPU {
	return &CPU{
		name:    name,
		modules: make(map[string]string),
	}
}

func (this CPU) Name() string {
	return this.name
}

func (this *CPU) Configure(e *etree.Element) error {
	for _, child := range e.ChildElements() {
		switch child.Tag {
		case "pub-endpoint":
			this.pubEndpoint = child.Text()
		case "pull-endpoint":
			this.pullEndpoint = child.Text()
		case "api-endpoint":
			// the api/endpoint element takes precedence
			if this.apiEndpoint == "" {
				this.apiEndpoint = child.Text()
			}
		case "api":
			for _, child := range child.ChildElements() {
				switch child.Tag {
				case "endpoint":
					this.apiEndpoint = child.Text()
				case "tls-key":
					this.apiKey = child.Text()
				case "tls-certificate":
					this.apiCert = child.Text()
				case "ca-certificate":
					this.apiCACert = child.Text()
				}
			}
		case "module":
			path := child.Text()
			name := child.SelectAttrValue("name", path)

			this.modules[name] = path
		}
	}

	return nil
}

func (this CPU) Run(ctx context.Context, pubEndpoint, pullEndpoint string) error {
	config := util.MustConfigFile(ctx)

	for name, path := range this.modules {
		path = strings.ReplaceAll(path, "{{config_file}}", config)
		parts := strings.Split(path, " ")

		if err := StartModule(ctx, name, parts[0], parts[1:]...); err != nil {
			return fmt.Errorf("failed to start module %s: %w", name, err)
		}
	}

	// Use ZeroMQ PUB endpoint specified in `cpu` config block if provided.
	if this.pubEndpoint != "" {
		pubEndpoint = this.pubEndpoint
	}

	// Use ZeroMQ PUSH endpoint specified in `cpu` config block if provided.
	if this.pullEndpoint != "" {
		pullEndpoint = this.pullEndpoint
	}

	if this.apiEndpoint != "" {
		api := NewAPIServer(pullEndpoint, pubEndpoint)
		if err := api.Start(this.apiEndpoint, this.apiCert, this.apiKey, this.apiCACert); err != nil {
			fmt.Printf("[CPU] [ERROR] starting API server: %v\n", err)
		}
	}

	logger := func(topic, msg string) error {
		fmt.Printf("[%s] %s\n", topic, msg)
		return nil
	}

	var (
		logErrors     = make(chan error)
		healthErrors  = make(chan error)
		metricsErrors = make(chan error)
	)

	logHandlers := []MsgBusHandler{logger}
	healthHandlers := []MsgBusHandler{metricsHandler}
	runtimeHandlers := []MsgBusHandler{logger}

	go func() {
		for {
			select {
			case <-ctx.Done():
				return
			case err := <-logErrors:
				fmt.Printf("[CPU] error processing logs: %v\n", err)
				go MonitorMsgBusChannel(ctx, pubEndpoint, "LOG", logHandlers, logErrors)
			case err := <-healthErrors:
				fmt.Printf("[CPU] error processing health updates: %v\n", err)
				go MonitorMsgBusChannel(ctx, pubEndpoint, "HEALTH", healthHandlers, healthErrors)
			case err := <-metricsErrors:
				fmt.Printf("[CPU] error processing metrics: %v\n", err)
				go MonitorMsgBusChannel(ctx, pubEndpoint, "RUNTIME", runtimeHandlers, metricsErrors)
			}
		}
	}()

	go MonitorMsgBusChannel(ctx, pubEndpoint, "LOG", logHandlers, logErrors)
	go MonitorMsgBusChannel(ctx, pubEndpoint, "HEALTH", healthHandlers, healthErrors)
	go MonitorMsgBusChannel(ctx, pubEndpoint, "RUNTIME", runtimeHandlers, metricsErrors)

	return nil
}

func init() {
	otsim.AddModuleFactory("cpu", new(Factory))
}
