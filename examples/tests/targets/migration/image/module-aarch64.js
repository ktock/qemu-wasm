Module['arguments'] = [
    '-incoming', 'file:/pack/vm.state',
    '-nographic', '-m', '512M', '-accel', 'tcg,tb-size=500',
    '-cpu', 'cortex-a53', '-machine', 'virt',
    '-L', '/pack/',
    '-bios', '/pack/edk2-aarch64-code.fd',
    '-drive', 'if=virtio,format=raw,file=/pack/rootfs.bin',
    '-kernel', '/pack/Image',
    '-append', 'earlyprintk=ttyS0 console=ttyS0 root=/dev/vda rootwait no_console_suspend ro loglevel=7',
];
