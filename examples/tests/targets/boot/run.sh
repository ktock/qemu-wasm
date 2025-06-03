#!/bin/bash

set -xeu -o pipefail

expect -c '
set timeout 180
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
send "\r"

set timeout 5
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
