package logic

import (
	"context"
	"errors"
	"fmt"
	"math/rand"
	"strconv"
	"strings"
	"sync"
	"time"

	otsim "github.com/patsec/ot-sim"
	"github.com/patsec/ot-sim/msgbus"

	"github.com/antonmedv/expr"
	"github.com/antonmedv/expr/vm"
	"github.com/beevik/etree"
)

type Factory struct{}

func (Factory) NewModule(e *etree.Element) (otsim.Module, error) {
	name := e.SelectAttrValue("name", "logic")
	return New(name), nil
}

type Logic struct {
	pullEndpoint string
	pubEndpoint  string

	name   string
	period time.Duration

	program map[int]*vm.Program
	order   []string

	env map[string]interface{}

	tagToVar map[string]string
	varToTag map[string]string

	processUpdates bool

	envMu sync.Mutex
}

func New(name string) *Logic {
	return &Logic{
		name:     name,
		period:   1 * time.Second,
		program:  make(map[int]*vm.Program),
		env:      make(map[string]interface{}),
		tagToVar: make(map[string]string),
		varToTag: make(map[string]string),
	}
}

func (this Logic) Name() string {
	return this.name
}

func (this *Logic) Configure(e *etree.Element) error {
	for _, child := range e.ChildElements() {
		switch child.Tag {
		case "pull-endpoint":
			this.pullEndpoint = child.Text()
		case "pub-endpoint":
			this.pubEndpoint = child.Text()
		case "period":
			var err error
			if this.period, err = time.ParseDuration(child.Text()); err != nil {
				return fmt.Errorf("invalid period duration '%s': %w", child.Text(), err)
			}
		case "program":
			lines := strings.Split(child.Text(), "\n")

			this.order = make([]string, len(lines))

			for i, line := range lines {
				if strings.HasPrefix(line, "#") {
					continue
				}

				sides := strings.SplitN(line, "=", 2)

				switch len(sides) {
				case 1:
					right := strings.TrimSpace(sides[0])

					if strings.HasPrefix(right, "sprintf") {
						left := fmt.Sprintf("sprintf%d", i)

						p, err := expr.Compile(right)
						if err != nil {
							return fmt.Errorf("compiling program code '%s': %w", right, err)
						}

						this.program[i] = p
						this.order[i] = left
					}
				case 2:
					var (
						left  = strings.TrimSpace(sides[0])
						right = strings.TrimSpace(sides[1])
					)

					code, err := expr.Compile(right)
					if err != nil {
						return fmt.Errorf("compiling program code '%s': %w", right, err)
					}

					this.program[i] = code
					this.order[i] = left

					if _, ok := this.env[left]; !ok {
						// Initialize variable in environment used by program, but only if
						// it wasn't already initialized by a variable definition.
						this.env[left] = 0.0
					}
				}
			}
		case "variables":
			for _, v := range child.ChildElements() {
				tag := v.SelectAttrValue("tag", v.Tag)

				this.varToTag[v.Tag] = tag
				this.tagToVar[tag] = v.Tag

				val, err := strconv.ParseFloat(v.Text(), 64)
				if err != nil {
					val, err := strconv.ParseBool(v.Text())
					if err != nil {
						return fmt.Errorf("unable to convert value for '%s' to bool or double: %w", v.Tag, err)
					}

					this.env[v.Tag] = val
					continue
				}

				this.env[v.Tag] = val
			}
		case "process-updates":
			val, err := strconv.ParseBool(child.Text())
			if err != nil {
				return fmt.Errorf("unable to convert value for '%s' to bool: %w", child.Tag, err)
			}

			this.processUpdates = val
		}
	}

	return nil
}

func (this *Logic) Run(ctx context.Context, pubEndpoint, pullEndpoint string) error {
	if len(this.program) == 0 {
		return fmt.Errorf("no logic program to execute")
	}

	// Use ZeroMQ PUB endpoint specified in `logic` config block if provided.
	if this.pubEndpoint != "" {
		pubEndpoint = this.pubEndpoint
	}

	// Use ZeroMQ PULL endpoint specified in `logic` config block if provided.
	if this.pullEndpoint != "" {
		pullEndpoint = this.pullEndpoint
	}

	var (
		pusher     = msgbus.MustNewPusher(pullEndpoint)
		subscriber = msgbus.MustNewSubscriber(pubEndpoint)
	)

	this.initEnv()

	if this.processUpdates {
		subscriber.AddUpdateHandler(this.handleMsgBusUpdate)
	}

	subscriber.AddStatusHandler(this.handleMsgBusStatus)
	subscriber.Start("RUNTIME")

	go func() {
		ticker := time.NewTicker(this.period)

		for {
			select {
			case <-ctx.Done():
				subscriber.Stop()
				ticker.Stop()

				return
			case <-ticker.C:
				this.envMu.Lock()

				var updated []string

				for i, o := range this.order {
					if o == "" {
						continue
					}

					result, err := expr.Run(this.program[i], this.env)
					if err != nil {
						this.log("[ERROR] running program code: %v", err)
						continue
					}

					if strings.HasPrefix(o, "sprintf") {
						this.log("PROGRAM OUTPUT (line %d): %s\n", i, result)
						continue
					}

					if result != this.env[o] {
						updated = append(updated, o)
					}

					this.env[o] = result
				}

				tags := make(map[string]msgbus.Point)

				for _, o := range this.order {
					if o == "" {
						continue
					}

					tag, ok := this.varToTag[o]
					if !ok {
						continue
					}

					point := msgbus.Point{Tag: tag}

					switch value := this.env[o].(type) {
					case bool:
						if value {
							point.Value = 1.0
						} else {
							point.Value = 0.0
						}
					case int:
						point.Value = float64(value)
					case float64:
						point.Value = value
					}

					tags[tag] = point
				}

				if len(tags) > 0 {
					var points []msgbus.Point

					for t := range tags {
						points = append(points, tags[t])
					}

					env, err := msgbus.NewStatusEnvelope(this.name, msgbus.Status{Measurements: points})
					if err != nil {
						this.log("[ERROR] creating new status message: %v", err)
						continue
					}

					if err := pusher.Push("RUNTIME", env); err != nil {
						this.log("[ERROR] sending status message: %v", err)
					}
				}

				if len(updated) > 0 {
					tags := make(map[string]msgbus.Point)

					for _, u := range updated {
						tag, ok := this.varToTag[u]
						if !ok {
							continue
						}

						point := msgbus.Point{Tag: tag}

						switch value := this.env[u].(type) {
						case int:
							point.Value = float64(value)
						case float64:
							point.Value = value
						}

						tags[tag] = point
					}

					if len(tags) > 0 {
						var updates []msgbus.Point

						for t := range tags {
							updates = append(updates, tags[t])
						}

						env, err := msgbus.NewUpdateEnvelope(this.name, msgbus.Update{Updates: updates})
						if err != nil {
							this.log("[ERROR] creating new update message: %v\n", err)
							continue
						}

						if err := pusher.Push("RUNTIME", env); err != nil {
							this.log("[ERROR] sending update message: %v", err)
						}
					}
				}

				this.envMu.Unlock()
			}
		}
	}()

	return nil
}

func (this *Logic) initEnv() {
	rand.Seed(time.Now().UnixNano())

	this.env["randInt"] = func(max int) float64 {
		return float64(rand.Intn(max))
	}

	this.env["randFloat"] = func() float64 {
		return rand.Float64()
	}

	this.env["randBool"] = func(likely float64) bool {
		return rand.Float64() >= (1.0 - likely)
	}

	this.env["sprintf"] = fmt.Sprintf
}

func (this *Logic) handleMsgBusStatus(env msgbus.Envelope) {
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

	this.envMu.Lock()

	for _, point := range status.Measurements {
		if v, ok := this.tagToVar[point.Tag]; ok {
			this.log("setting tag %s to value %f\n", point.Tag, point.Value)
			this.env[v] = point.Value
		}
	}

	this.envMu.Unlock()
}

func (this *Logic) handleMsgBusUpdate(env msgbus.Envelope) {
	if env.Sender() == this.name {
		return
	}

	update, err := env.Update()
	if err != nil {
		if !errors.Is(err, msgbus.ErrKindNotUpdate) {
			this.log("[ERROR] getting Update message from envelope: %v", err)
		}

		return
	}

	this.envMu.Lock()

	for _, point := range update.Updates {
		if v, ok := this.tagToVar[point.Tag]; ok {
			this.log("setting tag %s to value %f\n", point.Tag, point.Value)
			this.env[v] = point.Value
		}
	}

	this.envMu.Unlock()
}

func (this Logic) log(format string, a ...any) {
	fmt.Printf("[%s] %s\n", this.name, fmt.Sprintf(format, a...))
}

func init() {
	otsim.AddModuleFactory("logic", new(Factory))
}
