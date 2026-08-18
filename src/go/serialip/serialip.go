// Package serialip exposes a local PTY through a raw TCP serial service.
package serialip

import (
	"context"
	"errors"
	"fmt"
	"io"
	"net"
	"os"
	"path/filepath"
	"strings"
	"sync"
	"syscall"
	"time"

	otsim "github.com/patsec/ot-sim"

	"github.com/beevik/etree"
	"github.com/creack/pty"
	"golang.org/x/sys/unix"
	"golang.org/x/term"
)

const (
	defaultReconnectDelay = time.Second
	bufferSize            = 4096
	bufferCount           = 16
)

type Factory struct{}

func init() {
	otsim.AddModuleFactory("serial-ip", new(Factory))
}

func (Factory) NewModule(e *etree.Element) (otsim.Module, error) {
	return New(e.SelectAttrValue("name", "serial-ip")), nil
}

type SerialIP struct {
	name           string
	endpoint       string
	device         string
	reconnectDelay time.Duration
}

type ptyMessage struct {
	data []byte
	ctx  context.Context
}

func New(name string) *SerialIP {
	return &SerialIP{
		name:           name,
		reconnectDelay: defaultReconnectDelay,
	}
}

func (s SerialIP) Name() string {
	return s.name
}

func (s *SerialIP) Configure(e *etree.Element) error {
	for _, child := range e.ChildElements() {
		switch child.Tag {
		case "endpoint":
			s.endpoint = child.Text()
		case "device":
			s.device = child.Text()
		case "reconnect-delay":
			var err error

			s.reconnectDelay, err = time.ParseDuration(child.Text())
			if err != nil {
				return fmt.Errorf("invalid reconnect delay %q: %w", child.Text(), err)
			}
		}
	}

	if s.endpoint == "" {
		return errors.New("endpoint is required")
	}

	if _, _, err := net.SplitHostPort(s.endpoint); err != nil {
		return fmt.Errorf("invalid endpoint %q: %w", s.endpoint, err)
	}

	if s.device == "" {
		return errors.New("device is required")
	}

	if !filepath.IsAbs(s.device) {
		return fmt.Errorf("device must be absolute: %q", s.device)
	}

	if s.reconnectDelay <= 0 {
		return errors.New("reconnect-delay must be greater than zero")
	}

	return nil
}

func (s *SerialIP) Run(ctx context.Context, _, _ string) error {
	ptmx, tty, err := pty.Open()
	if err != nil {
		return fmt.Errorf("creating PTY: %w", err)
	}

	defer ptmx.Close()
	defer tty.Close()

	if _, err := term.MakeRaw(int(tty.Fd())); err != nil {
		return fmt.Errorf("setting PTY to raw mode: %w", err)
	}

	if err := unix.SetNonblock(int(ptmx.Fd()), true); err != nil {
		return fmt.Errorf("setting PTY to nonblocking mode: %w", err)
	}

	if err := s.createLink(tty.Name()); err != nil {
		return err
	}

	defer os.Remove(s.device)

	s.log("created PTY %s at %s", tty.Name(), s.device)

	var (
		outbound = make(chan []byte, bufferCount)
		inbound  = make(chan ptyMessage, bufferCount)
		workers  sync.WaitGroup
	)

	workers.Add(2)

	go func() {
		defer workers.Done()
		s.readPTY(ctx, ptmx, outbound)
	}()

	go func() {
		defer workers.Done()
		s.writePTY(ctx, ptmx, inbound)
	}()

	s.connectLoop(ctx, ptmx, outbound, inbound)
	workers.Wait()

	return nil
}

func (s *SerialIP) createLink(target string) error {
	if err := os.MkdirAll(filepath.Dir(s.device), 0755); err != nil {
		return fmt.Errorf("creating device directory: %w", err)
	}

	if info, err := os.Lstat(s.device); err == nil {
		if info.Mode()&os.ModeSymlink == 0 {
			return fmt.Errorf("device already exists and is not a symlink: %s", s.device)
		}

		existingTarget, err := os.Readlink(s.device)
		if err != nil {
			return fmt.Errorf("reading existing device link: %w", err)
		}

		if !strings.HasPrefix(existingTarget, "/dev/pts/") {
			return fmt.Errorf("device link points outside /dev/pts: %s", s.device)
		}

		if err := os.Remove(s.device); err != nil {
			return fmt.Errorf("removing stale device link: %w", err)
		}
	} else if !errors.Is(err, os.ErrNotExist) {
		return fmt.Errorf("checking device path: %w", err)
	}

	if err := os.Symlink(target, s.device); err != nil {
		return fmt.Errorf("creating PTY link: %w", err)
	}

	return nil
}

func (s *SerialIP) connectLoop(ctx context.Context, ptmx *os.File, outbound <-chan []byte, inbound chan<- ptyMessage) {
	dialer := net.Dialer{KeepAlive: 30 * time.Second}

	for ctx.Err() == nil {
		conn, err := dialer.DialContext(ctx, "tcp", s.endpoint)
		if err != nil {
			s.log("[ERROR] connecting to %s: %v", s.endpoint, err)
			s.wait(ctx)
			continue
		}

		if tcpConn, ok := conn.(*net.TCPConn); ok {
			_ = tcpConn.SetNoDelay(true)
		}

		s.log("connected to %s", s.endpoint)
		s.runConnection(ctx, ptmx, conn, outbound, inbound)

		_ = conn.Close()

		s.log("disconnected from %s", s.endpoint)
		s.wait(ctx)
	}
}

func (s *SerialIP) runConnection(parent context.Context, ptmx *os.File, conn net.Conn, outbound <-chan []byte, inbound chan<- ptyMessage) {
	ctx, cancel := context.WithCancel(parent)
	defer cancel()

	done := make(chan struct{}, 2)

	go func() {
		defer func() { done <- struct{}{} }()

		buf := make([]byte, bufferSize)

		for {
			n, err := conn.Read(buf)

			if n > 0 {
				data := append([]byte(nil), buf[:n]...)

				select {
				case inbound <- ptyMessage{data: data, ctx: ctx}:
				case <-ctx.Done():
					return
				}
			}

			if err != nil {
				return
			}
		}
	}()

	go func() {
		defer func() { done <- struct{}{} }()

		for {
			select {
			case data := <-outbound:
				if err := writeAll(conn, data); err != nil {
					return
				}
			case <-ctx.Done():
				return
			}
		}
	}()

	<-done

	_ = conn.Close()
	cancel()

	<-done
}

func (s *SerialIP) readPTY(ctx context.Context, ptmx *os.File, outbound chan<- []byte) {
	buf := make([]byte, bufferSize)

	for ctx.Err() == nil {
		ready, err := pollPTY(ctx, int(ptmx.Fd()), unix.POLLIN)
		if err != nil {
			s.log("[ERROR] polling PTY: %v", err)
			return
		}

		if !ready {
			return
		}

		n, err := unix.Read(int(ptmx.Fd()), buf)

		if n > 0 {
			data := append([]byte(nil), buf[:n]...)

			select {
			case outbound <- data:
			case <-ctx.Done():
				return
			}
		}

		if err != nil {
			if errors.Is(err, syscall.EAGAIN) {
				continue
			}

			if errors.Is(err, syscall.EIO) {
				s.wait(ctx)
				continue
			}

			if ctx.Err() == nil {
				s.log("[ERROR] reading PTY: %v", err)
			}

			return
		}
	}
}

func (s *SerialIP) writePTY(ctx context.Context, ptmx *os.File, inbound <-chan ptyMessage) {
	for {
		select {
		case message := <-inbound:
			for len(message.data) > 0 && message.ctx.Err() == nil {
				ready, err := pollPTY(ctx, int(ptmx.Fd()), unix.POLLOUT)
				if err != nil {
					s.log("[ERROR] polling PTY: %v", err)
					return
				}

				if !ready {
					return
				}

				if message.ctx.Err() != nil {
					break
				}

				n, err := unix.Write(int(ptmx.Fd()), message.data)
				if err != nil {
					if errors.Is(err, syscall.EAGAIN) {
						continue
					}

					if errors.Is(err, syscall.EIO) {
						s.wait(ctx)
						continue
					}

					if ctx.Err() == nil {
						s.log("[ERROR] writing PTY: %v", err)
					}

					break
				}

				message.data = message.data[n:]
			}
		case <-ctx.Done():
			return
		}
	}
}

func pollPTY(ctx context.Context, fd int, events int16) (bool, error) {
	for ctx.Err() == nil {
		fds := []unix.PollFd{{Fd: int32(fd), Events: events}}

		n, err := unix.Poll(fds, 100)
		if err != nil {
			if errors.Is(err, syscall.EINTR) {
				continue
			}

			return false, err
		}

		if n > 0 {
			return true, nil
		}
	}

	return false, nil
}

func (s *SerialIP) wait(ctx context.Context) {
	timer := time.NewTimer(s.reconnectDelay)
	defer timer.Stop()

	select {
	case <-timer.C:
	case <-ctx.Done():
	}
}

func writeAll(w io.Writer, data []byte) error {
	for len(data) > 0 {
		n, err := w.Write(data)
		if err != nil {
			return err
		}

		if n == 0 {
			return io.ErrShortWrite
		}

		data = data[n:]
	}

	return nil
}

func (s SerialIP) log(format string, a ...any) {
	fmt.Printf("[%s] %s\n", s.name, fmt.Sprintf(format, a...))
}
