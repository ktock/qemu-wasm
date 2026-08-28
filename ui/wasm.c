/*
 * QEMU browser-canvas display driver
 *
 * Renders the guest framebuffer into an HTML canvas and feeds browser
 * keyboard and pointer events back into QEMU's input layer, so that
 * qemu-system-* built with Emscripten can host a graphical guest rather
 * than only a serial console.
 *
 * Copyright (c) 2026 Eduardo Aguilar Pelaez
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL
 * THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
 * THE SOFTWARE.
 *
 *
 * THREADING
 * ---------
 * Built with -sPROXY_TO_PTHREAD, QEMU's main loop runs in a Web Worker while
 * DOM objects live on the browser's main thread. Two directions, two different
 * answers:
 *
 *   guest -> screen   The worker owns the pixels. It converts them once into a
 *                     scratch buffer and asks the main thread to blit, via
 *                     MAIN_THREAD_ASYNC_EM_ASM so the vCPU is never blocked on
 *                     the compositor. Scratch buffers are double-buffered
 *                     because "async" means the main thread may still be
 *                     reading buffer N while the worker fills N+1.
 *
 *   input -> guest    QEMU's input API must be called on the main-loop thread
 *                     under the BQL. Instead of locking across threads, browser
 *                     event handlers push into a single-producer /
 *                     single-consumer ring in shared linear memory, and
 *                     dpy_refresh drains it. dpy_refresh already runs on the
 *                     right thread holding the right lock, so the cross-thread
 *                     problem is removed rather than synchronised.
 */

#include "qemu/osdep.h"
#include "qemu/atomic.h"
#include "qemu/module.h"
#include "qemu/error-report.h"
#include "ui/console.h"
#include "ui/input.h"

#include <emscripten.h>

/* ------------------------------------------------------------------ input */

#define WASM_RING_LEN 512               /* must be a power of two */
#define WASM_RING_MASK (WASM_RING_LEN - 1)

enum {
    WASM_EV_KEY = 1,                    /* a = QKeyCode, b = down            */
    WASM_EV_BTN = 2,                    /* a = InputButton, b = down         */
    WASM_EV_ABS = 3,                    /* a = x, b = y, c = w<<16 | h       */
};

typedef struct WasmInputEvent {
    int32_t type;
    int32_t a;
    int32_t b;
    int32_t c;
} WasmInputEvent;

typedef struct WasmInputRing {
    uint32_t head;                      /* producer: browser main thread     */
    uint32_t tail;                      /* consumer: QEMU main loop thread   */
    uint32_t dropped;                   /* producer; diagnostics only        */
    uint32_t _pad;
    WasmInputEvent ev[WASM_RING_LEN];
} WasmInputRing;

static WasmInputRing wasm_ring;

/* ---------------------------------------------------------------- display */

typedef struct WasmDisplay {
    DisplayChangeListener dcl;
    QemuConsole *con;
    DisplaySurface *surface;

    /*
     * Double-buffered RGBA scratch handed to the main thread for blitting.
     * Only the damaged rectangle is written each frame, so a buffer holds a
     * stale full frame underneath. That is harmless: the browser side keeps
     * the composed image in its own backing canvas and only ever reads the
     * rectangle it was told about.
     */
    pixman_image_t *scratch[2];
    int cur;

    int width;
    int height;

    bool dirty;
    int dx, dy, dw, dh;                 /* accumulated damage rectangle      */

    uint64_t frames;
    uint64_t events_key;
    uint64_t events_btn;
    uint64_t events_abs;
} WasmDisplay;

static WasmDisplay *wasm_dpy;

/*
 * Hand a converted frame to the browser.
 *
 * ImageData cannot be constructed over a SharedArrayBuffer, so the main
 * thread copies out of the wasm heap with slice(). That copy is the price of
 * SharedArrayBuffer, not an oversight; it is one memcpy of w*h*4 and it is
 * dwarfed by the software rasterisation that produced the pixels.
 */
static void wasm_display_present_js(int ptr, int w, int h,
                                    int x, int y, int dw, int dh)
{
    MAIN_THREAD_ASYNC_EM_ASM({
        const api = globalThis.qemuWasmDisplay;
        if (api && api.present) {
            api.present($0, $1, $2, $3, $4, $5, $6);
        }
    }, ptr, w, h, x, y, dw, dh);
}

static void wasm_display_resize_js(int w, int h)
{
    MAIN_THREAD_ASYNC_EM_ASM({
        const api = globalThis.qemuWasmDisplay;
        if (api && api.resize) {
            api.resize($0, $1);
        }
    }, w, h);
}

/*
 * Exported so the page can wire its own event handlers without needing to
 * know QEMU's ABI: JS hands over the ring's address once and then writes
 * into it directly with Atomics.
 */
EMSCRIPTEN_KEEPALIVE
uintptr_t qemu_wasm_input_ring(void)
{
    return (uintptr_t)&wasm_ring;
}

EMSCRIPTEN_KEEPALIVE
uint32_t qemu_wasm_ring_capacity(void)
{
    return WASM_RING_LEN;
}

/* ------------------------------------------------------------- key map */

/*
 * KeyboardEvent.code is a stable, layout-independent physical-key name, which
 * makes it the right source: "KeyA" is the key where A sits on a US layout
 * regardless of what the user's layout produces, exactly like a scancode.
 *
 * The table lives here rather than in JavaScript so that QKeyCode values are
 * never written down as integers outside QEMU. The page fetches this mapping
 * at startup, so renumbering the enum upstream cannot silently desynchronise
 * the browser from the emulator.
 */
typedef struct WasmKeyMap {
    const char *code;
    QKeyCode qcode;
} WasmKeyMap;

static const WasmKeyMap wasm_keymap[] = {
    { "KeyA",          Q_KEY_CODE_A },

    { "KeyB",          Q_KEY_CODE_B },

    { "KeyC",          Q_KEY_CODE_C },

    { "KeyD",          Q_KEY_CODE_D },

    { "KeyE",          Q_KEY_CODE_E },

    { "KeyF",          Q_KEY_CODE_F },

    { "KeyG",          Q_KEY_CODE_G },

    { "KeyH",          Q_KEY_CODE_H },

    { "KeyI",          Q_KEY_CODE_I },

    { "KeyJ",          Q_KEY_CODE_J },

    { "KeyK",          Q_KEY_CODE_K },

    { "KeyL",          Q_KEY_CODE_L },

    { "KeyM",          Q_KEY_CODE_M },

    { "KeyN",          Q_KEY_CODE_N },

    { "KeyO",          Q_KEY_CODE_O },

    { "KeyP",          Q_KEY_CODE_P },

    { "KeyQ",          Q_KEY_CODE_Q },

    { "KeyR",          Q_KEY_CODE_R },

    { "KeyS",          Q_KEY_CODE_S },

    { "KeyT",          Q_KEY_CODE_T },

    { "KeyU",          Q_KEY_CODE_U },

    { "KeyV",          Q_KEY_CODE_V },

    { "KeyW",          Q_KEY_CODE_W },

    { "KeyX",          Q_KEY_CODE_X },

    { "KeyY",          Q_KEY_CODE_Y },

    { "KeyZ",          Q_KEY_CODE_Z },

    { "Digit1",        Q_KEY_CODE_1 },
    { "Digit2",        Q_KEY_CODE_2 },
    { "Digit3",        Q_KEY_CODE_3 },
    { "Digit4",        Q_KEY_CODE_4 },
    { "Digit5",        Q_KEY_CODE_5 },
    { "Digit6",        Q_KEY_CODE_6 },
    { "Digit7",        Q_KEY_CODE_7 },
    { "Digit8",        Q_KEY_CODE_8 },
    { "Digit9",        Q_KEY_CODE_9 },
    { "Digit0",        Q_KEY_CODE_0 },

    { "Enter",         Q_KEY_CODE_RET },

    { "Escape",        Q_KEY_CODE_ESC },

    { "Backspace",     Q_KEY_CODE_BACKSPACE },

    { "Tab",           Q_KEY_CODE_TAB },

    { "Space",         Q_KEY_CODE_SPC },

    { "Minus",         Q_KEY_CODE_MINUS },

    { "Equal",         Q_KEY_CODE_EQUAL },

    { "BracketLeft",   Q_KEY_CODE_BRACKET_LEFT },

    { "BracketRight",  Q_KEY_CODE_BRACKET_RIGHT },

    { "Backslash",     Q_KEY_CODE_BACKSLASH },

    { "Semicolon",     Q_KEY_CODE_SEMICOLON },

    { "Quote",         Q_KEY_CODE_APOSTROPHE },

    { "Backquote",     Q_KEY_CODE_GRAVE_ACCENT },

    { "Comma",         Q_KEY_CODE_COMMA },

    { "Period",        Q_KEY_CODE_DOT },

    { "Slash",         Q_KEY_CODE_SLASH },

    { "CapsLock",      Q_KEY_CODE_CAPS_LOCK },

    { "IntlBackslash", Q_KEY_CODE_LESS },

    { "ControlLeft",   Q_KEY_CODE_CTRL },

    { "ControlRight",  Q_KEY_CODE_CTRL_R },

    { "ShiftLeft",     Q_KEY_CODE_SHIFT },

    { "ShiftRight",    Q_KEY_CODE_SHIFT_R },

    { "AltLeft",       Q_KEY_CODE_ALT },

    { "AltRight",      Q_KEY_CODE_ALT_R },

    { "MetaLeft",      Q_KEY_CODE_META_L },

    { "MetaRight",     Q_KEY_CODE_META_R },

    { "ContextMenu",   Q_KEY_CODE_MENU },

    { "F1",            Q_KEY_CODE_F1 },
    { "F2",            Q_KEY_CODE_F2 },
    { "F3",            Q_KEY_CODE_F3 },
    { "F4",            Q_KEY_CODE_F4 },
    { "F5",            Q_KEY_CODE_F5 },
    { "F6",            Q_KEY_CODE_F6 },
    { "F7",            Q_KEY_CODE_F7 },
    { "F8",            Q_KEY_CODE_F8 },
    { "F9",            Q_KEY_CODE_F9 },
    { "F10",           Q_KEY_CODE_F10 },
    { "F11",           Q_KEY_CODE_F11 },
    { "F12",           Q_KEY_CODE_F12 },

    { "PrintScreen",   Q_KEY_CODE_PRINT },

    { "ScrollLock",    Q_KEY_CODE_SCROLL_LOCK },

    { "Pause",         Q_KEY_CODE_PAUSE },

    { "Insert",        Q_KEY_CODE_INSERT },

    { "Home",          Q_KEY_CODE_HOME },

    { "PageUp",        Q_KEY_CODE_PGUP },

    { "Delete",        Q_KEY_CODE_DELETE },

    { "End",           Q_KEY_CODE_END },

    { "PageDown",      Q_KEY_CODE_PGDN },

    { "ArrowRight",    Q_KEY_CODE_RIGHT },

    { "ArrowLeft",     Q_KEY_CODE_LEFT },

    { "ArrowDown",     Q_KEY_CODE_DOWN },

    { "ArrowUp",       Q_KEY_CODE_UP },

    { "NumLock",       Q_KEY_CODE_NUM_LOCK },

    { "NumpadDivide",  Q_KEY_CODE_KP_DIVIDE },

    { "NumpadMultiply", Q_KEY_CODE_KP_MULTIPLY },

    { "NumpadSubtract", Q_KEY_CODE_KP_SUBTRACT },

    { "NumpadAdd",     Q_KEY_CODE_KP_ADD },

    { "NumpadEnter",   Q_KEY_CODE_KP_ENTER },

    { "NumpadDecimal", Q_KEY_CODE_KP_DECIMAL },

    { "NumpadComma",   Q_KEY_CODE_KP_COMMA },

    { "NumpadEqual",   Q_KEY_CODE_KP_EQUALS },

    { "Numpad0",       Q_KEY_CODE_KP_0 },
    { "Numpad1",       Q_KEY_CODE_KP_1 },
    { "Numpad2",       Q_KEY_CODE_KP_2 },
    { "Numpad3",       Q_KEY_CODE_KP_3 },
    { "Numpad4",       Q_KEY_CODE_KP_4 },
    { "Numpad5",       Q_KEY_CODE_KP_5 },
    { "Numpad6",       Q_KEY_CODE_KP_6 },
    { "Numpad7",       Q_KEY_CODE_KP_7 },
    { "Numpad8",       Q_KEY_CODE_KP_8 },
    { "Numpad9",       Q_KEY_CODE_KP_9 },
};

/*
 * Returned as JSON so the page needs no knowledge of QEMU's ABI beyond
 * "call this once".
 *
 * The result is built once and cached for the process lifetime, and the caller
 * must NOT free it. That deliberately avoids having to export free() to
 * JavaScript for the sake of one allocation of a few kilobytes made once at
 * startup: an exported free() is a foot-gun for every other pointer the page
 * ever sees, which is a worse trade than this leak.
 */
EMSCRIPTEN_KEEPALIVE
const char *qemu_wasm_keymap_json(void)
{
    static char *cached;

    if (!cached) {
        GString *s = g_string_new("{");

        for (size_t i = 0; i < ARRAY_SIZE(wasm_keymap); i++) {
            g_string_append_printf(s, "%s\"%s\":%d",
                                   i ? "," : "",
                                   wasm_keymap[i].code,
                                   (int)wasm_keymap[i].qcode);
        }
        g_string_append_c(s, '}');
        cached = g_string_free(s, false);
    }

    return cached;
}

/* Frame and event counters for the page's own FPS readout. */
EMSCRIPTEN_KEEPALIVE
double qemu_wasm_frame_count(void)
{
    return wasm_dpy ? (double)wasm_dpy->frames : 0;
}

/*
 * Per-kind counters, exported so a test can prove each input path reaches
 * QEMU's input layer rather than only that "something happened". A pointer
 * path that is never exercised is a pointer path that does not work, and
 * counting keys alone cannot tell the difference.
 */
EMSCRIPTEN_KEEPALIVE
double qemu_wasm_events_key(void)
{
    return wasm_dpy ? (double)wasm_dpy->events_key : 0;
}

EMSCRIPTEN_KEEPALIVE
double qemu_wasm_events_btn(void)
{
    return wasm_dpy ? (double)wasm_dpy->events_btn : 0;
}

EMSCRIPTEN_KEEPALIVE
double qemu_wasm_events_abs(void)
{
    return wasm_dpy ? (double)wasm_dpy->events_abs : 0;
}

EMSCRIPTEN_KEEPALIVE
double qemu_wasm_dropped_input(void)
{
    return (double)qatomic_read(&wasm_ring.dropped);
}

/* ------------------------------------------------------------ ring drain */

static void wasm_drain_input(WasmDisplay *wd)
{
    uint32_t head = qatomic_read(&wasm_ring.head);
    uint32_t tail = wasm_ring.tail;
    bool sync_needed = false;

    while (tail != head) {
        WasmInputEvent *e = &wasm_ring.ev[tail & WASM_RING_MASK];

        switch (e->type) {
        case WASM_EV_KEY:
            /*
             * The browser gives no key-repeat suppression and no guarantee of
             * a matching keyup when focus is lost. qkbd_state is not used here
             * because the guest's own repeat handling is what a user expects;
             * stuck keys on blur are handled JS-side by releasing everything.
             */
            qemu_input_event_send_key_qcode(wd->con, (QKeyCode)e->a, !!e->b);
            wd->events_key++;
            break;

        case WASM_EV_BTN:
            qemu_input_queue_btn(wd->con, (InputButton)e->a, !!e->b);
            wd->events_btn++;
            sync_needed = true;
            break;

        case WASM_EV_ABS: {
            int w = (e->c >> 16) & 0xffff;
            int h = e->c & 0xffff;
            if (w > 0 && h > 0) {
                qemu_input_queue_abs(wd->con, INPUT_AXIS_X, e->a, 0, w);
                qemu_input_queue_abs(wd->con, INPUT_AXIS_Y, e->b, 0, h);
                wd->events_abs++;
                sync_needed = true;
            }
            break;
        }

        default:
            /* Unknown opcode: skip it rather than desynchronising the ring. */
            break;
        }

        tail++;
    }

    /* Publish the new tail only once the events have actually been consumed. */
    qatomic_set(&wasm_ring.tail, tail);

    if (sync_needed) {
        qemu_input_event_sync();
    }
}

/* --------------------------------------------------------------- pixels */

static void wasm_free_scratch(WasmDisplay *wd)
{
    for (int i = 0; i < 2; i++) {
        if (wd->scratch[i]) {
            pixman_image_unref(wd->scratch[i]);
            wd->scratch[i] = NULL;
        }
    }
}

static bool wasm_alloc_scratch(WasmDisplay *wd, int w, int h)
{
    wasm_free_scratch(wd);

    for (int i = 0; i < 2; i++) {
        /*
         * PIXMAN_a8b8g8r8 is a<<24 | b<<16 | g<<8 | r, which on a
         * little-endian target is the byte order R,G,B,A that ImageData
         * wants. Getting this wrong yields a picture with correct geometry
         * and swapped red and blue, which is easy to misread as a guest
         * problem rather than a display-backend one.
         */
        wd->scratch[i] = pixman_image_create_bits(PIXMAN_a8b8g8r8, w, h,
                                                  NULL, 0);
        if (!wd->scratch[i]) {
            wasm_free_scratch(wd);
            return false;
        }
    }
    return true;
}

/*
 * Convert one damaged rectangle from the guest surface into the RGBA scratch.
 *
 * Written out by hand rather than delegated to pixman_image_composite()
 * because QEMU can be configured without pixman (--without-default-features
 * does exactly that), in which case include/ui/pixman-minimal.h supplies the
 * types and create_bits but no compositing at all. Depending on full pixman
 * here would make this backend unbuildable in precisely the minimal
 * configuration a browser build wants. The loop only ever touches the damaged
 * rectangle, and this build has no SIMD enabled, so the cost of doing it here
 * is close to what a library call would have been.
 */
static void wasm_convert_rect(WasmDisplay *wd, int x, int y, int w, int h)
{
    DisplaySurface *s = wd->surface;
    const uint8_t *src = (const uint8_t *)surface_data(s);
    int src_stride = surface_stride(s);
    uint8_t *dst = (uint8_t *)pixman_image_get_data(wd->scratch[wd->cur]);
    int dst_stride = wd->width * 4;
    pixman_format_code_t fmt = surface_format(s);

    /* Byte offsets of each channel within a 32bpp little-endian pixel. */
    int r_off, g_off, b_off;

    switch (fmt) {
    case PIXMAN_x8r8g8b8:
    case PIXMAN_a8r8g8b8:
        b_off = 0; g_off = 1; r_off = 2;
        break;
    case PIXMAN_x8b8g8r8:
    case PIXMAN_a8b8g8r8:
        r_off = 0; g_off = 1; b_off = 2;
        break;
    case PIXMAN_b8g8r8x8:
    case PIXMAN_b8g8r8a8:
        b_off = 3; g_off = 2; r_off = 1;
        break;
    case PIXMAN_r8g8b8x8:
    case PIXMAN_r8g8b8a8:
        r_off = 3; g_off = 2; b_off = 1;
        break;
    default:
        /* Refused in dpy_gfx_check_format, so reaching here is a bug. */
        return;
    }

    for (int row = 0; row < h; row++) {
        const uint8_t *sp = src + (size_t)(y + row) * src_stride
                                + (size_t)x * 4;
        uint8_t *dp = dst + (size_t)(y + row) * dst_stride + (size_t)x * 4;

        for (int col = 0; col < w; col++) {
            dp[0] = sp[r_off];
            dp[1] = sp[g_off];
            dp[2] = sp[b_off];
            /*
             * Force opaque: an x-format source carries no meaningful alpha,
             * and a canvas will happily render a fully transparent guest.
             */
            dp[3] = 0xff;
            sp += 4;
            dp += 4;
        }
    }
}

static void wasm_present(WasmDisplay *wd)
{
    if (!wd->surface || !wd->dirty || !wd->scratch[0]) {
        return;
    }

    /*
     * Clamp the damage to the surface: a device model that reports a rectangle
     * larger than its own surface would otherwise walk off the end.
     */
    int x = MAX(0, MIN(wd->dx, wd->width));
    int y = MAX(0, MIN(wd->dy, wd->height));
    int w = MIN(wd->dw, wd->width - x);
    int h = MIN(wd->dh, wd->height - y);

    if (w <= 0 || h <= 0) {
        wd->dirty = false;
        return;
    }

    wasm_convert_rect(wd, x, y, w, h);

    void *pixels = pixman_image_get_data(wd->scratch[wd->cur]);

    wasm_display_present_js((int)(uintptr_t)pixels,
                            wd->width, wd->height, x, y, w, h);

    wd->cur ^= 1;
    wd->dirty = false;
    wd->frames++;
}

/* ------------------------------------------------------- dcl callbacks */

static void wasm_gfx_update(DisplayChangeListener *dcl,
                            int x, int y, int w, int h)
{
    WasmDisplay *wd = container_of(dcl, WasmDisplay, dcl);

    if (!wd->dirty) {
        wd->dx = x;
        wd->dy = y;
        wd->dw = w;
        wd->dh = h;
        wd->dirty = true;
        return;
    }

    /*
     * Union with the pending damage; presenting once per refresh beats
     * presenting once per update region, because each present costs a
     * cross-thread hop and a copy.
     */
    int x1 = MIN(wd->dx, x);
    int y1 = MIN(wd->dy, y);
    int x2 = MAX(wd->dx + wd->dw, x + w);
    int y2 = MAX(wd->dy + wd->dh, y + h);

    wd->dx = x1;
    wd->dy = y1;
    wd->dw = x2 - x1;
    wd->dh = y2 - y1;
}

static void wasm_gfx_switch(DisplayChangeListener *dcl,
                            DisplaySurface *new_surface)
{
    WasmDisplay *wd = container_of(dcl, WasmDisplay, dcl);

    wd->surface = new_surface;
    wd->dirty = false;

    if (!new_surface) {
        return;
    }

    int w = surface_width(new_surface);
    int h = surface_height(new_surface);

    if (w == wd->width && h == wd->height && wd->scratch[0]) {
        return;
    }

    if (!wasm_alloc_scratch(wd, w, h)) {
        error_report("wasm display: cannot allocate a %dx%d scratch surface",
                     w, h);
        wd->width = wd->height = 0;
        return;
    }

    wd->width = w;
    wd->height = h;
    wasm_display_resize_js(w, h);

    /* A mode switch invalidates everything, so damage the whole surface. */
    wasm_gfx_update(dcl, 0, 0, w, h);
}

static void wasm_refresh(DisplayChangeListener *dcl)
{
    WasmDisplay *wd = container_of(dcl, WasmDisplay, dcl);

    graphic_hw_update(wd->con);
    wasm_drain_input(wd);
    wasm_present(wd);
}

static bool wasm_check_format(DisplayChangeListener *dcl,
                              pixman_format_code_t format)
{
    /*
     * Only the 32bpp layouts wasm_convert_rect() knows how to unpack. Saying
     * no to the rest is what makes the console layer hand us a converted
     * surface instead, which is both correct and less code than handling
     * 16bpp and paletted modes here.
     */
    switch (format) {
    case PIXMAN_x8r8g8b8:
    case PIXMAN_a8r8g8b8:
    case PIXMAN_x8b8g8r8:
    case PIXMAN_a8b8g8r8:
    case PIXMAN_b8g8r8x8:
    case PIXMAN_b8g8r8a8:
    case PIXMAN_r8g8b8x8:
    case PIXMAN_r8g8b8a8:
        return true;
    default:
        return false;
    }
}

static const DisplayChangeListenerOps wasm_dcl_ops = {
    .dpy_name             = "wasm",
    .dpy_refresh          = wasm_refresh,
    .dpy_gfx_update       = wasm_gfx_update,
    .dpy_gfx_switch       = wasm_gfx_switch,
    .dpy_gfx_check_format = wasm_check_format,
};

/* ----------------------------------------------------------- init/register */

static void wasm_display_init(DisplayState *ds, DisplayOptions *o)
{
    QemuConsole *con = qemu_console_lookup_by_index(0);

    if (!con || !qemu_console_is_graphic(con)) {
        error_report("wasm display: no graphic console; "
                     "add a display device such as -device virtio-gpu-pci");
        exit(1);
    }

    wasm_dpy = g_new0(WasmDisplay, 1);
    wasm_dpy->con = con;
    wasm_dpy->dcl.ops = &wasm_dcl_ops;
    wasm_dpy->dcl.con = con;

    register_displaychangelistener(&wasm_dpy->dcl);
}

static QemuDisplay qemu_display_wasm = {
    .type = DISPLAY_TYPE_WASM,
    .init = wasm_display_init,
};

static void register_wasm(void)
{
    qemu_display_register(&qemu_display_wasm);
}

type_init(register_wasm);
