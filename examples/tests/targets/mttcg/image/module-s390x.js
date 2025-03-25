Module['arguments'] = [
    '-nographic', '-m', '512M', '-accel', 'tcg,tb-size=500,thread=multi', '-smp', '4,sockets=4',
    '-L', '/pack/',
    '-nic', 'none',
    '-drive', 'if=virtio,format=raw,file=/pack/rootfs.bin',
    '-kernel', '/pack/Image',
    '-append', 'earlyprintk=ttyS0 console=ttyS0 root=/dev/vda rootwait ro loglevel=7',
];
