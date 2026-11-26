package cpu

import (
	"testing"
	"time"
)

func TestRestartDelay(t *testing.T) {
	// A module that keeps exiting at once backs off up to the maximum.
	var (
		delay time.Duration
		want  = []time.Duration{1, 2, 4, 8, 16, 32, 60, 60}
	)

	for i, w := range want {
		delay = restartDelay(delay, 100*time.Millisecond)

		if delay != w*time.Second {
			t.Fatalf("restart %d: got delay %v, want %v", i+1, delay, w*time.Second)
		}
	}

	// A module that stayed up for a while restarts at once and starts over.
	if delay = restartDelay(delay, RESTART_DELAY_MAX); delay != 0 {
		t.Fatalf("after long uptime: got delay %v, want 0", delay)
	}

	if delay = restartDelay(delay, time.Second); delay != RESTART_DELAY_MIN {
		t.Fatalf("after reset: got delay %v, want %v", delay, RESTART_DELAY_MIN)
	}
}
