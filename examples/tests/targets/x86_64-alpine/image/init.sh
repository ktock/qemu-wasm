#!/bin/sh

set -eu

/bin/busybox mkdir -p /bin /dev
/bin/busybox --install -s /bin
export PATH="$PATH:/bin"

modprobe -a ext4 virtio_blk virtio_net
mount -t devtmpfs devtmpfs /dev

mkdir /sysroot /sysroot-ovl /sysroot-lower
mount -t tmpfs tmpfs /sysroot-ovl
mkdir /sysroot-ovl/upper /sysroot-ovl/work
mount -t ext4 -oro,noatime,nodiratime,nobarrier /dev/vda /sysroot-lower
mount -t overlay -olowerdir=/sysroot-lower,upperdir=/sysroot-ovl/upper,workdir=/sysroot-ovl/work overlayfs /sysroot
mount -t devtmpfs devtmpfs /sysroot/dev

exec switch_root /sysroot /sbin/init
