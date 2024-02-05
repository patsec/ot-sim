package cpu

import (
	"errors"
	"fmt"
	"log"

	"github.com/patsec/ot-sim/msgbus"
)

func (this CPU) internalHandler(topic, msg string) error {
	env, err := msgbus.ParseEnvelope([]byte(msg))
	if err != nil {
		return fmt.Errorf("creating new envelope: %w", err)
	}

	switch env.Kind {
	case msgbus.ENVELOPE_MODULE_CONTROL:
		control, err := env.ModuleControl()
		if err != nil {
			if errors.Is(err, msgbus.ErrKindNotModuleControl) {
				return nil
			}

			return fmt.Errorf("getting module controls from envelope: %w", err)
		}

		if control.Recipient != "" && control.Recipient != "CPU" {
			return nil
		}

		var (
			results = make(map[string]any)
			errs    = make(msgbus.ConfirmationErrors)
		)

		if control.List {
			for _, mod := range modules {
				if mod.canceler == nil {
					results[mod.name] = "disabled"
				} else {
					results[mod.name] = "enabled"
				}
			}
		}

		for _, name := range control.Enable {
			if mod, ok := modules[name]; ok {
				if mod.canceler == nil {
					if err := StartModule(mod.ctx, mod); err == nil {
						results[mod.name] = "enabled"
					} else {
						log.Printf("[CPU] [ERROR] failed to enable module %s: %v\n", name, err)

						errs[mod.name] = err.Error()
					}
				} else {
					errs[mod.name] = "already enabled"
				}
			} else {
				errs[name] = "does not exist"
			}
		}

		for _, name := range control.Disable {
			if mod, ok := modules[name]; ok {
				if mod.canceler != nil {
					close(mod.canceler)

					results[mod.name] = "disabled"
				} else {
					errs[mod.name] = "already disabled"
				}
			} else {
				errs[name] = "does not exist"
			}
		}

		if control.Confirm != "" {
			confirm := msgbus.Confirmation{Confirm: control.Confirm, Results: results, Errors: errs}

			env, err := msgbus.NewEnvelope("CPU", confirm)
			if err != nil {
				return fmt.Errorf("creating new confirmation envelope: %w", err)
			}

			if err := this.pusher.Push("INTERNAL", env); err != nil {
				log.Printf("[CPU] [ERROR] sending module control confirmation message: %v", err)

				return err
			}
		}
	}

	return nil
}
