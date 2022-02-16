package otsim

import (
	"context"
	"fmt"
	"sync"

	"github.com/beevik/etree"
)

type ModuleFactory interface {
	NewModule(*etree.Element) (Module, error)
}

type Module interface {
	Name() string
	Configure(*etree.Element) error
}

type Runner interface {
	Name() string
	Run(context.Context, string, string) error
}

var Waiter sync.WaitGroup

var (
	factories = make(map[string]ModuleFactory)
	runners   []Runner

	pubEndpoint  string
	pullEndpoint string
)

func AddModuleFactory(tag string, factory ModuleFactory) {
	factories[tag] = factory
}

func ParseConfigFile(path string) error {
	doc := etree.NewDocument()

	if err := doc.ReadFromFile(path); err != nil {
		return fmt.Errorf("reading XML config file %s: %w", path, err)
	}

	root := doc.SelectElement("ot-sim")

	if root == nil {
		return fmt.Errorf("root element of XML config file must be 'ot-sim'")
	}

	if bus := root.FindElement("message-bus"); bus != nil {
		for _, child := range bus.ChildElements() {
			switch child.Tag {
			case "pub-endpoint":
				pubEndpoint = child.Text()
			case "pull-endpoint":
				pullEndpoint = child.Text()
			}
		}
	}

	for _, child := range root.ChildElements() {
		if factory, ok := factories[child.Tag]; ok {
			module, err := factory.NewModule(child)
			if err != nil {
				return fmt.Errorf("creating new module for %s: %w", child.Tag, err)
			}

			if err := module.Configure(child); err != nil {
				return fmt.Errorf("running configure for module %s for %s: %w", module.Name(), child.Tag, err)
			}

			if runner, ok := module.(Runner); ok {
				runners = append(runners, runner)
			}
		}
	}

	return nil
}

func Start(ctx context.Context) error {
	if len(runners) == 0 {
		return fmt.Errorf("no registered runners")
	}

	for _, runner := range runners {
		if err := runner.Run(ctx, pubEndpoint, pullEndpoint); err != nil {
			return fmt.Errorf("starting %s: %w", runner.Name(), err)
		}
	}

	return nil
}
