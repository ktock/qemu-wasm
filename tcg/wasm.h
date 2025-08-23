/* SPDX-License-Identifier: GPL-2.0-or-later */
#ifndef TCG_WASM_H
#define TCG_WASM_H

/*
 * WasmContext is a data shared among QEMU and wasm modules.
 */
struct WasmContext {
    /*
     * Pointer to the TB to be executed.
     */
    void *tb_ptr;

    /*
     * Pointer to the tci_tb_ptr variable.
     */
    void *tci_tb_ptr;

    /*
     * Buffer to store 128bit return value on call.
     */
    void *buf128;

    /*
     * Pointer to the CPUArchState struct.
     */
    CPUArchState *env;

    /*
     * Pointer to a stack array.
     */
    uint64_t *stack;

    /*
     * Flag indicating whether to initialize the block index(1) or not(0).
     */
    uint32_t do_init;
};

/* Instantiated Wasm function of a TB */
typedef uintptr_t (*wasm_tb_func)(struct WasmContext *);

static inline uintptr_t call_wasm_tb(wasm_tb_func f, struct WasmContext *ctx)
{
    ctx->do_init = 1; /* reset the block index (rewinding will skip this) */
    return f(ctx);
}

/*
 * A TB of the Wasm backend starts from a header which contains pointers for
 * each data stored in the following region in the TB.
 */
struct WasmTBHeader {
    /*
     * Pointer to the region containing TCI instructions.
     */
    void *tci_ptr;

    /*
     * Pointer to the region containing Wasm instructions.
     */
    void *wasm_ptr;
    int wasm_size;

    /*
     * Pointer to the array containing imported function pointers.
     */
    void *import_ptr;
    int import_size;
};

#endif
