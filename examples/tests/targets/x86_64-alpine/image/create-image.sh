#!/bin/sh

set -xeu -o pipefail

ip link set dev eth0 up
udhcpc -i eth0

mkdir /mnt/sdb
mount /dev/sdb /mnt/sdb
mkdir /mnt/sdb/boot/
mount /dev/sda /mnt/sdb/boot/

setup-apkrepos -1
BOOTLOADER=none setup-disk -m sys /mnt/sdb/
cp /mnt/sdb/boot/vmlinuz-virt /mnt/sdc1/
KERNEL_VERSION=$(ls /mnt/sdb/lib/modules | head -n 1 | tr -d '\n')

echo 'rc_sys="docker"' >> /mnt/sdb/etc/rc.conf
echo "auto eth0
iface eth0 inet dhcp" > /mnt/sdb/etc/network/interfaces
( chroot /mnt/sdb/ apk add --no-progress --no-cache ca-certificates ) || true
chroot /mnt/sdb/ update-ca-certificates
PACKAGES=$(cat /mnt/sdc1/packages)
if [ "$PACKAGES" != "" ] ; then
    chroot /mnt/sdb/ apk add --no-progress --no-cache $PACKAGES
fi

cp /mnt/sdc1/setup-wasm-networking /mnt/sdb/etc/init.d/
chmod 755 /mnt/sdb/etc/init.d/setup-wasm-networking
mkdir /mnt/sdb/etc/runlevels/additional
cat <<'EOF' >> /mnt/sdb/etc/inittab
# Additional initialization
::once:/sbin/openrc additional &> /dev/null
EOF
chroot /mnt/sdb/ rc-update add setup-wasm-networking additional
cp /mnt/sdc1/root-profile /mnt/sdb/root/.profile
chmod 644 /mnt/sdb/root/.profile

apk add --no-progress --no-cache mkinitfs
cp /mnt/sdc1/init.sh /mnt/sdb/sbin/init.sh
chmod 755 /mnt/sdb/sbin/init.sh
mkinitfs -i /mnt/sdb/sbin/init.sh -c /etc/mkinitfs/mkinitfs.conf -b /mnt/sdb/ $KERNEL_VERSION
mv /mnt/sdb/boot/initramfs-virt /mnt/sdc1/

umount /mnt/sdb/boot/
umount /mnt/sdb
