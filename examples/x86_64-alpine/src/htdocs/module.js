Module['arguments'] = [
    '-nographic', '-M', 'pc', '-m', '512M', '-accel', 'tcg,tb-size=500',
    '-L', '/pack-rom/',
    '-nic', 'none',
    '-kernel', '/pack-kernel/vmlinuz-virt',
    '-initrd', '/pack-initramfs/initramfs-virt',
    '-append', 'console=ttyS0 noautodetect hostname=demo',
    '-drive', 'id=test,file=/pack-rootfs/disk-rootfs.img,format=raw,if=none',
    '-device', 'virtio-blk-pci,drive=test',
    '-virtfs', 'local,path=/.wasmenv,mount_tag=wasm0,security_model=passthrough,id=wasm0',
    '-netdev', 'socket,id=vmnic,connect=localhost:8888', '-device', 'virtio-net-pci,netdev=vmnic'
];
