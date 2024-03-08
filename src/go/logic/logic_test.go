package logic

import (
	"math"
	"strings"
	"testing"

	"github.com/antonmedv/expr"
)

var testFilterLogic = `
mod = 5
count = count + 1
foo = int(count) % mod
bar = int(count) % mod
active = filter(variables, {# matches "_active_power$"})
active_sum = sum(active)
active_avg = avg(active)
`

var filterLogicVariables = map[string]float64{
	"foo_active_power":   5.0,
	"foo_reactive_power": 0.0,
	"bar_active_power":   3.2,
	"bar_reactive_power": 0.0,
}

func TestFilterLogic(t *testing.T) {
	l := New("test")

	lines := strings.Split(testFilterLogic, "\n")

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

	for k, v := range filterLogicVariables {
		l.variables = append(l.variables, k)
		l.env[k] = v
	}

	l.initEnv()
	l.execute()
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

var testMathLogic = `
power = [gen1, gen2, gen3]
total_gen = sum(power)

dir = [dir1, dir2, dir3]
filtered_dir = filter(dir, {# != 0})
wind_dir = avg(filtered_dir)

speed = [speed1, speed2, speed3]
filtered_speed = filter(speed, {# != 0})
wind_speed = avg(filtered_speed)
`

var mathLogicVariables = map[string]float64{
	"gen1":       4.5,
	"gen2":       5.4,
	"gen3":       0.0,
	"dir1":       330.6,
	"dir2":       330.7,
	"dir3":       0.0,
	"speed1":     31.4,
	"speed2":     32.3,
	"speed3":     0.0,
	"total_gen":  0.0,
	"wind_dir":   0.0,
	"wind_speed": 0.0,
}

func TestMathLogic(t *testing.T) {
	l := New("test")

	lines := strings.Split(testMathLogic, "\n")

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

	for k, v := range mathLogicVariables {
		l.variables = append(l.variables, k)
		l.env[k] = v
	}

	l.initEnv()
	l.execute()

	expected := map[string]float64{
		"total_gen":  9.9,
		"wind_dir":   330.65,
		"wind_speed": 31.85,
	}

	for k, v := range expected {
		val, ok := l.env[k]
		if !ok {
			t.FailNow()
		}

		value, ok := val.(float64)
		if !ok {
			t.FailNow()
		}

		if math.Abs(v-value) > 1e-6 {
			t.FailNow()
		}
	}
}
