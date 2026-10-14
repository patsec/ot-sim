package cpu

import (
	"strings"
	"testing"

	"github.com/beevik/etree"
)

func parseCPUConfig(t *testing.T, contents string) *etree.Element {
	t.Helper()

	doc := etree.NewDocument()
	if err := doc.ReadFromString(contents); err != nil {
		t.Fatalf("failed to parse config: %v", err)
	}

	return doc.Root()
}

func TestCPUMetricsDisabledByDefault(t *testing.T) {
	cpu := New("cpu")

	if err := cpu.Configure(parseCPUConfig(t, `<cpu></cpu>`)); err != nil {
		t.Fatalf("failed to configure cpu: %v", err)
	}

	if cpu.metricsEnabled {
		t.Fatal("expected metrics to be disabled by default")
	}

	if cpu.metricsEndpoint != "127.0.0.1:9100" {
		t.Fatalf("unexpected metrics endpoint: %s", cpu.metricsEndpoint)
	}
}

func TestCPUMetricsCanBeEnabled(t *testing.T) {
	cpu := New("cpu")

	if err := cpu.Configure(parseCPUConfig(t, `<cpu><metrics enabled="true"></metrics></cpu>`)); err != nil {
		t.Fatalf("failed to configure cpu: %v", err)
	}

	if !cpu.metricsEnabled {
		t.Fatal("expected metrics to be enabled")
	}
}

func TestCPUMetricsEndpointCanBeConfigured(t *testing.T) {
	cpu := New("cpu")

	if err := cpu.Configure(parseCPUConfig(t, `<cpu><metrics enabled="true"><endpoint>127.0.0.1:9200</endpoint></metrics></cpu>`)); err != nil {
		t.Fatalf("failed to configure cpu: %v", err)
	}

	if cpu.metricsEndpoint != "127.0.0.1:9200" {
		t.Fatalf("unexpected metrics endpoint: %s", cpu.metricsEndpoint)
	}
}

func TestCPUMetricsCanBeDisabled(t *testing.T) {
	cpu := New("cpu")

	if err := cpu.Configure(parseCPUConfig(t, `<cpu><metrics enabled="false"></metrics></cpu>`)); err != nil {
		t.Fatalf("failed to configure cpu: %v", err)
	}

	if cpu.metricsEnabled {
		t.Fatal("expected metrics to be disabled")
	}
}

func TestCPUMetricsInvalidEnabled(t *testing.T) {
	cpu := New("cpu")

	err := cpu.Configure(parseCPUConfig(t, `<cpu><metrics enabled="nope"></metrics></cpu>`))
	if err == nil {
		t.Fatal("expected configure error")
	}

	if !strings.Contains(err.Error(), "parsing 'enabled' attribute for metrics") {
		t.Fatalf("unexpected error: %v", err)
	}
}
