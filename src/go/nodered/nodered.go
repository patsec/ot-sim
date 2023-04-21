package nodered

import (
	"bufio"
	"context"
	_ "embed"
	"fmt"
	"os"
	"os/exec"
	"syscall"
	"text/template"
	"time"

	otsim "github.com/patsec/ot-sim"

	"github.com/beevik/etree"
	"golang.org/x/crypto/bcrypt"
)

//go:embed settings.js.tmpl
var settingsTmpl string

type Factory struct{}

func (Factory) NewModule(e *etree.Element) (otsim.Module, error) {
	name := e.SelectAttrValue("name", "node-red")
	return New(name), nil
}

type NodeRED struct {
	name string

	pubEndpoint  string
	pullEndpoint string

	executable   string
	settingsPath string

	settings map[string]string
}

func New(name string) *NodeRED {
	return &NodeRED{
		name:         name,
		executable:   "node-red",
		settingsPath: "/etc/node-red.js",
		settings: map[string]string{
			"theme": "dark",
			"host":  "0.0.0.0",
			"port":  "1880",
		},
	}
}

func (this NodeRED) Name() string {
	return this.name
}

func (this *NodeRED) Configure(e *etree.Element) error {
	for _, child := range e.ChildElements() {
		switch child.Tag {
		case "pub-endpoint":
			this.pubEndpoint = child.Text()
		case "pull-endpoint":
			this.pullEndpoint = child.Text()
		case "executable":
			this.executable = child.Text()
		case "settings-path":
			this.settingsPath = child.Text()
		case "theme":
			this.settings["theme"] = child.Text()
		case "flow-path":
			this.settings["flow-path"] = child.Text()
		case "authentication":
			for _, child := range child.ChildElements() {
				switch child.Tag {
				case "editor":
					pass := child.SelectAttrValue("password", "admin")

					hash, err := bcrypt.GenerateFromPassword([]byte(pass), 8)
					if err != nil {
						return fmt.Errorf("generating hash for editor password %s: %w", pass, err)
					}

					this.settings["editor-user"] = child.SelectAttrValue("username", "admin")
					this.settings["editor-pass"] = string(hash)
				case "ui":
					pass := child.SelectAttrValue("password", "admin")

					hash, err := bcrypt.GenerateFromPassword([]byte(pass), 8)
					if err != nil {
						return fmt.Errorf("generating hash for ui password %s: %w", pass, err)
					}

					this.settings["ui-user"] = child.SelectAttrValue("username", "admin")
					this.settings["ui-pass"] = string(hash)
				}
			}
		case "endpoint":
			this.settings["host"] = child.SelectAttrValue("host", "0.0.0.0")
			this.settings["port"] = child.SelectAttrValue("port", "1880")
		}
	}

	return nil
}

func (this *NodeRED) Run(ctx context.Context, pubEndpoint, pullEndpoint string) error {
	// Use ZeroMQ PUB endpoint specified in `node-red` config block if provided.
	if this.pubEndpoint != "" {
		pubEndpoint = this.pubEndpoint
	}

	// Use ZeroMQ PUSH endpoint specified in `node-red` config block if provided.
	if this.pullEndpoint != "" {
		pullEndpoint = this.pullEndpoint
	}

	this.settings["pub-endpoint"] = pubEndpoint
	this.settings["pull-endpoint"] = pullEndpoint

	tmpl, err := template.New("settings").Parse(settingsTmpl)
	if err != nil {
		return fmt.Errorf("parsing Node-RED settings template: %w", err)
	}

	out, err := os.Create(this.settingsPath)
	if err != nil {
		return fmt.Errorf("opening Node-RED settings file for writing: %w", err)
	}

	defer out.Close()

	if err := tmpl.Execute(out, this.settings); err != nil {
		return fmt.Errorf("writing Node-RED settings file: %w", err)
	}

	if err := this.start(ctx); err != nil {
		return fmt.Errorf("starting Node-RED: %w", err)
	}

	return nil
}

func (this NodeRED) start(ctx context.Context) error {
	exePath, err := exec.LookPath(this.executable)
	if err != nil {
		return fmt.Errorf("module executable does not exist at %s", this.executable)
	}

	otsim.Waiter.Add(1)

	go func() {
		defer otsim.Waiter.Done()

		for {
			// Not using `exec.CommandContext` here since we're catching the context
			// being canceled below in order to gracefully terminate the child
			// process. Using `exec.CommandContext` forcefully kills the child process
			// when the context is canceled.
			cmd := exec.Command(exePath, "--settings", this.settingsPath)
			cmd.Env = os.Environ()

			stdout, _ := cmd.StdoutPipe()
			stderr, _ := cmd.StderrPipe()

			this.log("starting Node-RED")

			if err := cmd.Start(); err != nil {
				this.log("[ERROR] starting Node-RED: %v", err)
				return
			}

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

			wait := make(chan error)

			go func() {
				err := cmd.Wait()
				wait <- err
			}()

			select {
			case err := <-wait:
				this.log("[ERROR] Node-RED died (%v)... restarting", err)
				continue
			case <-ctx.Done():
				this.log("stopping Node-RED")
				cmd.Process.Signal(syscall.SIGTERM)

				select {
				case <-wait: // SIGTERM *should* cause cmd to exit
					this.log("Node-RED has stopped")
					return
				case <-time.After(10 * time.Second):
					this.log("forcefully killing Node-RED")
					cmd.Process.Kill()

					return
				}
			}
		}
	}()

	return nil
}

func (this NodeRED) log(format string, a ...any) {
	fmt.Printf("[%s] %s\n", this.name, fmt.Sprintf(format, a...))
}

func init() {
	otsim.AddModuleFactory("node-red", new(Factory))
}
