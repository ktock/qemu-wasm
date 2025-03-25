Module['arguments'] = [
    '-nographic', '-m', '512M', '-accel', 'tcg,tb-size=500',
    '-cpu', 'max', '-machine', 'virt',
    '-L', '/pack/',
    '-device', 'virtio-blk-device,drive=hd', '-drive', 'file=/pack/rootfs.bin,if=none,format=raw,id=hd',
    '-kernel', '/pack/Image',
    '-append', 'earlyprintk=ttyAMA0 console=ttyAMA0 root=/dev/vda rootwait no_console_suspend ro loglevel=7',
    '-fsdev', 'local,path=/share,security_model=passthrough,id=share0', '-device', 'virtio-9p-device,fsdev=share0,mount_tag=share0'
];
Module['preRun'].push((mod) => {
    mod.FS.mkdir('/share');
    mod.FS.writeFile('/share/file', 'test\n');
});
