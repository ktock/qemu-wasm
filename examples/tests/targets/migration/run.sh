#!/bin/bash

set -xeu -o pipefail

expect -c '
spawn python3 /run.py

set timeout 5
while {1} {
  send "\r"
  expect {
      "#" {
        break
      }
      eof {
          puts "Process exited unexpectedly. Exiting..."
          exit 1
      }
      timeout {
          puts "Waiting for prompt..."
      }
  }
}

send "cat /tmp/file\r"
expect {
    "test" {
        # Continue execution normally
    }
    timeout {
        puts "Timeout occurred. Exiting..."
        close
        wait
        exit 1
    }
}

send \x03
expect eof
close
wait
'
