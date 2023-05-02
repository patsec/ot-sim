package main

import (
	"context"
	"errors"
	"fmt"
	"os"

	otsim "github.com/patsec/ot-sim"
	"github.com/patsec/ot-sim/util"
	"github.com/patsec/ot-sim/util/sigterm"

	// This will cause the CPU module to register itself with the otsim package so
	// it gets run by the otsim.Start function below.
	_ "github.com/patsec/ot-sim/cpu"
)

func main() {
	if len(os.Args) != 2 {
		panic("path to config file not provided")
	}

	if err := otsim.ParseConfigFile(os.Args[1]); err != nil {
		fmt.Printf("Error parsing config file: %v\n", err)
		os.Exit(util.ExitNoRestart)
	}

	ctx := sigterm.CancelContext(context.Background())
	ctx = util.SetConfigFile(ctx, os.Args[1])

	if err := otsim.Start(ctx); err != nil {
		fmt.Printf("Error starting CPU module: %v\n", err)

		var exitErr util.ExitError
		if errors.As(err, &exitErr) {
			os.Exit(exitErr.ExitCode)
		}

		os.Exit(1)
	}

	<-ctx.Done()
	otsim.Waiter.Wait() // wait for all started modules to stop

	if err := ctx.Err(); err != nil && !errors.Is(err, context.Canceled) {
		fmt.Printf("Error running CPU module: %v\n", err)

		var exitErr util.ExitError
		if errors.As(err, &exitErr) {
			os.Exit(exitErr.ExitCode)
		}

		os.Exit(1)
	}
}
