/*
 * Tiny Code Generator for QEMU
 *
 * Copyright (c) 2018 SiFive, Inc
 * Copyright (c) 2008-2009 Arnaud Patard <arnaud.patard@rtp-net.org>
 * Copyright (c) 2009 Aurelien Jarno <aurelien@aurel32.net>
 * Copyright (c) 2008 Fabrice Bellard
 * Copyright (c) 2009, 2011 Stefan Weil
 *
 * Based on riscv/tcg-target.c.inc and tci/tcg-target.c
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
 */

#include "../wasm32.h"
#include <emscripten.h>

static TCGConstraintSetIndex tcg_target_op_def(TCGOpcode op)
{
    switch (op) {
    case INDEX_op_goto_ptr:
        return C_O0_I1(r);

    case INDEX_op_ld8u_i32:
    case INDEX_op_ld8s_i32:
    case INDEX_op_ld16u_i32:
    case INDEX_op_ld16s_i32:
    case INDEX_op_ld_i32:
    case INDEX_op_ld8u_i64:
    case INDEX_op_ld8s_i64:
    case INDEX_op_ld16u_i64:
    case INDEX_op_ld16s_i64:
    case INDEX_op_ld32u_i64:
    case INDEX_op_ld32s_i64:
    case INDEX_op_ld_i64:
    case INDEX_op_not_i32:
    case INDEX_op_not_i64:
    case INDEX_op_neg_i32:
    case INDEX_op_neg_i64:
    case INDEX_op_ext8s_i32:
    case INDEX_op_ext8s_i64:
    case INDEX_op_ext16s_i32:
    case INDEX_op_ext16s_i64:
    case INDEX_op_ext8u_i32:
    case INDEX_op_ext8u_i64:
    case INDEX_op_ext16u_i32:
    case INDEX_op_ext16u_i64:
    case INDEX_op_ext32s_i64:
    case INDEX_op_ext32u_i64:
    case INDEX_op_ext_i32_i64:
    case INDEX_op_extu_i32_i64:
    case INDEX_op_bswap16_i32:
    case INDEX_op_bswap16_i64:
    case INDEX_op_bswap32_i32:
    case INDEX_op_bswap32_i64:
    case INDEX_op_bswap64_i64:
    case INDEX_op_extract_i32:
    case INDEX_op_extract_i64:
    case INDEX_op_sextract_i32:
    case INDEX_op_sextract_i64:
    case INDEX_op_extrl_i64_i32:
    case INDEX_op_extrh_i64_i32:
    case INDEX_op_ctpop_i32:
    case INDEX_op_ctpop_i64:
        return C_O1_I1(r, r);

    case INDEX_op_st8_i32:
    case INDEX_op_st16_i32:
    case INDEX_op_st_i32:
    case INDEX_op_st8_i64:
    case INDEX_op_st16_i64:
    case INDEX_op_st32_i64:
    case INDEX_op_st_i64:
        return C_O0_I2(r, r);

    case INDEX_op_div_i32:
    case INDEX_op_div_i64:
    case INDEX_op_divu_i32:
    case INDEX_op_divu_i64:
    case INDEX_op_rem_i32:
    case INDEX_op_rem_i64:
    case INDEX_op_remu_i32:
    case INDEX_op_remu_i64:
    case INDEX_op_add_i32:
    case INDEX_op_add_i64:
    case INDEX_op_sub_i32:
    case INDEX_op_sub_i64:
    case INDEX_op_mul_i32:
    case INDEX_op_mul_i64:
    case INDEX_op_and_i32:
    case INDEX_op_and_i64:
    case INDEX_op_andc_i32:
    case INDEX_op_andc_i64:
    case INDEX_op_eqv_i32:
    case INDEX_op_eqv_i64:
    case INDEX_op_nand_i32:
    case INDEX_op_nand_i64:
    case INDEX_op_nor_i32:
    case INDEX_op_nor_i64:
    case INDEX_op_or_i32:
    case INDEX_op_or_i64:
    case INDEX_op_orc_i32:
    case INDEX_op_orc_i64:
    case INDEX_op_xor_i32:
    case INDEX_op_xor_i64:
    case INDEX_op_shl_i32:
    case INDEX_op_shl_i64:
    case INDEX_op_shr_i32:
    case INDEX_op_shr_i64:
    case INDEX_op_sar_i32:
    case INDEX_op_sar_i64:
    case INDEX_op_rotl_i32:
    case INDEX_op_rotl_i64:
    case INDEX_op_rotr_i32:
    case INDEX_op_rotr_i64:
    case INDEX_op_setcond_i32:
    case INDEX_op_setcond_i64:
    case INDEX_op_deposit_i32:
    case INDEX_op_deposit_i64:
    case INDEX_op_clz_i32:
    case INDEX_op_clz_i64:
    case INDEX_op_ctz_i32:
    case INDEX_op_ctz_i64:
        return C_O1_I2(r, r, r);

    case INDEX_op_brcond_i32:
    case INDEX_op_brcond_i64:
        return C_O0_I2(r, r);

    case INDEX_op_add2_i32:
    case INDEX_op_add2_i64:
    case INDEX_op_sub2_i32:
    case INDEX_op_sub2_i64:
        return C_O2_I4(r, r, r, r, r, r);

    case INDEX_op_mulu2_i32:
    case INDEX_op_mulu2_i64:
    case INDEX_op_muls2_i32:
    case INDEX_op_muls2_i64:
        return C_O2_I2(r, r, r, r);

    case INDEX_op_movcond_i32:
    case INDEX_op_movcond_i64:
        return C_O1_I4(r, r, r, r, r);

    case INDEX_op_setcond2_i32:
        return C_O1_I4(r, r, r, r, r);
    case INDEX_op_brcond2_i32:
        return C_O0_I4(r, r, r, r);

    case INDEX_op_qemu_ld_a32_i32:
    case INDEX_op_qemu_ld_a64_i32:
        return C_O1_I1(r, r);

    case INDEX_op_qemu_ld_a32_i64:
    case INDEX_op_qemu_ld_a64_i64:
        return C_O1_I1(r, r);
    case INDEX_op_qemu_st_a32_i32:
    case INDEX_op_qemu_st_a64_i32:
        return C_O0_I2(r, r);
    case INDEX_op_qemu_st_a32_i64:
    case INDEX_op_qemu_st_a64_i64:
        return C_O0_I2(r, r);

    case INDEX_op_muluh_i32:
    case INDEX_op_mulsh_i32:
        return C_O1_I2(r, r, r);
    case INDEX_op_extract2_i32:
    case INDEX_op_extract2_i64:
        return C_O1_I2(r, r, r);

    default:
        g_assert_not_reached();
    }
}

static const int tcg_target_reg_alloc_order[] = {
    TCG_REG_R0,
    TCG_REG_R1,
    TCG_REG_R2,
    TCG_REG_R3,
    TCG_REG_R4,
    TCG_REG_R5,
    TCG_REG_R6,
    TCG_REG_R7,
    TCG_REG_R8,
    TCG_REG_R9,
    TCG_REG_R10,
    TCG_REG_R11,
    TCG_REG_R12,
    TCG_REG_R13,
    TCG_REG_R14,
    TCG_REG_R15,

    // Arguments
    TCG_REG_A0,
    TCG_REG_A1,
    TCG_REG_A2,
    TCG_REG_A3,
    TCG_REG_A4,
    TCG_REG_A5,
    TCG_REG_A6,
    TCG_REG_A7,
};

#define NUM_OF_IARG_REGS 8
static const int tcg_target_call_iarg_regs[NUM_OF_IARG_REGS] = {
    TCG_REG_A0,
    TCG_REG_A1,
    TCG_REG_A2,
    TCG_REG_A3,
    TCG_REG_A4,
    TCG_REG_A5,
    TCG_REG_A6,
    TCG_REG_A7,
};

static TCGReg tcg_target_call_oarg_reg(TCGCallReturnKind kind, int slot)
{
    tcg_debug_assert(kind == TCG_CALL_RET_NORMAL);
    tcg_debug_assert(slot >= 0 && slot < 128 / TCG_TARGET_REG_BITS);
    return TCG_REG_A0 + slot;
}

#ifdef CONFIG_DEBUG_TCG
static const char *const tcg_target_reg_names[TCG_TARGET_NB_REGS] = {
    "r00",
    "r01",
    "r02",
    "r03",
    "r04",
    "r05",
    "r06",
    "r07",
    "r08",
    "r09",
    "r10",
    "r11",
    "r12",
    "r13",
    "r14",
    "r15",
    "a0",
    "a1",
    "a2",
    "a3",
    "a4",
    "a5",
    "a6",
    "a7",
};
#endif

#define REG_INDEX_IARG_BASE 16
static const uint8_t tcg_target_reg_index[TCG_TARGET_NB_REGS] = {
    0, // TCG_REG_R0
    1, // TCG_REG_R1
    2, // TCG_REG_R2
    3, // TCG_REG_R3
    4, // TCG_REG_R4
    5, // TCG_REG_R5
    6, // TCG_REG_R6
    7, // TCG_REG_R7
    8, // TCG_REG_R8
    9, // TCG_REG_R9
    10, // TCG_REG_R10
    11, // TCG_REG_R11
    12, // TCG_REG_R12
    13, // TCG_REG_R13
    14, // TCG_REG_R14
    15, // TCG_REG_R15

    16, // TCG_REG_A0
    17, // TCG_REG_A1
    18, // TCG_REG_A2
    19, // TCG_REG_A3
    20, // TCG_REG_A4
    21, // TCG_REG_A5
    22, // TCG_REG_A6
    23, // TCG_REG_A7
};

#define BLOCK_PTR_IDX 24

#define CTX_IDX 0
#define TMP32_LOCAL_ENV_IDX 1
#define TMP32_LOCAL_0_IDX 2
#define TMP64_0_IDX 3
#define TMP64_1_IDX 4
#define TMP64_2_IDX 5
#define TMP64_3_IDX 6
#define TMP64_4_IDX 7

__thread bool env_cached = false;

// function index
#define RETURN_CALL_IDX 0
#define FUNC_HELPER_CALL_IDX 1
#define FUNC_CALL_LD_HELPER_IDX 2
#define FUNC_CALL_ST_HELPER_IDX 3

// table index
#define HELPER_TABLE_IDX 0

static void tcg_out_leb128_sint32_t(TCGContext *s, int32_t v) {
    bool more = true;
    bool negative = (v < 0);
    uint8_t b;
    uint32_t low7 = 0x7f;
    uint32_t uv = v;
    while (more) {
        b = uv & low7;
        uv >>= 7;
        if (negative)
            uv |= (~0 << (32 - 7));
        if (((uv == 0) && ((b & 0x40) == 0)) || ((uv == -1) && ((b & 0x40) != 0)))
            more = false;
        else
            b |= 0x80;
        tcg_out8(s, b);
    }
}

static void tcg_out_leb128_sint64_t(TCGContext *s, int64_t v) {
    bool more = true;
    bool negative = (v < 0);
    uint8_t b;
    uint64_t low7 = 0x7f;
    uint64_t uv = v;
    while (more) {
        b = uv & low7;
        uv >>= 7;
        if (negative)
            uv |= ((int64_t)(~0) << (64 - 7));
        if (((uv == 0) && ((b & 0x40) == 0)) || (((int64_t)uv == -1) && ((b & 0x40) != 0)))
            more = false;
        else
            b |= 0x80;
        tcg_out8(s, b);
    }
}

static void tcg_out_leb128_uint32_t(TCGContext *s, uint32_t v) {
    uint32_t low7 = 0x7f;
    uint8_t b;
    do {
        b = v & low7;
        v >>= 7;
        if (v != 0)
            b |= 0x80;
        tcg_out8(s, b);
    } while (v != 0);
}

static void tcg_out_op_br(TCGContext *s, int i)
{
    tcg_out8(s, 0x0c);
    tcg_out8(s, i);
}

static void tcg_out_op_if_noret(TCGContext *s)
{
    tcg_out8(s, 0x04);
    tcg_out8(s, 0x40);
}

static void tcg_out_op_if_ret_i64(TCGContext *s)
{
    tcg_out8(s, 0x04);
    tcg_out8(s, 0x7e);
}

static void tcg_out_op_if_ret_i32(TCGContext *s)
{
    tcg_out8(s, 0x04);
    tcg_out8(s, 0x7f);
}

static void tcg_out_op_else(TCGContext *s)
{
    tcg_out8(s, 0x05);
}

static void tcg_out_op_end(TCGContext *s)
{
    tcg_out8(s, 0x0b);
}

static void tcg_out_op_i32_eqz(TCGContext *s){ tcg_out8(s, 0x45); }
static void tcg_out_op_i32_eq(TCGContext *s){ tcg_out8(s, 0x46); }
static void tcg_out_op_i32_and(TCGContext *s){ tcg_out8(s, 0x71); }
static void tcg_out_op_i32_or(TCGContext *s){ tcg_out8(s, 0x72); }
//static void tcg_out_op_i32_xor(TCGContext *s){ tcg_out8(s, 0x73); }
static void tcg_out_op_i32_shl(TCGContext *s){ tcg_out8(s, 0x74); }
static void tcg_out_op_i32_shr_s(TCGContext *s){ tcg_out8(s, 0x75); }
static void tcg_out_op_i32_shr_u(TCGContext *s){ tcg_out8(s, 0x76); }
static void tcg_out_op_i32_rotl(TCGContext *s){ tcg_out8(s, 0x77); }
static void tcg_out_op_i32_rotr(TCGContext *s){ tcg_out8(s, 0x78); }
static void tcg_out_op_i32_clz(TCGContext *s){ tcg_out8(s, 0x67); }
static void tcg_out_op_i32_ctz(TCGContext *s){ tcg_out8(s, 0x68); }
//static void tcg_out_op_i32_popcnt(TCGContext *s){ tcg_out8(s, 0x69); }
static void tcg_out_op_i32_add(TCGContext *s){ tcg_out8(s, 0x6a); }
//static void tcg_out_op_i32_sub(TCGContext *s){ tcg_out8(s, 0x6b); }
//static void tcg_out_op_i32_mul(TCGContext *s){ tcg_out8(s, 0x6c); }
//static void tcg_out_op_i32_div_s(TCGContext *s){ tcg_out8(s, 0x6d); }
//static void tcg_out_op_i32_div_u(TCGContext *s){ tcg_out8(s, 0x6e); }
//static void tcg_out_op_i32_rem_s(TCGContext *s){ tcg_out8(s, 0x6f); }
//static void tcg_out_op_i32_rem_u(TCGContext *s){ tcg_out8(s, 0x70); }
static void tcg_out_op_i32_ne(TCGContext *s){ tcg_out8(s, 0x47); }
//static void tcg_out_op_i32_le_u(TCGContext *s){ tcg_out8(s, 0x4d); }

static void tcg_out_op_i64_eqz(TCGContext *s){ tcg_out8(s, 0x50); }
static void tcg_out_op_i64_eq(TCGContext *s){ tcg_out8(s, 0x51); }
static void tcg_out_op_i64_and(TCGContext *s){ tcg_out8(s, 0x83); }
static void tcg_out_op_i64_or(TCGContext *s){ tcg_out8(s, 0x84); }
static void tcg_out_op_i64_xor(TCGContext *s){ tcg_out8(s, 0x85); }
static void tcg_out_op_i64_shl(TCGContext *s){ tcg_out8(s, 0x86); }
static void tcg_out_op_i64_shr_s(TCGContext *s){ tcg_out8(s, 0x87); }
static void tcg_out_op_i64_shr_u(TCGContext *s){ tcg_out8(s, 0x88); }
static void tcg_out_op_i64_rotl(TCGContext *s){ tcg_out8(s, 0x89); }
static void tcg_out_op_i64_rotr(TCGContext *s){ tcg_out8(s, 0x8a); }
static void tcg_out_op_i64_clz(TCGContext *s){ tcg_out8(s, 0x79); }
static void tcg_out_op_i64_ctz(TCGContext *s){ tcg_out8(s, 0x7a); }
static void tcg_out_op_i64_popcnt(TCGContext *s){ tcg_out8(s, 0x7b); }
static void tcg_out_op_i64_add(TCGContext *s){ tcg_out8(s, 0x7c); }
static void tcg_out_op_i64_sub(TCGContext *s){ tcg_out8(s, 0x7d); }
static void tcg_out_op_i64_mul(TCGContext *s){ tcg_out8(s, 0x7e); }
static void tcg_out_op_i64_div_s(TCGContext *s){ tcg_out8(s, 0x7f); }
static void tcg_out_op_i64_div_u(TCGContext *s){ tcg_out8(s, 0x80); }
static void tcg_out_op_i64_rem_s(TCGContext *s){ tcg_out8(s, 0x81); }
static void tcg_out_op_i64_rem_u(TCGContext *s){ tcg_out8(s, 0x82); }
//static void tcg_out_op_i64_ne(TCGContext *s){ tcg_out8(s, 0x52); }
static void tcg_out_op_i64_le_u(TCGContext *s){ tcg_out8(s, 0x58); }
static void tcg_out_op_i64_lt_u(TCGContext *s){ tcg_out8(s, 0x54); }
static void tcg_out_op_i64_gt_u(TCGContext *s){ tcg_out8(s, 0x56); }

static void tcg_out_op_i32_wrap_i64(TCGContext *s){ tcg_out8(s, 0xa7); }

static void tcg_out_op_var(TCGContext *s, uint8_t instr, uint8_t i)
{
    tcg_out8(s, instr);
    tcg_out8(s, i);
}

static void tcg_out_op_local_get(TCGContext *s, uint8_t i)
{
    tcg_out_op_var(s, 0x20, i);
}

static void tcg_out_op_local_set(TCGContext *s, uint8_t i)
{
    tcg_out_op_var(s, 0x21, i);
}

static void tcg_out_op_local_tee(TCGContext *s, uint8_t i)
{
    tcg_out_op_var(s, 0x22, i);
}

static void tcg_out_op_global_get(TCGContext *s, uint8_t i)
{
    tcg_out_op_var(s, 0x23, i);
}

static void tcg_out_op_global_set(TCGContext *s, uint8_t i)
{
    if (i == tcg_target_reg_index[TCG_REG_R14]) {
        env_cached = false;
    }
    tcg_out_op_var(s, 0x24, i);
}

static void tcg_out_op_global_get_r_i32(TCGContext *s, TCGReg r0)
{
    if (r0 == TCG_REG_R14) {
        if (!env_cached) {
            tcg_out_op_global_get(s, tcg_target_reg_index[r0]);
            tcg_out_op_i32_wrap_i64(s);
            tcg_out_op_local_tee(s, TMP32_LOCAL_ENV_IDX);
            env_cached = true;
        } else {
            tcg_out_op_local_get(s, TMP32_LOCAL_ENV_IDX);
        }
        return;
    }
    tcg_out_op_global_get(s, tcg_target_reg_index[r0]);
    tcg_out_op_i32_wrap_i64(s);
}

static void tcg_out_op_global_get_r(TCGContext *s, TCGReg r0)
{
    tcg_out_op_global_get(s, tcg_target_reg_index[r0]);
}

static void tcg_out_op_global_set_r(TCGContext *s, TCGReg r0)
{
    tcg_out_op_global_set(s, tcg_target_reg_index[r0]);
}

static void tcg_out_op_i32_const(TCGContext *s, int32_t v)
{
    tcg_out8(s, 0x41);
    tcg_out_leb128_sint32_t(s, v);
}

static void tcg_out_op_i64_const(TCGContext *s, int64_t v)
{
    tcg_out8(s, 0x42);
    tcg_out_leb128_sint64_t(s, v);
}

static void tcg_out_op_loadstore(TCGContext *s, uint8_t instr, uint32_t a, uint32_t o)
{
    tcg_out8(s, instr);
    tcg_out_leb128_uint32_t(s, a);
    tcg_out_leb128_uint32_t(s, o);
}

static void tcg_out_op_i64_store(TCGContext *s, uint32_t a, uint32_t o)
{
    tcg_out_op_loadstore(s, 0x37, a, o);
}

static void tcg_out_op_i32_store(TCGContext *s, uint32_t a, uint32_t o)
{
    tcg_out_op_loadstore(s, 0x36, a, o);
}

static void tcg_out_op_i64_store8(TCGContext *s, uint32_t a, uint32_t o)
{
    tcg_out_op_loadstore(s, 0x3c, a, o);
}

//static void tcg_out_op_i32_store8(TCGContext *s, uint32_t a, uint32_t o)
//{
//    tcg_out_op_loadstore(s, 0x3a, a, o);
//}
    
static void tcg_out_op_i64_store16(TCGContext *s, uint32_t a, uint32_t o)
{
    tcg_out_op_loadstore(s, 0x3d, a, o);
}

//static void tcg_out_op_i32_store16(TCGContext *s, uint32_t a, uint32_t o)
//{
//    tcg_out_op_loadstore(s, 0x3b, a, o);
//}

static void tcg_out_op_i64_store32(TCGContext *s, uint32_t a, uint32_t o)
{
    tcg_out_op_loadstore(s, 0x3e, a, o);
}

static void tcg_out_op_i64_load(TCGContext *s, uint32_t a, uint32_t o)
{
    tcg_out_op_loadstore(s, 0x29, a, o);
}

static void tcg_out_op_i32_load(TCGContext *s, uint32_t a, uint32_t o)
{
    tcg_out_op_loadstore(s, 0x28, a, o);
}

 static void tcg_out_op_i64_load8_s(TCGContext *s, uint32_t a, uint32_t o)
{
    tcg_out_op_loadstore(s, 0x30, a, o);
}

//static void tcg_out_op_i32_load8_s(TCGContext *s, uint32_t a, uint32_t o)
//{
//    tcg_out_op_loadstore(s, 0x2c, a, o);
//}
//    
static void tcg_out_op_i64_load8_u(TCGContext *s, uint32_t a, uint32_t o)
{
    tcg_out_op_loadstore(s, 0x31, a, o);
}

//static void tcg_out_op_i32_load8_u(TCGContext *s, uint32_t a, uint32_t o)
//{
//    tcg_out_op_loadstore(s, 0x2d, a, o);
//}

static void tcg_out_op_i64_load16_s(TCGContext *s, uint32_t a, uint32_t o)
{
    tcg_out_op_loadstore(s, 0x32, a, o);
}

//static void tcg_out_op_i32_load16_s(TCGContext *s, uint32_t a, uint32_t o)
//{
//    tcg_out_op_loadstore(s, 0x2e, a, o);
//}

static void tcg_out_op_i64_load16_u(TCGContext *s, uint32_t a, uint32_t o)
{
    tcg_out_op_loadstore(s, 0x33, a, o);
}

//static void tcg_out_op_i32_load16_u(TCGContext *s, uint32_t a, uint32_t o)
//{
//    tcg_out_op_loadstore(s, 0x2f, a, o);
//}

static void tcg_out_op_i64_load32_u(TCGContext *s, uint32_t a, uint32_t o)
{
    tcg_out_op_loadstore(s, 0x35, a, o);
}

static void tcg_out_op_i64_load32_s(TCGContext *s, uint32_t a, uint32_t o)
{
    tcg_out_op_loadstore(s, 0x34, a, o);
}

static void tcg_out_op_return(TCGContext *s)
{
    tcg_out8(s, 0x0f);
}

static void tcg_out_op_call(TCGContext *s, uint32_t func_idx)
{
    tcg_out8(s, 0x10);
    tcg_out_leb128_uint32_t(s, func_idx);
}

static void tcg_out_op_i64_extend_i32_u(TCGContext *s)
{
    tcg_out8(s, 0xad);
}

static void tcg_out_op_i64_extend_i32_s(TCGContext *s)
{
    tcg_out8(s, 0xac);
}

static void tcg_out_op_i64_extend8_s(TCGContext *s)
{
    tcg_out8(s, 0xc2);
}

//static void tcg_out_op_i32_extend8_s(TCGContext *s)
//{
//    tcg_out8(s, 0xc0);
//}

static void tcg_out_op_i64_extend16_s(TCGContext *s)
{
    tcg_out8(s, 0xc3);
}

//static void tcg_out_op_i32_extend16_s(TCGContext *s)
//{
//    tcg_out8(s, 0xc1);
//}

static void tcg_out_op_not(TCGContext *s){
    tcg_out_op_i64_const(s, -1);
    tcg_out_op_i64_xor(s);
}

static void tcg_out_op_set_r_as_i64(TCGContext *s, TCGReg al, TCGReg ah)
{
    tcg_out_op_local_set(s, TMP64_4_IDX);

    // set lower bits
    tcg_out_op_local_get(s, TMP64_4_IDX);
    tcg_out_op_i64_const(s, 0xffffffff);
    tcg_out_op_i64_and(s);
    tcg_out_op_global_set_r(s, al);

    // set higher bits
    tcg_out_op_local_get(s, TMP64_4_IDX);
    tcg_out_op_i64_const(s, 32);
    tcg_out_op_i64_shr_u(s);
    tcg_out_op_i64_const(s, 0xffffffff);
    tcg_out_op_i64_and(s);
    tcg_out_op_global_set_r(s, ah);
}

static const struct {
    uint8_t i32;
    uint8_t i64;
} tcg_cond_to_inst[] = {
    [TCG_COND_EQ] =  { 0x46 /* i32.eq */   , 0x51 /* i64.eq */},
    [TCG_COND_NE] =  { 0x47 /* i32.ne */   , 0x52 /* i64.ne */},
    [TCG_COND_LT] =  { 0x48 /* i32.lt_s */ , 0x53 /* i64.lt_s */},
    [TCG_COND_GE] =  { 0x4e /* i32.ge_s */ , 0x59 /* i64.ge_s */},
    [TCG_COND_LE] =  { 0x4c /* i32.le_s */ , 0x57 /* i64.le_s */},
    [TCG_COND_GT] =  { 0x4a /* i32.gt_s */ , 0x55 /* i64.gt_s */},
    [TCG_COND_LTU] = { 0x49 /* i32.lt_u */ , 0x54 /* i64.lt_u */},
    [TCG_COND_GEU] = { 0x4f /* i32.ge_u */ , 0x5a /* i64.ge_u */},
    [TCG_COND_LEU] = { 0x4d /* i32.le_u */ , 0x58 /* i64.le_u */},
    [TCG_COND_GTU] = { 0x4b /* i32.gt_u */ , 0x56 /* i64.gt_u */}
};

static void tcg_out_op_cond_i64(TCGContext *s, TCGCond cond, TCGReg arg1, TCGReg arg2)
{
    uint8_t op = tcg_cond_to_inst[cond].i64;
    tcg_out_op_global_get_r(s, arg1);
    tcg_out_op_global_get_r(s, arg2);
    tcg_out8(s, op);
}

#define tcg_out_i64_calc(op)                                            \
    static void tcg_out_i64_calc_##op(TCGContext *s, TCGReg ret, TCGReg arg1, TCGReg arg2){ \
        tcg_out_op_global_get_r(s, arg1);                               \
        tcg_out_op_global_get_r(s, arg2);                               \
        tcg_out_op_i64_##op(s);                                         \
        tcg_out_op_global_set_r(s, ret);                                \
    }
tcg_out_i64_calc(and);
tcg_out_i64_calc(or);
tcg_out_i64_calc(xor);
tcg_out_i64_calc(shl);
tcg_out_i64_calc(shr_s);
tcg_out_i64_calc(shr_u);
tcg_out_i64_calc(rotl);
tcg_out_i64_calc(rotr);
tcg_out_i64_calc(add);
tcg_out_i64_calc(sub);
tcg_out_i64_calc(mul);
tcg_out_i64_calc(div_s);
tcg_out_i64_calc(div_u);
tcg_out_i64_calc(rem_s);
tcg_out_i64_calc(rem_u);

static void tcg_out_i32_rotl(TCGContext *s, TCGReg ret, TCGReg arg1, TCGReg arg2){
    tcg_out_op_global_get_r(s, arg1);
    tcg_out_op_i32_wrap_i64(s);
    tcg_out_op_global_get_r(s, arg2);
    tcg_out_op_i32_wrap_i64(s);
    tcg_out_op_i32_rotl(s);
    tcg_out_op_i64_extend_i32_s(s);
    tcg_out_op_global_set_r(s, ret); 
}

static void tcg_out_i32_rotr(TCGContext *s, TCGReg ret, TCGReg arg1, TCGReg arg2){
    tcg_out_op_global_get_r(s, arg1);
    tcg_out_op_i32_wrap_i64(s);
    tcg_out_op_global_get_r(s, arg2);
    tcg_out_op_i32_wrap_i64(s);
    tcg_out_op_i32_rotr(s);
    tcg_out_op_i64_extend_i32_s(s);
    tcg_out_op_global_set_r(s, ret); 
}

static void tcg_out_clz64(TCGContext *s, TCGReg ret, TCGReg arg1, TCGReg arg2){
    tcg_out_op_global_get_r(s, arg1);
    tcg_out_op_i64_eqz(s);
    tcg_out_op_if_ret_i64(s);
    tcg_out_op_global_get_r(s, arg2);
    tcg_out_op_else(s);
    tcg_out_op_global_get_r(s, arg1);
    tcg_out_op_i64_clz(s);
    tcg_out_op_end(s);
    tcg_out_op_global_set_r(s, ret);
}

static void tcg_out_clz32(TCGContext *s, TCGReg ret, TCGReg arg1, TCGReg arg2){
    tcg_out_op_global_get_r(s, arg1);
    tcg_out_op_i64_eqz(s);
    tcg_out_op_if_ret_i32(s);
    tcg_out_op_global_get_r(s, arg2);
    tcg_out_op_i32_wrap_i64(s);
    tcg_out_op_else(s);
    tcg_out_op_global_get_r(s, arg1);
    tcg_out_op_i32_wrap_i64(s);
    tcg_out_op_i32_clz(s);
    tcg_out_op_end(s);
    tcg_out_op_i64_extend_i32_s(s);
    tcg_out_op_global_set_r(s, ret);
}

static void tcg_out_ctz64(TCGContext *s, TCGReg ret, TCGReg arg1, TCGReg arg2){
    tcg_out_op_global_get_r(s, arg1);
    tcg_out_op_i64_eqz(s);
    tcg_out_op_if_ret_i64(s);
    tcg_out_op_global_get_r(s, arg2);
    tcg_out_op_else(s);
    tcg_out_op_global_get_r(s, arg1);
    tcg_out_op_i64_ctz(s);
    tcg_out_op_end(s);
    tcg_out_op_global_set_r(s, ret);
}

static void tcg_out_ctz32(TCGContext *s, TCGReg ret, TCGReg arg1, TCGReg arg2){
    tcg_out_op_global_get_r(s, arg1);
    tcg_out_op_i64_eqz(s);
    tcg_out_op_if_ret_i32(s);
    tcg_out_op_global_get_r(s, arg2);
    tcg_out_op_i32_wrap_i64(s);
    tcg_out_op_else(s);
    tcg_out_op_global_get_r(s, arg1);
    tcg_out_op_i32_wrap_i64(s);
    tcg_out_op_i32_ctz(s);
    tcg_out_op_end(s);
    tcg_out_op_i64_extend_i32_s(s);
    tcg_out_op_global_set_r(s, ret);
}

static void tcg_out_not(TCGContext *s, TCGReg ret, TCGReg arg){
    tcg_out_op_global_get_r(s, arg);
    tcg_out_op_not(s);
    tcg_out_op_global_set_r(s, ret);
}

static void tcg_out_andc(TCGContext *s, TCGReg ret, TCGReg arg1, TCGReg arg2){
    tcg_out_op_global_get_r(s, arg1);
    tcg_out_op_global_get_r(s, arg2);
    tcg_out_op_not(s);
    tcg_out_op_i64_and(s);
    tcg_out_op_global_set_r(s, ret);
}

static void tcg_out_orc(TCGContext *s, TCGReg ret, TCGReg arg1, TCGReg arg2){
    tcg_out_op_global_get_r(s, arg1);
    tcg_out_op_global_get_r(s, arg2);
    tcg_out_op_not(s);
    tcg_out_op_i64_or(s);
    tcg_out_op_global_set_r(s, ret);
}

static void tcg_out_eqv(TCGContext *s, TCGReg ret, TCGReg arg1, TCGReg arg2){
    tcg_out_op_global_get_r(s, arg1);
    tcg_out_op_global_get_r(s, arg2);
    tcg_out_op_i64_xor(s);
    tcg_out_op_not(s);
    tcg_out_op_global_set_r(s, ret);
}

static void tcg_out_nand(TCGContext *s, TCGReg ret, TCGReg arg1, TCGReg arg2){
    tcg_out_op_global_get_r(s, arg1);
    tcg_out_op_global_get_r(s, arg2);
    tcg_out_op_i64_and(s);
    tcg_out_op_not(s);
    tcg_out_op_global_set_r(s, ret);
}

static void tcg_out_nor(TCGContext *s, TCGReg ret, TCGReg arg1, TCGReg arg2){
    tcg_out_op_global_get_r(s, arg1);
    tcg_out_op_global_get_r(s, arg2);
    tcg_out_op_i64_or(s);
    tcg_out_op_not(s);
    tcg_out_op_global_set_r(s, ret);
}

static void tcg_out_neg(TCGContext *s, TCGReg ret, TCGReg arg){
    tcg_out_op_global_get_r(s, arg);
    tcg_out_op_not(s);
    tcg_out_op_i64_const(s, 1);
    tcg_out_op_i64_add(s);
    tcg_out_op_global_set_r(s, ret);
}

static bool patch_reloc(tcg_insn_unit *code_ptr, int type,
                        intptr_t value, intptr_t addend)
{
    return false;
}

static void tcg_out_ld(TCGContext *s, TCGType type, TCGReg val, TCGReg base,
                       intptr_t offset)
{
    switch (type) {
    case TCG_TYPE_I32:
        tcg_out_op_global_get_r_i32(s, base);
        if ((int32_t)offset < 0) {
            tcg_out_op_i32_const(s, (int32_t)offset);
            tcg_out_op_i32_add(s);
            offset = 0;
        }
        tcg_out_op_i64_load32_u(s, 0, (uint32_t)offset);
        tcg_out_op_global_set_r(s, val);
        break;
    case TCG_TYPE_I64:
        tcg_out_op_global_get_r_i32(s, base);
        if ((int32_t)offset < 0) {
            tcg_out_op_i32_const(s, (int32_t)offset);
            tcg_out_op_i32_add(s);
            offset = 0;
        }
        tcg_out_op_i64_load(s, 0, (uint32_t)offset);
        tcg_out_op_global_set_r(s, val);
        break;
    default:
        g_assert_not_reached();
    }
}

static void tcg_out_ld8s(TCGContext *s, TCGType type, TCGReg val, TCGReg base,
                       intptr_t offset)
{
    switch (type) {
    case TCG_TYPE_I32:
    case TCG_TYPE_I64:
        tcg_out_op_global_get_r_i32(s, base);
        if ((int32_t)offset < 0) {
            tcg_out_op_i32_const(s, (int32_t)offset);
            tcg_out_op_i32_add(s);
            offset = 0;
        }
        tcg_out_op_i64_load8_s(s, 0, (uint32_t)offset);
        tcg_out_op_global_set_r(s, val);
        break;
    default:
        g_assert_not_reached();
    }
}

static void tcg_out_ld8u(TCGContext *s, TCGType type, TCGReg val, TCGReg base,
                       intptr_t offset)
{
    switch (type) {
    case TCG_TYPE_I32:
    case TCG_TYPE_I64:
        tcg_out_op_global_get_r_i32(s, base);
        if ((int32_t)offset < 0) {
            tcg_out_op_i32_const(s, (int32_t)offset);
            tcg_out_op_i32_add(s);
            offset = 0;
        }
        tcg_out_op_i64_load8_u(s, 0, (uint32_t)offset);
        tcg_out_op_global_set_r(s, val);
        break;
    default:
        g_assert_not_reached();
    }
}

static void tcg_out_ld16s(TCGContext *s, TCGType type, TCGReg val, TCGReg base,
                       intptr_t offset)
{
    switch (type) {
    case TCG_TYPE_I32:
    case TCG_TYPE_I64:
        tcg_out_op_global_get_r_i32(s, base);
        if ((int32_t)offset < 0) {
            tcg_out_op_i32_const(s, (int32_t)offset);
            tcg_out_op_i32_add(s);
            offset = 0;
        }
        tcg_out_op_i64_load16_s(s, 0, (uint32_t)offset);
        tcg_out_op_global_set_r(s, val);
        break;
    default:
        g_assert_not_reached();
    }
}

static void tcg_out_ld16u(TCGContext *s, TCGType type, TCGReg val, TCGReg base,
                       intptr_t offset)
{
    switch (type) {
    case TCG_TYPE_I32:
    case TCG_TYPE_I64:
        tcg_out_op_global_get_r_i32(s, base);
        if ((int32_t)offset < 0) {
            tcg_out_op_i32_const(s, (int32_t)offset);
            tcg_out_op_i32_add(s);
            offset = 0;
        }
        tcg_out_op_i64_load16_u(s, 0, (uint32_t)offset);
        tcg_out_op_global_set_r(s, val);
        break;
    default:
        g_assert_not_reached();
    }
}

static void tcg_out_ld32s(TCGContext *s, TCGType type, TCGReg val, TCGReg base,
                       intptr_t offset)
{
    switch (type) {
    case TCG_TYPE_I32:
    case TCG_TYPE_I64:
        tcg_out_op_global_get_r_i32(s, base);
        if ((int32_t)offset < 0) {
            tcg_out_op_i32_const(s, (int32_t)offset);
            tcg_out_op_i32_add(s);
            offset = 0;
        }
        tcg_out_op_i64_load32_s(s, 0, (uint32_t)offset);
        tcg_out_op_global_set_r(s, val);
        break;
    default:
        g_assert_not_reached();
    }
}

static void tcg_out_ld32u(TCGContext *s, TCGType type, TCGReg val, TCGReg base,
                       intptr_t offset)
{
    switch (type) {
    case TCG_TYPE_I32:
    case TCG_TYPE_I64:
        tcg_out_op_global_get_r_i32(s, base);
        if ((int32_t)offset < 0) {
            tcg_out_op_i32_const(s, (int32_t)offset);
            tcg_out_op_i32_add(s);
            offset = 0;
        }
        tcg_out_op_i64_load32_u(s, 0, (uint32_t)offset);
        tcg_out_op_global_set_r(s, val);
        break;
    default:
        g_assert_not_reached();
    }
}

static void tcg_out_st(TCGContext *s, TCGType type, TCGReg val, TCGReg base,
                       intptr_t offset)
{
    switch (type) {
    case TCG_TYPE_I32:
        tcg_out_op_global_get_r_i32(s, base);
        if ((int32_t)offset < 0) {
            tcg_out_op_i32_const(s, (int32_t)offset);
            tcg_out_op_i32_add(s);
            offset = 0;
        }
        tcg_out_op_global_get_r(s, val);
        tcg_out_op_i64_store32(s, 0, (uint32_t)offset);
        break;
    case TCG_TYPE_I64:
        tcg_out_op_global_get_r_i32(s, base);
        if ((int32_t)offset < 0) {
            tcg_out_op_i32_const(s, (int32_t)offset);
            tcg_out_op_i32_add(s);
            offset = 0;
        }
        tcg_out_op_global_get_r(s, val);
        tcg_out_op_i64_store(s, 0, (uint32_t)offset);
        break;
    default:
        g_assert_not_reached();
    }
}

static void tcg_out_st8(TCGContext *s, TCGType type, TCGReg val, TCGReg base,
                       intptr_t offset)
{
    switch (type) {
    case TCG_TYPE_I32:
    case TCG_TYPE_I64:
        tcg_out_op_global_get_r_i32(s, base);
        if ((int32_t)offset < 0) {
            tcg_out_op_i32_const(s, (int32_t)offset);
            tcg_out_op_i32_add(s);
            offset = 0;
        }
        tcg_out_op_global_get_r(s, val);
        tcg_out_op_i64_store8(s, 0, (uint32_t)offset);
        break;
    default:
        g_assert_not_reached();
    }
}

static void tcg_out_st16(TCGContext *s, TCGType type, TCGReg val, TCGReg base,
                       intptr_t offset)
{
    switch (type) {
    case TCG_TYPE_I32:
    case TCG_TYPE_I64:
        tcg_out_op_global_get_r_i32(s, base);
        if ((int32_t)offset < 0) {
            tcg_out_op_i32_const(s, (int32_t)offset);
            tcg_out_op_i32_add(s);
            offset = 0;
        }
        tcg_out_op_global_get_r(s, val);
        tcg_out_op_i64_store16(s, 0, (uint32_t)offset);
        break;
    default:
        g_assert_not_reached();
    }
}

static void tcg_out_st32(TCGContext *s, TCGType type, TCGReg val, TCGReg base,
                       intptr_t offset)
{
    switch (type) {
    case TCG_TYPE_I32:
    case TCG_TYPE_I64:
        tcg_out_op_global_get_r_i32(s, base);
        if ((int32_t)offset < 0) {
            tcg_out_op_i32_const(s, (int32_t)offset);
            tcg_out_op_i32_add(s);
            offset = 0;
        }
        tcg_out_op_global_get_r(s, val);
        tcg_out_op_i64_store32(s, 0, (uint32_t)offset);
        break;
    default:
        g_assert_not_reached();
    }
}

static inline bool tcg_out_sti(TCGContext *s, TCGType type, TCGArg val,
                               TCGReg base, intptr_t offset)
{
    tcg_out_op_global_get_r_i32(s, base);
    if ((int32_t)offset < 0) {
        tcg_out_op_i32_const(s, (int32_t)offset);
        tcg_out_op_i32_add(s);
        offset = 0;
    }
    switch (type) {
    case TCG_TYPE_I32:
        tcg_out_op_i64_const(s, (int32_t)val);
        tcg_out_op_i64_store32(s, 0, (uint32_t)offset);
        break;
    case TCG_TYPE_I64:
        tcg_out_op_i64_const(s, val);
        tcg_out_op_i64_store(s, 0, (uint32_t)offset);
        break;
    default:
        g_assert_not_reached();
    }
    return true;
}

static bool tcg_out_mov(TCGContext *s, TCGType type, TCGReg ret, TCGReg arg)
{
   switch (type) {
   case TCG_TYPE_I32:
   case TCG_TYPE_I64:
       tcg_out_op_global_get_r(s, arg);
       tcg_out_op_global_set_r(s, ret);
       break;
   default:
       g_assert_not_reached();
   }
    return true;
}

static void tcg_out_movi(TCGContext *s, TCGType type,
                        TCGReg ret, tcg_target_long arg)
{
   switch (type) {
   case TCG_TYPE_I32:
       tcg_out_op_i64_const(s, (int32_t)arg);
       break;
   case TCG_TYPE_I64:
       tcg_out_op_i64_const(s, arg);
       break;
   default:
       g_assert_not_reached();
   }
   tcg_out_op_global_set_r(s, ret);
}

static void tcg_out_ext8s(TCGContext *s, TCGType type, TCGReg rd, TCGReg rs)
{
    switch (type) {
    case TCG_TYPE_I32:
    case TCG_TYPE_I64:
        tcg_out_op_global_get_r(s, rs);
        tcg_out_op_i64_extend8_s(s);
        tcg_out_op_global_set_r(s, rd);
        break;
    default:
        g_assert_not_reached();
    }
}

static void tcg_out_ext8u(TCGContext *s, TCGReg rd, TCGReg rs)
{
    tcg_out_op_global_get_r(s, rs);
    tcg_out_op_i64_const(s, 0xff);
    tcg_out_op_i64_and(s);
    tcg_out_op_global_set_r(s, rd);
}

static void tcg_out_ext16s(TCGContext *s, TCGType type, TCGReg rd, TCGReg rs)
{
    switch (type) {
    case TCG_TYPE_I32:
    case TCG_TYPE_I64:
        tcg_out_op_global_get_r(s, rs);
        tcg_out_op_i64_extend16_s(s);
        tcg_out_op_global_set_r(s, rd);
        break;
    default:
        g_assert_not_reached();
    }
}

static void tcg_out_ext16u(TCGContext *s, TCGReg rd, TCGReg rs)
{
    tcg_out_op_global_get_r(s, rs);
    tcg_out_op_i64_const(s, 0xffff);
    tcg_out_op_i64_and(s);
    tcg_out_op_global_set_r(s, rd);
}

static void tcg_out_ext32s(TCGContext *s, TCGReg rd, TCGReg rs)
{
    tcg_out_op_global_get_r(s, rs);
    tcg_out_op_i32_wrap_i64(s);
    tcg_out_op_i64_extend_i32_s(s);
    tcg_out_op_global_set_r(s, rd);
}

static void tcg_out_ext32u(TCGContext *s, TCGReg rd, TCGReg rs)
{
    tcg_out_op_global_get_r(s, rs);
    tcg_out_op_i64_const(s, 0xffffffff);
    tcg_out_op_i64_and(s);
    tcg_out_op_global_set_r(s, rd);
}

static void tcg_out_exts_i32_i64(TCGContext *s, TCGReg rd, TCGReg rs)
{
    tcg_out_ext32s(s, rd, rs);
}

static void tcg_out_extu_i32_i64(TCGContext *s, TCGReg rd, TCGReg rs)
{
    tcg_out_ext32u(s, rd, rs);
}

static void tcg_out_extrl_i64_i32(TCGContext *s, TCGReg rd, TCGReg rs)
{
    tcg_out_op_global_get_r(s, rs);
    tcg_out_op_i64_const(s, 0xffffffff);
    tcg_out_op_i64_and(s);
    tcg_out_op_global_set_r(s, rd);
}

static void tcg_out_extrh_i64_i32(TCGContext *s, TCGReg rd, TCGReg rs)
{
    tcg_out_op_global_get_r(s, rs);
    tcg_out_op_i64_const(s, 32);
    tcg_out_op_i64_shr_u(s);
    tcg_out_op_global_set_r(s, rd);
}

static bool tcg_out_xchg(TCGContext *s, TCGType type, TCGReg r1, TCGReg r2)
{
    return false;
}

static void tcg_out_addi_ptr(TCGContext *s, TCGReg rd, TCGReg rs,
                             tcg_target_long imm)
{
    /* This function is only used for passing structs by reference. */
    g_assert_not_reached();
}

static void tcg_out_setcond(TCGContext *s, TCGCond cond, TCGReg ret,
                            TCGReg arg1, TCGReg arg2)
{
    tcg_out_op_cond_i64(s, cond, arg1, arg2);
    tcg_out_op_i64_extend_i32_u(s);
    tcg_out_op_global_set_r(s, ret);
}

static void tcg_out_movcond(TCGContext *s, TCGCond cond, TCGReg ret,
                            TCGReg c1, TCGReg c2, TCGReg v1, TCGReg v2)
{
    tcg_out_op_cond_i64(s, cond, c1, c2);
    tcg_out_op_if_ret_i64(s);
    tcg_out_op_global_get_r(s, v1);
    tcg_out_op_else(s);
    tcg_out_op_global_get_r(s, v2);
    tcg_out_op_end(s);
    tcg_out_op_global_set_r(s, ret);
}

static void tcg_out_add2(TCGContext *s, TCGReg retl, TCGReg reth,
                             TCGReg al, TCGReg ah, TCGReg bl, TCGReg bh)
{
    // add higer
    tcg_out_op_global_get_r(s, ah);
    tcg_out_op_global_get_r(s, bh);
    tcg_out_op_i64_add(s);

    // add lower
    tcg_out_op_global_get_r(s, al);
    tcg_out_op_global_get_r(s, bl);
    tcg_out_op_i64_add(s);

    // get carry
    if ((al == retl) && (bl == retl)) {
        tcg_out_op_local_set(s, TMP64_0_IDX);
        tcg_out_op_local_get(s, TMP64_0_IDX);
        tcg_out_op_global_get_r(s, al);
        tcg_out_op_i64_lt_u(s);
        tcg_out_op_local_get(s, TMP64_0_IDX);
        tcg_out_op_global_set_r(s, retl);
    } else {
        tcg_out_op_global_set_r(s, retl);
        tcg_out_op_global_get_r(s, retl);
        if (al == retl) {
            tcg_out_op_global_get_r(s, bl);
        } else {
            tcg_out_op_global_get_r(s, al);
        }
        tcg_out_op_i64_lt_u(s);
    }

    // add carry to higher
    tcg_out_op_i64_add(s);
    tcg_out_op_global_set_r(s, reth);
}

static void tcg_out_sub2(TCGContext *s, TCGReg retl, TCGReg reth,
                             TCGReg al, TCGReg ah, TCGReg bl, TCGReg bh)
{
    // sub higher
    tcg_out_op_global_get_r(s, ah);
    tcg_out_op_global_get_r(s, bh);
    tcg_out_op_i64_sub(s);

    // sub lower
    tcg_out_op_global_get_r(s, al);
    tcg_out_op_global_get_r(s, bl);
    tcg_out_op_i64_sub(s);

    // get underflow
    if (al == retl) {
        tcg_out_op_local_set(s, TMP64_0_IDX);

        tcg_out_op_local_get(s, TMP64_0_IDX);
        tcg_out_op_global_get_r(s, al);
        tcg_out_op_i64_gt_u(s);

        tcg_out_op_local_get(s, TMP64_0_IDX);
        tcg_out_op_global_set_r(s, retl);
    } else {
        tcg_out_op_global_set_r(s, retl);

        tcg_out_op_global_get_r(s, retl);
        tcg_out_op_global_get_r(s, al);
        tcg_out_op_i64_gt_u(s);
    }

    tcg_out_op_i64_sub(s);
    tcg_out_op_global_set_r(s, reth);
}

static void tcg_out_mulu2_i32(TCGContext *s, TCGReg retl, TCGReg reth, TCGReg arg1, TCGReg arg2)
{
    tcg_out_op_global_get_r(s, arg1);
    tcg_out_op_global_get_r(s, arg2);
    tcg_out_op_i64_mul(s);
    tcg_out_op_set_r_as_i64(s, retl, reth);
}

static void tcg_out_muls2_i32(TCGContext *s, TCGReg retl, TCGReg reth, TCGReg arg1, TCGReg arg2)
{
    tcg_out_op_global_get_r(s, arg1);
    tcg_out_op_global_get_r(s, arg2);
    tcg_out_op_i64_mul(s);
    tcg_out_op_set_r_as_i64(s, retl, reth);
}

static void tcg_out_muluh_i32(TCGContext *s, TCGReg ret, TCGReg arg1, TCGReg arg2)
{
    tcg_out_op_global_get_r(s, arg1);
    tcg_out_op_global_get_r(s, arg2);
    tcg_out_op_i64_mul(s);
    tcg_out_op_i64_const(s, 32);
    tcg_out_op_i64_shr_u(s);
    tcg_out_op_i32_wrap_i64(s);
    tcg_out_op_global_set_r(s, ret);
}

static void tcg_out_mulsh_i32(TCGContext *s, TCGReg ret, TCGReg arg1, TCGReg arg2)
{
    tcg_out_op_global_get_r(s, arg1);
    tcg_out_op_global_get_r(s, arg2);
    tcg_out_op_i64_mul(s);
    tcg_out_op_i64_const(s, 32);
    tcg_out_op_i64_shr_u(s);
    tcg_out_op_i32_wrap_i64(s);
    tcg_out_op_global_set_r(s, ret);
}

static void tcg_out_ctpop(TCGContext *s, TCGReg dest, TCGReg src)
{
    tcg_out_op_global_get_r(s, src);
    tcg_out_op_i64_popcnt(s);
    tcg_out_op_global_set_r(s, dest);
}

static void tcg_out_deposit(TCGContext *s, TCGReg dest, TCGReg arg1, TCGReg arg2, int pos, int len)
{
    int mask = ((1<<len)-1)<<pos;
    tcg_out_op_global_get_r(s, arg1);
    tcg_out_op_i64_const(s, ~mask);
    tcg_out_op_i64_and(s);

    tcg_out_op_global_get_r(s, arg2);
    tcg_out_op_i64_const(s, pos);
    tcg_out_op_i64_shl(s);
    tcg_out_op_i64_const(s, mask);
    tcg_out_op_i64_and(s);

    tcg_out_op_i64_or(s);
    
    tcg_out_op_global_set_r(s, dest);
}

static void tcg_out_extract(TCGContext *s, TCGReg dest, TCGReg arg1, int pos, int len, TCGType type, bool sign)
{
    int rs;
    switch (type) {
    case TCG_TYPE_I32:
        rs = 32 - len;
        break;
    case TCG_TYPE_I64:
        rs = 64 - len;
        break;
    default:
        g_assert_not_reached();
    }
    int sl = rs - pos;
    tcg_out_op_global_get_r(s, arg1);
    if (sl > 0) {
        tcg_out_op_i64_const(s, sl);
        tcg_out_op_i64_shl(s);
    }
    tcg_out_op_i64_const(s, rs);
    if (sign) {
        tcg_out_op_i64_shr_s(s);
    } else {
        tcg_out_op_i64_shr_u(s);
    }
    tcg_out_op_global_set_r(s, dest);
}

static void tcg_out_extract2_i32(TCGContext *s, TCGReg dest, TCGReg arg1, TCGReg arg2, int pos)
{
    tcg_out_op_global_get_r(s, arg2);
    tcg_out_op_i64_const(s, 32-pos);
    tcg_out_op_i64_shl(s);

    tcg_out_op_global_get_r(s, arg1);
    tcg_out_op_i64_const(s, 0xffffffff);
    tcg_out_op_i64_and(s);
    tcg_out_op_i64_const(s, pos);
    tcg_out_op_i64_shr_u(s);

    tcg_out_op_i64_or(s);

    tcg_out_op_i64_const(s, 0xffffffff);
    tcg_out_op_i64_and(s);
    tcg_out_op_global_set_r(s, dest);
}

static void tcg_out_extract2_i64(TCGContext *s, TCGReg dest, TCGReg arg1, TCGReg arg2, int pos)
{
    tcg_out_op_global_get_r(s, arg2);
    tcg_out_op_i64_const(s, 64-pos);
    tcg_out_op_i64_shl(s);
        
    tcg_out_op_global_get_r(s, arg1);
    tcg_out_op_i64_const(s, pos);
    tcg_out_op_i64_shr_u(s);

    tcg_out_op_i64_or(s);
    tcg_out_op_global_set_r(s, dest);
}

static void tcg_out_bswap64(TCGContext *s, TCGReg dest, TCGReg src, int flags)
{
    tcg_out_op_global_get_r(s, src); // ABCDEFGH
    tcg_out_op_i64_const(s, 32);
    tcg_out_op_i64_rotr(s);
    tcg_out_op_local_set(s, TMP64_0_IDX); // EFGHABCD

    tcg_out_op_local_get(s, TMP64_0_IDX);
    tcg_out_op_i64_const(s, 0xf000f000);
    tcg_out_op_i64_and(s);
    tcg_out_op_i64_const(s, 12);
    tcg_out_op_i64_shr_u(s); // ___E___A

    tcg_out_op_local_get(s, TMP64_0_IDX);
    tcg_out_op_i64_const(s, 0x0f000f00);
    tcg_out_op_i64_and(s);
    tcg_out_op_i64_const(s, 4);
    tcg_out_op_i64_shr_u(s); // __F___B_

    tcg_out_op_i64_or(s);

    tcg_out_op_local_get(s, TMP64_0_IDX);
    tcg_out_op_i64_const(s, 0x00f000f0);
    tcg_out_op_i64_and(s);
    tcg_out_op_i64_const(s, 4);
    tcg_out_op_i64_shl(s); // _G___C__

    tcg_out_op_local_get(s, TMP64_0_IDX);
    tcg_out_op_i64_const(s, 0x000f000f);
    tcg_out_op_i64_and(s);
    tcg_out_op_i64_const(s, 12);
    tcg_out_op_i64_shl(s); // H___D___
    
    tcg_out_op_i64_or(s);

    tcg_out_op_i64_or(s); // HGFEDCBA
    tcg_out_op_global_set_r(s, dest);
}

static void tcg_out_bswap32(TCGContext *s, TCGReg dest, TCGReg src, int flags)
{
    tcg_out_op_global_get_r(s, src);
    tcg_out_op_i32_wrap_i64(s);
    tcg_out_op_local_set(s, TMP32_LOCAL_0_IDX);
    
    tcg_out_op_local_get(s, TMP32_LOCAL_0_IDX); // ABCD
    tcg_out_op_i32_const(s, 16);
    tcg_out_op_i32_rotr(s);
    tcg_out_op_local_set(s, TMP32_LOCAL_0_IDX); // CDAB

    tcg_out_op_local_get(s, TMP32_LOCAL_0_IDX);
    tcg_out_op_i32_const(s, 0xff00ff00);
    tcg_out_op_i32_and(s);
    tcg_out_op_i32_const(s, 8);
    tcg_out_op_i32_shr_u(s); // _C_A

    tcg_out_op_local_get(s, TMP32_LOCAL_0_IDX);
    tcg_out_op_i32_const(s, 0x00ff00ff);
    tcg_out_op_i32_and(s);
    tcg_out_op_i32_const(s, 8);
    tcg_out_op_i32_shl(s); // D_B_

    tcg_out_op_i32_or(s); // DCBA
    tcg_out_op_i64_extend_i32_u(s);
    tcg_out_op_global_set_r(s, dest);
}

static void tcg_out_bswap16(TCGContext *s, TCGReg dest, TCGReg src, int flags)
{
    tcg_out_op_global_get_r(s, src);
    tcg_out_op_i32_wrap_i64(s);
    tcg_out_op_local_set(s, TMP32_LOCAL_0_IDX);

    tcg_out_op_local_get(s, TMP32_LOCAL_0_IDX); // __AB
    tcg_out_op_i32_const(s, 8);
    tcg_out_op_i32_rotr(s);
    tcg_out_op_local_set(s, TMP32_LOCAL_0_IDX); // B__A

    tcg_out_op_local_get(s, TMP32_LOCAL_0_IDX);
    tcg_out_op_i32_const(s, 0x000000ff);
    tcg_out_op_i32_and(s); // ___A

    tcg_out_op_local_get(s, TMP32_LOCAL_0_IDX);
    tcg_out_op_i32_const(s, 0xff000000);
    tcg_out_op_i32_and(s);
    tcg_out_op_i32_const(s, 24);
    if (flags & TCG_BSWAP_OS) {
        tcg_out_op_i32_shr_s(s); // SSB_
    } else {
        tcg_out_op_i32_shr_u(s); // 00B_
    }

    tcg_out_op_i32_or(s); // **BA
    tcg_out_op_i64_extend_i32_u(s);
    tcg_out_op_global_set_r(s, dest);
}

static void tcg_out_ctx_i32_store_const(TCGContext *s, int off, int32_t v)
{
    tcg_out_op_local_get(s, CTX_IDX);
    tcg_out_op_i32_const(s, v);
    tcg_out_op_i32_store(s, 0, off);
}

static void tcg_out_ctx_i32_store_r(TCGContext *s, int off, TCGReg r0)
{
    tcg_out_op_local_get(s, CTX_IDX);
    tcg_out_op_global_get_r(s, r0);
    tcg_out_op_i32_wrap_i64(s);
    tcg_out_op_i32_store(s, 0, off);
}

static void tcg_out_ctx_i32_load(TCGContext *s, int off)
{
    tcg_out_op_local_get(s, CTX_IDX);
    tcg_out_op_i32_load(s, 0, off);
}

static void tcg_out_label_idx(TCGContext *s, int label)
{
    int block_idx = wasm_alloc_block_idx(s);
    wasm_add_label_context(s, label, block_idx);

    tcg_out_op_end(s); // end if of the previous block

    // following block
    tcg_out_op_global_get(s, BLOCK_PTR_IDX);
    tcg_out_op_i64_const(s, block_idx);
    tcg_out_op_i64_le_u(s);
    tcg_out_op_if_noret(s);
    env_cached = false;
}

__thread int current_label[100];
__thread int current_label_pos;

static void tcg_out_label_cb(TCGContext *s, TCGLabel *l)
{
    current_label[current_label_pos++] = l->id;
    tcg_debug_assert(current_label_pos < 100);
    tcg_out_label_idx(s, l->id + 1);
}

static void tcg_out_op_br_to_label(TCGContext *s, TCGLabel *l, bool br_if)
{
    int toploop_depth = 1;
    if (br_if) {
        tcg_out_op_if_noret(s);
        toploop_depth++;
    }
    tcg_out8(s, 0x42); // i64.const
    wasm_add_label_block_ptr_placeholder(s, l->id + 1);
    tcg_out8(s, 0x80); // filled before instantiation
    tcg_out8(s, 0x80);
    tcg_out8(s, 0x80);
    tcg_out8(s, 0x80);
    tcg_out8(s, 0x00);
    tcg_out_op_global_set(s, BLOCK_PTR_IDX);
    bool found = false;
    for (int i = 0; i < current_label_pos; i++) {
        if (current_label[i] == l->id) {
            found = true;
            break;
        }
    }
    if (found) {
        tcg_out_op_br(s, toploop_depth); // br to the top of loop
    } else {
        tcg_out_op_br(s, toploop_depth - 1); // br to the end of the current block
    }
    if (br_if) {
        tcg_out_op_end(s);
    }
}

static void tcg_out_br(TCGContext *s, TCGLabel *l)
{
    tcg_out_op_br_to_label(s, l, false);
}

static void tcg_out_brcond(TCGContext *s, TCGCond cond, TCGReg arg1,
                           TCGReg arg2, TCGLabel *l)
{
    tcg_out_op_cond_i64(s, cond, arg1, arg2);
    tcg_out_op_br_to_label(s, l, true);
}

static void tcg_out_exit_tb(TCGContext *s, uintptr_t arg)
{
    tcg_out_ctx_i32_store_const(s, TB_PTR_OFF, 0);
    tcg_out_op_i32_const(s, (int32_t)arg);
    tcg_out_op_return(s);
}

static void tcg_out_goto_ptr(TCGContext *s, TCGReg arg)
{
    tcg_out_op_global_get_r(s, arg);
    tcg_out_op_i32_wrap_i64(s);
    tcg_out_ctx_i32_load(s, TB_PTR_OFF);
    tcg_out_op_i32_eq(s);
    tcg_out_op_if_noret(s);
    tcg_out_op_i64_const(s, 0);
    tcg_out_op_global_set(s, BLOCK_PTR_IDX);
    tcg_out_op_br(s, 2); // br to the top of loop
    tcg_out_op_end(s);

    tcg_out_ctx_i32_store_r(s, TB_PTR_OFF, arg);
    tcg_out_ctx_i32_store_const(s, DO_INIT_OFF, 1);
    tcg_out_op_i32_const(s, 0);
    tcg_out_op_return(s);
}

static void tcg_out_goto_tb(TCGContext *s, int which)
{
    tcg_out_op_i32_const(s, (int32_t)get_jmp_target_addr(s, which));
    tcg_out_op_i32_load(s, 0, 0);
    tcg_out_op_local_set(s, TMP32_LOCAL_0_IDX);

    tcg_out_op_local_get(s, TMP32_LOCAL_0_IDX);
    tcg_out_op_i32_const(s, 0);
    tcg_out_op_i32_ne(s);
    tcg_out_op_if_noret(s);

    tcg_out_op_local_get(s, TMP32_LOCAL_0_IDX);
    tcg_out_ctx_i32_load(s, TB_PTR_OFF);
    tcg_out_op_i32_eq(s);
    tcg_out_op_if_noret(s);
    tcg_out_op_i64_const(s, 0);
    tcg_out_op_global_set(s, BLOCK_PTR_IDX);
    tcg_out_op_br(s, 3); // br to the top of loop
    tcg_out_op_end(s);
    
    // store jmp target address to buf
    tcg_out_op_local_get(s, CTX_IDX);
    tcg_out_op_local_get(s, TMP32_LOCAL_0_IDX);
    tcg_out_op_i32_store(s, 0, TB_PTR_OFF);
    tcg_out_ctx_i32_store_const(s, DO_INIT_OFF, 1);

    tcg_out_op_i32_const(s, 0);
    tcg_out_op_return(s);
    tcg_out_op_end(s);

    set_jmp_reset_offset(s, which);
}

static void push_arg_i64(TCGContext *s, int *reg_idx, int *stack_offset) {
    if (*reg_idx < NUM_OF_IARG_REGS) {
        tcg_out_op_global_get_r(s, REG_INDEX_IARG_BASE + *reg_idx);  // arg register
        int addend = 1;
        *reg_idx = *reg_idx + addend;
    } else {
        tcg_out_op_global_get_r(s, TCG_REG_CALL_STACK);
        tcg_out_op_i64_load(s, 0, *stack_offset);
        int addend = 8;
        *stack_offset = *stack_offset + addend;
    }
}

void gen_func_wrapper_code(TCGContext *s, const tcg_insn_unit *func, const TCGHelperInfo *info, int func_idx)
{
    int nargs;
    unsigned typemask = info->typemask;
    int rettype = typemask & 7;

    if (rettype ==  dh_typecode_i128) {
        // receive 128bit return value via the stack buffer
        tcg_out_op_global_get_r(s, TCG_REG_CALL_STACK);
    }
    
    nargs = 32 - clz32(typemask >> 3);
    nargs = DIV_ROUND_UP(nargs, 3);
    int stack_offset = 0;
    int reg_idx = 0;
    int stack128_base = 0;
    bool cached_128base = true;
    for (int j = 0; j < nargs; ++j) {
        int typecode = extract32(typemask, (j + 1) * 3, 3);
        if (typecode == dh_typecode_void) {
            continue;
        }
        switch (typecode) {
        case dh_typecode_i32:
        case dh_typecode_s32:
        case dh_typecode_ptr:
            push_arg_i64(s, &reg_idx, &stack_offset);
            tcg_out_op_i32_wrap_i64(s);
            break;
        case dh_typecode_i64:
        case dh_typecode_s64:
            push_arg_i64(s, &reg_idx, &stack_offset);
            break;
        case dh_typecode_i128:
            // push current 128stack pointer
            tcg_out_op_local_get(s, TMP64_0_IDX);
            tcg_out_op_i32_const(s, stack128_base);
            tcg_out_op_i32_add(s);

            // copy data to 128stack
            if (!cached_128base) {
                tcg_out_ctx_i32_load(s, STACK128_OFF);
                tcg_out_op_i64_extend_i32_s(s);
                tcg_out_op_local_set(s, TMP64_0_IDX);
                cached_128base = true;
            }

            tcg_out_op_local_get(s, TMP64_0_IDX);
            tcg_out_op_i32_wrap_i64(s);
            push_arg_i64(s, &reg_idx, &stack_offset);
            tcg_out_op_i64_store(s, 0, stack128_base);
            stack128_base += 8;

            tcg_out_op_local_get(s, TMP64_0_IDX);
            tcg_out_op_i32_wrap_i64(s);
            push_arg_i64(s, &reg_idx, &stack_offset);
            tcg_out_op_i64_store(s, 0, stack128_base);
            stack128_base += 8;
            break;
        default:
            g_assert_not_reached();
        }
    }

    tcg_out_op_call(s, func_idx);

    stack_offset = 0;
    if (rettype != dh_typecode_void) {
        switch (rettype) {
        case dh_typecode_i32:
        case dh_typecode_s32:
        case dh_typecode_ptr:
            tcg_out_op_i64_extend_i32_s(s);
            tcg_out_op_global_set_r(s, TCG_REG_A0);
            break;
        case dh_typecode_i64:
        case dh_typecode_s64:
            tcg_out_op_global_set_r(s, TCG_REG_A0);
            break;
        case dh_typecode_i128:
            tcg_out_op_global_get_r(s, TCG_REG_CALL_STACK);
            tcg_out_op_i64_load(s, 0, stack_offset);
            tcg_out_op_global_set_r(s, TCG_REG_A0);
            stack_offset += 8;

            tcg_out_op_global_get_r(s, TCG_REG_CALL_STACK);
            tcg_out_op_i64_load(s, 0, stack_offset);
            tcg_out_op_global_set_r(s, TCG_REG_A1);
            stack_offset += 8;
            break;
        default:
            g_assert_not_reached();
        }
    }

    return;
}

static void gen_func_type(TCGContext *s, const TCGHelperInfo *info)
{
    int nargs;
    unsigned typemask = info->typemask;
    int rettype = typemask & 7;
    uint8_t * buf_start = wasm_get_helper_types_begin(s);
    uint8_t * buf_ptr = buf_start;
    nargs = 32 - clz32(typemask >> 3);
    nargs = DIV_ROUND_UP(nargs, 3);

    *buf_ptr++ = 0x60;
    uint8_t * buf_ptr_vec_size = buf_ptr;
    *buf_ptr += 0x80;
    *buf_ptr += 0x80;
    *buf_ptr += 0x80;
    *buf_ptr += 0x80;
    *buf_ptr += 0x00;
    *buf_ptr++ = 0; // vector size (placeholder)

    int vec_size = 0;
    
    if (rettype == dh_typecode_i128) {
        *buf_ptr++ = 0x7f; // i32 (stack buffer pointer)
        vec_size++;
    }
    
    for (int j = 0; j < nargs; ++j) {
        int typecode = extract32(typemask, (j + 1) * 3, 3);
        if (typecode == dh_typecode_void) {
            continue;
        }
        switch (typecode) {
        case dh_typecode_i32:
        case dh_typecode_s32:
        case dh_typecode_ptr:
            *buf_ptr++ = 0x7f;
            vec_size++;
            break;
        case dh_typecode_i64:
        case dh_typecode_s64:
            *buf_ptr++ = 0x7e;
            vec_size++;
            break;
        case dh_typecode_i128:
            *buf_ptr++ = 0x7f;
            vec_size++;
            break;
        default:
            g_assert_not_reached();
        }
    }
    fill_uint32_leb128((uintptr_t)buf_ptr_vec_size, vec_size); // fill size

    if ((rettype == dh_typecode_void) || (rettype == dh_typecode_i128)) {
        *buf_ptr++ = 0x0; // no return value
    } else {
        *buf_ptr++ = 0x1;
        switch (rettype) {
        case dh_typecode_i32:
        case dh_typecode_s32:
        case dh_typecode_ptr:
            *buf_ptr++ = 0x7f;
            break;
        case dh_typecode_i64:
        case dh_typecode_s64:
            *buf_ptr++ = 0x7e;
            break;
        default:
            g_assert_not_reached();
        }
    }
    int sz = (uint32_t)(buf_ptr - buf_start);
    wasm_add_helper_types_pos(s, sz);
    return;
}

static void gen_func_type_qemu_ld(TCGContext *s, uint32_t oi)
{
    uint8_t * buf_start = wasm_get_helper_types_begin(s);
    uint8_t * buf_ptr = buf_start;
    *buf_ptr++ = 0x60;
    *buf_ptr++ = 0x4;
    *buf_ptr++ = 0x7f;
    *buf_ptr++ = 0x7e;
    *buf_ptr++ = 0x7f;
    *buf_ptr++ = 0x7f;
    *buf_ptr++ = 0x1;
    MemOp mop = get_memop(oi);
    switch (mop & MO_SSIZE) {
    case MO_UQ:
        *buf_ptr++ = 0x7e;
        break;
    default:
        *buf_ptr++ = 0x7e;
        break;
    }
    int sz = (uint32_t)(buf_ptr - buf_start);
    wasm_add_helper_types_pos(s, sz);
}

static void gen_func_type_qemu_st(TCGContext *s, uint32_t oi)
{
    uint8_t * buf_start = wasm_get_helper_types_begin(s);
    uint8_t * buf_ptr = buf_start;
    *buf_ptr++ = 0x60;
    *buf_ptr++ = 0x5;
    *buf_ptr++ = 0x7f;
    *buf_ptr++ = 0x7e;
    MemOp mop = get_memop(oi);
    switch (mop & MO_SSIZE) {
    case MO_UQ:
        *buf_ptr++ = 0x7e;
        break;
    default:
        *buf_ptr++ = 0x7f;
        break;
    }
    *buf_ptr++ = 0x7f;
    *buf_ptr++ = 0x7f;
    *buf_ptr++ = 0x0;
    int sz = (uint32_t)(buf_ptr - buf_start);
    wasm_add_helper_types_pos(s, sz);
}

static void tcg_out_call(TCGContext *s, const tcg_insn_unit *func,
                         const TCGHelperInfo *info)
{
    // set return position
    tcg_out_ctx_i32_load(s, HELPER_RET_TB_PTR_OFF);
    tcg_out_op_i32_const(s, (int32_t)s->code_buf + tcg_current_code_size(s));
    tcg_out_op_i32_store(s, 0, 0);

    int func_idx = get_wasm_helper_idx(s, (int)func);
    if (func_idx < 0) {
        func_idx = wasm_register_helper_alloc_num(s);
        tcg_debug_assert(func >= 0);
        wasm_register_helper(s, func_idx, (int)func);
        gen_func_type(s, info);
    }

    int target_block_idx = wasm_block_current_idx(s) + 1;
    tcg_out_op_i64_const(s, target_block_idx);
    tcg_out_op_global_set(s, BLOCK_PTR_IDX);
    tcg_out_op_end(s); // close if of this block (rewind skips this)
    env_cached = false;

    int block_idx = wasm_alloc_block_idx(s);
    tcg_out_op_global_get(s, BLOCK_PTR_IDX);
    tcg_out_op_i64_const(s, block_idx);
    tcg_out_op_i64_le_u(s);
    tcg_out_op_if_noret(s);
    tcg_out_ctx_i32_store_const(s, UNWINDING_OFF, 0);
    gen_func_wrapper_code(s, func, info, func_idx);

    tcg_out_op_i32_const(s, 1);
    tcg_out_ctx_i32_load(s, UNWINDING_OFF);
    tcg_out_op_i32_eq(s);
    tcg_out_op_if_noret(s);
    tcg_out_op_i32_const(s, 0);
    tcg_out_op_return(s);
    tcg_out_op_end(s);
}

void tb_target_set_jmp_target(const TranslationBlock *tb, int n,
                              uintptr_t jmp_rx, uintptr_t jmp_rw)
{
    /* Always indirect, nothing to do */
}

static uint8_t tcg_out_tlb_load(TCGContext *s, TCGReg addr, MemOpIdx oi, bool is_ld)
{
    MemOp opc = get_memop(oi);
    TCGAtomAlign aa;
    unsigned a_mask;

    aa = atom_and_align_for_opc(s, opc, MO_ATOM_IFALIGN, false);
    a_mask = (1u << aa.align) - 1;

    unsigned s_bits = opc & MO_SIZE;
    unsigned s_mask = (1u << s_bits) - 1;
    tcg_target_long compare_mask;
    int mem_index = get_mmuidx(oi);
    int fast_ofs = tlb_mask_table_ofs(s, mem_index);
    int mask_ofs = fast_ofs + offsetof(CPUTLBDescFast, mask);
    int add_off = offsetof(CPUTLBEntry, addend);

    tcg_out_op_global_get_r(s, addr);
    tcg_out_op_i64_const(s, s->page_bits - CPU_TLB_ENTRY_BITS);
    tcg_out_op_i64_shr_u(s);
    
    tcg_out_op_global_get_r_i32(s, TCG_AREG0);
    int off = mask_ofs;
    if ((int64_t)off < 0) {
        tcg_out_op_i32_const(s, (int64_t)off);
        tcg_out_op_i32_add(s);
        off = 0;
    }
    tcg_out_op_i64_load(s, 0, (uint64_t)off);
    tcg_out_op_local_set(s, TMP64_2_IDX);

    tcg_out_op_local_get(s, TMP64_2_IDX);

    tcg_out_op_i64_and(s);

    tcg_out_op_local_get(s, TMP64_2_IDX);
    tcg_out_op_i64_const(s, 32);
    tcg_out_op_i64_shr_u(s);
    tcg_out_op_i64_add(s);
    
    tcg_out_op_i32_wrap_i64(s);
    tcg_out_op_local_tee(s, TMP32_LOCAL_0_IDX);

    off = is_ld ? offsetof(CPUTLBEntry, addr_read)
        : offsetof(CPUTLBEntry, addr_write);
    if ((int64_t)off < 0) {
        tcg_out_op_i32_const(s, (int64_t)off);
        tcg_out_op_i32_add(s);
        off = 0;
    }
    tcg_out_op_i64_load(s, 0, (uint64_t)off);

    tcg_out_op_global_get_r(s, addr);
    if (a_mask < s_mask) {
        tcg_out_op_i64_const(s, s_mask - a_mask);
        tcg_out_op_i64_add(s);
    }
    compare_mask = (uint64_t)s->page_mask | a_mask;
    tcg_out_op_i64_const(s, compare_mask);
    tcg_out_op_i64_and(s);

    tcg_out_op_i64_eq(s);

    tcg_out_op_i64_const(s, 0);
    tcg_out_op_local_set(s, TMP64_0_IDX);
    
    tcg_out_op_if_noret(s);
    tcg_out_op_local_get(s, TMP32_LOCAL_0_IDX);
    off = add_off;
    if ((int64_t)off < 0) {
        tcg_out_op_i32_const(s, (int64_t)off);
        tcg_out_op_i32_add(s);
        off = 0;
    }
    tcg_out_op_i32_load(s, 0, (uint64_t)off);
    tcg_out_op_i64_extend_i32_u(s);
    tcg_out_op_global_get_r(s, addr);
    tcg_out_op_i64_add(s);
    tcg_out_op_local_set(s, TMP64_0_IDX);
    
    tcg_out_op_end(s);
    
    
    return TMP64_0_IDX;
}

static void tcg_out_qemu_ld_direct(TCGContext *s, TCGReg r, uint8_t base, MemOp opc)
{
    /* Byte swapping is left to middle-end expansion. */
    tcg_debug_assert((opc & MO_BSWAP) == 0);

    switch (opc & (MO_SSIZE)) {
    case MO_UB:
        tcg_out_op_local_get(s, base);
        tcg_out_op_i32_wrap_i64(s);
        tcg_out_op_i64_load8_u(s, 0, 0);
        tcg_out_op_global_set_r(s, r);
        break;
    case MO_SB:
        tcg_out_op_local_get(s, base);
        tcg_out_op_i32_wrap_i64(s);
        tcg_out_op_i64_load8_s(s, 0, 0);
        tcg_out_op_global_set_r(s, r);
        break;
    case MO_UW:
        tcg_out_op_local_get(s, base);
        tcg_out_op_i32_wrap_i64(s);
        tcg_out_op_i64_load16_u(s, 0, 0);
        tcg_out_op_global_set_r(s, r);
        break;
    case MO_SW:
        tcg_out_op_local_get(s, base);
        tcg_out_op_i32_wrap_i64(s);
        tcg_out_op_i64_load16_s(s, 0, 0);
        tcg_out_op_global_set_r(s, r);
        break;
    case MO_UL:
        tcg_out_op_local_get(s, base);
        tcg_out_op_i32_wrap_i64(s);
        tcg_out_op_i64_load32_u(s, 0, 0);
        tcg_out_op_global_set_r(s, r);
        break;
    case MO_SL:
        tcg_out_op_local_get(s, base);
        tcg_out_op_i32_wrap_i64(s);
        tcg_out_op_i64_load32_s(s, 0, 0);
        tcg_out_op_global_set_r(s, r);
        break;
    case MO_UQ:
        tcg_out_op_local_get(s, base);
        tcg_out_op_i32_wrap_i64(s);
        tcg_out_op_i64_load(s, 0, 0);
        tcg_out_op_global_set_r(s, r);
        break;
    default:
        g_assert_not_reached();
    }
}

static void* qemu_ld_helper_ptr(uint32_t oi)
{
    MemOp mop = get_memop(oi);
    switch (mop & MO_SSIZE) {
    case MO_UB:
        return helper_ldub_mmu;
    case MO_SB:
        return helper_ldsb_mmu;
    case MO_UW:
        return helper_lduw_mmu;
    case MO_SW:
        return helper_ldsw_mmu;
    case MO_UL:
        return helper_ldul_mmu;
    case MO_SL:
        return helper_ldsl_mmu;
    case MO_UQ:
        return helper_ldq_mmu;
    default:
        g_assert_not_reached();
    }
}

static void tcg_out_qemu_ld(TCGContext *s, const TCGArg *args, bool is_64)
{
    TCGReg addr_reg;
    TCGReg data_reg;
    MemOpIdx oi;
    MemOp opc;

    data_reg = *args++;
    addr_reg = *args++;
    oi = *args++;
    opc = get_memop(oi);

    uint8_t base = tcg_out_tlb_load(s, addr_reg, oi, true);
    
    tcg_out_op_local_get(s, base);
    tcg_out_op_i64_eqz(s);
    tcg_out_op_if_noret(s);

    // path for miss case
    int helper_func_idx = (uint32_t)qemu_ld_helper_ptr(oi);
    int func_idx = get_wasm_helper_idx(s, helper_func_idx);
    if (func_idx < 0) {
        func_idx = wasm_register_helper_alloc_num(s);
        tcg_debug_assert(helper_func_idx >= 0);
        wasm_register_helper(s, func_idx, helper_func_idx);
        gen_func_type_qemu_ld(s, oi);
    }

    // save function pointer
    tcg_out_ctx_i32_store_const(s, DONE_FLAG_OFF, 0);

    tcg_out_op_else(s);

    // fast path
    tcg_out_qemu_ld_direct(s, data_reg, base, opc);

    tcg_out_op_end(s);

    int target_block_idx = wasm_block_current_idx(s) + 1;
    tcg_out_op_i64_const(s, target_block_idx);
    tcg_out_op_global_set(s, BLOCK_PTR_IDX);

    tcg_out_op_end(s); // close if of this block (rewind skips this)
    env_cached = false;
    
    // block for calling helper (+1)
    int block_idx = wasm_alloc_block_idx(s);
    tcg_out_op_global_get(s, BLOCK_PTR_IDX);
    tcg_out_op_i64_const(s, block_idx);
    tcg_out_op_i64_le_u(s);
    tcg_out_op_if_noret(s);

    tcg_out_op_local_get(s, base);
    tcg_out_op_i64_eqz(s);
    tcg_out_op_if_noret(s);

    // call helper
    tcg_out_op_global_get_r(s, TCG_AREG0);
    tcg_out_op_i32_wrap_i64(s);
    tcg_out_op_global_get_r(s, addr_reg);
    tcg_out_op_i32_const(s, oi);
    tcg_out_op_i32_const(s, (int32_t)s->code_buf + tcg_current_code_size(s));
    tcg_out_op_call(s, func_idx);
    tcg_out_op_global_set_r(s, data_reg);
    tcg_out_ctx_i32_load(s, DONE_FLAG_OFF);
    tcg_out_op_i32_eqz(s);

    tcg_out_op_if_noret(s);
    tcg_out_op_i32_const(s, 0);
    tcg_out_op_return(s);
    tcg_out_op_end(s);
    tcg_out_op_end(s);
}

static void tcg_out_qemu_st_direct(TCGContext *s, TCGReg lo, uint8_t base, MemOp opc)
{
    /* Byte swapping is left to middle-end expansion. */
    tcg_debug_assert((opc & MO_BSWAP) == 0);

    switch (opc & (MO_SSIZE)) {
    case MO_8:
        tcg_out_op_local_get(s, base);
        tcg_out_op_i32_wrap_i64(s);
        tcg_out_op_global_get_r(s, lo);
        tcg_out_op_i64_store8(s, 0, 0);
        break;
    case MO_16:
        tcg_out_op_local_get(s, base);
        tcg_out_op_i32_wrap_i64(s);
        tcg_out_op_global_get_r(s, lo);
        tcg_out_op_i64_store16(s, 0, 0);
        break;
    case MO_32:
        tcg_out_op_local_get(s, base);
        tcg_out_op_i32_wrap_i64(s);
        tcg_out_op_global_get_r(s, lo);
        tcg_out_op_i64_store32(s, 0, 0);
        break;
    case MO_64:
        tcg_out_op_local_get(s, base);
        tcg_out_op_i32_wrap_i64(s);
        tcg_out_op_global_get_r(s, lo);
        tcg_out_op_i64_store(s, 0, 0);
        break;
    default:
        g_assert_not_reached();
    }
}

static void* qemu_st_helper_ptr(uint32_t oi)
{
    MemOp mop = get_memop(oi);
    switch (mop & MO_SIZE) {
    case MO_UB:
        return helper_stb_mmu;
    case MO_UW:
        return helper_stw_mmu;
    case MO_UL:
        return helper_stl_mmu;
    case MO_UQ:
        return helper_stq_mmu;
    default:
        g_assert_not_reached();
    }
}

static void tcg_out_qemu_st(TCGContext *s, const TCGArg *args, bool is_64)
{
    TCGReg addr_reg;
    TCGReg data_reg;
    MemOpIdx oi;
    MemOp opc;

    data_reg = *args++;
    addr_reg = *args++;
    oi = *args++;
    opc = get_memop(oi);

    uint8_t base = tcg_out_tlb_load(s, addr_reg, oi, false);

    tcg_out_op_local_get(s, base);
    tcg_out_op_i64_eqz(s);
    tcg_out_op_if_noret(s);

    // path for miss case
    int helper_func_idx = (uint32_t)qemu_st_helper_ptr(oi);
    int func_idx = get_wasm_helper_idx(s, helper_func_idx);
    if (func_idx < 0) {
        func_idx = wasm_register_helper_alloc_num(s);
        tcg_debug_assert(helper_func_idx >= 0);
        wasm_register_helper(s, func_idx, helper_func_idx);
        gen_func_type_qemu_st(s, oi);
    }

    // save function pointer
    tcg_out_ctx_i32_store_const(s, DONE_FLAG_OFF, 0);

    tcg_out_op_else(s);

    // fast path
    tcg_out_qemu_st_direct(s, data_reg, base, opc);

    tcg_out_op_end(s);

    int target_block_idx = wasm_block_current_idx(s) + 1;
    tcg_out_op_i64_const(s, target_block_idx);
    tcg_out_op_global_set(s, BLOCK_PTR_IDX);

    tcg_out_op_end(s); // close if of this block (rewind skips this)
    env_cached = false;

    // block for calling helper (+1)
    int block_idx = wasm_alloc_block_idx(s);
    tcg_out_op_global_get(s, BLOCK_PTR_IDX);
    tcg_out_op_i64_const(s, block_idx);
    tcg_out_op_i64_le_u(s);
    tcg_out_op_if_noret(s);

    tcg_out_op_local_get(s, base);
    tcg_out_op_i64_eqz(s);
    tcg_out_op_if_noret(s);
    
    // call helper
    tcg_out_op_global_get_r(s, TCG_AREG0);
    tcg_out_op_i32_wrap_i64(s);
    tcg_out_op_global_get_r(s, addr_reg);
    MemOp mop = get_memop(oi);
    switch (mop & MO_SSIZE) {
    case MO_UQ:
        tcg_out_op_global_get_r(s, data_reg);
        break;
    default:
        tcg_out_op_global_get_r(s, data_reg);
        tcg_out_op_i32_wrap_i64(s);
        break;
    }
    tcg_out_op_i32_const(s, oi);
    tcg_out_op_i32_const(s, (int32_t)s->code_buf + tcg_current_code_size(s));
    tcg_out_op_call(s, func_idx);

    tcg_out_ctx_i32_load(s, DONE_FLAG_OFF);
    tcg_out_op_i32_eqz(s);
    tcg_out_op_if_noret(s);
    tcg_out_op_i32_const(s, 0);
    tcg_out_op_return(s);
    tcg_out_op_end(s);
    tcg_out_op_end(s);
}

static void tcg_out_op(TCGContext *s, TCGOpcode opc,
                       const TCGArg args[TCG_MAX_OP_ARGS],
                       const int const_args[TCG_MAX_OP_ARGS])
{
    switch (opc) {
    case INDEX_op_goto_ptr:
        tcg_out_goto_ptr(s, args[0]);
        break;
    case INDEX_op_br:
        tcg_out_br(s, arg_label(args[0]));
        break;
    case INDEX_op_setcond_i32:
    case INDEX_op_setcond_i64:
        tcg_out_setcond(s, args[3], args[0], args[1], args[2]);//
        break;
    case INDEX_op_movcond_i32:
    case INDEX_op_movcond_i64:
        tcg_out_movcond(s, args[5], args[0], args[1], args[2], args[3], args[4]);//
        break;
    case INDEX_op_ld_i64:
        tcg_out_ld(s, TCG_TYPE_I64, args[0], args[1], args[2]);
        break;
    case INDEX_op_ld8s_i32:
    case INDEX_op_ld8s_i64:
        tcg_out_ld8s(s, TCG_TYPE_I64, args[0], args[1], args[2]);
        break;
    case INDEX_op_ld8u_i32:
    case INDEX_op_ld8u_i64:
        tcg_out_ld8u(s, TCG_TYPE_I64, args[0], args[1], args[2]);
        break;
    case INDEX_op_ld16s_i32:
    case INDEX_op_ld16s_i64:
        tcg_out_ld16s(s, TCG_TYPE_I64, args[0], args[1], args[2]);
        break;
    case INDEX_op_ld16u_i32:
    case INDEX_op_ld16u_i64:
        tcg_out_ld16u(s, TCG_TYPE_I64, args[0], args[1], args[2]);
        break;
    case INDEX_op_ld32u_i64:
        tcg_out_ld32u(s, TCG_TYPE_I64, args[0], args[1], args[2]);
        break;
    case INDEX_op_ld_i32:
    case INDEX_op_ld32s_i64:
        tcg_out_ld32s(s, TCG_TYPE_I64, args[0], args[1], args[2]);
        break;
    case INDEX_op_st_i64:
        tcg_out_st(s, TCG_TYPE_I64, args[0], args[1], args[2]);
        break;
    case INDEX_op_st8_i32:
    case INDEX_op_st8_i64:
        tcg_out_st8(s, TCG_TYPE_I64, args[0], args[1], args[2]);
        break;
    case INDEX_op_st16_i32:
    case INDEX_op_st16_i64:
        tcg_out_st16(s, TCG_TYPE_I64, args[0], args[1], args[2]);
        break;
    case INDEX_op_st_i32:
    case INDEX_op_st32_i64:
        tcg_out_st32(s, TCG_TYPE_I64, args[0], args[1], args[2]);
        break;
    case INDEX_op_add_i32:
    case INDEX_op_add_i64:
        tcg_out_i64_calc_add(s, args[0], args[1], args[2]);
        break;
    case INDEX_op_sub_i32:
    case INDEX_op_sub_i64:
        tcg_out_i64_calc_sub(s, args[0], args[1], args[2]);
        break;
    case INDEX_op_mul_i32:
    case INDEX_op_mul_i64:
        tcg_out_i64_calc_mul(s, args[0], args[1], args[2]);
        break;
    case INDEX_op_and_i32:
    case INDEX_op_and_i64:
        tcg_out_i64_calc_and(s, args[0], args[1], args[2]);
        break;
    case INDEX_op_or_i32:
    case INDEX_op_or_i64:
        tcg_out_i64_calc_or(s, args[0], args[1], args[2]);
        break;
    case INDEX_op_xor_i32:
    case INDEX_op_xor_i64:
        tcg_out_i64_calc_xor(s, args[0], args[1], args[2]);
        break;
    case INDEX_op_shl_i32:
    case INDEX_op_shl_i64:
        tcg_out_i64_calc_shl(s, args[0], args[1], args[2]);
        break;
    case INDEX_op_shr_i32:
    case INDEX_op_shr_i64:
        tcg_out_i64_calc_shr_u(s, args[0], args[1], args[2]);
        break;
    case INDEX_op_sar_i32:
    case INDEX_op_sar_i64:
        tcg_out_i64_calc_shr_s(s, args[0], args[1], args[2]);
        break;
    case INDEX_op_rotl_i32:
        tcg_out_i32_rotl(s, args[0], args[1], args[2]);
        break;
    case INDEX_op_rotl_i64:
        tcg_out_i64_calc_rotl(s, args[0], args[1], args[2]);
        break;
    case INDEX_op_rotr_i32:
        tcg_out_i32_rotr(s, args[0], args[1], args[2]);
        break;
    case INDEX_op_rotr_i64:
        tcg_out_i64_calc_rotr(s, args[0], args[1], args[2]);
        break;
    case INDEX_op_div_i32:
    case INDEX_op_div_i64:
        tcg_out_i64_calc_div_s(s, args[0], args[1], args[2]);
        break;
    case INDEX_op_divu_i32:
    case INDEX_op_divu_i64:
        tcg_out_i64_calc_div_u(s, args[0], args[1], args[2]);
        break;
    case INDEX_op_rem_i32:
    case INDEX_op_rem_i64:
        tcg_out_i64_calc_rem_s(s, args[0], args[1], args[2]);
        break;
    case INDEX_op_remu_i32:
    case INDEX_op_remu_i64:
        tcg_out_i64_calc_rem_u(s, args[0], args[1], args[2]);
        break;
    case INDEX_op_andc_i32:
    case INDEX_op_andc_i64:
        tcg_out_andc(s, args[0], args[1], args[2]);
        break;
    case INDEX_op_orc_i32:
    case INDEX_op_orc_i64:
        tcg_out_orc(s, args[0], args[1], args[2]);
        break;
    case INDEX_op_eqv_i32:
    case INDEX_op_eqv_i64:
        tcg_out_eqv(s, args[0], args[1], args[2]);
        break;
    case INDEX_op_nand_i32:
    case INDEX_op_nand_i64:
        tcg_out_nand(s, args[0], args[1], args[2]);
        break;
    case INDEX_op_nor_i32:
    case INDEX_op_nor_i64:
        tcg_out_nor(s, args[0], args[1], args[2]);
        break;
    case INDEX_op_clz_i32:
        tcg_out_clz32(s, args[0], args[1], args[2]);
        break;
    case INDEX_op_clz_i64:
        tcg_out_clz64(s, args[0], args[1], args[2]);
        break;
    case INDEX_op_ctz_i32:
        tcg_out_ctz32(s, args[0], args[1], args[2]);
        break;
    case INDEX_op_ctz_i64:
        tcg_out_ctz64(s, args[0], args[1], args[2]);
        break;
    case INDEX_op_brcond_i32:
    case INDEX_op_brcond_i64:
        tcg_out_brcond(s, args[2], args[0], args[1], arg_label(args[3]));
        break;
    case INDEX_op_neg_i32:
    case INDEX_op_neg_i64:
        tcg_out_neg(s, args[0], args[1]);
        break;
    case INDEX_op_not_i32:
    case INDEX_op_not_i64:
        tcg_out_not(s, args[0], args[1]);
        break;
    case INDEX_op_ctpop_i32:
    case INDEX_op_ctpop_i64:
        tcg_out_ctpop(s, args[1], args[2]);
        break;
    case INDEX_op_add2_i32:
    case INDEX_op_add2_i64:
        tcg_out_add2(s, args[0], args[1], args[2], args[3], args[4], args[5]);
        break;
    case INDEX_op_sub2_i32:
    case INDEX_op_sub2_i64:
        tcg_out_sub2(s, args[0], args[1], args[2], args[3], args[4], args[5]);
        break;
    case INDEX_op_muls2_i32:
        tcg_out_muls2_i32(s, args[0], args[1], args[2], args[3]);
        break;
    case INDEX_op_mulu2_i32:
        tcg_out_mulu2_i32(s, args[0], args[1], args[2], args[3]);
        break;
    case INDEX_op_mulsh_i32:
        tcg_out_mulsh_i32(s, args[0], args[1], args[2]);
        break;
    case INDEX_op_muluh_i32:
        tcg_out_muluh_i32(s, args[0], args[1], args[2]);
        break;
    case INDEX_op_bswap16_i32:
    case INDEX_op_bswap16_i64:
        tcg_out_bswap16(s, args[0], args[1], args[2]);
        break;
    case INDEX_op_bswap32_i32:
    case INDEX_op_bswap32_i64:
        tcg_out_bswap32(s, args[0], args[1], args[2]);
        break;
    case INDEX_op_bswap64_i64:
        tcg_out_bswap64(s, args[0], args[1], args[2]);
        break;
    case INDEX_op_qemu_ld_a32_i32:
    case INDEX_op_qemu_ld_a64_i32:
        tcg_out_qemu_ld(s, args, false);
        break;
    case INDEX_op_qemu_st_a32_i32:
    case INDEX_op_qemu_st_a64_i32:
        tcg_out_qemu_st(s, args, false);
        break;
    case INDEX_op_qemu_ld_a32_i64:
    case INDEX_op_qemu_ld_a64_i64:
        tcg_out_qemu_ld(s, args, true);
        break;
    case INDEX_op_qemu_st_a32_i64:
    case INDEX_op_qemu_st_a64_i64:
        tcg_out_qemu_st(s, args, true);
        break;
    case INDEX_op_mb:
        break;
    case INDEX_op_deposit_i32:
    case INDEX_op_deposit_i64:
        tcg_out_deposit(s, args[0], args[1], args[2], args[3], args[4]);
        break;
    case INDEX_op_extract_i32:
        tcg_out_extract(s, args[0], args[1], args[2], args[3], TCG_TYPE_I32, false);
        break;
    case INDEX_op_extract_i64:
        tcg_out_extract(s, args[0], args[1], args[2], args[3], TCG_TYPE_I64, false);
        break;
    case INDEX_op_sextract_i32:
        tcg_out_extract(s, args[0], args[1], args[2], args[3], TCG_TYPE_I32, true);
        break;
    case INDEX_op_sextract_i64:
        tcg_out_extract(s, args[0], args[1], args[2], args[3], TCG_TYPE_I64, true);
        break;
    case INDEX_op_extrl_i64_i32:
        tcg_out_extrl_i64_i32(s, args[0], args[1]);
        break;
    case INDEX_op_extrh_i64_i32:
        tcg_out_extrh_i64_i32(s, args[0], args[1]);
        break;
    case INDEX_op_extract2_i32:
        tcg_out_extract2_i32(s, args[0], args[1], args[2], args[3]);
        break;
    case INDEX_op_extract2_i64:
        tcg_out_extract2_i64(s, args[0], args[1], args[2], args[3]);
        break;
    default:
        g_assert_not_reached();
    }
}

void tcg_out_init() {
    current_label_pos = 0;
    env_cached = false;
}

/* Test if a constant matches the constraint. */
static bool tcg_target_const_match(int64_t val, TCGType type, int ct)
{
    return ct & TCG_CT_CONST;
}

bool tcg_target_has_memory_bswap(MemOp memop)
{
    return false;
}

static void tcg_target_init(TCGContext *s)
{
    /* The current code uses uint8_t for tcg operations. */
    tcg_debug_assert(tcg_op_defs_max <= UINT8_MAX);

    /* Registers available for 32 bit operations. */
    tcg_target_available_regs[TCG_TYPE_I64] = 0xffffff;
    tcg_target_available_regs[TCG_TYPE_I32] = 0xffffff;
    /*
     * The interpreter "registers" are in the local stack frame and
     * cannot be clobbered by the called helper functions.  However,
     * the interpreter assumes a 64-bit return value and assigns to
     * the return value registers.
     */
    tcg_target_call_clobber_regs =
        MAKE_64BIT_MASK(TCG_REG_A0, 64 / TCG_TARGET_REG_BITS);

    s->reserved_regs = 0;
    tcg_regset_set_reg(s->reserved_regs, TCG_REG_TMP);
    tcg_regset_set_reg(s->reserved_regs, TCG_REG_CALL_STACK);

    /* The call arguments come first, followed by the temp storage. */
    tcg_set_frame(s, TCG_REG_CALL_STACK, TCG_STATIC_CALL_ARGS_SIZE,
                  TCG_STATIC_FRAME_SIZE);
}

/* Generate global QEMU prologue and epilogue code. */
static inline void tcg_target_qemu_prologue(TCGContext *s)
{
}
