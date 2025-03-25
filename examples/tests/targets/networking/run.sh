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

send "mount -t 9p -o trans=virtio wasm0 /mnt -oversion=9p2000.L\r"
send "export SSL_CERT_FILE=/mnt/proxy.crt\r"
send "export https_proxy=http://192.168.127.253:80\r"
send "export http_proxy=http://192.168.127.253:80\r"
send "export HTTPS_PROXY=http://192.168.127.253:80\r"
send "export HTTP_PROXY=http://192.168.127.253:80\r"

set timeout 30
send "wget -S -O - https://ktock.github.io/container2wasm-demo/\r"
expect {
    "HTTP/1.1 200 OK" {
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

# TODO: Add c2w-net test once it supports WSS.
# wget -O /tmp/out.tar.gz https://github.com/container2wasm/container2wasm/releases/download/v0.8.0/container2wasm-v0.8.0-linux-amd64.tar.gz
# tar -C /usr/local/bin/ -zxvf /tmp/out.tar.gz

# c2w-net --debug --listen-ws 0.0.0.0:9999 &
# sleep 3

# expect -c '
# set timeout 1000
# spawn python3 /run.py ?net=delegate=ws://runner:9999
# expect "Please press Enter to activate this console."
# send "\r"

# set timeout 30
# send "wget -S -O - https://ktock.github.io/container2wasm-demo/\r"
# expect {
#     "HTTP/1.1 200 OK" {
#         # Continue execution normally
#     }
#     timeout {
#         puts "Timeout occurred. Exiting..."
#         close
#         wait
#         exit 1
#     }
# }

# send \x03
# expect eof
# close
# wait
# '
