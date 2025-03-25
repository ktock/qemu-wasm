/*
 * SPDX-License-Identifier: GPL-2.0-or-later
 */
#ifndef TCG_WASM32_H
#define TCG_WASM32_H

struct wasmContext {
    CPUArchState *env;
    uint64_t *stack;
    void *tb_ptr;
    void *tci_tb_ptr;
    uint32_t do_init;
    void *stack128;
};

#define ENV_OFF 0
#define STACK_OFF 4
#define TB_PTR_OFF 8
#define HELPER_RET_TB_PTR_OFF 12
#define DO_INIT_OFF 16
#define STACK128_OFF 20

int get_core_nums(void);

/*
 * TB of wasm backend starts from a header which stores pointers for each data
 * stored in the following region of the TB.
 */
struct wasmTBHeader {
    void *tci_ptr;
    void *wasm_ptr;
    int wasm_size;
    void *import_ptr;
    int import_size;
    void *counter_ptr;
    void *info_ptr;
};

#endif
