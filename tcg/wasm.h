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
};

#endif
