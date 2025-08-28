Module['arguments'] = [
    "-nographic", "-m", "512M", "-accel", "tcg,tb-size=500",
    "-L", "/pack/",
    "-drive", "if=virtio,format=raw,file=/pack/rootfs.bin",
    "-kernel", "/pack/bzImage",
    "-append", "earlyprintk=ttyS0,115200n8 console=ttyS0,115200n8 slub_debug=F root=/dev/vda rootwait acpi=off ro virtio_net.napi_tx=false loglevel=7",
    "-virtfs", "local,path=/.wasmenv,mount_tag=wasm0,security_model=passthrough,id=wasm0",
    "-netdev", "socket,id=vmnic,connect=localhost:8888", "-device", "virtio-net-pci,netdev=vmnic"
];
