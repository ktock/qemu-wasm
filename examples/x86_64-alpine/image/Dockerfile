ARG PACKAGES=

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

FROM scratch
COPY --from=image-dev-x86_64 /out/disk-rootfs.img /
COPY --from=image-dev-x86_64 /out/vmlinuz-virt /
COPY --from=image-dev-x86_64 /out/initramfs-virt /
