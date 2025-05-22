#!/bin/bash

set -xeu -o pipefail

expect -c '
set timeout 1000
spawn python3 /run.py
expect {
    "Please press Enter to activate this console." {
        # Continue execution normally
    }
    eof {
        puts "Process exited unexpectedly. Exiting..."
        exit 1
    }
    timeout {
        puts "Boot timeout. Exiting..."
        close
        wait
        exit 1
    }
}

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

set timeout 30
send "uname -a\r"
expect {
    "Linux" {
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
