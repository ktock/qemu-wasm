/* SPDX-License-Identifier: MIT */
/*
 * Tiny Code Generator for QEMU
 *
 * Copyright (c) 2009, 2011 Stefan Weil
 *
 * Based on tci/tcg-target.c.inc
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

#include "qemu/queue.h"

typedef uint32_t tcg_insn_unit_tci;

static const int tcg_target_reg_alloc_order[] = {
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
    /* 2 of these are call clobbered, so use them last. */
    TCG_REG_R1,
    TCG_REG_R0,
};

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
};
#endif

/* No call arguments via registers.  All will be stored on the "stack". */
static const int tcg_target_call_iarg_regs[] = { };

static TCGReg tcg_target_call_oarg_reg(TCGCallReturnKind kind, int slot)
{
    tcg_debug_assert(kind == TCG_CALL_RET_NORMAL);
    tcg_debug_assert(slot >= 0 && slot < 128 / TCG_TARGET_REG_BITS);
    return TCG_REG_R0 + slot;
}

static TCGConstraintSetIndex
tcg_target_op_def(TCGOpcode op, TCGType type, unsigned flags)
{
    return C_NotImplemented;
}

/* Test if a constant matches the constraint. */
static bool tcg_target_const_match(int64_t val, int ct,
                                   TCGType type, TCGCond cond, int vece)
{
    return ct & TCG_CT_CONST;
}

static void tcg_out_nop_fill(tcg_insn_unit *p, int count)
{
    memset(p, 0, sizeof(*p) * count);
}

static bool patch_reloc(tcg_insn_unit *code_ptr, int type,
                        intptr_t value, intptr_t addend)
{
    intptr_t diff = value - (intptr_t)(code_ptr + 4);

    tcg_debug_assert(addend == 0);
    tcg_debug_assert(type == 20);

    if (diff == sextract32(diff, 0, type)) {
        tcg_patch32(code_ptr,
                    deposit32(*(uint32_t *)code_ptr, 32 - type, type, diff));
        return true;
    }
    return false;
}

/* converts a TCG register to a wasm variable index */
static const uint8_t tcg_target_reg_index[TCG_TARGET_NB_REGS] = {
    0,  /* TCG_REG_R0 */
    1,  /* TCG_REG_R1 */
    2,  /* TCG_REG_R2 */
    3,  /* TCG_REG_R3 */
    4,  /* TCG_REG_R4 */
    5,  /* TCG_REG_R5 */
    6,  /* TCG_REG_R6 */
    7,  /* TCG_REG_R7 */
    8,  /* TCG_REG_R8 */
    9,  /* TCG_REG_R9 */
    10, /* TCG_REG_R10 */
    11, /* TCG_REG_R11 */
    12, /* TCG_REG_R12 */
    13, /* TCG_REG_R13 */
    14, /* TCG_REG_R14 */
    15, /* TCG_REG_R15 */
};

#define REG_IDX(r) tcg_target_reg_index[r]

typedef enum {
    OPC_IF = 0x04,
    OPC_ELSE = 0x05,
    OPC_END = 0x0b,
    OPC_GLOBAL_GET = 0x23,
    OPC_GLOBAL_SET = 0x24,

    OPC_I64_LOAD = 0x29,
    OPC_I64_LOAD8_S = 0x30,
    OPC_I64_LOAD8_U = 0x31,
    OPC_I64_LOAD16_S = 0x32,
    OPC_I64_LOAD16_U = 0x33,
    OPC_I64_LOAD32_S = 0x34,
    OPC_I64_LOAD32_U = 0x35,
    OPC_I64_STORE = 0x37,
    OPC_I64_STORE8 = 0x3c,
    OPC_I64_STORE16 = 0x3d,
    OPC_I64_STORE32 = 0x3e,

    OPC_I32_CONST = 0x41,
    OPC_I64_CONST = 0x42,

    OPC_I32_EQ = 0x46,
    OPC_I32_NE = 0x47,
    OPC_I32_LT_S = 0x48,
    OPC_I32_LT_U = 0x49,
    OPC_I32_GT_S = 0x4a,
    OPC_I32_GT_U = 0x4b,
    OPC_I32_LE_S = 0x4c,
    OPC_I32_LE_U = 0x4d,
    OPC_I32_GE_S = 0x4e,
    OPC_I32_GE_U = 0x4f,

    OPC_I64_EQ = 0x51,
    OPC_I64_NE = 0x52,
    OPC_I64_LT_S = 0x53,
    OPC_I64_LT_U = 0x54,
    OPC_I64_GT_S = 0x55,
    OPC_I64_GT_U = 0x56,
    OPC_I64_LE_S = 0x57,
    OPC_I64_LE_U = 0x58,
    OPC_I64_GE_S = 0x59,
    OPC_I64_GE_U = 0x5a,

    OPC_I32_ADD = 0x6a,
    OPC_I32_DIV_S = 0x6d,
    OPC_I32_DIV_U = 0x6e,
    OPC_I32_REM_S = 0x6f,
    OPC_I32_REM_U = 0x70,
    OPC_I32_SHR_S = 0x75,
    OPC_I32_SHR_U = 0x76,

    OPC_I64_ADD = 0x7c,
    OPC_I64_SUB = 0x7d,
    OPC_I64_MUL = 0x7e,
    OPC_I64_DIV_S = 0x7f,
    OPC_I64_DIV_U = 0x80,
    OPC_I64_REM_S = 0x81,
    OPC_I64_REM_U = 0x82,
    OPC_I64_AND = 0x83,
    OPC_I64_OR = 0x84,
    OPC_I64_XOR = 0x85,
    OPC_I64_SHL = 0x86,
    OPC_I64_SHR_S = 0x87,
    OPC_I64_SHR_U = 0x88,

    OPC_I32_WRAP_I64 = 0xa7,
    OPC_I64_EXTEND_I32_S = 0xac,
    OPC_I64_EXTEND_I32_U = 0xad,
    OPC_I64_EXTEND8_S = 0xc2,
    OPC_I64_EXTEND16_S = 0xc3,
} WasmInsn;

typedef enum {
    BLOCK_I64 = 0x7e,
} WasmBlockType;

#define BUF_SIZE 1024
typedef struct LinkedBufEntry {
    uint8_t data[BUF_SIZE];
    uint32_t size;
    QSIMPLEQ_ENTRY(LinkedBufEntry) entry;
} LinkedBufEntry;

typedef QSIMPLEQ_HEAD(, LinkedBufEntry) LinkedBuf;

static void linked_buf_out8(LinkedBuf *linked_buf, uint8_t v)
{
    LinkedBufEntry *buf = QSIMPLEQ_LAST(linked_buf, LinkedBufEntry, entry);
    if (!buf || (buf->size == BUF_SIZE)) {
        LinkedBufEntry *e = tcg_malloc(sizeof(LinkedBufEntry));
        e->size = 0;
        QSIMPLEQ_INSERT_TAIL(linked_buf, e, entry);
        buf = e;
    }
    buf->data[buf->size++] = v;
}

static void linked_buf_out_leb128(LinkedBuf *p, uint64_t v)
{
    uint8_t b;
    do {
        b = v & 0x7f;
        v >>= 7;
        if (v != 0) {
            b |= 0x80;
        }
        linked_buf_out8(p, b);
    } while (v != 0);
}

static void linked_buf_out_sleb128(LinkedBuf *p, int64_t v)
{
    bool more = true;
    uint8_t b;
    while (more) {
        b = v & 0x7f;
        v >>= 7;
        if (((v == 0) && ((b & 0x40) == 0)) ||
            ((v == -1) && ((b & 0x40) != 0))) {
            more = false;
        } else {
            b |= 0x80;
        }
        linked_buf_out8(p, b);
    }
}

/*
 * wasm code is generataed in the dynamically allocated buffer which
 * are managed as a linked list.
 */
static __thread LinkedBuf sub_buf;

static void init_sub_buf(void)
{
    QSIMPLEQ_INIT(&sub_buf);
}
static void tcg_wasm_out8(TCGContext *s, uint8_t v)
{
    linked_buf_out8(&sub_buf, v);
}
static void tcg_wasm_out_leb128(TCGContext *s, uint64_t v)
{
    linked_buf_out_leb128(&sub_buf, v);
}
static void tcg_wasm_out_sleb128(TCGContext *s, int64_t v)
{
    linked_buf_out_sleb128(&sub_buf, v);
}

static void tcg_wasm_out_op(TCGContext *s, WasmInsn opc)
{
    tcg_wasm_out8(s, opc);
}
static void tcg_wasm_out_op_idx(TCGContext *s, WasmInsn opc, uint32_t idx)
{
    tcg_wasm_out8(s, opc);
    tcg_wasm_out_leb128(s, idx);
}
static void tcg_wasm_out_op_block(TCGContext *s, WasmInsn opc, WasmBlockType t)
{
    tcg_wasm_out8(s, opc);
    tcg_wasm_out8(s, t);
}
static void tcg_wasm_out_op_const(TCGContext *s, WasmInsn opc, int64_t v)
{
    tcg_wasm_out8(s, opc);
    switch (opc) {
    case OPC_I32_CONST:
        tcg_wasm_out_sleb128(s, (int32_t)v);
        break;
    case OPC_I64_CONST:
        tcg_wasm_out_sleb128(s, v);
        break;
    default:
        g_assert_not_reached();
    }
}

static void tcg_wasm_out_o1_i2(
    TCGContext *s, WasmInsn opc, TCGReg ret, TCGReg arg1, TCGReg arg2)
{
    tcg_wasm_out_op_idx(s, OPC_GLOBAL_GET, REG_IDX(arg1));
    tcg_wasm_out_op_idx(s, OPC_GLOBAL_GET, REG_IDX(arg2));
    tcg_wasm_out_op(s, opc);
    tcg_wasm_out_op_idx(s, OPC_GLOBAL_SET, REG_IDX(ret));
}
static void tcg_wasm_out_o1_i2_type(
    TCGContext *s, TCGType type, WasmInsn opc32, WasmInsn opc64,
    TCGReg ret, TCGReg arg1, TCGReg arg2)
{
    switch (type) {
    case TCG_TYPE_I32:
        tcg_wasm_out_op_idx(s, OPC_GLOBAL_GET, REG_IDX(arg1));
        tcg_wasm_out_op(s, OPC_I32_WRAP_I64);
        tcg_wasm_out_op_idx(s, OPC_GLOBAL_GET, REG_IDX(arg2));
        tcg_wasm_out_op(s, OPC_I32_WRAP_I64);
        tcg_wasm_out_op(s, opc32);
        tcg_wasm_out_op(s, OPC_I64_EXTEND_I32_U);
        tcg_wasm_out_op_idx(s, OPC_GLOBAL_SET, REG_IDX(ret));
        break;
    case TCG_TYPE_I64:
        tcg_wasm_out_o1_i2(s, opc64, ret, arg1, arg2);
        break;
    default:
        g_assert_not_reached();
    }
}

static const struct {
    WasmInsn i32;
    WasmInsn i64;
} tcg_cond_to_inst[] = {
    [TCG_COND_EQ] =  { OPC_I32_EQ,   OPC_I64_EQ },
    [TCG_COND_NE] =  { OPC_I32_NE,   OPC_I64_NE },
    [TCG_COND_LT] =  { OPC_I32_LT_S, OPC_I64_LT_S },
    [TCG_COND_GE] =  { OPC_I32_GE_S, OPC_I64_GE_S },
    [TCG_COND_LE] =  { OPC_I32_LE_S, OPC_I64_LE_S },
    [TCG_COND_GT] =  { OPC_I32_GT_S, OPC_I64_GT_S },
    [TCG_COND_LTU] = { OPC_I32_LT_U, OPC_I64_LT_U },
    [TCG_COND_GEU] = { OPC_I32_GE_U, OPC_I64_GE_U },
    [TCG_COND_LEU] = { OPC_I32_LE_U, OPC_I64_LE_U },
    [TCG_COND_GTU] = { OPC_I32_GT_U, OPC_I64_GT_U }
};

static void tcg_wasm_out_cond(
    TCGContext *s, TCGType type, TCGCond cond, TCGReg arg1, TCGReg arg2)
{
    switch (type) {
    case TCG_TYPE_I32:
        tcg_wasm_out_op_idx(s, OPC_GLOBAL_GET, REG_IDX(arg1));
        tcg_wasm_out_op(s, OPC_I32_WRAP_I64);
        tcg_wasm_out_op_idx(s, OPC_GLOBAL_GET, REG_IDX(arg2));
        tcg_wasm_out_op(s, OPC_I32_WRAP_I64);
        tcg_wasm_out_op(s, tcg_cond_to_inst[cond].i32);
        break;
    case TCG_TYPE_I64:
        tcg_wasm_out_op_idx(s, OPC_GLOBAL_GET, REG_IDX(arg1));
        tcg_wasm_out_op_idx(s, OPC_GLOBAL_GET, REG_IDX(arg2));
        tcg_wasm_out_op(s, tcg_cond_to_inst[cond].i64);
        break;
    default:
        g_assert_not_reached();
    }
}

static void tcg_wasm_out_setcond(TCGContext *s, TCGType type, TCGReg ret,
                                 TCGReg arg1, TCGReg arg2, TCGCond cond)
{
    tcg_wasm_out_cond(s, type, cond, arg1, arg2);
    tcg_wasm_out_op(s, OPC_I64_EXTEND_I32_U);
    tcg_wasm_out_op_idx(s, OPC_GLOBAL_SET, REG_IDX(ret));
}

static void tcg_wasm_out_negsetcond(TCGContext *s, TCGType type, TCGReg ret,
                                    TCGReg arg1, TCGReg arg2, TCGCond cond)
{
    tcg_wasm_out_op_const(s, OPC_I64_CONST, 0);
    tcg_wasm_out_cond(s, type, cond, arg1, arg2);
    tcg_wasm_out_op(s, OPC_I64_EXTEND_I32_U);
    tcg_wasm_out_op(s, OPC_I64_SUB);
    tcg_wasm_out_op_idx(s, OPC_GLOBAL_SET, REG_IDX(ret));
}

static void tcg_wasm_out_movcond(TCGContext *s, TCGType type, TCGReg ret,
                                 TCGReg c1, TCGReg c2,
                                 TCGReg v1, TCGReg v2,
                                 TCGCond cond)
{
    tcg_wasm_out_cond(s, type, cond, c1, c2);
    tcg_wasm_out_op_block(s, OPC_IF, BLOCK_I64);
    tcg_wasm_out_op_idx(s, OPC_GLOBAL_GET, REG_IDX(v1));
    tcg_wasm_out_op(s, OPC_ELSE);
    tcg_wasm_out_op_idx(s, OPC_GLOBAL_GET, REG_IDX(v2));
    tcg_wasm_out_op(s, OPC_END);
    tcg_wasm_out_op_idx(s, OPC_GLOBAL_SET, REG_IDX(ret));
}

static void tcg_wasm_out_sextract(TCGContext *s, TCGReg dest, TCGReg arg1,
                                  int pos, int len)
{
    tcg_wasm_out_op_idx(s, OPC_GLOBAL_GET, REG_IDX(arg1));

    if (pos == 0) {
        switch (len) {
        case 8:
            tcg_wasm_out_op(s, OPC_I64_EXTEND8_S);
            break;
        case 16:
            tcg_wasm_out_op(s, OPC_I64_EXTEND16_S);
            break;
        case 32:
            tcg_wasm_out_op(s, OPC_I32_WRAP_I64);
            tcg_wasm_out_op(s, OPC_I64_EXTEND_I32_S);
            break;
        default:
            g_assert_not_reached();
        }
    } else {
        g_assert_not_reached();
    }

    tcg_wasm_out_op_idx(s, OPC_GLOBAL_SET, REG_IDX(dest));
}

static void tcg_wasm_out_extract(TCGContext *s, TCGReg dest, TCGReg arg1,
                                 int pos, int len)
{
    int64_t mask = ~0ULL >> (64 - len);
    tcg_wasm_out_op_idx(s, OPC_GLOBAL_GET, REG_IDX(arg1));
    if (pos > 0) {
        tcg_wasm_out_op_const(s, OPC_I64_CONST, pos);
        tcg_wasm_out_op(s, OPC_I64_SHR_U);
    }
    if ((pos + len) < 64) {
        tcg_wasm_out_op_const(s, OPC_I64_CONST, mask);
        tcg_wasm_out_op(s, OPC_I64_AND);
    }
    tcg_wasm_out_op_idx(s, OPC_GLOBAL_SET, REG_IDX(dest));
}

/*
 * The size of the offset field of Wasm's load/store instruction defers
 * depending on the "-sMEMORY64" flag value: 64bit when "-sMEMORY64=1"
 * and 32bit when "-sMEMORY64=2".
 */
#if defined(WASM64_MEMORY64_2)
typedef uint32_t wasm_ldst_offset_t;
#else
typedef uint64_t wasm_ldst_offset_t;
#endif
static void tcg_wasm_out_op_ldst(
    TCGContext *s, WasmInsn instr, uint32_t a, wasm_ldst_offset_t o)
{
    tcg_wasm_out_op(s, instr);
    tcg_wasm_out_leb128(s, a);
    tcg_wasm_out_leb128(s, (wasm_ldst_offset_t)o);
}

/*
 * tcg_wasm_out_norm_ptr emits instructions to adjust the 64bit pointer value
 * at the top of the stack to satisfy Wasm's memory addressing requirements.
 */
static intptr_t tcg_wasm_out_norm_ptr(TCGContext *s, intptr_t offset)
{
#if defined(WASM64_MEMORY64_2)
    /*
     * If Emscripten's "-sMEMORY64=2" is enabled,
     * the address size is limited to 32bit.
     */
    tcg_wasm_out_op(s, OPC_I32_WRAP_I64);
#endif
    /*
     * Wasm's load/store instructions don't support negative value in
     * the offset field. So this function calculates the target address
     * using the base and the offset and makes the offset field 0.
     */
    if (offset < 0) {
#if defined(WASM64_MEMORY64_2)
        tcg_wasm_out_op_const(s, OPC_I32_CONST, offset);
        tcg_wasm_out_op(s, OPC_I32_ADD);
#else
        tcg_wasm_out_op_const(s, OPC_I64_CONST, offset);
        tcg_wasm_out_op(s, OPC_I64_ADD);
#endif
        offset = 0;
    }
    return offset;
}

static void tcg_wasm_out_ld(
    TCGContext *s, WasmInsn opc, TCGReg val, TCGReg base, intptr_t offset)
{
    tcg_wasm_out_op_idx(s, OPC_GLOBAL_GET, REG_IDX(base));
    offset = tcg_wasm_out_norm_ptr(s, offset);
    tcg_wasm_out_op_ldst(s, opc, 0, offset);
    tcg_wasm_out_op_idx(s, OPC_GLOBAL_SET, REG_IDX(val));
}

static void tcg_wasm_out_st(
    TCGContext *s, WasmInsn opc, TCGReg val, TCGReg base, intptr_t offset)
{
    tcg_wasm_out_op_idx(s, OPC_GLOBAL_GET, REG_IDX(base));
    offset = tcg_wasm_out_norm_ptr(s, offset);
    tcg_wasm_out_op_idx(s, OPC_GLOBAL_GET, REG_IDX(val));
    tcg_wasm_out_op_ldst(s, opc, 0, offset);
}

static void tcg_wasm_out_mov(TCGContext *s, TCGReg ret, TCGReg arg)
{
    tcg_wasm_out_op_idx(s, OPC_GLOBAL_GET, REG_IDX(arg));
    tcg_wasm_out_op_idx(s, OPC_GLOBAL_SET, REG_IDX(ret));
}

static void tcg_wasm_out_movi(TCGContext *s, TCGType type,
                              TCGReg ret, tcg_target_long arg)
{
   switch (type) {
   case TCG_TYPE_I32:
       tcg_wasm_out_op_const(s, OPC_I64_CONST, (int32_t)arg);
       break;
   case TCG_TYPE_I64:
       tcg_wasm_out_op_const(s, OPC_I64_CONST, arg);
       break;
   default:
       g_assert_not_reached();
   }
   tcg_wasm_out_op_idx(s, OPC_GLOBAL_SET, REG_IDX(ret));
}

static void tcg_out_op_ri(TCGContext *s, TCGOpcode op, TCGReg r0, int32_t i1)
{
    tcg_insn_unit_tci insn = 0;

    tcg_debug_assert(i1 == sextract32(i1, 0, 20));
    insn = deposit32(insn, 0, 8, op);
    insn = deposit32(insn, 8, 4, r0);
    insn = deposit32(insn, 12, 20, i1);
    tcg_out32(s, insn);
}

static void tcg_out_op_rr(TCGContext *s, TCGOpcode op, TCGReg r0, TCGReg r1)
{
    tcg_insn_unit_tci insn = 0;

    insn = deposit32(insn, 0, 8, op);
    insn = deposit32(insn, 8, 4, r0);
    insn = deposit32(insn, 12, 4, r1);
    tcg_out32(s, insn);
}

static void tcg_out_op_rrr(TCGContext *s, TCGOpcode op,
                           TCGReg r0, TCGReg r1, TCGReg r2)
{
    tcg_insn_unit_tci insn = 0;

    insn = deposit32(insn, 0, 8, op);
    insn = deposit32(insn, 8, 4, r0);
    insn = deposit32(insn, 12, 4, r1);
    insn = deposit32(insn, 16, 4, r2);
    tcg_out32(s, insn);
}

static void tcg_out_op_rrs(TCGContext *s, TCGOpcode op,
                           TCGReg r0, TCGReg r1, intptr_t i2)
{
    tcg_insn_unit_tci insn = 0;

    tcg_debug_assert(i2 == sextract32(i2, 0, 16));
    insn = deposit32(insn, 0, 8, op);
    insn = deposit32(insn, 8, 4, r0);
    insn = deposit32(insn, 12, 4, r1);
    insn = deposit32(insn, 16, 16, i2);
    tcg_out32(s, insn);
}

static void tcg_out_op_rrbb(TCGContext *s, TCGOpcode op, TCGReg r0,
                            TCGReg r1, uint8_t b2, uint8_t b3)
{
    tcg_insn_unit_tci insn = 0;

    tcg_debug_assert(b2 == extract32(b2, 0, 6));
    tcg_debug_assert(b3 == extract32(b3, 0, 6));
    insn = deposit32(insn, 0, 8, op);
    insn = deposit32(insn, 8, 4, r0);
    insn = deposit32(insn, 12, 4, r1);
    insn = deposit32(insn, 16, 6, b2);
    insn = deposit32(insn, 22, 6, b3);
    tcg_out32(s, insn);
}

static void tcg_out_op_rrrc(TCGContext *s, TCGOpcode op,
                            TCGReg r0, TCGReg r1, TCGReg r2, TCGCond c3)
{
    tcg_insn_unit_tci insn = 0;

    insn = deposit32(insn, 0, 8, op);
    insn = deposit32(insn, 8, 4, r0);
    insn = deposit32(insn, 12, 4, r1);
    insn = deposit32(insn, 16, 4, r2);
    insn = deposit32(insn, 20, 4, c3);
    tcg_out32(s, insn);
}

static void tcg_out_op_rrrrrc(TCGContext *s, TCGOpcode op,
                              TCGReg r0, TCGReg r1, TCGReg r2,
                              TCGReg r3, TCGReg r4, TCGCond c5)
{
    tcg_insn_unit_tci insn = 0;

    insn = deposit32(insn, 0, 8, op);
    insn = deposit32(insn, 8, 4, r0);
    insn = deposit32(insn, 12, 4, r1);
    insn = deposit32(insn, 16, 4, r2);
    insn = deposit32(insn, 20, 4, r3);
    insn = deposit32(insn, 24, 4, r4);
    insn = deposit32(insn, 28, 4, c5);
    tcg_out32(s, insn);
}

static void tgen_and(TCGContext *s, TCGType type,
                     TCGReg a0, TCGReg a1, TCGReg a2)
{
    tcg_out_op_rrr(s, INDEX_op_and, a0, a1, a2);
    tcg_wasm_out_o1_i2(s, OPC_I64_AND, a0, a1, a2);
}

static const TCGOutOpBinary outop_and = {
    .base.static_constraint = C_O1_I2(r, r, r),
    .out_rrr = tgen_and,
};

static void tgen_or(TCGContext *s, TCGType type,
                     TCGReg a0, TCGReg a1, TCGReg a2)
{
    tcg_out_op_rrr(s, INDEX_op_or, a0, a1, a2);
    tcg_wasm_out_o1_i2(s, OPC_I64_OR, a0, a1, a2);
}

static const TCGOutOpBinary outop_or = {
    .base.static_constraint = C_O1_I2(r, r, r),
    .out_rrr = tgen_or,
};

static void tgen_xor(TCGContext *s, TCGType type,
                     TCGReg a0, TCGReg a1, TCGReg a2)
{
    tcg_out_op_rrr(s, INDEX_op_xor, a0, a1, a2);
    tcg_wasm_out_o1_i2(s, OPC_I64_XOR, a0, a1, a2);
}

static const TCGOutOpBinary outop_xor = {
    .base.static_constraint = C_O1_I2(r, r, r),
    .out_rrr = tgen_xor,
};

static void tgen_add(TCGContext *s, TCGType type,
                     TCGReg a0, TCGReg a1, TCGReg a2)
{
    tcg_out_op_rrr(s, INDEX_op_add, a0, a1, a2);
    tcg_wasm_out_o1_i2(s, OPC_I64_ADD, a0, a1, a2);
}

static const TCGOutOpBinary outop_add = {
    .base.static_constraint = C_O1_I2(r, r, r),
    .out_rrr = tgen_add,
};

static void tgen_sub(TCGContext *s, TCGType type,
                     TCGReg a0, TCGReg a1, TCGReg a2)
{
    tcg_out_op_rrr(s, INDEX_op_sub, a0, a1, a2);
    tcg_wasm_out_o1_i2(s, OPC_I64_SUB, a0, a1, a2);
}

static const TCGOutOpSubtract outop_sub = {
    .base.static_constraint = C_O1_I2(r, r, r),
    .out_rrr = tgen_sub,
};

static void tgen_mul(TCGContext *s, TCGType type,
                     TCGReg a0, TCGReg a1, TCGReg a2)
{
    tcg_out_op_rrr(s, INDEX_op_mul, a0, a1, a2);
    tcg_wasm_out_o1_i2(s, OPC_I64_MUL, a0, a1, a2);
}

static const TCGOutOpBinary outop_mul = {
    .base.static_constraint = C_O1_I2(r, r, r),
    .out_rrr = tgen_mul,
};

static void tcg_out_sextract(TCGContext *s, TCGType type, TCGReg rd,
                             TCGReg rs, unsigned pos, unsigned len)
{
    tcg_out_op_rrbb(s, INDEX_op_sextract, rd, rs, pos, len);
    tcg_wasm_out_sextract(s, rd, rs, pos, len);
}

static const TCGOutOpExtract outop_sextract = {
    .base.static_constraint = C_O1_I1(r, r),
    .out_rr = tcg_out_sextract,
};

static void tgen_shl(TCGContext *s, TCGType type,
                     TCGReg a0, TCGReg a1, TCGReg a2)
{
    tcg_out_op_rrr(s, INDEX_op_shl, a0, a1, a2);
    tcg_wasm_out_o1_i2(s, OPC_I64_SHL, a0, a1, a2);
}

static const TCGOutOpBinary outop_shl = {
    .base.static_constraint = C_O1_I2(r, r, r),
    .out_rrr = tgen_shl,
};

static void tgen_shr(TCGContext *s, TCGType type,
                     TCGReg a0, TCGReg a1, TCGReg a2)
{
    TCGReg orig_a1 = a1;
    if (type < TCG_TYPE_REG) {
        tcg_out_op_rrbb(s, INDEX_op_extract, TCG_REG_TMP, a1, 0, 32);
        a1 = TCG_REG_TMP;
    }
    tcg_out_op_rrr(s, INDEX_op_shr, a0, a1, a2);
    tcg_wasm_out_o1_i2_type(s, type, OPC_I32_SHR_U, OPC_I64_SHR_U,
                            a0, orig_a1, a2);
}

static const TCGOutOpBinary outop_shr = {
    .base.static_constraint = C_O1_I2(r, r, r),
    .out_rrr = tgen_shr,
};

static void tgen_sar(TCGContext *s, TCGType type,
                     TCGReg a0, TCGReg a1, TCGReg a2)
{
    TCGReg orig_a1 = a1;
    if (type < TCG_TYPE_REG) {
        tcg_out_op_rrbb(s, INDEX_op_sextract, TCG_REG_TMP, a1, 0, 32);
        a1 = TCG_REG_TMP;
    }
    tcg_out_op_rrr(s, INDEX_op_sar, a0, a1, a2);
    tcg_wasm_out_o1_i2_type(s, type, OPC_I32_SHR_S, OPC_I64_SHR_S,
                            a0, orig_a1, a2);
}

static const TCGOutOpBinary outop_sar = {
    .base.static_constraint = C_O1_I2(r, r, r),
    .out_rrr = tgen_sar,
};

static void tgen_setcond_tci(TCGContext *s, TCGType type, TCGCond cond,
                             TCGReg dest, TCGReg arg1, TCGReg arg2)
{
    TCGOpcode opc = (type == TCG_TYPE_I32
                     ? INDEX_op_tci_setcond32
                     : INDEX_op_setcond);
    tcg_out_op_rrrc(s, opc, dest, arg1, arg2, cond);
}

static void tgen_setcond(TCGContext *s, TCGType type, TCGCond cond,
                         TCGReg dest, TCGReg arg1, TCGReg arg2)
{
    tgen_setcond_tci(s, type, cond, dest, arg1, arg2);
    tcg_wasm_out_setcond(s, type, dest, arg1, arg2, cond);
}

static const TCGOutOpSetcond outop_setcond = {
    .base.static_constraint = C_O1_I2(r, r, r),
    .out_rrr = tgen_setcond,
};

static void tgen_negsetcond(TCGContext *s, TCGType type, TCGCond cond,
                            TCGReg dest, TCGReg arg1, TCGReg arg2)
{
    tgen_setcond_tci(s, type, cond, dest, arg1, arg2);
    tcg_out_op_rr(s, INDEX_op_neg, dest, dest);
    tcg_wasm_out_negsetcond(s, type, dest, arg1, arg2, cond);
}

static const TCGOutOpSetcond outop_negsetcond = {
    .base.static_constraint = C_O1_I2(r, r, r),
    .out_rrr = tgen_negsetcond,
};

static void tgen_movcond(TCGContext *s, TCGType type, TCGCond cond,
                         TCGReg ret, TCGReg c1, TCGArg c2, bool const_c2,
                         TCGArg vt, bool const_vt, TCGArg vf, bool consf_vf)
{
    TCGOpcode opc = (type == TCG_TYPE_I32
                     ? INDEX_op_tci_movcond32
                     : INDEX_op_movcond);
    tcg_out_op_rrrrrc(s, opc, ret, c1, c2, vt, vf, cond);
    tcg_wasm_out_movcond(s, type, ret, c1, c2, vt, vf, cond);
}

static const TCGOutOpMovcond outop_movcond = {
    .base.static_constraint = C_O1_I4(r, r, r, r, r),
    .out = tgen_movcond,
};

static void tcg_tci_out_movi(TCGContext *s, TCGType type,
                             TCGReg ret, tcg_target_long arg)
{
    switch (type) {
    case TCG_TYPE_I32:
        arg = (int32_t)arg;
        /* fall through */
    case TCG_TYPE_I64:
        break;
    default:
        g_assert_not_reached();
    }

    if (arg == sextract32(arg, 0, 20)) {
        tcg_out_op_ri(s, INDEX_op_tci_movi, ret, arg);
    } else {
        tcg_insn_unit_tci insn = 0;

        new_pool_label(s, arg, 20, s->code_ptr, 0);
        insn = deposit32(insn, 0, 8, INDEX_op_tci_movl);
        insn = deposit32(insn, 8, 4, ret);
        tcg_out32(s, insn);
    }
}

static bool tcg_out_mov(TCGContext *s, TCGType type, TCGReg ret, TCGReg arg)
{
    tcg_out_op_rr(s, INDEX_op_mov, ret, arg);
    tcg_wasm_out_mov(s, ret, arg);
    return true;
}

static void tcg_out_movi(TCGContext *s, TCGType type,
                         TCGReg ret, tcg_target_long arg)
{
    tcg_tci_out_movi(s, type, ret, arg);
    tcg_wasm_out_movi(s, type, ret, arg);
}

static void stack_bounds_check(TCGReg base, intptr_t offset)
{
    if (base == TCG_REG_CALL_STACK) {
        tcg_debug_assert(offset >= 0);
        tcg_debug_assert(offset < (TCG_STATIC_CALL_ARGS_SIZE +
                                   TCG_STATIC_FRAME_SIZE));
    }
}

static void tcg_out_ldst(TCGContext *s, TCGOpcode op, TCGReg val,
                         TCGReg base, intptr_t offset)
{
    stack_bounds_check(base, offset);
    if (offset != sextract32(offset, 0, 16)) {
        tcg_tci_out_movi(s, TCG_TYPE_PTR, TCG_REG_TMP, offset);
        tcg_out_op_rrr(s, INDEX_op_add, TCG_REG_TMP, TCG_REG_TMP, base);
        base = TCG_REG_TMP;
        offset = 0;
    }
    tcg_out_op_rrs(s, op, val, base, offset);
}

static void tcg_out_ld(TCGContext *s, TCGType type, TCGReg val, TCGReg base,
                       intptr_t offset)
{
    TCGOpcode op = INDEX_op_ld;
    WasmInsn wasm_opc = OPC_I64_LOAD;

    if (type == TCG_TYPE_I32) {
        op = INDEX_op_ld32u;
        wasm_opc = OPC_I64_LOAD32_U;
    }
    tcg_out_ldst(s, op, val, base, offset);
    tcg_wasm_out_ld(s, wasm_opc, val, base, offset);
}

static void tgen_ld8u(TCGContext *s, TCGType type, TCGReg dest,
                      TCGReg base, ptrdiff_t offset)
{
    tcg_out_ldst(s, INDEX_op_ld8u, dest, base, offset);
    tcg_wasm_out_ld(s, OPC_I64_LOAD8_U, dest, base, offset);
}

static const TCGOutOpLoad outop_ld8u = {
    .base.static_constraint = C_O1_I1(r, r),
    .out = tgen_ld8u,
};

static void tgen_ld8s(TCGContext *s, TCGType type, TCGReg dest,
                      TCGReg base, ptrdiff_t offset)
{
    tcg_out_ldst(s, INDEX_op_ld8s, dest, base, offset);
    tcg_wasm_out_ld(s, OPC_I64_LOAD8_S, dest, base, offset);
}

static const TCGOutOpLoad outop_ld8s = {
    .base.static_constraint = C_O1_I1(r, r),
    .out = tgen_ld8s,
};

static void tgen_ld16u(TCGContext *s, TCGType type, TCGReg dest,
                       TCGReg base, ptrdiff_t offset)
{
    tcg_out_ldst(s, INDEX_op_ld16u, dest, base, offset);
    tcg_wasm_out_ld(s, OPC_I64_LOAD16_U, dest, base, offset);
}

static const TCGOutOpLoad outop_ld16u = {
    .base.static_constraint = C_O1_I1(r, r),
    .out = tgen_ld16u,
};

static void tgen_ld16s(TCGContext *s, TCGType type, TCGReg dest,
                       TCGReg base, ptrdiff_t offset)
{
    tcg_out_ldst(s, INDEX_op_ld16s, dest, base, offset);
    tcg_wasm_out_ld(s, OPC_I64_LOAD16_S, dest, base, offset);
}

static const TCGOutOpLoad outop_ld16s = {
    .base.static_constraint = C_O1_I1(r, r),
    .out = tgen_ld16s,
};

static void tgen_ld32u(TCGContext *s, TCGType type, TCGReg dest,
                       TCGReg base, ptrdiff_t offset)
{
    tcg_out_ldst(s, INDEX_op_ld32u, dest, base, offset);
    tcg_wasm_out_ld(s, OPC_I64_LOAD32_U, dest, base, offset);
}

static const TCGOutOpLoad outop_ld32u = {
    .base.static_constraint = C_O1_I1(r, r),
    .out = tgen_ld32u,
};

static void tgen_ld32s(TCGContext *s, TCGType type, TCGReg dest,
                       TCGReg base, ptrdiff_t offset)
{
    tcg_out_ldst(s, INDEX_op_ld32s, dest, base, offset);
    tcg_wasm_out_ld(s, OPC_I64_LOAD32_S, dest, base, offset);
}

static const TCGOutOpLoad outop_ld32s = {
    .base.static_constraint = C_O1_I1(r, r),
    .out = tgen_ld32s,
};

static void tgen_st8(TCGContext *s, TCGType type, TCGReg data,
                     TCGReg base, ptrdiff_t offset)
{
    tcg_out_ldst(s, INDEX_op_st8, data, base, offset);
    tcg_wasm_out_st(s, OPC_I64_STORE8, data, base, offset);
}

static const TCGOutOpStore outop_st8 = {
    .base.static_constraint = C_O0_I2(r, r),
    .out_r = tgen_st8,
};

static void tgen_st16(TCGContext *s, TCGType type, TCGReg data,
                      TCGReg base, ptrdiff_t offset)
{
    tcg_out_ldst(s, INDEX_op_st16, data, base, offset);
    tcg_wasm_out_st(s, OPC_I64_STORE16, data, base, offset);
}

static const TCGOutOpStore outop_st16 = {
    .base.static_constraint = C_O0_I2(r, r),
    .out_r = tgen_st16,
};

static void tcg_out_st(TCGContext *s, TCGType type, TCGReg val, TCGReg base,
                       intptr_t offset)
{
    TCGOpcode op = INDEX_op_st;
    WasmInsn wasm_opc = OPC_I64_STORE;

    if (type == TCG_TYPE_I32) {
        op = INDEX_op_st32;
        wasm_opc = OPC_I64_STORE32;
    }
    tcg_out_ldst(s, op, val, base, offset);
    tcg_wasm_out_st(s, wasm_opc, val, base, offset);
}

static const TCGOutOpStore outop_st = {
    .base.static_constraint = C_O0_I2(r, r),
    .out_r = tcg_out_st,
};

static inline bool tcg_out_sti(TCGContext *s, TCGType type, TCGArg val,
                               TCGReg base, intptr_t ofs)
{
    return false;
}

static void tcg_out_ext8s(TCGContext *s, TCGType type, TCGReg rd, TCGReg rs)
{
    tcg_out_sextract(s, type, rd, rs, 0, 8);
    tcg_wasm_out_sextract(s, rd, rs, 0, 8);
}

static void tcg_out_ext8u(TCGContext *s, TCGReg rd, TCGReg rs)
{
    tcg_out_op_rrbb(s, INDEX_op_extract, rd, rs, 0, 8);
    tcg_wasm_out_extract(s, rd, rs, 0, 8);
}

static void tcg_out_ext16s(TCGContext *s, TCGType type, TCGReg rd, TCGReg rs)
{
    tcg_out_sextract(s, type, rd, rs, 0, 16);
    tcg_wasm_out_sextract(s, rd, rs, 0, 16);
}

static void tcg_out_ext16u(TCGContext *s, TCGReg rd, TCGReg rs)
{
    tcg_out_op_rrbb(s, INDEX_op_extract, rd, rs, 0, 16);
    tcg_wasm_out_extract(s, rd, rs, 0, 16);
}

static void tcg_out_ext32s(TCGContext *s, TCGReg rd, TCGReg rs)
{
    tcg_out_sextract(s, TCG_TYPE_I64, rd, rs, 0, 32);
    tcg_wasm_out_sextract(s, rd, rs, 0, 32);
}

static void tcg_out_ext32u(TCGContext *s, TCGReg rd, TCGReg rs)
{
    tcg_out_op_rrbb(s, INDEX_op_extract, rd, rs, 0, 32);
    tcg_wasm_out_extract(s, rd, rs, 0, 32);
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
    tcg_out_op_rr(s, INDEX_op_mov, rd, rs);
    tcg_wasm_out_extract(s, rd, rs, 0, 32);
}

static void tgen_extrh_i64_i32(TCGContext *s, TCGType t, TCGReg a0, TCGReg a1)
{
    tcg_out_op_rrbb(s, INDEX_op_extract, a0, a1, 32, 32);
    tcg_wasm_out_extract(s, a0, a1, 32, 32);
}

static const TCGOutOpUnary outop_extrh_i64_i32 = {
    .base.static_constraint = C_O1_I1(r, r),
    .out_rr = tgen_extrh_i64_i32,
};

static void tgen_divs(TCGContext *s, TCGType type,
                      TCGReg a0, TCGReg a1, TCGReg a2)
{
    TCGOpcode opc = (type == TCG_TYPE_I32
                     ? INDEX_op_tci_divs32
                     : INDEX_op_divs);
    tcg_out_op_rrr(s, opc, a0, a1, a2);
    tcg_wasm_out_o1_i2_type(s, type, OPC_I32_DIV_S, OPC_I64_DIV_S, a0, a1, a2);
}

static const TCGOutOpBinary outop_divs = {
    .base.static_constraint = C_O1_I2(r, r, r),
    .out_rrr = tgen_divs,
};

static void tgen_divu(TCGContext *s, TCGType type,
                      TCGReg a0, TCGReg a1, TCGReg a2)
{
    TCGOpcode opc = (type == TCG_TYPE_I32
                     ? INDEX_op_tci_divu32
                     : INDEX_op_divu);
    tcg_out_op_rrr(s, opc, a0, a1, a2);
    tcg_wasm_out_o1_i2_type(s, type, OPC_I32_DIV_U, OPC_I64_DIV_U, a0, a1, a2);
}

static const TCGOutOpBinary outop_divu = {
    .base.static_constraint = C_O1_I2(r, r, r),
    .out_rrr = tgen_divu,
};

static void tgen_rems(TCGContext *s, TCGType type,
                      TCGReg a0, TCGReg a1, TCGReg a2)
{
    TCGOpcode opc = (type == TCG_TYPE_I32
                     ? INDEX_op_tci_rems32
                     : INDEX_op_rems);
    tcg_out_op_rrr(s, opc, a0, a1, a2);
    tcg_wasm_out_o1_i2_type(s, type, OPC_I32_REM_S, OPC_I64_REM_S, a0, a1, a2);
}

static const TCGOutOpBinary outop_rems = {
    .base.static_constraint = C_O1_I2(r, r, r),
    .out_rrr = tgen_rems,
};

static void tgen_remu(TCGContext *s, TCGType type,
                      TCGReg a0, TCGReg a1, TCGReg a2)
{
    TCGOpcode opc = (type == TCG_TYPE_I32
                     ? INDEX_op_tci_remu32
                     : INDEX_op_remu);
    tcg_out_op_rrr(s, opc, a0, a1, a2);
    tcg_wasm_out_o1_i2_type(s, type, OPC_I32_REM_U, OPC_I64_REM_U, a0, a1, a2);
}

static const TCGOutOpBinary outop_remu = {
    .base.static_constraint = C_O1_I2(r, r, r),
    .out_rrr = tgen_remu,
};

static void tcg_out_tb_start(TCGContext *s)
{
    init_sub_buf();
}
