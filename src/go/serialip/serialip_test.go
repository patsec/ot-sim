package serialip

import (
	"context"
	"errors"
	"io"
	"net"
	"os"
	"path/filepath"
	"testing"
	"time"

	"github.com/beevik/etree"
)

func TestConfigure(t *testing.T) {
	doc := etree.NewDocument()
	if err := doc.ReadFromString(`<serial-ip><endpoint>127.0.0.1:4001</endpoint><device>/run/ot-sim/digi</device><reconnect-delay>2s</reconnect-delay></serial-ip>`); err != nil {
		t.Fatal(err)
	}

	serialIP := New("test")
	if err := serialIP.Configure(doc.Root()); err != nil {
		t.Fatal(err)
	}
	if serialIP.endpoint != "127.0.0.1:4001" || serialIP.device != "/run/ot-sim/digi" || serialIP.reconnectDelay != 2*time.Second {
		t.Fatal("serial-IP configuration was not applied")
	}
}

func TestConfigureRejectsInvalidLinkPath(t *testing.T) {
	doc := etree.NewDocument()
	if err := doc.ReadFromString(`<serial-ip><endpoint>127.0.0.1:4001</endpoint><device>relative</device></serial-ip>`); err != nil {
		t.Fatal(err)
	}

	if err := New("test").Configure(doc.Root()); err == nil {
		t.Fatal("expected relative device path to be rejected")
	}
}

func TestBridgeForwardsBytesAndCleansLink(t *testing.T) {
	listener, err := net.Listen("tcp", "127.0.0.1:0")
	if err != nil {
		t.Fatal(err)
	}
	defer listener.Close()

	connection := make(chan net.Conn, 2)
	go func() {
		for {
			conn, err := listener.Accept()
			if err != nil {
				return
			}
			connection <- conn
		}
	}()

	devicePath := filepath.Join(t.TempDir(), "digi-serial")
	serialIP := New("test")
	serialIP.endpoint = listener.Addr().String()
	serialIP.device = devicePath
	serialIP.reconnectDelay = 10 * time.Millisecond

	ctx, cancel := context.WithCancel(context.Background())
	run := make(chan error, 1)
	go func() {
		run <- serialIP.Run(ctx, "", "")
	}()

	waitFor(t, time.Second, func() bool {
		_, err := os.Lstat(devicePath)
		return err == nil
	})
	target, err := os.Readlink(devicePath)
	if err != nil {
		t.Fatal(err)
	}

	conn := waitConnection(t, connection)
	defer conn.Close()

	tty, err := os.OpenFile(devicePath, os.O_RDWR, 0)
	if err != nil {
		t.Fatal(err)
	}
	defer tty.Close()

	fromTTY := []byte{0x00, 0x03, 0x7f, 0x80, 0xff}
	if _, err := tty.Write(fromTTY); err != nil {
		t.Fatal(err)
	}
	got := make([]byte, len(fromTTY))
	readFull(t, conn, got)
	if string(got) != string(fromTTY) {
		t.Fatalf("TCP received %x, want %x", got, fromTTY)
	}

	fromTCP := []byte{0xff, 0x00, 0x01, 0xfe}
	if _, err := conn.Write(fromTCP); err != nil {
		t.Fatal(err)
	}
	got = make([]byte, len(fromTCP))
	readFull(t, tty, got)
	if string(got) != string(fromTCP) {
		t.Fatalf("PTY received %x, want %x", got, fromTCP)
	}

	if err := conn.Close(); err != nil {
		t.Fatal(err)
	}
	conn = waitConnection(t, connection)
	defer conn.Close()
	if currentTarget, err := os.Readlink(devicePath); err != nil || currentTarget != target {
		t.Fatalf("PTY link changed after reconnect: target=%q err=%v", currentTarget, err)
	}

	if _, err := conn.Write([]byte{0x42}); err != nil {
		t.Fatal(err)
	}
	readFull(t, tty, got[:1])
	if got[0] != 0x42 {
		t.Fatalf("PTY received %x after reconnect, want 42", got[0])
	}

	cancel()
	select {
	case err := <-run:
		if err != nil {
			t.Fatal(err)
		}
	case <-time.After(time.Second):
		t.Fatal("serial-IP module did not stop")
	}

	if _, err := os.Lstat(devicePath); !errors.Is(err, os.ErrNotExist) {
		t.Fatalf("PTY link still exists after shutdown: %v", err)
	}
}

func waitConnection(t *testing.T, connections <-chan net.Conn) net.Conn {
	t.Helper()
	select {
	case conn := <-connections:
		return conn
	case <-time.After(time.Second):
		t.Fatal("serial-IP module did not connect")
		return nil
	}
}

func readFull(t *testing.T, r io.Reader, data []byte) {
	t.Helper()
	done := make(chan error, 1)
	go func() {
		_, err := io.ReadFull(r, data)
		done <- err
	}()
	select {
	case err := <-done:
		if err != nil {
			t.Fatal(err)
		}
	case <-time.After(time.Second):
		t.Fatal("timed out reading bridge data")
	}
}

func waitFor(t *testing.T, timeout time.Duration, ready func() bool) {
	t.Helper()
	deadline := time.Now().Add(timeout)
	for time.Now().Before(deadline) {
		if ready() {
			return
		}
		time.Sleep(10 * time.Millisecond)
	}
	t.Fatal("condition was not met before timeout")
}
