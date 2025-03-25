Module['arguments'] = [
    '-nographic', '-m', '512M', '-accel', 'tcg,tb-size=500',
    //Use the following to enable MTTCG
    //'-nographic', '-m', '512M', '-accel', 'tcg,tb-size=500,thread=multi', '-smp', '4,sockets=4',
    '-machine', 'virt',
    '-L', '/pack/',
    '-nic', 'none',
    '-drive', 'if=virtio,format=raw,file=/pack/rootfs.bin',
    '-kernel', '/pack/Image',
    '-append', 'earlyprintk=ttyS0 console=ttyS0 root=/dev/vda rootwait ro quiet virtio_net.napi_tx=false loglevel=7',
    '-virtfs', 'local,path=/.wasmenv,mount_tag=wasm0,security_model=passthrough,id=wasm0',
    '-netdev', 'socket,id=vmnic,connect=localhost:8888', '-device', 'virtio-net-pci,netdev=vmnic'
];
