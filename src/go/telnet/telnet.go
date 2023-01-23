// Package telnet implements a Telnet server as an OT-sim module.
package telnet

import (
	"context"
	"errors"
	"fmt"
	"io"
	"strconv"
	"time"

	otsim "github.com/patsec/ot-sim"
	"github.com/patsec/ot-sim/msgbus"

	"github.com/beevik/etree"
	"github.com/reiver/go-telnet"
	"github.com/reiver/go-telnet/telsh"
)

func init() {
	otsim.AddModuleFactory("telnet", new(Factory))
}

type Factory struct{}

func (Factory) NewModule(e *etree.Element) (otsim.Module, error) {
	name := e.SelectAttrValue("name", "telnet")
	return New(name), nil
}

type Telnet struct {
	name     string
	endpoint string
	banner   string
	tags     map[string]float64

	pusher *msgbus.Pusher
}

func New(name string) *Telnet {
	return &Telnet{
		name:     name,
		endpoint: ":23",
		tags:     make(map[string]float64),
	}
}

func (this Telnet) Name() string {
	return this.name
}

func (this *Telnet) Configure(e *etree.Element) error {
	for _, child := range e.ChildElements() {
		switch child.Tag {
		case "endpoint":
			this.endpoint = child.Text()
		case "banner":
			this.banner = child.Text()
		}
	}

	return nil
}

func (this *Telnet) Run(ctx context.Context, pubEndpoint, pullEndpoint string) error {
	this.pusher = msgbus.MustNewPusher(pullEndpoint)
	subscriber := msgbus.MustNewSubscriber(pubEndpoint)

	subscriber.AddStatusHandler(this.handleMsgBusStatus)
	subscriber.Start("RUNTIME")

	shellHandler := telsh.NewShellHandler()

	if banner, ok := banners[this.banner]; ok {
		shellHandler.WelcomeMessage = banner
	} else {
		shellHandler.WelcomeMessage = banners["default"]
	}

	shellHandler.Register("date", telsh.ProducerFunc(dateProducerFunc))
	shellHandler.Register("query", telsh.ProducerFunc(this.queryProducerFunc))
	shellHandler.Register("write", telsh.ProducerFunc(this.writeProducerFunc))

	if err := telnet.ListenAndServe(this.endpoint, shellHandler); nil != err {
		panic(err)
	}

	return nil
}

func (this *Telnet) handleMsgBusStatus(env msgbus.Envelope) {
	if env.Sender() == this.name {
		return
	}

	status, err := env.Status()
	if err != nil {
		if !errors.Is(err, msgbus.ErrKindNotStatus) {
			this.log("[ERROR] getting Status message from envelope: %v", err)
		}

		return
	}

	for _, point := range status.Measurements {
		this.tags[point.Tag] = point.Value
	}
}

func (this Telnet) log(format string, a ...any) {
	fmt.Printf("[%s] %s\n", this.name, fmt.Sprintf(format, a...))
}

func (this Telnet) queryHandlerFunc(stdin io.ReadCloser, stdout io.WriteCloser, stderr io.WriteCloser, args ...string) error {
	defer fmt.Fprintln(stdout)

	if len(args) == 0 {
		for tag, val := range this.tags {
			fmt.Fprintf(stdout, "%s = %f\n", tag, val)
		}
	} else {
		for _, tag := range args {
			if val, ok := this.tags[tag]; ok {
				fmt.Fprintf(stdout, "%s = %f\n", tag, val)
			} else {
				fmt.Fprintf(stderr, "tag %s is unknown\n", tag)
			}
		}
	}

	return nil
}

func (this Telnet) queryProducerFunc(ctx telnet.Context, name string, args ...string) telsh.Handler {
	return telsh.PromoteHandlerFunc(this.queryHandlerFunc, args...)
}

func (this Telnet) writeHandlerFunc(stdin io.ReadCloser, stdout io.WriteCloser, stderr io.WriteCloser, args ...string) error {
	defer fmt.Fprintln(stdout)

	if len(args) != 2 {
		fmt.Fprintln(stderr, "must provide two values to write function")
		return fmt.Errorf("must provide two values to write function")
	}

	tag := args[0]
	val, err := strconv.ParseFloat(args[1], 64)
	if err != nil {
		fmt.Fprintf(stderr, "invalid value %s\n", args[1])
		return err
	}

	updates := []msgbus.Point{{Tag: tag, Value: val}}

	env, err := msgbus.NewUpdateEnvelope(this.name, msgbus.Update{Updates: updates})
	if err != nil {
		this.log("[ERROR] creating new update message: %v", err)
		return err
	}

	if err := this.pusher.Push("RUNTIME", env); err != nil {
		this.log("[ERROR] sending update message: %v", err)
		return err
	}

	fmt.Fprintf(stdout, "wrote %s=%f\n", tag, val)
	return nil
}

func (this Telnet) writeProducerFunc(ctx telnet.Context, name string, args ...string) telsh.Handler {
	return telsh.PromoteHandlerFunc(this.writeHandlerFunc, args...)
}

func dateHandlerFunc(stdin io.ReadCloser, stdout io.WriteCloser, stderr io.WriteCloser, args ...string) error {
	defer fmt.Fprintln(stdout)

	fmt.Fprintln(stdout, time.Now().Format("Mon Jan 2 15:04:05 -0700 MST 2006"))
	return nil
}

func dateProducerFunc(ctx telnet.Context, name string, args ...string) telsh.Handler {
	return telsh.PromoteHandlerFunc(dateHandlerFunc)
}
