Module['arguments'] = [
    '-incoming', 'file:/pack/vm.state',
    '-nographic', '-m', '512M', '-accel', 'tcg,tb-size=500',
    '-L', '/pack/',
    '-nic', 'none',
    '-drive', 'if=virtio,format=raw,file=/pack/rootfs.bin',
    '-kernel', '/pack/bzImage',
    '-append', 'earlyprintk=ttyS0,115200n8 console=ttyS0,115200n8 root=/dev/vda rootwait ro loglevel=7',
];
