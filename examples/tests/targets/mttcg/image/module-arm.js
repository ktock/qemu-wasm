Module['arguments'] = [
    '-nographic', '-m', '512M', '-machine', 'virt', '-accel', 'tcg,tb-size=500,thread=multi', '-smp', '4,sockets=4',
    '-L', '/pack/',
    '-global', 'virtio-mmio.force-legacy=false',
    '-device', 'virtio-blk-device,drive=d0',
    '-drive', 'file=/pack/rootfs.bin,if=none,format=raw,id=d0',
    '-kernel', '/pack/Image',
    '-append', 'console=ttyAMA0 root=/dev/vda loglevel=7',
];
