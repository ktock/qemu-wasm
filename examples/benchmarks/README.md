Prepare pigz image

```
mkdir /tmp/testbench
./run.sh /tmp/testbench/
docker run --rm -d -p 127.0.0.1:8111:80 \
                           -v "/tmp/testbench/htdocs:/usr/local/apache2/htdocs/:ro" \
                           -v "/tmp/testbench/xterm-pty.conf:/usr/local/apache2/conf/extra/xterm-pty.conf:ro" \
                           --entrypoint=/bin/sh httpd -c 'echo "Include conf/extra/xterm-pty.conf" >> /usr/local/apache2/conf/httpd.conf && httpd-foreground'
```
