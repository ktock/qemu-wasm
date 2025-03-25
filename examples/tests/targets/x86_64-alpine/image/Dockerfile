ARG NODE_VERSION=20.2.0
ARG QEMU_BUILD_BASE=
ARG TEST_TARGET_ARCH=
ARG PACKAGES=

FROM scratch AS qemusrc

FROM golang AS create-image-dev
COPY ./create-image ./create-image
RUN cd create-image && go build -o /out/create-image .

FROM ubuntu:24.04 AS image-dev-x86_64
RUN apt-get update && apt-get install -y qemu-system qemu-utils wget

RUN mkdir /out/
RUN wget -O /out/alpine.iso https://dl-cdn.alpinelinux.org/alpine/v3.21/releases/x86_64/alpine-virt-3.21.2-x86_64.iso
RUN dd if=/dev/zero of=/out/tmp.img bs=80M count=1 && \
    mkfs.ext4 /out/tmp.img
RUN dd if=/dev/zero of=/out/disk-rootfs.img bs=150M count=1 && \
    mkfs.ext4 -O ^has_journal /out/disk-rootfs.img
COPY ./init.sh /out/
COPY ./create-image.sh /out/
COPY ./setup-wasm-networking /out/
COPY ./root-profile /out/
COPY --from=create-image-dev /out/create-image /
COPY ./create-image-args-x86_64.json .
ARG PACKAGES
RUN echo -n $PACKAGES > /out/packages
RUN /create-image --args-json=create-image-args-x86_64.json qemu-system-x86_64

FROM scratch AS image-dev
COPY --from=image-dev-x86_64 /out/disk-rootfs.img /
COPY --from=image-dev-x86_64 /out/vmlinuz-virt /
COPY --from=image-dev-x86_64 /out/initramfs-virt /

FROM ${QEMU_BUILD_BASE} AS dev
ARG TEST_TARGET_ARCH

COPY --from=qemusrc / /qemu/

WORKDIR /build/
RUN emconfigure /qemu/configure --static --target-list=${TEST_TARGET_ARCH}-softmmu --without-default-features --enable-system --with-coroutine=fiber --enable-virtfs
RUN emmake make -j$(nproc)

RUN mkdir -p /pack/
COPY --from=image-dev / /pack/
RUN cp /qemu/pc-bios/* /pack/ || true
RUN /emsdk/upstream/emscripten/tools/file_packager.py qemu-system-${TEST_TARGET_ARCH}.data --preload /pack > load.js

FROM node:${NODE_VERSION} AS assets-dev

COPY ./assets/ /assets/
WORKDIR /assets/
RUN npm install webpack webpack-cli
RUN wget -O /assets/c2w-net-proxy.wasm https://github.com/ktock/container2wasm/releases/download/v0.5.0/c2w-net-proxy.wasm
RUN npx webpack

RUN mkdir /out/
RUN cp -R index.html dist /out/
RUN cat c2w-net-proxy.wasm | gzip > /out/c2w-net-proxy.wasm.gzip

FROM scratch
ARG TEST_TARGET_ARCH
COPY --from=dev /build/qemu-system-${TEST_TARGET_ARCH}.js out.js
COPY --from=dev /build/qemu-system-${TEST_TARGET_ARCH}.wasm /build/qemu-system-${TEST_TARGET_ARCH}.worker.js /build/qemu-system-${TEST_TARGET_ARCH}.data /build/load.js .
COPY module-${TEST_TARGET_ARCH}.js ./module.js
COPY --from=assets-dev /out/ /
