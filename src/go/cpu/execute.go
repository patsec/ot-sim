package cpu

import (
	"bufio"
	"context"
	"fmt"
	"os"
	"os/exec"
	"syscall"
	"time"

	otsim "github.com/patsec/ot-sim"
)

// StartModule starts the process for another module and monitors it to make
// sure it doesn't die, restarting it if it does.
func StartModule(ctx context.Context, name, path string, args ...string) error {
	exePath, err := exec.LookPath(path)
	if err != nil {
		return fmt.Errorf("module executable does not exist at %s", path)
	}

	otsim.Waiter.Add(1)

	go func() {
		defer otsim.Waiter.Done()

		for {
			// Not using `exec.CommandContext` here since we're catching the context
			// being canceled below in order to gracefully terminate the child
			// process. Using `exec.CommandContext` forcefully kills the child process
			// when the context is canceled.
			cmd := exec.Command(exePath, args...)
			cmd.Env = os.Environ()

			stdout, _ := cmd.StdoutPipe()
			stderr, _ := cmd.StderrPipe()

			fmt.Printf("[CPU] starting %s module\n", name)

			if err := cmd.Start(); err != nil {
				fmt.Printf("[CPU] [ERROR] starting %s module: %v\n", name, err)
				return
			}

			go func() {
				scanner := bufio.NewScanner(stdout)
				scanner.Split(bufio.ScanLines)

				for scanner.Scan() {
					fmt.Printf("[LOG] %s\n", scanner.Text())
				}
			}()

			go func() {
				scanner := bufio.NewScanner(stderr)
				scanner.Split(bufio.ScanLines)

				for scanner.Scan() {
					fmt.Printf("[LOG] [ERROR] %s\n", scanner.Text())
				}
			}()

			wait := make(chan error)

			go func() {
				err := cmd.Wait()
				wait <- err
			}()

			select {
			case err := <-wait:
				fmt.Printf("[CPU] [ERROR] %s module died (%v)... restarting\n", name, err)
				continue
			case <-ctx.Done():
				fmt.Printf("[CPU] stopping %s module\n", name)
				cmd.Process.Signal(syscall.SIGTERM)

				select {
				case <-wait: // SIGTERM *should* cause cmd to exit
					fmt.Printf("[CPU] %s module has stopped\n", name)
					return
				case <-time.After(10 * time.Second):
					fmt.Printf("[CPU] forcefully killing %s module\n", name)
					cmd.Process.Kill()

					return
				}
			}
		}
	}()

	return nil
}
