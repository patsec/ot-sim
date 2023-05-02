package util

import "fmt"

const (
	ExitNoRestart int = 101
)

type ExitError struct {
	ExitCode int
	errorMsg string
}

func NewExitError(code int, format string, a ...any) ExitError {
	return ExitError{
		ExitCode: code,
		errorMsg: fmt.Sprintf(format, a...),
	}
}

func (this ExitError) Error() string {
	return this.errorMsg
}
