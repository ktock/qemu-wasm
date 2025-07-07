ARG BUSYBOX_VERSION=1.36.1

ARG QEMU_BUILD_BASE=
ARG TEST_TARGET_ARCH=
ARG TARGET_WASM=
ARG WASM64_MEMORY64_FLAG=

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

FROM ${QEMU_BUILD_BASE} AS dev
ARG TEST_TARGET_ARCH
ARG TARGET_WASM
ARG WASM64_MEMORY64_FLAG

COPY --from=qemusrc / /qemu/

WORKDIR /build/
RUN emconfigure /qemu/configure --static --cpu=${TARGET_WASM} ${WASM64_MEMORY64_FLAG} --target-list=${TEST_TARGET_ARCH}-softmmu --disable-tools --enable-tcg-interpreter
RUN emmake make -j$(nproc)

RUN mkdir -p /pack/
RUN cp /qemu/pc-bios/* /pack/ || true
RUN bzip2 -d /pack/edk2-aarch64-code.fd.bz2
RUN bzip2 -d /pack/edk2-arm-code.fd.bz2
COPY --from=rootfs-dev /out/rootfs.bin /pack/
COPY --from=kernel-dev /out/Image /pack/
RUN /emsdk/upstream/emscripten/tools/file_packager.py qemu-system-${TEST_TARGET_ARCH}.data --preload /pack > load.js

FROM scratch
ARG TEST_TARGET_ARCH
COPY --from=dev /build/qemu-system-${TEST_TARGET_ARCH}.js out.js
COPY --from=dev /build/qemu-system-${TEST_TARGET_ARCH}.wasm /build/qemu-system-${TEST_TARGET_ARCH}.data /build/load.js .
COPY module-${TEST_TARGET_ARCH}.js ./module.js
