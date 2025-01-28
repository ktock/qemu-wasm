# Running Alpine Linux on QEMU Wasm

This directory contains an example to run Alpine Linux inside browser.

To build QEMU Wasm, run the following at the repository root directory (they are same steps as written in [`../../README.md`](../../README.md)).

```
$ docker build -t buildqemu - < Dockerfile
$ docker run --rm -d --name build-qemu-wasm -v $(pwd):/qemu/:ro buildqemu
$ EXTRA_CFLAGS="-O3 -g -Wno-error=unused-command-line-argument -matomics -mbulk-memory -DNDEBUG -DG_DISABLE_ASSERT -D_GNU_SOURCE -sASYNCIFY=1 -pthread -sPROXY_TO_PTHREAD=1 -sFORCE_FILESYSTEM -sALLOW_TABLE_GROWTH -sTOTAL_MEMORY=2300MB -sWASM_BIGINT -sMALLOC=mimalloc --js-library=/build/node_modules/xterm-pty/emscripten-pty.js -sEXPORT_ES6=1 -sASYNCIFY_IMPORTS=ffi_call_js" ; \
  docker exec -it build-qemu-wasm emconfigure /qemu/configure --static --target-list=x86_64-softmmu --cpu=wasm32 --cross-prefix= \
    --without-default-features --enable-system --with-coroutine=fiber --enable-virtfs \
    --extra-cflags="$EXTRA_CFLAGS" --extra-cxxflags="$EXTRA_CFLAGS" --extra-ldflags="-sEXPORTED_RUNTIME_METHODS=getTempRet0,setTempRet0,addFunction,removeFunction,TTY,FS" && \
  docker exec -it build-qemu-wasm emmake make -j $(nproc) qemu-system-x86_64
```

Build kernel, initramfs, rootfs using the Dockerfile.
This example adds vim and python3 apk packages to the rootfs (`PACKAGES="vim python3"`).

> NOTE: initramfs is simplified for the demo purpose. [`./image/init.sh`](./image/init.sh) is used as the init script in initramfs.

```
$ mkdir /tmp/genimgout/
$ docker build --progress=plain --build-arg PACKAGES="vim python3" --output type=local,dest=/tmp/genimgout/ ./examples/x86_64-alpine/image/
```

Package dependencies and store them to the server root dir (`/tmp/test-js/htdocs/`).

```
$ mkdir -p /tmp/test-js/htdocs/
$ cp ./examples/networking/xterm-pty.conf /tmp/test-js/
$ mkdir -p /tmp/pack-kernel && cp /tmp/genimgout/vmlinuz-virt /tmp/pack-kernel/ && \
  mkdir -p /tmp/pack-initramfs && cp /tmp/genimgout/initramfs-virt /tmp/pack-initramfs/ && \
  mkdir -p /tmp/pack-rootfs && cp /tmp/genimgout/disk-rootfs.img /tmp/pack-rootfs/ && \
  mkdir -p /tmp/pack-rom && cp ./pc-bios/{bios-256k.bin,vgabios-stdvga.bin,kvmvapic.bin,linuxboot_dma.bin,efi-virtio.rom} /tmp/pack-rom/
$ for f in kernel initramfs rom rootfs ; do
    docker cp /tmp/pack-${f} build-qemu-wasm:/ && \
    docker exec -it build-qemu-wasm /bin/sh -c "/emsdk/upstream/emscripten/tools/file_packager.py load-${f}.data --preload /pack-${f} > load-${f}.js" && \
    docker cp build-qemu-wasm:/build/load-${f}.js /tmp/test-js/htdocs/ && \
    docker cp build-qemu-wasm:/build/load-${f}.data /tmp/test-js/htdocs/
  done
$ wget -O /tmp/c2w-net-proxy.wasm https://github.com/ktock/container2wasm/releases/download/v0.5.0/c2w-net-proxy.wasm
$ ( cd ./examples/networking/htdocs/ && npx webpack && cp -R dist vendor/xterm.css /tmp/test-js/htdocs/ )
$ cat /tmp/c2w-net-proxy.wasm | gzip > /tmp/test-js/htdocs/c2w-net-proxy.wasm.gzip
$ docker cp build-qemu-wasm:/build/qemu-system-x86_64 /tmp/test-js/htdocs/out.js
$ for f in qemu-system-x86_64.wasm qemu-system-x86_64.worker.js ; do
    docker cp build-qemu-wasm:/build/${f} /tmp/test-js/htdocs/
  done
```

Store additional page assets to the server root.

```
cp ./examples/x86_64-alpine/src/htdocs/module.js /tmp/test-js/htdocs/arg-module.js
cp ./examples/x86_64-alpine/src/htdocs/index.html /tmp/test-js/htdocs/index.html
```

Start the server.

```
$ docker run --rm -p 127.0.0.1:8088:80 \
    -v "/tmp/test-js/htdocs:/usr/local/apache2/htdocs/:ro" \
    -v "/tmp/test-js/xterm-pty.conf:/usr/local/apache2/conf/extra/xterm-pty.conf:ro" \
    --entrypoint=/bin/sh httpd -c 'echo "Include conf/extra/xterm-pty.conf" >> /usr/local/apache2/conf/httpd.conf && httpd-foreground'
```

Then `localhost:8088` serves the VM.
Login as `root` user without password.

To enable networking, acccess to `localhost:8088?net=browser`.

This runs a networking stack ([c2w-net-proxy.wasm](https://github.com/ktock/container2wasm/tree/da372f28342f73be1857e1ab5f67eae56280b021/extras/c2w-net-proxy)) inside browser relying on Fetch API for HTTP(S) connection to the outside of the browser.
From the guest VM, that networking stack can be seen as a HTTP(S) proxy running inside browser.
The proxy's certificate is shared to the guest VM via a mount `wasm0`.
In the guest, you can setup the interface as the following.

```
# mount -t 9p -o trans=virtio wasm0 /mnt -oversion=9p2000.L
# export SSL_CERT_FILE=/mnt/proxy.crt
# export https_proxy=http://192.168.127.253:80
# export http_proxy=http://192.168.127.253:80
# export HTTPS_PROXY=http://192.168.127.253:80
# export HTTP_PROXY=http://192.168.127.253:80
# rc-service networking start
```

In the guest, the following fetches a page from `https://ktock.github.io/container2wasm-demo/`.

```
# wget -O - https://ktock.github.io/container2wasm-demo/
```

> NOTE: Also refer to [`../networking`](../networking) for details about networking.
