#!/bin/bash

set -xeu -o pipefail

expect -c '
set timeout 2000
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

set timeout 240
send "nproc\r"
expect {
    "4" {
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
