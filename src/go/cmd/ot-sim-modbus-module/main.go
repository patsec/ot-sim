package main

import (
	"context"
	"fmt"
	"os"

	otsim "github.com/patsec/ot-sim"
	"github.com/patsec/ot-sim/util/sigterm"

	// This will cause the Modbus module to register itself with the otsim package
	// so it gets run by the otsim.Start function below.
	_ "github.com/patsec/ot-sim/modbus"
)

func main() {
	if len(os.Args) != 2 {
		panic("path to config file not provided")
	}

	if err := otsim.ParseConfigFile(os.Args[1]); err != nil {
		fmt.Printf("Error parsing config file: %v\n", err)
		os.Exit(1)
	}

	ctx := sigterm.CancelContext(context.Background())

	if err := otsim.Start(ctx); err != nil {
		fmt.Printf("Error starting Modbus module: %v\n", err)
		os.Exit(1)
	}

	<-ctx.Done()

	if err := ctx.Err(); err != nil && err != context.Canceled {
		fmt.Printf("Error running Modbus module: %v\n", err)
		os.Exit(1)
	}
}
