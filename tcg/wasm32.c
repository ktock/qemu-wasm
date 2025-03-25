/*
 * Tiny Code Generator for QEMU
 *
 * Wasm integration + ported TCI interpreter from tci.c
 *
 * Copyright (c) 2009, 2011, 2016 Stefan Weil
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

/*
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#include "qemu/osdep.h"
#include <string.h>
#include <emscripten.h>
#include <emscripten/threading.h>
#include "exec/cpu_ldst.h"
#include "tcg/tcg-op.h"
#include "tcg/tcg.h"
#include "tcg/helper-info.h"
#include "tcg/tcg-ldst.h"
#include "disas/dis-asm.h"
#include "tcg-has.h"
#include "wasm32.h"

/* TBs executed more than this value will be compiled to wasm */
#define INSTANTIATE_NUM 1500

__thread uintptr_t tci_tb_ptr;

/* Disassemble TCI bytecode. */
int print_insn_tci(bfd_vma addr, disassemble_info *info)
{
    return 0; /* nop */
}

EM_JS(int, instantiate_wasm, (int wasm_begin,
                              int wasm_size,
                              int import_vec_begin,
                              int import_vec_size),
{
    const memory_v = new DataView(HEAP8.buffer);
    const wasm = HEAP8.subarray(wasm_begin, wasm_begin + wasm_size);
    var helper = {};
    helper.u = () => {
        return (Asyncify.state != Asyncify.State.Unwinding) ? 1 : 0;
    };
    for (var i = 0; i < import_vec_size / 4; i++) {
        helper[i] = wasmTable.get(
            memory_v.getInt32(import_vec_begin + i * 4, true));
    }
    const mod = new WebAssembly.Module(wasm);
    const inst = new WebAssembly.Instance(mod, {
            "env" : {
                "buffer" : wasmMemory,
            },
            "helper" : helper,
    });

    Module.__wasm32_tb.inst_gc_registry.register(inst, "instance");

    return addFunction(inst.exports.start, 'ii');
});

__thread int cur_core_num;

static inline int32_t *get_counter_ptr(void *tb_ptr)
{
    return (int32_t *)(((struct wasmTBHeader *)tb_ptr)->counter_ptr
                       + cur_core_num * 4);
}

static inline uint32_t *get_info_ptr(void *tb_ptr)
{
    return (uint32_t *)(((struct wasmTBHeader *)tb_ptr)->info_ptr
                        + cur_core_num * 4);
}

static inline uint32_t *get_tci_ptr(void *tb_ptr)
{
    return (uint32_t *)(((struct wasmTBHeader *)tb_ptr)->tci_ptr);
}

__thread struct wasmContext ctx = {
    .tb_ptr = 0,
    .stack = NULL,
    .do_init = 1,
    .stack128 = NULL,
};

static void tci_write_reg64(tcg_target_ulong *regs, uint32_t high_index,
                            uint32_t low_index, uint64_t value)
{
    regs[low_index] = (uint32_t)value;
    regs[high_index] = value >> 32;
}

/* Create a 64 bit value from two 32 bit values. */
static uint64_t tci_uint64(uint32_t high, uint32_t low)
{
    return ((uint64_t)high << 32) + low;
}

static void tci_args_ldst(uint32_t insn,
                          TCGReg *r0,
                          TCGReg *r1,
                          MemOpIdx *m2,
                          const void *tb_ptr,
                          void **l0)
{
    int diff = sextract32(insn, 12, 20);
    *l0 = diff ? (uint8_t *)tb_ptr + diff : NULL;

    uint64_t *data64 = (uint64_t *)*l0;
    *r0 = (TCGReg)data64[0];
    *r1 = (TCGReg)data64[1];
    *m2 = (MemOpIdx)data64[2];
}

/*
 * Load sets of arguments all at once.  The naming convention is:
 *   tci_args_<arguments>
 * where arguments is a sequence of
 *
 *   b = immediate (bit position)
 *   c = condition (TCGCond)
 *   i = immediate (uint32_t)
 *   I = immediate (tcg_target_ulong)
 *   l = label or pointer
 *   m = immediate (MemOpIdx)
 *   n = immediate (call return length)
 *   r = register
 *   s = signed ldst offset
 */

static void tci_args_l(uint32_t insn, const void *tb_ptr, void **l0)
{
    int diff = sextract32(insn, 12, 20);
    *l0 = diff ? (uint8_t *)tb_ptr + diff : NULL;
}

static void tci_args_r(uint32_t insn, TCGReg *r0)
{
    *r0 = extract32(insn, 8, 4);
}

static void tci_args_nl(uint32_t insn, const void *tb_ptr,
                        uint8_t *n0, void **l1)
{
    *n0 = extract32(insn, 8, 4);
    *l1 = sextract32(insn, 12, 20) + (void *)tb_ptr;
}

static void tci_args_rl(uint32_t insn, const void *tb_ptr,
                        TCGReg *r0, void **l1)
{
    *r0 = extract32(insn, 8, 4);
    *l1 = sextract32(insn, 12, 20) + (void *)tb_ptr;
}

static void tci_args_rr(uint32_t insn, TCGReg *r0, TCGReg *r1)
{
    *r0 = extract32(insn, 8, 4);
    *r1 = extract32(insn, 12, 4);
}

static void tci_args_ri(uint32_t insn, TCGReg *r0, tcg_target_ulong *i1)
{
    *r0 = extract32(insn, 8, 4);
    *i1 = sextract32(insn, 12, 20);
}

static void tci_args_rrr(uint32_t insn, TCGReg *r0, TCGReg *r1, TCGReg *r2)
{
    *r0 = extract32(insn, 8, 4);
    *r1 = extract32(insn, 12, 4);
    *r2 = extract32(insn, 16, 4);
}

static void tci_args_rrs(uint32_t insn, TCGReg *r0, TCGReg *r1, int32_t *i2)
{
    *r0 = extract32(insn, 8, 4);
    *r1 = extract32(insn, 12, 4);
    *i2 = sextract32(insn, 16, 16);
}

static void tci_args_rrbb(uint32_t insn, TCGReg *r0, TCGReg *r1,
                          uint8_t *i2, uint8_t *i3)
{
    *r0 = extract32(insn, 8, 4);
    *r1 = extract32(insn, 12, 4);
    *i2 = extract32(insn, 16, 6);
    *i3 = extract32(insn, 22, 6);
}

static void tci_args_rrrc(uint32_t insn,
                          TCGReg *r0, TCGReg *r1, TCGReg *r2, TCGCond *c3)
{
    *r0 = extract32(insn, 8, 4);
    *r1 = extract32(insn, 12, 4);
    *r2 = extract32(insn, 16, 4);
    *c3 = extract32(insn, 20, 4);
}

static void tci_args_rrrbb(uint32_t insn, TCGReg *r0, TCGReg *r1,
                           TCGReg *r2, uint8_t *i3, uint8_t *i4)
{
    *r0 = extract32(insn, 8, 4);
    *r1 = extract32(insn, 12, 4);
    *r2 = extract32(insn, 16, 4);
    *i3 = extract32(insn, 20, 6);
    *i4 = extract32(insn, 26, 6);
}

static void tci_args_rrrr(uint32_t insn,
                          TCGReg *r0, TCGReg *r1, TCGReg *r2, TCGReg *r3)
{
    *r0 = extract32(insn, 8, 4);
    *r1 = extract32(insn, 12, 4);
    *r2 = extract32(insn, 16, 4);
    *r3 = extract32(insn, 20, 4);
}

static void tci_args_rrrrrc(uint32_t insn, TCGReg *r0, TCGReg *r1,
                            TCGReg *r2, TCGReg *r3, TCGReg *r4, TCGCond *c5)
{
    *r0 = extract32(insn, 8, 4);
    *r1 = extract32(insn, 12, 4);
    *r2 = extract32(insn, 16, 4);
    *r3 = extract32(insn, 20, 4);
    *r4 = extract32(insn, 24, 4);
    *c5 = extract32(insn, 28, 4);
}

static void tci_args_rrrrrr(uint32_t insn, TCGReg *r0, TCGReg *r1,
                            TCGReg *r2, TCGReg *r3, TCGReg *r4, TCGReg *r5)
{
    *r0 = extract32(insn, 8, 4);
    *r1 = extract32(insn, 12, 4);
    *r2 = extract32(insn, 16, 4);
    *r3 = extract32(insn, 20, 4);
    *r4 = extract32(insn, 24, 4);
    *r5 = extract32(insn, 28, 4);
}

static bool tci_compare32(uint32_t u0, uint32_t u1, TCGCond condition)
{
    bool result = false;
    int32_t i0 = u0;
    int32_t i1 = u1;
    switch (condition) {
    case TCG_COND_EQ:
        result = (u0 == u1);
        break;
    case TCG_COND_NE:
        result = (u0 != u1);
        break;
    case TCG_COND_LT:
        result = (i0 < i1);
        break;
    case TCG_COND_GE:
        result = (i0 >= i1);
        break;
    case TCG_COND_LE:
        result = (i0 <= i1);
        break;
    case TCG_COND_GT:
        result = (i0 > i1);
        break;
    case TCG_COND_LTU:
        result = (u0 < u1);
        break;
    case TCG_COND_GEU:
        result = (u0 >= u1);
        break;
    case TCG_COND_LEU:
        result = (u0 <= u1);
        break;
    case TCG_COND_GTU:
        result = (u0 > u1);
        break;
    case TCG_COND_TSTEQ:
        result = (u0 & u1) == 0;
        break;
    case TCG_COND_TSTNE:
        result = (u0 & u1) != 0;
        break;
    default:
        g_assert_not_reached();
    }
    return result;
}

static bool tci_compare64(uint64_t u0, uint64_t u1, TCGCond condition)
{
    bool result = false;
    int64_t i0 = u0;
    int64_t i1 = u1;
    switch (condition) {
    case TCG_COND_EQ:
        result = (u0 == u1);
        break;
    case TCG_COND_NE:
        result = (u0 != u1);
        break;
    case TCG_COND_LT:
        result = (i0 < i1);
        break;
    case TCG_COND_GE:
        result = (i0 >= i1);
        break;
    case TCG_COND_LE:
        result = (i0 <= i1);
        break;
    case TCG_COND_GT:
        result = (i0 > i1);
        break;
    case TCG_COND_LTU:
        result = (u0 < u1);
        break;
    case TCG_COND_GEU:
        result = (u0 >= u1);
        break;
    case TCG_COND_LEU:
        result = (u0 <= u1);
        break;
    case TCG_COND_GTU:
        result = (u0 > u1);
        break;
    case TCG_COND_TSTEQ:
        result = (u0 & u1) == 0;
        break;
    case TCG_COND_TSTNE:
        result = (u0 & u1) != 0;
        break;
    default:
        g_assert_not_reached();
    }
    return result;
}

static uint32_t tlb_load(
    CPUArchState *env, uint64_t taddr, MemOp mop, uint64_t *ptr, bool is_ld)
{
    unsigned a_mask = (unsigned)ptr[3];
    int mask_ofs = (int)ptr[4];
    int8_t page_bits = (int8_t)ptr[5];
    uint64_t page_mask = ptr[6];
    int table_ofs = (uint64_t)ptr[7];

    unsigned s_mask = (1u << (mop & MO_SIZE)) - 1;
    tcg_target_long compare_mask;

    uintptr_t table = *(uintptr_t *)((int)env + table_ofs);
    uintptr_t mask = *(uintptr_t *)((int)env + mask_ofs);
    uintptr_t entry = table +
        ((taddr >> (page_bits - CPU_TLB_ENTRY_BITS)) & mask);
    int off = is_ld ? offsetof(CPUTLBEntry, addr_read)
        : offsetof(CPUTLBEntry, addr_write);
    uint64_t target = *(uint64_t *)(entry + off);
    uint64_t c_addr = taddr;
    if (a_mask < s_mask) {
        c_addr += s_mask - a_mask;
    }
    compare_mask = page_mask | a_mask;
    c_addr &= compare_mask;

    if (c_addr == target) {
        return taddr + *(uintptr_t *)(entry + offsetof(CPUTLBEntry, addend));
    }
    return 0;
}

static uint64_t tci_qemu_ld(CPUArchState *env, uint64_t taddr,
                            MemOpIdx oi, const void *tb_ptr, uint64_t *ptr)
{
    MemOp mop = get_memop(oi);
    uintptr_t ra = (uintptr_t)tb_ptr;

    uint32_t target_addr = tlb_load(env, taddr, mop, ptr, true);
    if (target_addr != 0) {
        switch (mop & MO_SSIZE) {
        case MO_UB:
            return *(uint8_t *)target_addr;
        case MO_SB:
            return *(int8_t *)target_addr;
        case MO_UW:
            return *(uint16_t *)target_addr;
        case MO_SW:
            return *(int16_t *)target_addr;
        case MO_UL:
            return *(uint32_t *)target_addr;
        case MO_SL:
            return *(int32_t *)target_addr;
        case MO_UQ:
            return *(uint64_t *)target_addr;
        default:
            g_assert_not_reached();
        }
    }

    switch (mop & MO_SSIZE) {
    case MO_UB:
        return helper_ldub_mmu(env, taddr, oi, ra);
    case MO_SB:
        return helper_ldsb_mmu(env, taddr, oi, ra);
    case MO_UW:
        return helper_lduw_mmu(env, taddr, oi, ra);
    case MO_SW:
        return helper_ldsw_mmu(env, taddr, oi, ra);
    case MO_UL:
        return helper_ldul_mmu(env, taddr, oi, ra);
    case MO_SL:
        return helper_ldsl_mmu(env, taddr, oi, ra);
    case MO_UQ:
        return helper_ldq_mmu(env, taddr, oi, ra);
    default:
        g_assert_not_reached();
    }
}

static void tci_qemu_st(CPUArchState *env, uint64_t taddr, uint64_t val,
                        MemOpIdx oi, const void *tb_ptr, uint64_t *ptr)
{
    MemOp mop = get_memop(oi);
    uintptr_t ra = (uintptr_t)tb_ptr;

    uint32_t target_addr = tlb_load(env, taddr, mop, ptr, false);
    if (target_addr != 0) {
        switch (mop & MO_SIZE) {
        case MO_UB:
            *(uint8_t *)target_addr = (uint8_t)val;
            break;
        case MO_UW:
            *(uint16_t *)target_addr = (uint16_t)val;
            break;
        case MO_UL:
            *(uint32_t *)target_addr = (uint32_t)val;
            break;
        case MO_UQ:
            *(uint64_t *)target_addr = (uint64_t)val;
            break;
        default:
            g_assert_not_reached();
        }
        return;
    }

    switch (mop & MO_SIZE) {
    case MO_UB:
        helper_stb_mmu(env, taddr, val, oi, ra);
        break;
    case MO_UW:
        helper_stw_mmu(env, taddr, val, oi, ra);
        break;
    case MO_UL:
        helper_stl_mmu(env, taddr, val, oi, ra);
        break;
    case MO_UQ:
        helper_stq_mmu(env, taddr, val, oi, ra);
        break;
    default:
        g_assert_not_reached();
    }
}

#define CASE_32_64(x)                           \
    case glue(glue(INDEX_op_, x), _i64):        \
    case glue(glue(INDEX_op_, x), _i32):
# define CASE_64(x)                             \
    case glue(glue(INDEX_op_, x), _i64):

__thread tcg_target_ulong regs[TCG_TARGET_NB_REGS];

static inline uintptr_t tcg_qemu_tb_exec_tci(CPUArchState *env)
{
    uint32_t *tb_ptr = get_tci_ptr(ctx.tb_ptr);
    uint64_t *stack = ctx.stack;

    regs[TCG_AREG0] = (tcg_target_ulong)env;
    regs[TCG_REG_CALL_STACK] = (uintptr_t)stack;

    for (;;) {
        uint32_t insn;
        TCGOpcode opc;
        TCGReg r0, r1, r2, r3, r4, r5;
        tcg_target_ulong t1;
        TCGCond condition;
        uint8_t pos, len;
        uint32_t tmp32;
        uint64_t tmp64, taddr;
        uint64_t T1, T2;
        MemOpIdx oi;
        int32_t ofs;
        void *ptr;
        int32_t *counter_ptr;

        insn = *tb_ptr++;
        opc = extract32(insn, 0, 8);

        switch (opc) {
        case INDEX_op_call:
            {
                void *call_slots[MAX_CALL_IARGS];
                ffi_cif *cif;
                void *func;
                unsigned i, s, n;

                tci_args_nl(insn, tb_ptr, &len, &ptr);
                uint64_t *data64 = (uint64_t *)ptr;
                func = (void *)data64[0];
                cif = (void *)data64[1];

                int reg_iarg_base = 8;
                int reg_idx = 0;
                int reg_idx_end = 5; /* NUM_OF_IARG_REGS */
                int stack_idx = 0;
                n = cif->nargs;
                for (i = s = 0; i < n; ++i) {
                    ffi_type *t = cif->arg_types[i];
                    if (reg_idx < reg_idx_end) {
                        call_slots[i] = &regs[reg_iarg_base + reg_idx];
                        reg_idx += DIV_ROUND_UP(t->size, 8);
                    } else {
                        call_slots[i] = &stack[stack_idx];
                        stack_idx += DIV_ROUND_UP(t->size, 8);
                    }
                }

                /* Helper functions may need to access the "return address" */
                tci_tb_ptr = (uintptr_t)tb_ptr;
                ffi_call(cif, func, stack, call_slots);
            }

            switch (len) {
            case 0: /* void */
                break;
            case 1: /* uint32_t */
                /*
                 * The result winds up "left-aligned" in the stack[0] slot.
                 * Note that libffi has an odd special case in that it will
                 * always widen an integral result to ffi_arg.
                 */
                if (sizeof(ffi_arg) == 8) {
                    regs[TCG_REG_R0] = (uint32_t)stack[0];
                } else {
                    regs[TCG_REG_R0] = *(uint32_t *)stack;
                }
                break;
            case 2: /* uint64_t */
                /*
                 * For TCG_TARGET_REG_BITS == 32, the register pair
                 * must stay in host memory order.
                 */
                memcpy(&regs[TCG_REG_R0], stack, 8);
                break;
            case 3: /* Int128 */
                memcpy(&regs[TCG_REG_R0], stack, 16);
                break;
            default:
                g_assert_not_reached();
            }
            break;

        case INDEX_op_br:
            tci_args_l(insn, tb_ptr, &ptr);
            tb_ptr = ptr;
            continue;
        case INDEX_op_setcond_i32:
            tci_args_rrrc(insn, &r0, &r1, &r2, &condition);
            regs[r0] = tci_compare32(regs[r1], regs[r2], condition);
            break;
        case INDEX_op_movcond_i32:
            tci_args_rrrrrc(insn, &r0, &r1, &r2, &r3, &r4, &condition);
            tmp32 = tci_compare32(regs[r1], regs[r2], condition);
            regs[r0] = regs[tmp32 ? r3 : r4];
            break;
        case INDEX_op_setcond2_i32:
            tci_args_rrrrrc(insn, &r0, &r1, &r2, &r3, &r4, &condition);
            T1 = tci_uint64(regs[r2], regs[r1]);
            T2 = tci_uint64(regs[r4], regs[r3]);
            regs[r0] = tci_compare64(T1, T2, condition);
            break;
        case INDEX_op_setcond_i64:
            tci_args_rrrc(insn, &r0, &r1, &r2, &condition);
            regs[r0] = tci_compare64(regs[r1], regs[r2], condition);
            break;
        case INDEX_op_movcond_i64:
            tci_args_rrrrrc(insn, &r0, &r1, &r2, &r3, &r4, &condition);
            tmp32 = tci_compare64(regs[r1], regs[r2], condition);
            regs[r0] = regs[tmp32 ? r3 : r4];
            break;
        CASE_32_64(mov)
            tci_args_rr(insn, &r0, &r1);
            regs[r0] = regs[r1];
            break;
        case INDEX_op_tci_movi:
            tci_args_ri(insn, &r0, &t1);
            regs[r0] = t1;
            break;
        case INDEX_op_tci_movl:
            tci_args_rl(insn, tb_ptr, &r0, &ptr);
            regs[r0] = *(tcg_target_ulong *)ptr;
            break;

            /* Load/store operations (32 bit). */

        CASE_32_64(ld8u)
            tci_args_rrs(insn, &r0, &r1, &ofs);
            ptr = (void *)(regs[r1] + ofs);
            regs[r0] = *(uint8_t *)ptr;
            break;
        CASE_32_64(ld8s)
            tci_args_rrs(insn, &r0, &r1, &ofs);
            ptr = (void *)(regs[r1] + ofs);
            regs[r0] = *(int8_t *)ptr;
            break;
        CASE_32_64(ld16u)
            tci_args_rrs(insn, &r0, &r1, &ofs);
            ptr = (void *)(regs[r1] + ofs);
            regs[r0] = *(uint16_t *)ptr;
            break;
        CASE_32_64(ld16s)
            tci_args_rrs(insn, &r0, &r1, &ofs);
            ptr = (void *)(regs[r1] + ofs);
            regs[r0] = *(int16_t *)ptr;
            break;
        case INDEX_op_ld_i32:
        CASE_64(ld32u)
            tci_args_rrs(insn, &r0, &r1, &ofs);
            ptr = (void *)(regs[r1] + ofs);
            regs[r0] = *(uint32_t *)ptr;
            break;
        CASE_32_64(st8)
            tci_args_rrs(insn, &r0, &r1, &ofs);
            ptr = (void *)(regs[r1] + ofs);
            *(uint8_t *)ptr = regs[r0];
            break;
        CASE_32_64(st16)
            tci_args_rrs(insn, &r0, &r1, &ofs);
            ptr = (void *)(regs[r1] + ofs);
            *(uint16_t *)ptr = regs[r0];
            break;
        case INDEX_op_st_i32:
        CASE_64(st32)
            tci_args_rrs(insn, &r0, &r1, &ofs);
            ptr = (void *)(regs[r1] + ofs);
            *(uint32_t *)ptr = regs[r0];
            break;

            /* Arithmetic operations (mixed 32/64 bit). */

        CASE_32_64(add)
            tci_args_rrr(insn, &r0, &r1, &r2);
            regs[r0] = regs[r1] + regs[r2];
            break;
        CASE_32_64(sub)
            tci_args_rrr(insn, &r0, &r1, &r2);
            regs[r0] = regs[r1] - regs[r2];
            break;
        CASE_32_64(mul)
            tci_args_rrr(insn, &r0, &r1, &r2);
            regs[r0] = regs[r1] * regs[r2];
            break;
        CASE_32_64(and)
            tci_args_rrr(insn, &r0, &r1, &r2);
            regs[r0] = regs[r1] & regs[r2];
            break;
        CASE_32_64(or)
            tci_args_rrr(insn, &r0, &r1, &r2);
            regs[r0] = regs[r1] | regs[r2];
            break;
        CASE_32_64(xor)
            tci_args_rrr(insn, &r0, &r1, &r2);
            regs[r0] = regs[r1] ^ regs[r2];
            break;
#if TCG_TARGET_HAS_andc_i32 || TCG_TARGET_HAS_andc_i64
        CASE_32_64(andc)
            tci_args_rrr(insn, &r0, &r1, &r2);
            regs[r0] = regs[r1] & ~regs[r2];
            break;
#endif
#if TCG_TARGET_HAS_orc_i32 || TCG_TARGET_HAS_orc_i64
        CASE_32_64(orc)
            tci_args_rrr(insn, &r0, &r1, &r2);
            regs[r0] = regs[r1] | ~regs[r2];
            break;
#endif
#if TCG_TARGET_HAS_eqv_i32 || TCG_TARGET_HAS_eqv_i64
        CASE_32_64(eqv)
            tci_args_rrr(insn, &r0, &r1, &r2);
            regs[r0] = ~(regs[r1] ^ regs[r2]);
            break;
#endif
#if TCG_TARGET_HAS_nand_i32 || TCG_TARGET_HAS_nand_i64
        CASE_32_64(nand)
            tci_args_rrr(insn, &r0, &r1, &r2);
            regs[r0] = ~(regs[r1] & regs[r2]);
            break;
#endif
#if TCG_TARGET_HAS_nor_i32 || TCG_TARGET_HAS_nor_i64
        CASE_32_64(nor)
            tci_args_rrr(insn, &r0, &r1, &r2);
            regs[r0] = ~(regs[r1] | regs[r2]);
            break;
#endif

            /* Arithmetic operations (32 bit). */

        case INDEX_op_div_i32:
            tci_args_rrr(insn, &r0, &r1, &r2);
            regs[r0] = (int32_t)regs[r1] / (int32_t)regs[r2];
            break;
        case INDEX_op_divu_i32:
            tci_args_rrr(insn, &r0, &r1, &r2);
            regs[r0] = (uint32_t)regs[r1] / (uint32_t)regs[r2];
            break;
        case INDEX_op_rem_i32:
            tci_args_rrr(insn, &r0, &r1, &r2);
            regs[r0] = (int32_t)regs[r1] % (int32_t)regs[r2];
            break;
        case INDEX_op_remu_i32:
            tci_args_rrr(insn, &r0, &r1, &r2);
            regs[r0] = (uint32_t)regs[r1] % (uint32_t)regs[r2];
            break;
#if TCG_TARGET_HAS_clz_i32
        case INDEX_op_clz_i32:
            tci_args_rrr(insn, &r0, &r1, &r2);
            tmp32 = regs[r1];
            regs[r0] = tmp32 ? clz32(tmp32) : regs[r2];
            break;
#endif
#if TCG_TARGET_HAS_ctz_i32
        case INDEX_op_ctz_i32:
            tci_args_rrr(insn, &r0, &r1, &r2);
            tmp32 = regs[r1];
            regs[r0] = tmp32 ? ctz32(tmp32) : regs[r2];
            break;
#endif
#if TCG_TARGET_HAS_ctpop_i32
        case INDEX_op_ctpop_i32:
            tci_args_rr(insn, &r0, &r1);
            regs[r0] = ctpop32(regs[r1]);
            break;
#endif

            /* Shift/rotate operations (32 bit). */

        case INDEX_op_shl_i32:
            tci_args_rrr(insn, &r0, &r1, &r2);
            regs[r0] = (uint32_t)regs[r1] << (regs[r2] & 31);
            break;
        case INDEX_op_shr_i32:
            tci_args_rrr(insn, &r0, &r1, &r2);
            regs[r0] = (uint32_t)regs[r1] >> (regs[r2] & 31);
            break;
        case INDEX_op_sar_i32:
            tci_args_rrr(insn, &r0, &r1, &r2);
            regs[r0] = (int32_t)regs[r1] >> (regs[r2] & 31);
            break;
#if TCG_TARGET_HAS_rot_i32
        case INDEX_op_rotl_i32:
            tci_args_rrr(insn, &r0, &r1, &r2);
            regs[r0] = rol32(regs[r1], regs[r2] & 31);
            break;
        case INDEX_op_rotr_i32:
            tci_args_rrr(insn, &r0, &r1, &r2);
            regs[r0] = ror32(regs[r1], regs[r2] & 31);
            break;
#endif
        case INDEX_op_deposit_i32:
            tci_args_rrrbb(insn, &r0, &r1, &r2, &pos, &len);
            regs[r0] = deposit32(regs[r1], pos, len, regs[r2]);
            break;
        case INDEX_op_extract_i32:
            tci_args_rrbb(insn, &r0, &r1, &pos, &len);
            regs[r0] = extract32(regs[r1], pos, len);
            break;
        case INDEX_op_sextract_i32:
            tci_args_rrbb(insn, &r0, &r1, &pos, &len);
            regs[r0] = sextract32(regs[r1], pos, len);
            break;
        case INDEX_op_brcond_i32:
            tci_args_rl(insn, tb_ptr, &r0, &ptr);
            if ((uint32_t)regs[r0]) {
                tb_ptr = ptr;
            }
            break;
#if TCG_TARGET_HAS_add2_i32
        case INDEX_op_add2_i32:
            tci_args_rrrrrr(insn, &r0, &r1, &r2, &r3, &r4, &r5);
            T1 = tci_uint64(regs[r3], regs[r2]);
            T2 = tci_uint64(regs[r5], regs[r4]);
            tci_write_reg64(regs, r1, r0, T1 + T2);
            break;
#endif
#if TCG_TARGET_HAS_sub2_i32
        case INDEX_op_sub2_i32:
            tci_args_rrrrrr(insn, &r0, &r1, &r2, &r3, &r4, &r5);
            T1 = tci_uint64(regs[r3], regs[r2]);
            T2 = tci_uint64(regs[r5], regs[r4]);
            tci_write_reg64(regs, r1, r0, T1 - T2);
            break;
#endif
#if TCG_TARGET_HAS_mulu2_i32
        case INDEX_op_mulu2_i32:
            tci_args_rrrr(insn, &r0, &r1, &r2, &r3);
            tmp64 = (uint64_t)(uint32_t)regs[r2] * (uint32_t)regs[r3];
            tci_write_reg64(regs, r1, r0, tmp64);
            break;
#endif
#if TCG_TARGET_HAS_muls2_i32
        case INDEX_op_muls2_i32:
            tci_args_rrrr(insn, &r0, &r1, &r2, &r3);
            tmp64 = (int64_t)(int32_t)regs[r2] * (int32_t)regs[r3];
            tci_write_reg64(regs, r1, r0, tmp64);
            break;
#endif
#if TCG_TARGET_HAS_ext8s_i32 || TCG_TARGET_HAS_ext8s_i64
        CASE_32_64(ext8s)
            tci_args_rr(insn, &r0, &r1);
            regs[r0] = (int8_t)regs[r1];
            break;
#endif
#if TCG_TARGET_HAS_ext16s_i32 || TCG_TARGET_HAS_ext16s_i64 || \
    TCG_TARGET_HAS_bswap16_i32 || TCG_TARGET_HAS_bswap16_i64
        CASE_32_64(ext16s)
            tci_args_rr(insn, &r0, &r1);
            regs[r0] = (int16_t)regs[r1];
            break;
#endif
#if TCG_TARGET_HAS_ext8u_i32 || TCG_TARGET_HAS_ext8u_i64
        CASE_32_64(ext8u)
            tci_args_rr(insn, &r0, &r1);
            regs[r0] = (uint8_t)regs[r1];
            break;
#endif
#if TCG_TARGET_HAS_ext16u_i32 || TCG_TARGET_HAS_ext16u_i64
        CASE_32_64(ext16u)
            tci_args_rr(insn, &r0, &r1);
            regs[r0] = (uint16_t)regs[r1];
            break;
#endif
#if TCG_TARGET_HAS_bswap16_i32 || TCG_TARGET_HAS_bswap16_i64
        CASE_32_64(bswap16)
            tci_args_rr(insn, &r0, &r1);
            regs[r0] = bswap16(regs[r1]);
            break;
#endif
#if TCG_TARGET_HAS_bswap32_i32 || TCG_TARGET_HAS_bswap32_i64
        CASE_32_64(bswap32)
            tci_args_rr(insn, &r0, &r1);
            regs[r0] = bswap32(regs[r1]);
            break;
#endif
#if TCG_TARGET_HAS_not_i32 || TCG_TARGET_HAS_not_i64
        CASE_32_64(not)
            tci_args_rr(insn, &r0, &r1);
            regs[r0] = ~regs[r1];
            break;
#endif
        CASE_32_64(neg)
            tci_args_rr(insn, &r0, &r1);
            regs[r0] = -regs[r1];
            break;

            /* Load/store operations (64 bit). */

        case INDEX_op_ld32s_i64:
            tci_args_rrs(insn, &r0, &r1, &ofs);
            ptr = (void *)(regs[r1] + ofs);
            regs[r0] = *(int32_t *)ptr;
            break;
        case INDEX_op_ld_i64:
            tci_args_rrs(insn, &r0, &r1, &ofs);
            ptr = (void *)(regs[r1] + ofs);
            regs[r0] = *(uint64_t *)ptr;
            break;
        case INDEX_op_st_i64:
            tci_args_rrs(insn, &r0, &r1, &ofs);
            ptr = (void *)(regs[r1] + ofs);
            *(uint64_t *)ptr = regs[r0];
            break;

            /* Arithmetic operations (64 bit). */

        case INDEX_op_div_i64:
            tci_args_rrr(insn, &r0, &r1, &r2);
            regs[r0] = (int64_t)regs[r1] / (int64_t)regs[r2];
            break;
        case INDEX_op_divu_i64:
            tci_args_rrr(insn, &r0, &r1, &r2);
            regs[r0] = (uint64_t)regs[r1] / (uint64_t)regs[r2];
            break;
        case INDEX_op_rem_i64:
            tci_args_rrr(insn, &r0, &r1, &r2);
            regs[r0] = (int64_t)regs[r1] % (int64_t)regs[r2];
            break;
        case INDEX_op_remu_i64:
            tci_args_rrr(insn, &r0, &r1, &r2);
            regs[r0] = (uint64_t)regs[r1] % (uint64_t)regs[r2];
            break;
#if TCG_TARGET_HAS_clz_i64
        case INDEX_op_clz_i64:
            tci_args_rrr(insn, &r0, &r1, &r2);
            regs[r0] = regs[r1] ? clz64(regs[r1]) : regs[r2];
            break;
#endif
#if TCG_TARGET_HAS_ctz_i64
        case INDEX_op_ctz_i64:
            tci_args_rrr(insn, &r0, &r1, &r2);
            regs[r0] = regs[r1] ? ctz64(regs[r1]) : regs[r2];
            break;
#endif
#if TCG_TARGET_HAS_ctpop_i64
        case INDEX_op_ctpop_i64:
            tci_args_rr(insn, &r0, &r1);
            regs[r0] = ctpop64(regs[r1]);
            break;
#endif
#if TCG_TARGET_HAS_mulu2_i64
        case INDEX_op_mulu2_i64:
            tci_args_rrrr(insn, &r0, &r1, &r2, &r3);
            mulu64(&regs[r0], &regs[r1], regs[r2], regs[r3]);
            break;
#endif
#if TCG_TARGET_HAS_muls2_i64
        case INDEX_op_muls2_i64:
            tci_args_rrrr(insn, &r0, &r1, &r2, &r3);
            muls64(&regs[r0], &regs[r1], regs[r2], regs[r3]);
            break;
#endif
#if TCG_TARGET_HAS_add2_i64
        case INDEX_op_add2_i64:
            tci_args_rrrrrr(insn, &r0, &r1, &r2, &r3, &r4, &r5);
            T1 = regs[r2] + regs[r4];
            T2 = regs[r3] + regs[r5] + (T1 < regs[r2]);
            regs[r0] = T1;
            regs[r1] = T2;
            break;
#endif
#if TCG_TARGET_HAS_add2_i64
        case INDEX_op_sub2_i64:
            tci_args_rrrrrr(insn, &r0, &r1, &r2, &r3, &r4, &r5);
            T1 = regs[r2] - regs[r4];
            T2 = regs[r3] - regs[r5] - (regs[r2] < regs[r4]);
            regs[r0] = T1;
            regs[r1] = T2;
            break;
#endif

            /* Shift/rotate operations (64 bit). */

        case INDEX_op_shl_i64:
            tci_args_rrr(insn, &r0, &r1, &r2);
            regs[r0] = regs[r1] << (regs[r2] & 63);
            break;
        case INDEX_op_shr_i64:
            tci_args_rrr(insn, &r0, &r1, &r2);
            regs[r0] = regs[r1] >> (regs[r2] & 63);
            break;
        case INDEX_op_sar_i64:
            tci_args_rrr(insn, &r0, &r1, &r2);
            regs[r0] = (int64_t)regs[r1] >> (regs[r2] & 63);
            break;
#if TCG_TARGET_HAS_rot_i64
        case INDEX_op_rotl_i64:
            tci_args_rrr(insn, &r0, &r1, &r2);
            regs[r0] = rol64(regs[r1], regs[r2] & 63);
            break;
        case INDEX_op_rotr_i64:
            tci_args_rrr(insn, &r0, &r1, &r2);
            regs[r0] = ror64(regs[r1], regs[r2] & 63);
            break;
#endif
        case INDEX_op_deposit_i64:
            tci_args_rrrbb(insn, &r0, &r1, &r2, &pos, &len);
            regs[r0] = deposit64(regs[r1], pos, len, regs[r2]);
            break;
        case INDEX_op_extract_i64:
            tci_args_rrbb(insn, &r0, &r1, &pos, &len);
            regs[r0] = extract64(regs[r1], pos, len);
            break;
        case INDEX_op_sextract_i64:
            tci_args_rrbb(insn, &r0, &r1, &pos, &len);
            regs[r0] = sextract64(regs[r1], pos, len);
            break;
        case INDEX_op_brcond_i64:
            tci_args_rl(insn, tb_ptr, &r0, &ptr);
            if (regs[r0]) {
                tb_ptr = ptr;
            }
            break;
        case INDEX_op_ext32s_i64:
        case INDEX_op_ext_i32_i64:
            tci_args_rr(insn, &r0, &r1);
            regs[r0] = (int32_t)regs[r1];
            break;
        case INDEX_op_ext32u_i64:
        case INDEX_op_extu_i32_i64:
            tci_args_rr(insn, &r0, &r1);
            regs[r0] = (uint32_t)regs[r1];
            break;
#if TCG_TARGET_HAS_bswap64_i64
        case INDEX_op_bswap64_i64:
            tci_args_rr(insn, &r0, &r1);
            regs[r0] = bswap64(regs[r1]);
            break;
#endif

            /* QEMU specific operations. */

        case INDEX_op_exit_tb:
            tci_args_l(insn, tb_ptr, &ptr);
            ctx.tb_ptr = 0;
            return (uintptr_t)ptr;

        case INDEX_op_goto_tb:
            tci_args_l(insn, tb_ptr, &ptr);
            if (*(uint32_t **)ptr != tb_ptr) {
                tb_ptr = *(uint32_t **)ptr;
                ctx.tb_ptr = tb_ptr;
                counter_ptr = get_counter_ptr(tb_ptr);
                if ((*counter_ptr >= 0) && (*counter_ptr < INSTANTIATE_NUM)) {
                    *counter_ptr += 1;
                } else {
                    return 0; /* enter to wasm TB */
                }
                tb_ptr = get_tci_ptr(tb_ptr);
            }
            break;

        case INDEX_op_goto_ptr:
            tci_args_r(insn, &r0);
            ptr = (void *)regs[r0];
            if (!ptr) {
                ctx.tb_ptr = 0;
                return 0;
            }

            tb_ptr = ptr;
            ctx.tb_ptr = tb_ptr;
            counter_ptr = get_counter_ptr(tb_ptr);
            if ((*counter_ptr >= 0) && (*counter_ptr < INSTANTIATE_NUM)) {
                *counter_ptr += 1;
            } else {
                return 0; /* enter to wasm TB */
            }
            tb_ptr = get_tci_ptr(tb_ptr);

            break;

        case INDEX_op_qemu_ld_i32:
        case INDEX_op_qemu_ld_i64:
            tci_args_ldst(insn, &r0, &r1, &oi, tb_ptr, &ptr);
            taddr = regs[r1];
            regs[r0] = tci_qemu_ld(env, taddr, oi, tb_ptr, ptr);
            break;

        case INDEX_op_qemu_st_i32:
        case INDEX_op_qemu_st_i64:
            tci_args_ldst(insn, &r0, &r1, &oi, tb_ptr, &ptr);
            taddr = regs[r1];
            tci_qemu_st(env, taddr, regs[r0], oi, tb_ptr, ptr);
            break;

        case INDEX_op_mb:
            /* Ensure ordering for all kinds */
            smp_mb();
            break;
        default:
            g_assert_not_reached();
        }
    }
}

/*
 * Max number of instances can exist simultaneously.
 *
 * If the number of instances reaches this and a new instance needs to be
 * created, old instances are removed so that new instances can be created
 * without hitting the browser's limit.
 */
#define MAX_INSTANCES 15000
#define INSTANCES_BUF_MAX (MAX_INSTANCES + 1)

int instances_global;

/* Holds the relationship between TB and Wasm instance. */
struct instance_info {
    void *tb;
    int func_idx;
};
__thread struct instance_info instances[INSTANCES_BUF_MAX];
__thread int instances_begin;
__thread int instances_end;

static void add_instance(int fidx, void *tb_ptr)
{
    uint32_t *info_ptr = get_info_ptr(tb_ptr);

    instances[instances_end].tb = tb_ptr;
    instances[instances_end].func_idx = fidx;
    *info_ptr = (uint32_t)(&(instances[instances_end]));
    instances_end  = (instances_end + 1) % INSTANCES_BUF_MAX;

    qatomic_inc(&instances_global);
}

static int get_instance(void *tb_ptr)
{
    uint32_t *info_ptr = get_info_ptr(tb_ptr);
    struct instance_info *elm = (struct instance_info *)(*info_ptr);
    if (elm == NULL) {
        return 0;
    }
    if (elm->tb != tb_ptr) {
        /* invalidated */
        int32_t *counter_ptr = get_counter_ptr(tb_ptr);
        *counter_ptr = INSTANTIATE_NUM; /* instanciated immediately */
        *info_ptr = 0;
        return 0;
    }
    return elm->func_idx;
}

__thread int instance_pending_gc;
__thread int instance_done_gc;

static void remove_instance(void)
{
    int num;
    if (instance_pending_gc > 0) {
        return;
    }
    if (instances_begin <= instances_end) {
        num = instances_end - instances_begin;
    } else {
        num = instances_end + (INSTANCES_BUF_MAX - instances_begin);
    }
    num /= 2;
    for (int i = 0; i < num; i++) {
        EM_ASM({ removeFunction($0); }, instances[instances_begin].func_idx);
        instances[instances_begin].tb = NULL;
        instances_begin = (instances_begin + 1) % INSTANCES_BUF_MAX;
    }
    instance_pending_gc += num;
}

static bool can_add_instance(void)
{
    return qatomic_read(&instances_global) < MAX_INSTANCES;
}

static void check_instance_garbage_collected(void)
{
    if (instance_done_gc > 0) {
        qatomic_sub(&instances_global, instance_done_gc);
        instance_pending_gc -= instance_done_gc;
        instance_done_gc = 0;
    }
}

#define MAX_EXEC_NUM 50000
__thread int exec_cnt = MAX_EXEC_NUM;
static inline void trysleep(void)
{
    if (--exec_cnt == 0) {
        if (!can_add_instance()) {
            emscripten_sleep(0); /* return to the browser main loop */
            check_instance_garbage_collected();
        }
        exec_cnt = MAX_EXEC_NUM;
    }
}

EM_JS(void, init_wasm32_js, (int instance_done_gc_ptr),
{
    Module.__wasm32_tb = {
        inst_gc_registry: new FinalizationRegistry((i) => {
            if (i == "instance") {
                const memory_v = new DataView(HEAP8.buffer);
                let v = memory_v.getInt32(instance_done_gc_ptr, true);
                memory_v.setInt32(instance_done_gc_ptr, v + 1, true);
            }
        })
    };
});

int get_core_nums(void)
{
    return emscripten_num_logical_cores();
}

int cur_core_num_max;

static void init_wasm32(void)
{
    cur_core_num = qatomic_fetch_inc(&cur_core_num_max);
    ctx.stack = g_malloc(TCG_STATIC_CALL_ARGS_SIZE + TCG_STATIC_FRAME_SIZE);
    ctx.stack128 = g_malloc(TCG_STATIC_CALL_ARGS_SIZE);
    ctx.tci_tb_ptr = (uint32_t *)&tci_tb_ptr;
    init_wasm32_js((int)&instance_done_gc);
}

__thread bool initdone;

typedef uint32_t (*wasm_func_ptr)(struct wasmContext *);

uintptr_t QEMU_DISABLE_CFI tcg_qemu_tb_exec(CPUArchState *env,
                                            const void *v_tb_ptr)
{
    if (!initdone) {
        init_wasm32();
        initdone = true;
    }
    ctx.env = env;
    ctx.tb_ptr = (void *)v_tb_ptr;
    ctx.do_init = 1;
    while (true) {
        trysleep();
        struct wasmTBHeader *header = (struct wasmTBHeader *)ctx.tb_ptr;
        int32_t *counter_ptr = get_counter_ptr(header);
        uint32_t res;
        int fidx = get_instance(ctx.tb_ptr);
        if (fidx > 0) {
            res = ((wasm_func_ptr)(fidx))(&ctx);
        } else if (*counter_ptr < INSTANTIATE_NUM) {
            *counter_ptr += 1;
            res = tcg_qemu_tb_exec_tci(env);
        } else if (!can_add_instance()) {
            remove_instance();
            check_instance_garbage_collected();
            res = tcg_qemu_tb_exec_tci(env);
        } else {
            int fidx = instantiate_wasm((int)header->wasm_ptr,
                                        header->wasm_size,
                                        (int)header->import_ptr,
                                        header->import_size);
            add_instance(fidx, ctx.tb_ptr);
            res = ((wasm_func_ptr)(fidx))(&ctx);
        }
        if ((uint32_t)ctx.tb_ptr == 0) {
            return res;
        }
    }
}
