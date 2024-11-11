ARG BUSYBOX_VERSION=1.36.1
ARG KERNEL_TAG=1.20230405

FROM ubuntu:22.04 AS rootfs-dev
RUN apt-get update && apt-get install -y gcc-aarch64-linux-gnu linux-libc-dev-arm64-cross git make
ARG BUSYBOX_VERSION
RUN apt-get update -y && apt-get install -y gcc bzip2 wget
WORKDIR /work
RUN wget https://busybox.net/downloads/busybox-${BUSYBOX_VERSION}.tar.bz2
RUN bzip2 -d busybox-${BUSYBOX_VERSION}.tar.bz2
RUN tar xvf busybox-${BUSYBOX_VERSION}.tar
WORKDIR /work/busybox-${BUSYBOX_VERSION}
RUN make CROSS_COMPILE=aarch64-linux-gnu- LDFLAGS=--static defconfig
RUN make CROSS_COMPILE=aarch64-linux-gnu- LDFLAGS=--static -j$(nproc)
RUN mkdir -p /rootfs/bin && mv busybox /rootfs/bin/busybox
RUN make LDFLAGS=--static defconfig
RUN make LDFLAGS=--static -j$(nproc)
RUN for i in $(./busybox --list) ; do ln -s busybox /rootfs/bin/$i ; done
RUN mkdir -p /rootfs/proc /rootfs/sys /rootfs/mnt /rootfs/run /rootfs/tmp /rootfs/dev /rootfs/var /rootfs/etc && mknod /rootfs/dev/null c 1 3 && chmod 666 /rootfs/dev/null
COPY ./rcS /rootfs/etc/init.d/
RUN chmod 700 /rootfs/etc/init.d/rcS
RUN dd if=/dev/zero of=rootfs.bin bs=4M count=1
RUN mke2fs -d /rootfs rootfs.bin
RUN mkdir /out/ && mv rootfs.bin /out/

FROM ubuntu:24.04 AS kernel-dev
ARG KERNEL_TAG
WORKDIR /work
RUN apt-get update && apt-get install -y crossbuild-essential-arm64 git gperf bc bison flex libssl-dev make libc6-dev libncurses5-dev wget
RUN wget https://github.com/raspberrypi/linux/archive/refs/tags/${KERNEL_TAG}.tar.gz
RUN tar -xzvf ${KERNEL_TAG}.tar.gz
WORKDIR /work/linux-${KERNEL_TAG}
RUN ls
RUN make ARCH=arm64 CROSS_COMPILE=aarch64-linux-gnu- bcm2711_defconfig
RUN make ARCH=arm64 CROSS_COMPILE=aarch64-linux-gnu- -j$(nproc) Image dtbs
RUN mkdir /out
RUN cp arch/arm64/boot/Image /out/kernel8.img
RUN cp arch/arm64/boot/dts/broadcom/bcm2710-rpi-3-b-plus.dtb /out/

FROM scratch
COPY --from=rootfs-dev /out/rootfs.bin /
COPY --from=kernel-dev /out/ /
