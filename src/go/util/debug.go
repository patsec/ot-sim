package util

import (
	"os"
	"strconv"
)

// Debug is true when OTSIM_DEBUG is set. Modules log every value they handle
// only then, since a device with thousands of tags cannot keep up otherwise.
var Debug, _ = strconv.ParseBool(os.Getenv("OTSIM_DEBUG"))
