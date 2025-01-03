Module['arguments'] = [
    '-nographic', '-m', '512M', '-accel', 'tcg,tb-size=500',
    //Use the following to enable MTTCG
    //'-nographic', '-m', '512M', '-accel', 'tcg,tb-size=500,thread=multi', '-smp', '4,sockets=4',
    '-L', '/pack/',
    '-nic', 'none',
    '-drive', 'if=virtio,format=raw,file=/pack/rootfs.bin',
    '-kernel', '/pack/bzImage',
    '-append', 'earlyprintk=ttyS0,115200n8 console=ttyS0,115200n8 root=/dev/vda rootwait ro loglevel=7',
    '-virtfs', 'local,path=/share,mount_tag=share0,security_model=passthrough,id=share0',
];
Module['preRun'].push((mod) => {
    mod.FS.mkdir('/share');
    mod.FS.writeFile('/share/file', 'test\n');
});
