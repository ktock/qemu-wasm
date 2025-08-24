ARG BUSYBOX_VERSION=1.36.1
ARG PIGZ_VERSION=2.8

ARG QEMU_BUILD_BASE=
ARG TARGET_WASM=wasm64
ARG EXTRA_CONFIGURE_FLAGS=
ARG TARGET_MODULE=tcg

FROM scratch AS qemusrc

FROM ubuntu:22.04 AS rootfs-dev-base-x86_64
RUN apt-get update && apt-get install -y gcc-x86-64-linux-gnu linux-libc-dev-amd64-cross git make gcc bzip2 wget
ENV CROSS=x86_64-linux-gnu-

FROM rootfs-dev-base-x86_64 AS pigz-dev
ARG PIGZ_VERSION
RUN apt-get install -y zlib1g-dev
WORKDIR /work
RUN wget https://zlib.net/pigz/pigz-${PIGZ_VERSION}.tar.gz
RUN tar zxvf pigz-${PIGZ_VERSION}.tar.gz
WORKDIR /work/pigz-${PIGZ_VERSION}
RUN make CC=x86_64-linux-gnu-gcc LDFLAGS=-static -j$(nproc)
RUN mkdir /out/ && mv pigz /out/

FROM rootfs-dev-base-x86_64 AS rootfs-dev
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
COPY --from=pigz-dev /out/pigz /rootfs/bin/
COPY ./run.sh /rootfs/
RUN chmod 755 /rootfs/run.sh
RUN head -c 10M </dev/urandom > /rootfs/data
RUN dd if=/dev/zero of=rootfs.bin bs=16M count=1
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

FROM kernel-dev-x86_64 AS kernel-dev

FROM ${QEMU_BUILD_BASE} AS dev
ARG TARGET_WASM
ARG EXTRA_CONFIGURE_FLAGS

COPY --from=qemusrc / /qemu/

WORKDIR /build/
RUN emconfigure /qemu/configure --static --cpu=${TARGET_WASM} ${EXTRA_CONFIGURE_FLAGS} --target-list=x86_64-softmmu --disable-tools
RUN emmake make -j$(nproc)

RUN mkdir -p /pack/
RUN cp /qemu/pc-bios/* /pack/ || true
COPY --from=rootfs-dev /out/rootfs.bin /pack/
COPY --from=kernel-dev /out/Image /pack/
RUN /emsdk/upstream/emscripten/tools/file_packager.py qemu-system-x86_64.data --preload /pack > load.js

FROM scratch
ARG TARGET_MODULE
COPY ./htdocs .
COPY --from=dev /build/qemu-system-x86_64.js ./out.js
COPY --from=dev /build/qemu-system-x86_64.wasm /build/qemu-system-x86_64.data /build/load.js .
COPY ./module-${TARGET_MODULE}.js ./module.js

