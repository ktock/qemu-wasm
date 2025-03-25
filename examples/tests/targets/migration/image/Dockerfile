ARG BUSYBOX_VERSION=1.36.1

ARG QEMU_BUILD_BASE=
ARG TEST_TARGET_ARCH=

FROM scratch AS qemusrc

FROM ubuntu:22.04 AS rootfs-dev-base-x86_64
ARG TEST_TARGET_ARCH
RUN apt-get update && apt-get install -y gcc-x86-64-linux-gnu linux-libc-dev-amd64-cross git make gcc bzip2 wget
ENV CROSS=x86_64-linux-gnu-

FROM ubuntu:22.04 AS rootfs-dev-base-aarch64
ARG TEST_TARGET_ARCH
RUN apt-get update && apt-get install -y gcc-aarch64-linux-gnu linux-libc-dev-arm64-cross git make gcc bzip2 wget
ENV CROSS=aarch64-linux-gnu-

FROM ubuntu:22.04 AS rootfs-dev-base-riscv64
RUN apt-get update && apt-get install -y gcc-riscv64-linux-gnu linux-libc-dev-riscv64-cross git make gcc bzip2 wget
ENV CROSS=riscv64-linux-gnu-

FROM ubuntu:22.04 AS rootfs-dev-base-arm
RUN apt-get update && apt-get install -y gcc-arm-linux-gnueabi linux-libc-dev-armhf-cross git make gcc bzip2 wget
ENV CROSS=arm-linux-gnueabi-

FROM ubuntu:22.04 AS rootfs-dev-base-s390x
RUN apt-get update && apt-get install -y gcc-s390x-linux-gnu linux-libc-dev-s390x-cross git make gcc bzip2 wget
ENV CROSS=s390x-linux-gnu-

FROM rootfs-dev-base-${TEST_TARGET_ARCH} AS rootfs-dev
ARG BUSYBOX_VERSION
WORKDIR /work
RUN wget https://busybox.net/downloads/busybox-${BUSYBOX_VERSION}.tar.bz2
RUN bzip2 -d busybox-${BUSYBOX_VERSION}.tar.bz2
RUN tar xf busybox-${BUSYBOX_VERSION}.tar
WORKDIR /work/busybox-${BUSYBOX_VERSION}
RUN make CROSS_COMPILE=${CROSS} LDFLAGS=--static defconfig
RUN make CROSS_COMPILE=${CROSS} LDFLAGS=--static -j$(nproc)
RUN mkdir -p /rootfs/bin && mv busybox /rootfs/bin/busybox
RUN make LDFLAGS=--static defconfig
RUN make LDFLAGS=--static -j$(nproc)
RUN for i in $(./busybox --list) ; do ln -s busybox /rootfs/bin/$i ; done
RUN mkdir -p /rootfs/usr/share/udhcpc/ && cp ./examples/udhcp/simple.script /rootfs/usr/share/udhcpc/default.script
RUN mkdir -p /rootfs/proc /rootfs/sys /rootfs/mnt /rootfs/run /rootfs/tmp /rootfs/dev /rootfs/var /rootfs/etc && mknod /rootfs/dev/null c 1 3 && chmod 666 /rootfs/dev/null
COPY ./rcS /rootfs/etc/init.d/
RUN chmod 700 /rootfs/etc/init.d/rcS
RUN dd if=/dev/zero of=rootfs.bin bs=4M count=1
RUN mke2fs -d /rootfs rootfs.bin
RUN mkdir /out/ && mv rootfs.bin /out/

FROM ubuntu:24.04 AS kernel-dev-x86_64
RUN apt-get update && apt-get install -y gcc-x86-64-linux-gnu linux-libc-dev-amd64-cross git make gperf flex bison bc libelf-dev
RUN mkdir /work-buildlinux
WORKDIR /work-buildlinux
RUN git clone -b v6.1 --depth 1 https://github.com/torvalds/linux
WORKDIR /work-buildlinux/linux
COPY ./linux_x86_64_config ./.config
RUN make ARCH=x86 CROSS_COMPILE=x86_64-linux-gnu- -j$(nproc) all && \
    mkdir /out && \
    mv /work-buildlinux/linux/arch/x86/boot/bzImage /out/Image && \
    make clean

FROM ubuntu:24.04 AS kernel-dev-aarch64
RUN apt-get update && apt-get install -y gcc-aarch64-linux-gnu linux-libc-dev-arm64-cross git make gperf flex bison bc libelf-dev
RUN mkdir /work-buildlinux
WORKDIR /work-buildlinux
RUN git clone -b v6.1 --depth 1 https://github.com/torvalds/linux
WORKDIR /work-buildlinux/linux
COPY ./linux_aarch64_config ./.config
RUN make ARCH=arm64 CROSS_COMPILE=aarch64-linux-gnu- -j$(nproc) all && \
    mkdir /out && \
    mv /work-buildlinux/linux/arch/arm64/boot/Image /out/Image && \
    make clean

FROM ubuntu:24.04 AS kernel-dev-riscv64
RUN apt-get update && apt-get install -y gcc-riscv64-linux-gnu linux-libc-dev-riscv64-cross git make gperf flex bison bc libelf-dev
RUN mkdir /work-buildlinux
WORKDIR /work-buildlinux
RUN git clone -b v6.1 --depth 1 https://github.com/torvalds/linux
WORKDIR /work-buildlinux/linux
COPY ./linux_riscv64_config ./.config
RUN make ARCH=riscv CROSS_COMPILE=riscv64-linux-gnu- -j$(nproc) all && \
    mkdir /out && \
    mv /work-buildlinux/linux/arch/riscv/boot/Image /out/Image && \
    make clean

FROM ubuntu:24.04 AS kernel-dev-arm
RUN apt-get update && apt-get install -y gcc-arm-linux-gnueabihf linux-libc-dev-armhf-cross git make gperf flex bison bc libelf-dev
RUN mkdir /work-buildlinux
WORKDIR /work-buildlinux
RUN git clone -b v6.1 --depth 1 https://github.com/torvalds/linux
WORKDIR /work-buildlinux/linux
COPY ./linux_arm_config ./.config
RUN make ARCH=arm CROSS_COMPILE=arm-linux-gnueabihf- -j$(nproc) all
RUN mkdir /out && \
    mv /work-buildlinux/linux/arch/arm/boot/zImage /out/Image && \
    make clean

FROM ubuntu:24.04 AS kernel-dev-s390x
RUN apt-get update && apt-get install -y gcc-s390x-linux-gnu linux-libc-dev-s390x-cross git make gperf flex bison bc libelf-dev
RUN mkdir /work-buildlinux
WORKDIR /work-buildlinux
RUN git clone -b v6.1 --depth 1 https://github.com/torvalds/linux
WORKDIR /work-buildlinux/linux
COPY ./linux_s390x_config ./.config
RUN make ARCH=s390 CROSS_COMPILE=s390x-linux-gnu- -j$(nproc) all
RUN mkdir /out && \
    mv /work-buildlinux/linux/arch/s390/boot/bzImage /out/Image && \
    make clean

FROM kernel-dev-${TEST_TARGET_ARCH} AS kernel-dev

FROM gcc:14 AS state-dev-x86_64
RUN apt-get update && apt-get install -y libffi-dev libglib2.0-dev libpixman-1-dev libattr1 libattr1-dev ninja-build pipx expect
RUN PIPX_BIN_DIR=/usr/local/bin pipx install meson==1.5.0
COPY --from=qemusrc --link / /qemu/
WORKDIR /build/
RUN /qemu/configure --static --target-list=x86_64-softmmu --without-default-features --enable-system --with-coroutine=ucontext --enable-virtfs --enable-attr
RUN make -j $(nproc) qemu-system-x86_64
COPY --from=rootfs-dev /out/rootfs.bin /pack/
COPY --from=kernel-dev /out/Image /pack/
RUN cp /qemu/pc-bios/* /pack/ || true
RUN <<EOF
expect -c '
set timeout 1000
spawn ./qemu-system-x86_64 -nographic -m 512M -accel tcg,tb-size=500 \
                           -L /pack/ \
                           -nic none \
                           -drive if=virtio,format=raw,file=/pack/rootfs.bin \
                           -kernel /pack/Image \
                           -append "earlyprintk=ttyS0 console=ttyS0 root=/dev/vda rootwait ro loglevel=7"
expect "Please press Enter to activate this console."
send "\r"
send "echo test > /tmp/file\r"
expect "#"

puts "Opening monitor..."
send \x01
send "c"
expect "(qemu)"
send "migrate file:/pack/vm.state\r"
expect "(qemu)"
send "quit\r"

expect eof
close
wait
'
EOF

FROM gcc:14 AS state-dev-aarch64
RUN apt-get update && apt-get install -y libffi-dev libglib2.0-dev libpixman-1-dev libattr1 libattr1-dev ninja-build pipx expect
RUN PIPX_BIN_DIR=/usr/local/bin pipx install meson==1.5.0
COPY --from=qemusrc --link / /qemu/
WORKDIR /build/
RUN /qemu/configure --static --target-list=aarch64-softmmu --without-default-features --enable-system --with-coroutine=ucontext --enable-virtfs --enable-attr
RUN make -j $(nproc) qemu-system-aarch64
COPY --from=rootfs-dev /out/rootfs.bin /pack/
COPY --from=kernel-dev /out/Image /pack/
RUN cp /qemu/pc-bios/* /pack/ || true
RUN bzip2 -d /pack/edk2-aarch64-code.fd.bz2
RUN <<EOF
expect -c '
set timeout 1000
spawn ./qemu-system-aarch64 \
    -nographic -m 512M -accel tcg,tb-size=500 \
    -cpu cortex-a53 -machine virt \
    -L /pack/ \
    -bios /pack/edk2-aarch64-code.fd \
    -drive if=virtio,format=raw,file=/pack/rootfs.bin \
    -kernel /pack/Image \
    -append "earlyprintk=ttyS0 console=ttyS0 root=/dev/vda rootwait no_console_suspend ro loglevel=7"

expect "Please press Enter to activate this console."
send "\r"
send "echo test > /tmp/file\r"
expect "#"

puts "Opening monitor..."
send \x01
send "c"
expect "(qemu)"
send "migrate file:/pack/vm.state\r"
expect "(qemu)"
send "quit\r"

expect eof
close
wait
'
EOF

FROM gcc:14 AS state-dev-riscv64
RUN apt-get update && apt-get install -y libffi-dev libglib2.0-dev libpixman-1-dev libattr1 libattr1-dev ninja-build pipx expect
RUN PIPX_BIN_DIR=/usr/local/bin pipx install meson==1.5.0
COPY --from=qemusrc / /qemu/
WORKDIR /build/
RUN /qemu/configure --static --target-list=riscv64-softmmu --without-default-features --enable-system --with-coroutine=ucontext --enable-virtfs --enable-attr
RUN make -j $(nproc) qemu-system-riscv64
COPY --from=rootfs-dev /out/rootfs.bin /pack/
COPY --from=kernel-dev /out/Image /pack/
RUN cp /qemu/pc-bios/* /pack/ || true
RUN <<EOF
expect -c '
set timeout 1000
spawn ./qemu-system-riscv64 \
    -nographic -m 512M -accel tcg,tb-size=500 \
    -machine virt \
    -L /pack/ \
    -nic none \
    -drive if=virtio,format=raw,file=/pack/rootfs.bin \
    -kernel /pack/Image \
    -append "earlyprintk=ttyS0 console=ttyS0 root=/dev/vda rootwait ro quiet virtio_net.napi_tx=false loglevel=7"

expect "Please press Enter to activate this console."
send "\r"
send "echo test > /tmp/file\r"
expect "#"

puts "Opening monitor..."
send \x01
send "c"
expect "(qemu)"
send "migrate file:/pack/vm.state\r"
expect "(qemu)"
send "quit\r"

expect eof
close
wait
'
EOF

FROM gcc:14 AS state-dev-arm
RUN apt-get update && apt-get install -y libffi-dev libglib2.0-dev libpixman-1-dev libattr1 libattr1-dev ninja-build pipx expect
RUN PIPX_BIN_DIR=/usr/local/bin pipx install meson==1.5.0
COPY --from=qemusrc --link / /qemu/
WORKDIR /build/
RUN /qemu/configure --static --target-list=arm-softmmu --without-default-features --enable-system --with-coroutine=ucontext --enable-virtfs --enable-attr
RUN make -j $(nproc) qemu-system-arm
COPY --from=rootfs-dev /out/rootfs.bin /pack/
COPY --from=kernel-dev /out/Image /pack/
RUN cp /qemu/pc-bios/* /pack/ || true
RUN bzip2 -d /pack/edk2-arm-code.fd.bz2
RUN <<EOF
expect -c '
set timeout 1000
spawn ./qemu-system-arm \
    -nographic -m 512M -accel tcg,tb-size=500 \
    -cpu max -machine virt \
    -L /pack/ \
    -device virtio-blk-device,drive=hd -drive file=/pack/rootfs.bin,if=none,format=raw,id=hd \
    -kernel /pack/Image \
    -append "earlyprintk=ttyAMA0 console=ttyAMA0 root=/dev/vda rootwait no_console_suspend ro loglevel=7"

expect "Please press Enter to activate this console."
send "\r"
send "echo test > /tmp/file\r"
expect "#"

puts "Opening monitor..."
send \x01
send "c"
expect "(qemu)"
send "migrate file:/pack/vm.state\r"
expect "(qemu)"
send "quit\r"

expect eof
close
wait
'
EOF

FROM gcc:14 AS state-dev-s390x
RUN apt-get update && apt-get install -y libffi-dev libglib2.0-dev libpixman-1-dev libattr1 libattr1-dev ninja-build pipx expect
RUN PIPX_BIN_DIR=/usr/local/bin pipx install meson==1.5.0
COPY --from=qemusrc --link / /qemu/
WORKDIR /build/
RUN /qemu/configure --static --target-list=s390x-softmmu --without-default-features --enable-system --with-coroutine=ucontext --enable-virtfs --enable-attr
RUN make -j $(nproc) qemu-system-s390x
COPY --from=rootfs-dev /out/rootfs.bin /pack/
COPY --from=kernel-dev /out/Image /pack/
RUN cp /qemu/pc-bios/* /pack/ || true
RUN <<EOF
expect -c '
set timeout 1000
spawn ./qemu-system-s390x -nographic -m 512M -accel tcg,tb-size=500 \
                           -L /pack/ \
                           -nic none \
                           -drive if=virtio,format=raw,file=/pack/rootfs.bin \
                           -kernel /pack/Image \
                           -append "earlyprintk=ttyS0 console=ttyS0 root=/dev/vda rootwait ro loglevel=7"
expect "Please press Enter to activate this console."
send "\r"
send "echo test > /tmp/file\r"
expect "#"

puts "Opening monitor..."
send \x01
send "c"
expect "(qemu)"
send "migrate file:/pack/vm.state\r"
expect "(qemu)"
send "quit\r"

expect eof
close
wait
'
EOF

FROM state-dev-${TEST_TARGET_ARCH} AS state-dev

FROM ${QEMU_BUILD_BASE} AS dev
ARG TEST_TARGET_ARCH

COPY --from=qemusrc --link / /qemu/

WORKDIR /build/
RUN emconfigure /qemu/configure --static --target-list=${TEST_TARGET_ARCH}-softmmu --without-default-features --enable-system --with-coroutine=fiber --enable-virtfs
RUN emmake make -j$(nproc)

RUN mkdir -p /pack/
RUN cp /qemu/pc-bios/* /pack/ || true
RUN bzip2 -d /pack/edk2-aarch64-code.fd.bz2
RUN bzip2 -d /pack/edk2-arm-code.fd.bz2
COPY --from=rootfs-dev /out/rootfs.bin /pack/
COPY --from=kernel-dev /out/Image /pack/
COPY --from=state-dev /pack/vm.state /pack/
RUN /emsdk/upstream/emscripten/tools/file_packager.py qemu-system-${TEST_TARGET_ARCH}.data --preload /pack > load.js

FROM scratch
ARG TEST_TARGET_ARCH
COPY --from=dev /build/qemu-system-${TEST_TARGET_ARCH}.js out.js
COPY --from=dev /build/qemu-system-${TEST_TARGET_ARCH}.wasm /build/qemu-system-${TEST_TARGET_ARCH}.worker.js /build/qemu-system-${TEST_TARGET_ARCH}.data /build/load.js .
COPY module-${TEST_TARGET_ARCH}.js ./module.js
