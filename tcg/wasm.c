/* SPDX-License-Identifier: GPL-2.0-or-later */
/*
 * WebAssembly backend with forked TCI, based on tci.c
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

#include "qemu/osdep.h"
#include "tcg/tcg.h"
#include "tcg/tcg-ldst.h"

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
    default:
        g_assert_not_reached();
    }
    return result;
}

static uintptr_t tcg_qemu_tb_exec_tci(CPUArchState *env, const void *v_tb_ptr)
{
    const uint32_t *tb_ptr = v_tb_ptr;
    tcg_target_ulong regs[TCG_TARGET_NB_REGS];
    uint64_t stack[(TCG_STATIC_CALL_ARGS_SIZE + TCG_STATIC_FRAME_SIZE)
                   / sizeof(uint64_t)];

    regs[TCG_AREG0] = (tcg_target_ulong)env;
    regs[TCG_REG_CALL_STACK] = (uintptr_t)stack;

    for (;;) {
        uint32_t insn;
        TCGOpcode opc;
        TCGReg r0, r1, r2, r3, r4;
        tcg_target_ulong t1;
        uint8_t pos, len;
        TCGCond condition;
        uint32_t tmp32;
        int32_t ofs;
        void *ptr;

        insn = *tb_ptr++;
        opc = extract32(insn, 0, 8);

        switch (opc) {
        case INDEX_op_and:
            tci_args_rrr(insn, &r0, &r1, &r2);
            regs[r0] = regs[r1] & regs[r2];
            break;
        case INDEX_op_or:
            tci_args_rrr(insn, &r0, &r1, &r2);
            regs[r0] = regs[r1] | regs[r2];
            break;
        case INDEX_op_xor:
            tci_args_rrr(insn, &r0, &r1, &r2);
            regs[r0] = regs[r1] ^ regs[r2];
            break;
        case INDEX_op_add:
            tci_args_rrr(insn, &r0, &r1, &r2);
            regs[r0] = regs[r1] + regs[r2];
            break;
        case INDEX_op_sub:
            tci_args_rrr(insn, &r0, &r1, &r2);
            regs[r0] = regs[r1] - regs[r2];
            break;
        case INDEX_op_mul:
            tci_args_rrr(insn, &r0, &r1, &r2);
            regs[r0] = regs[r1] * regs[r2];
            break;
        case INDEX_op_extract:
            tci_args_rrbb(insn, &r0, &r1, &pos, &len);
            regs[r0] = extract64(regs[r1], pos, len);
            break;
        case INDEX_op_sextract:
            tci_args_rrbb(insn, &r0, &r1, &pos, &len);
            regs[r0] = sextract64(regs[r1], pos, len);
            break;
        case INDEX_op_shl:
            tci_args_rrr(insn, &r0, &r1, &r2);
            regs[r0] = regs[r1] << (regs[r2] % TCG_TARGET_REG_BITS);
            break;
        case INDEX_op_shr:
            tci_args_rrr(insn, &r0, &r1, &r2);
            regs[r0] = regs[r1] >> (regs[r2] % TCG_TARGET_REG_BITS);
            break;
        case INDEX_op_sar:
            tci_args_rrr(insn, &r0, &r1, &r2);
            regs[r0] = ((tcg_target_long)regs[r1]
                        >> (regs[r2] % TCG_TARGET_REG_BITS));
            break;
        case INDEX_op_neg:
            tci_args_rr(insn, &r0, &r1);
            regs[r0] = -regs[r1];
            break;
        case INDEX_op_setcond:
            tci_args_rrrc(insn, &r0, &r1, &r2, &condition);
            regs[r0] = tci_compare64(regs[r1], regs[r2], condition);
            break;
        case INDEX_op_movcond:
            tci_args_rrrrrc(insn, &r0, &r1, &r2, &r3, &r4, &condition);
            tmp32 = tci_compare64(regs[r1], regs[r2], condition);
            regs[r0] = regs[tmp32 ? r3 : r4];
            break;
        case INDEX_op_tci_setcond32:
            tci_args_rrrc(insn, &r0, &r1, &r2, &condition);
            regs[r0] = tci_compare32(regs[r1], regs[r2], condition);
            break;
        case INDEX_op_tci_movcond32:
            tci_args_rrrrrc(insn, &r0, &r1, &r2, &r3, &r4, &condition);
            tmp32 = tci_compare32(regs[r1], regs[r2], condition);
            regs[r0] = regs[tmp32 ? r3 : r4];
            break;
        case INDEX_op_mov:
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
        case INDEX_op_ld:
            tci_args_rrs(insn, &r0, &r1, &ofs);
            ptr = (void *)(regs[r1] + ofs);
            regs[r0] = *(tcg_target_ulong *)ptr;
            break;
        case INDEX_op_ld8u:
            tci_args_rrs(insn, &r0, &r1, &ofs);
            ptr = (void *)(regs[r1] + ofs);
            regs[r0] = *(uint8_t *)ptr;
            break;
        case INDEX_op_ld8s:
            tci_args_rrs(insn, &r0, &r1, &ofs);
            ptr = (void *)(regs[r1] + ofs);
            regs[r0] = *(int8_t *)ptr;
            break;
        case INDEX_op_ld16u:
            tci_args_rrs(insn, &r0, &r1, &ofs);
            ptr = (void *)(regs[r1] + ofs);
            regs[r0] = *(uint16_t *)ptr;
            break;
        case INDEX_op_ld16s:
            tci_args_rrs(insn, &r0, &r1, &ofs);
            ptr = (void *)(regs[r1] + ofs);
            regs[r0] = *(int16_t *)ptr;
            break;
        case INDEX_op_st:
            tci_args_rrs(insn, &r0, &r1, &ofs);
            ptr = (void *)(regs[r1] + ofs);
            *(tcg_target_ulong *)ptr = regs[r0];
            break;
        case INDEX_op_st8:
            tci_args_rrs(insn, &r0, &r1, &ofs);
            ptr = (void *)(regs[r1] + ofs);
            *(uint8_t *)ptr = regs[r0];
            break;
        case INDEX_op_st16:
            tci_args_rrs(insn, &r0, &r1, &ofs);
            ptr = (void *)(regs[r1] + ofs);
            *(uint16_t *)ptr = regs[r0];
            break;
        case INDEX_op_ld32u:
            tci_args_rrs(insn, &r0, &r1, &ofs);
            ptr = (void *)(regs[r1] + ofs);
            regs[r0] = *(uint32_t *)ptr;
            break;
        case INDEX_op_ld32s:
            tci_args_rrs(insn, &r0, &r1, &ofs);
            ptr = (void *)(regs[r1] + ofs);
            regs[r0] = *(int32_t *)ptr;
            break;
        case INDEX_op_st32:
            tci_args_rrs(insn, &r0, &r1, &ofs);
            ptr = (void *)(regs[r1] + ofs);
            *(uint32_t *)ptr = regs[r0];
            break;
        case INDEX_op_divs:
            tci_args_rrr(insn, &r0, &r1, &r2);
            regs[r0] = (int64_t)regs[r1] / (int64_t)regs[r2];
            break;
        case INDEX_op_divu:
            tci_args_rrr(insn, &r0, &r1, &r2);
            regs[r0] = (uint64_t)regs[r1] / (uint64_t)regs[r2];
            break;
        case INDEX_op_rems:
            tci_args_rrr(insn, &r0, &r1, &r2);
            regs[r0] = (int64_t)regs[r1] % (int64_t)regs[r2];
            break;
        case INDEX_op_remu:
            tci_args_rrr(insn, &r0, &r1, &r2);
            regs[r0] = (uint64_t)regs[r1] % (uint64_t)regs[r2];
            break;
        case INDEX_op_tci_divs32:
            tci_args_rrr(insn, &r0, &r1, &r2);
            regs[r0] = (int32_t)regs[r1] / (int32_t)regs[r2];
            break;
        case INDEX_op_tci_divu32:
            tci_args_rrr(insn, &r0, &r1, &r2);
            regs[r0] = (uint32_t)regs[r1] / (uint32_t)regs[r2];
            break;
        case INDEX_op_tci_rems32:
            tci_args_rrr(insn, &r0, &r1, &r2);
            regs[r0] = (int32_t)regs[r1] % (int32_t)regs[r2];
            break;
        case INDEX_op_tci_remu32:
            tci_args_rrr(insn, &r0, &r1, &r2);
            regs[r0] = (uint32_t)regs[r1] % (uint32_t)regs[r2];
            break;
        case INDEX_op_ctpop:
            tci_args_rr(insn, &r0, &r1);
            regs[r0] = ctpop64(regs[r1]);
            break;
        default:
            g_assert_not_reached();
        }
    }
}
