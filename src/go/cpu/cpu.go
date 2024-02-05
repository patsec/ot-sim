package cpu

import (
	"context"
	"fmt"
	"io"
	"log"
	"os"
	"strconv"
	"strings"

	otsim "github.com/patsec/ot-sim"
	"github.com/patsec/ot-sim/msgbus"

	"github.com/beevik/etree"
	lumberjack "gopkg.in/natefinch/lumberjack.v2"
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

	elasticEndpoint string
	elasticIndex    string
	lokiEndpoint    string

	pusher *msgbus.Pusher

	logFile io.Writer
}

func New(name string) *CPU {
	return &CPU{
		name:    name,
		logFile: os.Stdout,
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
		case "logs":
			for _, child := range child.ChildElements() {
				switch child.Tag {
				case "file":
					size, err := strconv.Atoi(child.SelectAttrValue("size", "5"))
					if err != nil {
						return fmt.Errorf("parsing 'size' attribute for log rotation: %w", err)
					}

					backups, err := strconv.Atoi(child.SelectAttrValue("backups", "1"))
					if err != nil {
						return fmt.Errorf("parsing 'backups' attribute for log rotation: %w", err)
					}

					age, err := strconv.Atoi(child.SelectAttrValue("age", "1"))
					if err != nil {
						return fmt.Errorf("parsing 'age' attribute for log rotation: %w", err)
					}

					compress, err := strconv.ParseBool(child.SelectAttrValue("compress", "true"))
					if err != nil {
						return fmt.Errorf("parsing 'compress' attribute for log rotation: %w", err)
					}

					log.SetOutput(&lumberjack.Logger{
						Filename:   child.Text(),
						MaxSize:    size,
						MaxBackups: backups,
						MaxAge:     age,
						Compress:   compress,
					})
				case "elastic":
					this.elasticEndpoint = child.Text()
					this.elasticIndex = child.SelectAttrValue("index", "ot-sim-logs")
				case "loki":
					this.lokiEndpoint = child.Text()
				}
			}
		case "module":
			mod := &module{
				name:    child.SelectAttrValue("name", child.Text()),
				path:    child.Text(),
				workDir: child.SelectAttrValue("workingDir", ""),
				env:     strings.Split(child.SelectAttrValue("env", ""), ":"),
			}

			modules[mod.name] = mod
		}
	}

	return nil
}

func (this *CPU) Run(ctx context.Context, pubEndpoint, pullEndpoint string) error {
	for _, module := range modules {
		ctx = ctxSetElasticEndpoint(ctx, this.elasticEndpoint)
		ctx = ctxSetElasticIndex(ctx, this.elasticIndex)
		ctx = ctxSetLokiEndpoint(ctx, this.lokiEndpoint)

		if err := StartModule(ctx, module); err != nil {
			return fmt.Errorf("failed to start module %s: %w", module.name, err)
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

	this.pusher = msgbus.MustNewPusher(pullEndpoint)

	if this.apiEndpoint != "" {
		api := NewAPIServer(pullEndpoint, pubEndpoint)
		if err := api.Start(this.apiEndpoint, this.apiCert, this.apiKey, this.apiCACert); err != nil {
			log.Printf("[CPU] [ERROR] starting API server: %v\n", err)
		}
	}

	logger := func(topic, msg string) error {
		log.Printf("[%s] %s\n", topic, msg)
		return nil
	}

	var (
		logErrors     = make(chan error)
		healthErrors  = make(chan error)
		metricsErrors = make(chan error)
	)

	var (
		logHandlers     = []MsgBusHandler{logger}
		healthHandlers  = []MsgBusHandler{metricsHandler}
		runtimeHandlers = []MsgBusHandler{logger}
	)

	go func() {
		for {
			select {
			case <-ctx.Done():
				return
			case err := <-logErrors:
				log.Printf("[CPU] [ERROR] processing logs: %v\n", err)
				go MonitorMsgBusChannel(ctx, pubEndpoint, "LOG", logHandlers, logErrors)
			case err := <-healthErrors:
				log.Printf("[CPU] [ERROR] processing health updates: %v\n", err)
				go MonitorMsgBusChannel(ctx, pubEndpoint, "HEALTH", healthHandlers, healthErrors)
			case err := <-metricsErrors:
				log.Printf("[CPU] [ERROR] error processing metrics: %v\n", err)
				go MonitorMsgBusChannel(ctx, pubEndpoint, "RUNTIME", runtimeHandlers, metricsErrors)
			}
		}
	}()

	go MonitorMsgBusChannel(ctx, pubEndpoint, "LOG", logHandlers, logErrors)
	go MonitorMsgBusChannel(ctx, pubEndpoint, "HEALTH", healthHandlers, healthErrors)
	go MonitorMsgBusChannel(ctx, pubEndpoint, "RUNTIME", runtimeHandlers, metricsErrors)
	go MonitorMsgBusChannel(ctx, pubEndpoint, "INTERNAL", []MsgBusHandler{this.internalHandler}, nil)

	return nil
}

func init() {
	otsim.AddModuleFactory("cpu", new(Factory))
}
