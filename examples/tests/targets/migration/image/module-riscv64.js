Module['arguments'] = [
    '-incoming', 'file:/pack/vm.state',
    '-nographic', '-m', '512M', '-accel', 'tcg,tb-size=500',
    '-machine', 'virt',
    '-L', '/pack/',
    '-nic', 'none',
    '-drive', 'if=virtio,format=raw,file=/pack/rootfs.bin',
    '-kernel', '/pack/Image',
    '-append', 'earlyprintk=ttyS0 console=ttyS0 root=/dev/vda rootwait ro quiet virtio_net.napi_tx=false loglevel=7',
];
