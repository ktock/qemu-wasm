package main

import (
	"encoding/json"
	"flag"
	"fmt"
	"io"
	"log"
	"os"
	"os/exec"
	"strings"
)

func main() {
	var argsJSON = flag.String("args-json", "", "path to json file containing args")

	flag.Parse()
	args := flag.Args()

	if *argsJSON == "" {
		log.Fatalf("specify args JSON")
	}

	var extraArgs []string
	argsData, err := os.ReadFile(*argsJSON)
	if err != nil {
		log.Fatalf("failed to get args json: %v", err)
	}
	if err := json.Unmarshal(argsData, &extraArgs); err != nil {
		log.Fatalf("failed to parse args json: %v", err)
	}
	log.Println(extraArgs)

	cmd := exec.Command(args[0], extraArgs...)

	stdin, err := cmd.StdinPipe()
	if err != nil {
		log.Fatal(err)
	}
	stdout, err := cmd.StdoutPipe()
	if err != nil {
		log.Fatal(err)
	}

	cmd.Stderr = os.Stderr

	if err := cmd.Start(); err != nil {
		log.Fatalf("failed to start: %v", err)
	}

	doneCh := make(chan struct{})
	go func() {
		waitForString(stdout, os.Stdout, "login: ")

		_, err := io.WriteString(stdin, "root\n\n") // login as root
		if err != nil {
			log.Fatalf("failed to login: %v", err)
		}

		waitForString(stdout, os.Stdout, "localhost:~#")

		if _, err := io.WriteString(stdin, "mkdir /mnt/sdc1 && mount /dev/sdc1 /mnt/sdc1 && /bin/sh /mnt/sdc1/create-image.sh && umount /mnt/sdc1 && poweroff\n"); err != nil {
			log.Fatalf("failed to invoke migrate: %v", err)
		}

		close(doneCh)

		if _, err := io.Copy(os.Stdout, stdout); err != nil {
			log.Fatalf("failed to copy stdout: %v", err)
		}
	}()

	<-doneCh

	if err := cmd.Wait(); err != nil {
		log.Fatalf("waiting for qemu: %v", err)
	}
}

func waitForString(r io.Reader, w io.Writer, s string) error {
	p := make([]byte, 1)
	ss := &searchString{target: s}
	for {
		if _, err := r.Read(p); err != nil {
			return fmt.Errorf("failed to read stream: %v", err)
		}
		if _, err := w.Write(p); err != nil {
			return fmt.Errorf("failed to copy strem: %v", err)
		}
		ss.add(p[0])
		if ss.isMatched() {
			log.Println("detected marker")
			return nil
		}
	}
}

type searchString struct {
	target  string
	matched bool
	buf     []byte
}

func (s *searchString) add(b byte) error {
	if s.matched {
		return nil
	}

	s.buf = append(s.buf, b)
	if len(s.buf) > len(s.target) {
		s.buf = s.buf[len(s.buf)-len(s.target):]
	}
	if strings.Contains(string(s.buf), s.target) {
		s.matched = true
	}

	return nil
}

func (s *searchString) isMatched() bool {
	return s.matched
}
