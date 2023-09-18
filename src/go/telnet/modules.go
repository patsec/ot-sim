package telnet

import (
	"fmt"
	"io"
	"time"

	"github.com/patsec/ot-sim/msgbus"
	"github.com/reiver/go-telnet"
	"github.com/reiver/go-telnet/telsh"

	"github.com/gofrs/uuid"
)

func (this Telnet) modulesHandlerFunc(stdin io.ReadCloser, stdout io.WriteCloser, stderr io.WriteCloser, args ...string) error {
	defer fmt.Fprintln(stdout)

	var (
		confirmation = uuid.Must(uuid.NewV4()).String()
		control      = msgbus.ModuleControl{List: true, Recipient: "CPU", Confirm: confirmation}
	)

	conf := this.internal.RegisterConfirmationHandler(confirmation)

	env, err := msgbus.NewEnvelope(this.name, control)
	if err != nil {
		this.log("[ERROR] creating new module control message: %v", err)
		return err
	}

	if err := this.pusher.Push("INTERNAL", env); err != nil {
		this.log("[ERROR] sending module control message: %v", err)
		return err
	}

	select {
	case c := <-conf:
		for k, v := range c.Results {
			fmt.Fprintf(stdout, "%s --> %s\n", k, v)
		}
	case <-time.After(5 * time.Second):
		fmt.Fprintln(stderr, "request for module list timed out")
	}

	return nil
}

func (this Telnet) modulesProducerFunc(ctx telnet.Context, name string, args ...string) telsh.Handler {
	return telsh.PromoteHandlerFunc(this.modulesHandlerFunc, args...)
}

func (this Telnet) disableModuleHandlerFunc(stdin io.ReadCloser, stdout io.WriteCloser, stderr io.WriteCloser, args ...string) error {
	defer fmt.Fprintln(stdout)

	if len(args) == 0 {
		fmt.Fprintln(stderr, "must provide at least one module to disable")
		return fmt.Errorf("must provide at least one module to disable")
	}

	var (
		confirmation = uuid.Must(uuid.NewV4()).String()
		control      = msgbus.ModuleControl{Disable: args, Recipient: "CPU", Confirm: confirmation}
	)

	conf := this.internal.RegisterConfirmationHandler(confirmation)

	env, err := msgbus.NewEnvelope(this.name, control)
	if err != nil {
		this.log("[ERROR] creating new module control message: %v", err)
		return err
	}

	if err := this.pusher.Push("INTERNAL", env); err != nil {
		this.log("[ERROR] sending module control message: %v", err)
		return err
	}

	select {
	case c := <-conf:
		for k, v := range c.Results {
			fmt.Fprintf(stdout, "%s --> %s\n", k, v)
		}

		for k, v := range c.Errors {
			fmt.Fprintf(stderr, "%s --> %s\n", k, v)
		}
	case <-time.After(5 * time.Second):
		fmt.Fprintln(stderr, "request for module list timed out")
	}

	return nil
}

func (this Telnet) disableModuleProducerFunc(ctx telnet.Context, name string, args ...string) telsh.Handler {
	return telsh.PromoteHandlerFunc(this.disableModuleHandlerFunc, args...)
}

func (this Telnet) enableModuleHandlerFunc(stdin io.ReadCloser, stdout io.WriteCloser, stderr io.WriteCloser, args ...string) error {
	defer fmt.Fprintln(stdout)

	if len(args) == 0 {
		fmt.Fprintln(stderr, "must provide at least one module to enable")
		return fmt.Errorf("must provide at least one module to enable")
	}

	var (
		confirmation = uuid.Must(uuid.NewV4()).String()
		control      = msgbus.ModuleControl{Enable: args, Recipient: "CPU", Confirm: confirmation}
	)

	conf := this.internal.RegisterConfirmationHandler(confirmation)

	env, err := msgbus.NewEnvelope(this.name, control)
	if err != nil {
		this.log("[ERROR] creating new module control message: %v", err)
		return err
	}

	if err := this.pusher.Push("INTERNAL", env); err != nil {
		this.log("[ERROR] sending module control message: %v", err)
		return err
	}

	select {
	case c := <-conf:
		for k, v := range c.Results {
			fmt.Fprintf(stdout, "%s --> %s\n", k, v)
		}

		for k, v := range c.Errors {
			fmt.Fprintf(stderr, "%s --> %s\n", k, v)
		}
	case <-time.After(5 * time.Second):
		fmt.Fprintln(stderr, "request for module list timed out")
	}

	return nil
}

func (this Telnet) enableModuleProducerFunc(ctx telnet.Context, name string, args ...string) telsh.Handler {
	return telsh.PromoteHandlerFunc(this.enableModuleHandlerFunc, args...)
}
