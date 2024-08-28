#if !defined(CONFIG_TCG_INTERPRETER) && defined(EMSCRIPTEN)

#include "qemu/osdep.h"
#include "exec/cpu_ldst.h"
#include "tcg/tcg-op.h"
#include "tcg/tcg-ldst.h"
#include <string.h>
#include <emscripten.h>
#include <emscripten/threading.h>
#include "wasm32.h"

__thread uintptr_t tci_tb_ptr;

/* Disassemble TCI bytecode. */
int print_insn_tci(bfd_vma addr, disassemble_info *info)
{
    return 0; //nop
}

EM_JS(int, instantiate_wasm, (int cur_core_num, int all_cores_num, int wasm_body_begin, int wasm_body_size, int wasm_header_begin, int wasm_header_size, int import_vec_begin, int import_vec_size, int export_vec_begin, int export_vec_size, int *to_remove_ptr, int *to_remove_num), {
        const memory_v = new DataView(HEAP8.buffer);
        var wasm = new Uint8Array(wasm_header_size + wasm_body_size);
        wasm.set(HEAP8.subarray(wasm_header_begin, wasm_header_begin + wasm_header_size));
        wasm.set(HEAP8.subarray(wasm_body_begin, wasm_body_begin + wasm_body_size), wasm_header_size);
        const mod = new WebAssembly.Module(wasm);
        var helper = {};
        for (var i = 0; i < import_vec_size / 4; i++) {
            helper[i] = wasmTable.get(memory_v.getInt32(import_vec_begin + i * 4, true));
        }
        const inst = new WebAssembly.Instance(mod, {
               "env": {
                   "buffer": wasmMemory,
               },
               "helper": helper,
        });
        var ptr = export_vec_begin + 4 * cur_core_num;
        const fidx = addFunction(inst.exports.start, 'ii');
        memory_v.setUint32(ptr, fidx, true);
        ptr += 4 * all_cores_num;

        const remove_n = memory_v.getInt32(to_remove_num, true);
        if (remove_n > 500) {
           for (var i = 0; i < remove_n * 4; i += 4) {
               removeFunction(memory_v.getInt32(to_remove_ptr + i, true));
           }
           memory_v.setInt32(to_remove_num, 0, true);
        }

        return fidx;
});

#define TO_REMOVE_INSTANCE_SIZE 50000
__thread static int to_remove_instance[TO_REMOVE_INSTANCE_SIZE];
__thread static int to_remove_instance_idx = 0;

#define ACTIVE_TBS_MAX 60000
#define REMOVE_TBS_NUM 10000
__thread static int active_tbs[ACTIVE_TBS_MAX];
__thread static int active_tbs_begin = 0;
__thread static int active_tbs_end = 0;
__thread static int active_tbs_lim;
__thread static int active_tbs_rmv;

static int active_tbs_len()
{
    if (active_tbs_begin == active_tbs_end) {
        return 0;
    }
    if (active_tbs_begin < active_tbs_end) {
        return active_tbs_end - active_tbs_begin;
    }
    if (active_tbs_begin > active_tbs_end) {
        return (active_tbs_lim - active_tbs_begin) + active_tbs_end;
    }
    g_assert_not_reached();
}

static void prepare_wasm(void *tb_ptr, int cur_core_num, int all_cores_num)
{
    uint32_t code_size = *(uint32_t*)((uint32_t)tb_ptr + 4);
    uint32_t code_begin = (uint32_t)tb_ptr + 4 + 4;
    uint32_t header_size = *(uint32_t*)(code_begin + code_size);
    uint32_t header_begin = code_begin + code_size + 4;
    uint32_t import_vec_size = *(uint32_t*)(header_begin + header_size);
    uint32_t import_vec_begin = header_begin + header_size + 4;
    uint32_t export_vec_size = *(uint32_t*)(import_vec_begin + import_vec_size);
    uint32_t export_vec_begin = import_vec_begin + import_vec_size + 4;

    if (active_tbs_len() == (active_tbs_lim-1)) {
        int idx = active_tbs_begin;
        for (int i = 0; i < active_tbs_rmv; i++) {
            uint8_t *p = (uint8_t*)active_tbs[idx];
            int f = *(uint32_t*)((uint8_t*)p + *(uint32_t*)p + cur_core_num * 4);
            *(uint32_t*)((uint8_t*)p + *(uint32_t*)p + cur_core_num * 4) = 0;
            to_remove_instance[to_remove_instance_idx++] = f;
            idx++;
            if (idx >= active_tbs_lim) {
                idx = 0;
            }
        }
        active_tbs_begin = idx;
    }
    
    int fidx = instantiate_wasm(cur_core_num, all_cores_num, code_begin, code_size, header_begin, header_size, import_vec_begin, import_vec_size, export_vec_begin, export_vec_size, to_remove_instance, &to_remove_instance_idx);
    active_tbs[active_tbs_end++] = (int)tb_ptr;
    if (active_tbs_end >= active_tbs_lim) {
        active_tbs_end = 0;
    }
}

__thread struct wasmContext ctx = {
    .tb_ptr = 0,
    .stack = NULL,
    /* .func_ptr = 0, */
    /* .next_func_ptr = 0, */
    .do_init = 1,
    .stack128 = NULL,
};

void set_done_flag()
{
    ctx.done_flag = 1;
}

void set_unwinding_flag()
{
    ctx.unwinding = 1;
}

typedef uint32_t (*wasm_func_ptr)(struct wasmContext*);

__thread bool initdone = false;
__thread int cur_core_num = -1;
__thread int all_cores_num = -1;
int cur_core_num_max = 0;

int get_core_nums()
{
    return emscripten_num_logical_cores();
}

extern unsigned int tcg_max_ctxs;

uintptr_t QEMU_DISABLE_CFI tcg_qemu_tb_exec(CPUArchState *env, const void *v_tb_ptr)
{
    if (!initdone) {
        active_tbs_lim = ACTIVE_TBS_MAX / tcg_max_ctxs;
        active_tbs_rmv = REMOVE_TBS_NUM / tcg_max_ctxs;
        cur_core_num = qatomic_fetch_inc(&cur_core_num_max);
        all_cores_num = get_core_nums();
        ctx.stack = (uint64_t*)malloc((TCG_STATIC_CALL_ARGS_SIZE + TCG_STATIC_FRAME_SIZE) / sizeof(uint64_t));
        ctx.stack128 = (uint64_t*)malloc((TCG_STATIC_CALL_ARGS_SIZE + TCG_STATIC_FRAME_SIZE) / sizeof(uint64_t));
        initdone = true;
    }
    ctx.env = env;
    ctx.tb_ptr = (uint32_t*)v_tb_ptr;
    ctx.do_init = 1;
    ctx.tci_tb_ptr = (uint32_t*)&tci_tb_ptr;
    uint32_t prev_tb_ptr = 0;
    while (true) {
        int tb_entry_ptr = (uint32_t)ctx.tb_ptr + *(uint32_t*)ctx.tb_ptr + cur_core_num * 4;
        if (*(uint32_t*)tb_entry_ptr == 0) {
            prepare_wasm(ctx.tb_ptr, cur_core_num, all_cores_num);
        }
        uint32_t res = ((wasm_func_ptr)(*(uint32_t*)tb_entry_ptr))(&ctx);
        if ((uint32_t)ctx.tb_ptr == 0) {
            return res;
        }
    }
}

#endif
