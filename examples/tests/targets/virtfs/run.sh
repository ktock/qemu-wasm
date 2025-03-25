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

send "mount -t 9p -o trans=virtio share0 /mnt/ -oversion=9p2000.L\r"

set timeout 5
send "cat /mnt/file\r"
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
