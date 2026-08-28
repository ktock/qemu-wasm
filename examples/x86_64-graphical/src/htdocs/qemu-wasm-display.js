/*
 * Browser half of QEMU's wasm display backend (see src/qemu/ui/wasm.c).
 *
 * Runs on the DOM main thread. QEMU runs in a Web Worker, so nothing here may
 * call into the emulator synchronously; the two directions are handled
 * differently and deliberately:
 *
 *   pixels in    ui/wasm.c fires MAIN_THREAD_ASYNC_EM_ASM, which lands in
 *                present() below with a pointer into the shared wasm heap.
 *
 *   input out    written into a lock-free ring in that same shared heap using
 *                Atomics, and drained by QEMU on its own thread. No locks
 *                cross the boundary in either direction.
 *
 * Key codes are never hardcoded here. ui/wasm.c exports its QKeyCode table as
 * JSON at startup, so renumbering the enum upstream cannot silently
 * desynchronise this file from the emulator.
 */

const EV_KEY = 1;
const EV_BTN = 2;
const EV_ABS = 3;

/* Must match InputButton in qapi/ui.json. Only the first five are wired. */
const BTN_LEFT = 0;
const BTN_MIDDLE = 1;
const BTN_RIGHT = 2;
const BTN_WHEEL_UP = 3;
const BTN_WHEEL_DOWN = 4;

/* Ring header layout, in 32-bit words. Mirrors struct WasmInputRing. */
const RING_HEAD = 0;
const RING_TAIL = 1;
const RING_DROPPED = 2;
const RING_HEADER_WORDS = 4;
const EVENT_WORDS = 4;

export class QemuWasmDisplay {
    constructor(canvas, { onStats = null } = {}) {
        this.canvas = canvas;
        this.ctx = canvas.getContext('2d', { alpha: false });
        this.onStats = onStats;

        this.module = null;
        this.ring = null;          /* Int32Array view over the shared heap */
        this.ringBase = 0;         /* word index of the header             */
        this.ringCapacity = 0;
        this.keymap = new Map();
        this.heldKeys = new Set();

        this.width = 0;
        this.height = 0;
        this.frames = 0;
        this.lastStatsAt = 0;
        this.lastStatsFrames = 0;

        /* Present into an offscreen buffer, then blit. Avoids a visible
         * partial frame when the guest damages a sub-rectangle. */
        this.backing = null;
        this.backingCtx = null;
    }

    /*
     * Called once the emulator module is up. Everything that needs the wasm
     * heap or exported functions happens here, not in the constructor,
     * because the heap does not exist until then.
     */
    attach(module) {
        this.module = module;

        const ringPtr = module._qemu_wasm_input_ring();
        this.ringCapacity = module._qemu_wasm_ring_capacity();
        this.ring = new Int32Array(module.HEAP8.buffer);
        this.ringBase = ringPtr >> 2;

        /* Cached and owned by the emulator; explicitly not ours to free. */
        const jsonPtr = module._qemu_wasm_keymap_json();
        const json = module.UTF8ToString(jsonPtr);
        for (const [code, qcode] of Object.entries(JSON.parse(json))) {
            this.keymap.set(code, qcode);
        }

        globalThis.qemuWasmDisplay = this;
        this._bindInput();
    }

    /* ------------------------------------------------------------ output */

    resize(w, h) {
        if (w === this.width && h === this.height) {
            return;
        }
        this.width = w;
        this.height = h;
        this.canvas.width = w;
        this.canvas.height = h;

        this.backing = new OffscreenCanvas(w, h);
        this.backingCtx = this.backing.getContext('2d', { alpha: false });
    }

    present(ptr, w, h, x, y, dw, dh) {
        if (w !== this.width || h !== this.height) {
            this.resize(w, h);
        }
        if (!this.backingCtx || dw <= 0 || dh <= 0) {
            return;
        }

        const stride = w * 4;
        const start = ptr + y * stride + x * 4;

        /*
         * ImageData cannot wrap a SharedArrayBuffer, so the row range is
         * copied out with slice(). One memcpy of the damaged rows, and it is
         * dwarfed by the software rasterisation that produced the pixels.
         * Copying whole rows rather than a tight sub-rectangle keeps this a
         * single contiguous copy instead of dh separate ones.
         */
        const rows = this.module.HEAPU8.slice(start, start + dh * stride);
        const img = new ImageData(new Uint8ClampedArray(rows.buffer), w, dh);

        this.backingCtx.putImageData(img, 0, y, x, 0, dw, dh);
        this.ctx.drawImage(this.backing, 0, 0);

        this.frames++;
        this._maybeReportStats();
    }

    _maybeReportStats() {
        if (!this.onStats) {
            return;
        }
        const now = performance.now();
        if (this.lastStatsAt === 0) {
            this.lastStatsAt = now;
            return;
        }
        const elapsed = now - this.lastStatsAt;
        if (elapsed < 1000) {
            return;
        }
        const fps = ((this.frames - this.lastStatsFrames) * 1000) / elapsed;
        this.lastStatsAt = now;
        this.lastStatsFrames = this.frames;
        this.onStats({
            fps,
            frames: this.frames,
            droppedInput: this.module._qemu_wasm_dropped_input(),
        });
    }

    /* ------------------------------------------------------------- input */

    _push(type, a, b, c) {
        if (!this.ring) {
            return;
        }
        const head = Atomics.load(this.ring, this.ringBase + RING_HEAD);
        const tail = Atomics.load(this.ring, this.ringBase + RING_TAIL);

        /*
         * Drop rather than block. A full ring means the guest is not draining,
         * and stalling the UI thread on a wedged VM would freeze the page.
         * The drop is counted so a page can show it rather than silently
         * losing keystrokes.
         */
        if ((head - tail) >>> 0 >= this.ringCapacity) {
            Atomics.add(this.ring, this.ringBase + RING_DROPPED, 1);
            return;
        }

        const slot = this.ringBase + RING_HEADER_WORDS +
                     (head & (this.ringCapacity - 1)) * EVENT_WORDS;
        this.ring[slot + 0] = type;
        this.ring[slot + 1] = a;
        this.ring[slot + 2] = b;
        this.ring[slot + 3] = c;

        /* Publish the slot only after its contents are written. */
        Atomics.store(this.ring, this.ringBase + RING_HEAD, (head + 1) | 0);
    }

    _bindInput() {
        const canvas = this.canvas;
        canvas.tabIndex = 0;

        /*
         * Only claim the keyboard while the screen actually has focus.
         *
         * Bound to window and swallowing every recognised key, this handler
         * also ate keystrokes meant for the serial terminal underneath, so
         * pressing Enter in the terminal did nothing at all: the event was
         * preventDefault()ed before xterm could see it. The guest was fine; the
         * key never left the page. Focus is the only thing that can arbitrate
         * between two consoles sharing one window.
         */
        const hasFocus = () => document.activeElement === canvas;

        const key = (e, down) => {
            if (!hasFocus()) {
                return;
            }
            const qcode = this.keymap.get(e.code);
            if (qcode === undefined) {
                return;
            }
            e.preventDefault();
            if (down) {
                this.heldKeys.add(qcode);
            } else {
                this.heldKeys.delete(qcode);
            }
            this._push(EV_KEY, qcode, down ? 1 : 0, 0);
        };

        window.addEventListener('keydown', (e) => key(e, true));
        window.addEventListener('keyup', (e) => key(e, false));

        /*
         * The browser stops delivering keyup once the tab loses focus, so a
         * key held at that moment stays down in the guest forever. Release
         * everything on blur. Without this, alt-tabbing away from the page
         * leaves the guest convinced Alt is held, which presents as the
         * desktop behaving bizarrely afterwards.
         */
        /* Releasing on focus loss matters for the same reason as on window
         * blur: a key held when focus moves to the terminal would otherwise
         * stay down in the guest forever. */
        canvas.addEventListener('blur', () => {
            for (const qcode of this.heldKeys) {
                this._push(EV_KEY, qcode, 0, 0);
            }
            this.heldKeys.clear();
        });

        window.addEventListener('blur', () => {
            for (const qcode of this.heldKeys) {
                this._push(EV_KEY, qcode, 0, 0);
            }
            this.heldKeys.clear();
        });

        const sendPointer = (e) => {
            const r = canvas.getBoundingClientRect();
            if (r.width === 0 || r.height === 0) {
                return;
            }
            /* Absolute positioning, so the guest pointer tracks the host
             * pointer with no relative-motion accumulation and no need for
             * pointer lock. Requires a tablet-style device in the guest,
             * which virtio-tablet-pci provides. */
            const x = Math.round(((e.clientX - r.left) / r.width) * this.width);
            const y = Math.round(((e.clientY - r.top) / r.height) * this.height);
            this._push(EV_ABS,
                       Math.max(0, Math.min(this.width, x)),
                       Math.max(0, Math.min(this.height, y)),
                       ((this.width & 0xffff) << 16) | (this.height & 0xffff));
        };

        canvas.addEventListener('mousemove', sendPointer);

        const BUTTONS = [BTN_LEFT, BTN_MIDDLE, BTN_RIGHT];
        canvas.addEventListener('mousedown', (e) => {
            e.preventDefault();
            canvas.focus();
            sendPointer(e);
            if (e.button < BUTTONS.length) {
                this._push(EV_BTN, BUTTONS[e.button], 1, 0);
            }
        });
        window.addEventListener('mouseup', (e) => {
            if (e.button < BUTTONS.length) {
                this._push(EV_BTN, BUTTONS[e.button], 0, 0);
            }
        });
        canvas.addEventListener('contextmenu', (e) => e.preventDefault());

        canvas.addEventListener('wheel', (e) => {
            e.preventDefault();
            const btn = e.deltaY < 0 ? BTN_WHEEL_UP : BTN_WHEEL_DOWN;
            /* QEMU models a wheel notch as a press and immediate release. */
            this._push(EV_BTN, btn, 1, 0);
            this._push(EV_BTN, btn, 0, 0);
        }, { passive: false });
    }
}
