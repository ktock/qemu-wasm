/*
 * Differences from examples/x86_64, and why each matters for a graphical guest:
 *
 *   -vga std        gives the guest a display device at all. stdvga rather than
 *                   virtio-gpu for a first example, because SeaBIOS writes to it
 *                   before any guest kernel loads, so pixels appear early and
 *                   "the display works" is separable from "the guest booted".
 *
 *   -display wasm   selects ui/wasm.c. Without it QEMU picks 'none'.
 *
 *   -serial stdio   NOT optional once -nographic is dropped. Without -nographic
 *                   QEMU's serial defaults to a virtual console, so the xterm
 *                   below the canvas receives nothing at all.
 *
 *   console=tty0    kernel messages to the framebuffer as well as the serial
 *                   port, so the two panels can be compared directly.
 *
 *   virtio-tablet   absolute pointer, so the guest cursor tracks the host cursor
 *                   without pointer lock or relative-motion drift.
 */
Module['arguments'] = [
    '-m', '512M',
    '-accel', 'tcg,tb-size=500,thread=multi',
    '-smp', '4,sockets=4',
    '-L', '/pack/',
    '-nic', 'none',

    '-serial', 'stdio',
    '-vga', 'std',
    '-display', 'wasm',
    '-device', 'virtio-tablet-pci',

    '-drive', 'if=virtio,format=raw,file=/pack/rootfs.bin',
    '-kernel', '/pack/bzImage',
    '-append', 'earlyprintk=ttyS0,115200n8 console=tty0 console=ttyS0,115200n8 root=/dev/vda rootwait ro loglevel=7',
];
