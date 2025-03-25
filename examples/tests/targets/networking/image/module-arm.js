Module['arguments'] = [
    '-nographic', '-m', '512M', '-accel', 'tcg,tb-size=500',
    '-cpu', 'max', '-machine', 'virt',
    '-L', '/pack/',
    '-device', 'virtio-blk-device,drive=hd', '-drive', 'file=/pack/rootfs.bin,if=none,format=raw,id=hd',
    '-kernel', '/pack/Image',
    '-append', 'earlyprintk=ttyAMA0 console=ttyAMA0 root=/dev/vda rootwait no_console_suspend ro loglevel=7',
    '-fsdev', 'local,path=/.wasmenv,security_model=passthrough,id=wasm0', '-device', 'virtio-9p-device,fsdev=wasm0,mount_tag=wasm0',
    '-netdev', 'socket,id=vmnic,connect=localhost:8888', '-device', 'virtio-net-device,netdev=vmnic'
];
