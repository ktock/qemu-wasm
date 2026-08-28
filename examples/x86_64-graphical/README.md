# Graphical x86_64 example

Same guest as `examples/x86_64`, but rendered to an HTML canvas through the
`wasm` display backend (`ui/wasm.c`) instead of only to the serial console.
The serial console is kept alongside it, because when the canvas shows nothing
it is the only way to tell a guest that failed to boot from a display backend
that failed to draw.

## Requirements

The page must be served **cross-origin isolated**:

```
Cross-Origin-Opener-Policy: same-origin
Cross-Origin-Embedder-Policy: require-corp
```

The build uses `-pthread` and `-sPROXY_TO_PTHREAD=1`, so multi-threaded TCG
needs `SharedArrayBuffer`, which a browser only exposes on a cross-origin
isolated document. Without both headers the page loads and then fails during
startup in a way that looks like an emulator bug rather than a hosting one.

This is also why the example cannot be opened as a local `file://` page.

## Building

Follow the `examples/x86_64` instructions, then serve
`examples/x86_64-graphical/src/htdocs` with the headers above and with
`qemu-wasm-display.js` alongside the generated files.

## Runtime exports used by the page

| Export | Purpose |
|---|---|
| `_qemu_wasm_input_ring` | address of the lock-free input ring |
| `_qemu_wasm_ring_capacity` | ring size, so the page need not hardcode it |
| `_qemu_wasm_keymap_json` | `KeyboardEvent.code` to `QKeyCode`, generated from the C table so the mapping cannot drift |
| `_qemu_wasm_frame_count` | frames presented, for an FPS readout |
| `_qemu_wasm_events_{key,btn,abs}` | per-kind input counters, useful for testing |
| `_qemu_wasm_dropped_input` | events dropped because the guest was not draining the ring |

`UTF8ToString` must be in `EXPORTED_RUNTIME_METHODS` for the keymap export.
