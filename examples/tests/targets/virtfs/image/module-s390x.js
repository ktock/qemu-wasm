Module['arguments'] = [
    '-nographic', '-m', '512M', '-accel', 'tcg,tb-size=500',
    '-L', '/pack/',
    '-nic', 'none',
    '-drive', 'if=virtio,format=raw,file=/pack/rootfs.bin',
    '-kernel', '/pack/Image',
    '-append', 'earlyprintk=ttyS0 console=ttyS0 root=/dev/vda rootwait ro loglevel=7',
    '-virtfs', 'local,path=/share,mount_tag=share0,security_model=passthrough,id=share0',
];
Module['preRun'].push((mod) => {
    mod.FS.mkdir('/share');
    mod.FS.writeFile('/share/file', 'test\n');
});
