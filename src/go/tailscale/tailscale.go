// Package tailscale implements a Tailscale client as an OT-sim module.
package tailscale

import (
	"bufio"
	"context"
	"errors"
	"fmt"
	"io"
	"io/fs"
	"os"
	"os/exec"
	"strconv"
	"syscall"
	"time"

	otsim "github.com/patsec/ot-sim"
	"github.com/patsec/ot-sim/msgbus"
	"github.com/patsec/ot-sim/util"

	"github.com/beevik/etree"
)

func init() {
	otsim.AddModuleFactory("tailscale", new(Factory))
}

type Factory struct{}

func (Factory) NewModule(e *etree.Element) (otsim.Module, error) {
	name := e.SelectAttrValue("name", "tailscale")
	return New(name), nil
}

type Tailscale struct {
	name string

	authKey  string
	hostname string
	dns      bool
	routes   bool

	pullEndpoint string
	pusher       *msgbus.Pusher
}

func New(name string) *Tailscale {
	return &Tailscale{
		name: name,
	}
}

func (this Tailscale) Name() string {
	return this.name
}

func (this *Tailscale) Configure(e *etree.Element) error {
	for _, child := range e.ChildElements() {
		switch child.Tag {
		case "pull-endpoint":
			this.pullEndpoint = child.Text()
		case "auth-key":
			this.authKey = child.Text()
		case "hostname":
			this.hostname = child.Text()
		case "accept-dns":
			this.dns, _ = strconv.ParseBool(child.Text())
		case "accept-routes":
			this.routes, _ = strconv.ParseBool(child.Text())
		}
	}

	if this.authKey == "" {
		this.authKey = os.Getenv("OTSIM_TAILSCALE_AUTHKEY")

		if this.authKey == "" {
			return fmt.Errorf("no Tailscale auth key provided")
		}
	}

	if this.hostname == "" {
		var err error

		this.hostname, err = os.Hostname()
		if err != nil {
			return fmt.Errorf("unable to set hostname: %w", err)
		}
	}

	return nil
}

func (this *Tailscale) Run(ctx context.Context, _, pullEndpoint string) error {
	// Use ZeroMQ PULL endpoint specified in `tailscale` config block if provided.
	if this.pullEndpoint != "" {
		pullEndpoint = this.pullEndpoint
	}

	this.pusher = msgbus.MustNewPusher(pullEndpoint)

	if err := this.start(ctx); err != nil {
		return fmt.Errorf("starting Tailscale daemon: %w", err)
	}

	if err := this.up(ctx); err != nil {
		return fmt.Errorf("bringing Tailscale up: %w", err)
	}

	go this.status(ctx)

	return nil
}

func (this Tailscale) log(format string, a ...any) {
	fmt.Printf("[%s] %s\n", this.name, fmt.Sprintf(format, a...))
}

func (this Tailscale) start(ctx context.Context) error {
	exePath, err := exec.LookPath("tailscaled")
	if err != nil {
		return util.NewExitError(util.ExitNoRestart, "module executable does not exist at tailscaled")
	}

	args := []string{
		"--socket=/tmp/tailscaled.sock",
		"--state=mem:", "--statedir=/tmp",
	}

	otsim.Waiter.Add(1)

	go func() {
		defer otsim.Waiter.Done()

		for {
			// Not using `exec.CommandContext` here since we're catching the context
			// being canceled below in order to gracefully terminate the child
			// process. Using `exec.CommandContext` forcefully kills the child process
			// when the context is canceled.
			cmd := exec.Command(exePath, args...)

			stdout, _ := cmd.StdoutPipe()
			stderr, _ := cmd.StderrPipe()

			go func() {
				scanner := bufio.NewScanner(stdout)
				scanner.Split(bufio.ScanLines)

				for scanner.Scan() {
					this.log(scanner.Text())
				}
			}()

			go func() {
				scanner := bufio.NewScanner(stderr)
				scanner.Split(bufio.ScanLines)

				for scanner.Scan() {
					this.log("[ERROR] %s", scanner.Text())
				}
			}()

			this.log("starting Tailscale daemon")

			if err := cmd.Start(); err != nil {
				this.log("[ERROR] starting Tailscale daemon: %v", err)
				return
			}

			wait := make(chan error)

			go func() {
				err := cmd.Wait()
				wait <- err
			}()

			select {
			case err := <-wait:
				this.log("[ERROR] Tailscale daemon died (%v)... restarting", err)
				continue
			case <-ctx.Done():
				this.log("stopping Tailscale daemon")
				cmd.Process.Signal(syscall.SIGTERM)

				select {
				case <-wait: // SIGTERM *should* cause cmd to exit
					this.log("Tailscale daemon has stopped")
					return
				case <-time.After(10 * time.Second):
					this.log("forcefully killing Tailscale daemon")
					cmd.Process.Kill()

					return
				}
			}
		}
	}()

	this.log("waiting for Tailscale socket")

	for {
		if ctx.Err() != nil {
			return util.NewExitError(util.ExitNoRestart, "timed out waiting for Tailscale socket")
		}

		if _, err := os.Stat("/tmp/tailscaled.sock"); err != nil {
			if errors.Is(err, fs.ErrNotExist) {
				time.Sleep(100 * time.Millisecond)
				continue
			} else {
				return util.NewExitError(util.ExitNoRestart, "waiting for Tailscale socket: %v", err)
			}
		}

		break
	}

	return nil
}

func (this Tailscale) up(ctx context.Context) error {
	exePath, err := exec.LookPath("tailscale")
	if err != nil {
		return util.NewExitError(util.ExitNoRestart, "module executable does not exist at tailscale")
	}

	args := []string{
		"--socket=/tmp/tailscaled.sock", "up",
		"--authkey=" + this.authKey,
		"--hostname=" + this.hostname,
		"--accept-dns=" + strconv.FormatBool(this.dns),
		"--accept-routes=" + strconv.FormatBool(this.routes),
	}

	this.log("initializing Tailscale")

	cmd := exec.CommandContext(ctx, exePath, args...)

	stdout, _ := cmd.StdoutPipe()
	stderr, _ := cmd.StderrPipe()

	go func() {
		scanner := bufio.NewScanner(stdout)
		scanner.Split(bufio.ScanLines)

		for scanner.Scan() {
			this.log(scanner.Text())
		}
	}()

	go func() {
		scanner := bufio.NewScanner(stderr)
		scanner.Split(bufio.ScanLines)

		for scanner.Scan() {
			this.log("[ERROR] %s", scanner.Text())
		}
	}()

	if err := cmd.Run(); err != nil {
		return fmt.Errorf("tailscale up failed: %v", err)
	}

	return nil
}

func (this Tailscale) status(ctx context.Context) {
	exePath, err := exec.LookPath("tailscale")
	if err != nil {
		return
	}

	args := []string{"--socket=/tmp/tailscaled.sock", "status"}

	for {
		select {
		case <-ctx.Done():
			return
		case <-time.After(5 * time.Second):
			point := msgbus.Point{
				Tag:    fmt.Sprintf("%s.connected", this.name),
				Tstamp: uint64(time.Now().Unix()),
			}

			cmd := exec.CommandContext(ctx, exePath, args...)
			cmd.Stdout = io.Discard
			cmd.Stderr = io.Discard

			if err := cmd.Run(); err == nil {
				point.Value = 1.0
			} else {
				point.Value = 0.0
			}

			env, err := msgbus.NewEnvelope(this.name, msgbus.Status{Measurements: []msgbus.Point{point}})
			if err != nil {
				this.log("[ERROR] creating status message: %v", err)
				continue
			}

			if err := this.pusher.Push("RUNTIME", env); err != nil {
				this.log("[ERROR] sending status message: %v", err)
			}
		}
	}
}
