package cpu

import (
	"bufio"
	"bytes"
	"context"
	"encoding/json"
	"fmt"
	"log"
	"net/http"
	"os"
	"os/exec"
	"strconv"
	"strings"
	"syscall"
	"time"

	otsim "github.com/patsec/ot-sim"
	"github.com/patsec/ot-sim/util"
)

type module struct {
	ctx      context.Context
	name     string
	path     string
	args     []string
	workDir  string
	canceler chan struct{}

	initialized bool
}

func (this *module) init(ctx context.Context) {
	if this.initialized {
		return
	}

	var (
		config = util.MustConfigFile(ctx)
		path   = strings.ReplaceAll(this.path, "{{config_file}}", config)
		parts  = strings.Split(path, " ")
	)

	this.ctx = ctx

	this.path = parts[0]
	this.args = parts[1:]

	this.initialized = true
}

var (
	hostname string
	modules  = make(map[string]*module)
)

func init() {
	var err error

	if hostname, err = os.Hostname(); err != nil {
		panic(err)
	}
}

// StartModule starts the process for another module and monitors it to make
// sure it doesn't die, restarting it if it does.
func StartModule(ctx context.Context, mod *module) error {
	mod.init(ctx)

	exePath, err := exec.LookPath(mod.path)
	if err != nil {
		return fmt.Errorf("module executable does not exist at %s", mod.path)
	}

	modules[mod.name] = mod

	otsim.Waiter.Add(1)

	go func() {
		defer otsim.Waiter.Done()

		var (
			elasticClient *http.Client
			elasticURL    string
			lokiClient    *http.Client
			lokiURL       string
		)

		if e := ctxGetElasticEndpoint(ctx); e != "" {
			index := ctxGetElasticIndex(ctx)

			elasticClient = new(http.Client)
			elasticURL = fmt.Sprintf("%s/%s/_bulk", e, index)
		}

		if e := ctxGetLokiEndpoint(ctx); e != "" {
			lokiClient = new(http.Client)
			lokiURL = fmt.Sprintf("%s/loki/api/v1/push", e)
		}

		for {
			// Not using `exec.CommandContext` here since we're catching the context
			// being canceled below in order to gracefully terminate the child
			// process. Using `exec.CommandContext` forcefully kills the child process
			// when the context is canceled.
			cmd := exec.Command(exePath, mod.args...)
			cmd.Env = os.Environ()

			if mod.workDir != "" {
				cmd.Dir = mod.workDir
				log.Printf("[CPU] working directory for module %s set to %s\n", mod.name, cmd.Dir)
			}

			stdout, _ := cmd.StdoutPipe()
			stderr, _ := cmd.StderrPipe()

			log.Printf("[CPU] starting %s module\n", mod.name)

			if err := cmd.Start(); err != nil {
				log.Printf("[CPU] [ERROR] starting %s module: %v\n", mod.name, err)
				return
			}

			go func() {
				scanner := bufio.NewScanner(stdout)
				scanner.Split(bufio.ScanLines)

				var values [][]string

				for scanner.Scan() {
					text := scanner.Text()

					if elasticClient != nil || lokiClient != nil {
						values = append(values, []string{fmt.Sprintf("%d", time.Now().UnixNano()), text})

						if len(values) >= 10 {
							if elasticClient != nil {
								bulk := buildElasticBulk(mod.name, "log", values)

								resp, err := elasticClient.Post(elasticURL, "application/x-ndjson", bytes.NewBuffer(bulk))
								if err == nil {
									resp.Body.Close()
								}
							}

							if lokiClient != nil {
								stream := buildLokiStream(mod.name, "log", values)

								resp, err := lokiClient.Post(lokiURL, "application/json", bytes.NewBuffer(stream))
								if err == nil {
									resp.Body.Close()
								}
							}

							values = nil
						}
					}

					log.Printf("[LOG] %s\n", text)
				}
			}()

			go func() {
				scanner := bufio.NewScanner(stderr)
				scanner.Split(bufio.ScanLines)

				var values [][]string

				for scanner.Scan() {
					text := scanner.Text()

					if elasticClient != nil || lokiClient != nil {
						values = append(values, []string{fmt.Sprintf("%d", time.Now().UnixNano()), text})

						if len(values) >= 10 {
							if elasticClient != nil {
								bulk := buildElasticBulk(mod.name, "error", values)

								resp, err := elasticClient.Post(elasticURL, "application/x-ndjson", bytes.NewBuffer(bulk))
								if err == nil {
									resp.Body.Close()
								}
							}

							if lokiClient != nil {
								stream := buildLokiStream(mod.name, "error", values)

								resp, err := lokiClient.Post(lokiURL, "application/json", bytes.NewBuffer(stream))
								if err == nil {
									resp.Body.Close()
								}
							}

							values = nil
						}
					}

					log.Printf("[LOG] [ERROR] %s\n", text)
				}
			}()

			wait := make(chan error)

			go func() {
				err := cmd.Wait()
				wait <- err
			}()

			mod.canceler = make(chan struct{})

			select {
			case err := <-wait:
				if err, ok := err.(*exec.ExitError); ok {
					if err.ExitCode() == util.ExitNoRestart {
						log.Printf("[CPU] [ERROR] %s module died and requested no restart\n", mod.name)
						return
					}
				}

				log.Printf("[CPU] [ERROR] %s module died (%v)... restarting\n", mod.name, err)
				continue
			case <-mod.canceler:
				log.Printf("[CPU] disabling %s module\n", mod.name)
				cmd.Process.Signal(syscall.SIGTERM)

				mod.canceler = nil

				select {
				case <-wait: // SIGTERM *should* cause cmd to exit
					log.Printf("[CPU] %s module has been disabled\n", mod.name)
					return
				case <-time.After(10 * time.Second):
					log.Printf("[CPU] forcefully disabling %s module\n", mod.name)
					cmd.Process.Kill()

					return
				}
			case <-ctx.Done():
				log.Printf("[CPU] stopping %s module\n", mod.name)
				cmd.Process.Signal(syscall.SIGTERM)

				select {
				case <-wait: // SIGTERM *should* cause cmd to exit
					log.Printf("[CPU] %s module has stopped\n", mod.name)
					return
				case <-time.After(10 * time.Second):
					log.Printf("[CPU] forcefully killing %s module\n", mod.name)
					cmd.Process.Kill()

					return
				}
			}
		}
	}()

	return nil
}

func buildElasticBulk(name, typ string, values [][]string) []byte {
	var docs []string

	for _, val := range values {
		ts, _ := strconv.ParseInt(val[0], 10, 64)
		ts = ts / 1000000 // milliseconds since epoch

		docs = append(docs, `{"index":{}}`)

		doc := fmt.Sprintf(
			`{"@timestamp": %d, "host": %s, "module": %s, "type": %s, "log": %s}`,
			ts, hostname, name, typ, val[1],
		)

		docs = append(docs, doc)
	}

	body := strings.Join(docs, "\n")
	return []byte(body)
}

func buildLokiStream(name, typ string, values [][]string) []byte {
	stream := map[string]any{
		"stream": map[string]any{
			"host":   hostname,
			"module": name,
			"type":   typ,
		},
		"values": values,
	}

	streams := map[string]any{
		"streams": []any{stream},
	}

	body, _ := json.Marshal(streams)
	return body
}
