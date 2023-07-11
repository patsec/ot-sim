package logic

import (
	"strings"
	"testing"

	"github.com/antonmedv/expr"
)

var testLogic = `
active = filter(variables, {# matches "_active_power$"})
active_sum = sum(active)
active_avg = avg(active)
`

var variables = map[string]float64{
	"foo_active_power":   5.0,
	"foo_reactive_power": 0.0,
	"bar_active_power":   3.2,
	"bar_reactive_power": 0.0,
}

func TestLogic(t *testing.T) {
	l := New("test")

	lines := strings.Split(testLogic, "\n")

	l.order = make([]string, len(lines))

	for i, line := range lines {
		if line == "" {
			continue
		}

		sides := strings.SplitN(line, "=", 2)

		var (
			left  = strings.TrimSpace(sides[0])
			right = strings.TrimSpace(sides[1])
		)

		code, err := expr.Compile(right)
		if err != nil {
			t.Logf("compiling program code '%s': %v", right, err)
			t.FailNow()
		}

		l.program[i] = code
		l.order[i] = left

		if _, ok := l.env[left]; !ok {
			// Initialize variable in environment used by program, but only if
			// it wasn't already initialized by a variable definition.
			l.env[left] = 0.0
		}
	}

	for k, v := range variables {
		l.variables = append(l.variables, k)
		l.env[k] = v
	}

	l.initEnv()
	l.execute()

	val, ok := l.env["active"]
	if !ok {
		t.FailNow()
	}

	active, ok := val.([]any)
	if !ok {
		t.FailNow()
	}

	activeExpected := []any{"foo_active_power", "bar_active_power"}

	for i, e := range active {
		if e != activeExpected[i] {
			t.FailNow()
		}
	}

	val, ok = l.env["active_sum"]
	if !ok {
		t.FailNow()
	}

	sum, ok := val.(float64)
	if !ok {
		t.FailNow()
	}

	if sum != 8.2 {
		t.FailNow()
	}

	val, ok = l.env["active_avg"]
	if !ok {
		t.FailNow()
	}

	avg, ok := val.(float64)
	if !ok {
		t.FailNow()
	}

	if avg != 4.1 {
		t.FailNow()
	}
}
