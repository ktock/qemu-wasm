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
#include "../wasm.h"

/* This is included to get the number of threads via tcg_max_ctxs. */
#include "../tcg-internal.h"

/* Used for function call generation. */
#define TCG_TARGET_CALL_STACK_OFFSET 0
#define TCG_TARGET_STACK_ALIGN       8
#define TCG_TARGET_CALL_ARG_I32      TCG_CALL_ARG_NORMAL
#define TCG_TARGET_CALL_ARG_I64      TCG_CALL_ARG_NORMAL
#define TCG_TARGET_CALL_ARG_I128     TCG_CALL_ARG_NORMAL
#define TCG_TARGET_CALL_RET_I128     TCG_CALL_RET_NORMAL

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

/* Global variable used for storing the current block index */
#define BLOCK_IDX 16

/* Local variable pointing to WasmContext */
#define CTX_IDX 0

/* Function index */
#define CHECK_UNWINDING_IDX 0 /* A function to check the Asyncify status */
#define HELPER_IDX_START 1 /* The first index of helper functions */

#define PTR_TYPE 0x7e

typedef enum {
    OPC_UNREACHABLE = 0x00,
    OPC_LOOP = 0x03,
    OPC_IF = 0x04,
    OPC_ELSE = 0x05,
    OPC_END = 0x0b,
    OPC_BR = 0x0c,
    OPC_RETURN = 0x0f,
    OPC_CALL = 0x10,
    OPC_LOCAL_GET = 0x20,
    OPC_GLOBAL_GET = 0x23,
    OPC_GLOBAL_SET = 0x24,

    OPC_I32_LOAD = 0x28,
    OPC_I64_LOAD = 0x29,
    OPC_I64_LOAD8_S = 0x30,
    OPC_I64_LOAD8_U = 0x31,
    OPC_I64_LOAD16_S = 0x32,
    OPC_I64_LOAD16_U = 0x33,
    OPC_I64_LOAD32_S = 0x34,
    OPC_I64_LOAD32_U = 0x35,
    OPC_I32_STORE = 0x36,
    OPC_I64_STORE = 0x37,
    OPC_I64_STORE8 = 0x3c,
    OPC_I64_STORE16 = 0x3d,
    OPC_I64_STORE32 = 0x3e,

    OPC_I32_CONST = 0x41,
    OPC_I64_CONST = 0x42,

    OPC_I32_EQZ = 0x45,
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

    OPC_I64_EQZ = 0x50,
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

    OPC_I32_CLZ = 0x67,
    OPC_I32_CTZ = 0x68,
    OPC_I32_ADD = 0x6a,
    OPC_I32_DIV_S = 0x6d,
    OPC_I32_DIV_U = 0x6e,
    OPC_I32_REM_S = 0x6f,
    OPC_I32_REM_U = 0x70,
    OPC_I32_SHR_S = 0x75,
    OPC_I32_SHR_U = 0x76,
    OPC_I32_ROTL = 0x77,
    OPC_I32_ROTR = 0x78,

    OPC_I64_CLZ = 0x79,
    OPC_I64_CTZ = 0x7a,
    OPC_I64_POPCNT = 0x7b,
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
    OPC_I64_ROTL = 0x89,
    OPC_I64_ROTR = 0x8a,

    OPC_I32_WRAP_I64 = 0xa7,
    OPC_I64_EXTEND_I32_S = 0xac,
    OPC_I64_EXTEND_I32_U = 0xad,
    OPC_I64_EXTEND8_S = 0xc2,
    OPC_I64_EXTEND16_S = 0xc3,
} WasmInsn;

typedef enum {
    BLOCK_NORET = 0x40,
    BLOCK_I64 = 0x7e,
    BLOCK_I32 = 0x7f,
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

static int linked_buf_len(LinkedBuf *p)
{
    int total = 0;
    LinkedBufEntry *e;

    QSIMPLEQ_FOREACH(e, p, entry) {
        total += e->size;
    }
    return total;
}

static int linked_buf_write(LinkedBuf *p, void *dst)
{
    int total = 0;
    LinkedBufEntry *e;

    QSIMPLEQ_FOREACH(e, p, entry) {
        memcpy(dst, e->data, e->size);
        dst += e->size;
        total += e->size;
    }
    return total;
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
static int sub_buf_len(void)
{
    return linked_buf_len(&sub_buf);
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

static void tcg_wasm_out_neg(TCGContext *s, TCGReg ret, TCGReg arg)
{
    tcg_wasm_out_op_const(s, OPC_I64_CONST, 0);
    tcg_wasm_out_op_idx(s, OPC_GLOBAL_GET, REG_IDX(arg));
    tcg_wasm_out_op(s, OPC_I64_SUB);
    tcg_wasm_out_op_idx(s, OPC_GLOBAL_SET, REG_IDX(ret));
}

static void tcg_wasm_out_ctpop64(TCGContext *s, TCGReg ret, TCGReg arg)
{
    tcg_wasm_out_op_idx(s, OPC_GLOBAL_GET, REG_IDX(arg));
    tcg_wasm_out_op(s, OPC_I64_POPCNT);
    tcg_wasm_out_op_idx(s, OPC_GLOBAL_SET, REG_IDX(ret));
}

static void tcg_wasm_out_cz(
    TCGContext *s, TCGType type, WasmInsn opc32, WasmInsn opc64,
    TCGReg ret, TCGReg arg1, TCGReg arg2)
{
    switch (type) {
    case TCG_TYPE_I32:
        tcg_wasm_out_op_idx(s, OPC_GLOBAL_GET, REG_IDX(arg1));
        tcg_wasm_out_op(s, OPC_I32_WRAP_I64);
        tcg_wasm_out_op(s, OPC_I32_EQZ);
        tcg_wasm_out_op_block(s, OPC_IF, BLOCK_I32);
        tcg_wasm_out_op_idx(s, OPC_GLOBAL_GET, REG_IDX(arg2));
        tcg_wasm_out_op(s, OPC_I32_WRAP_I64);
        tcg_wasm_out_op(s, OPC_ELSE);
        tcg_wasm_out_op_idx(s, OPC_GLOBAL_GET, REG_IDX(arg1));
        tcg_wasm_out_op(s, OPC_I32_WRAP_I64);
        tcg_wasm_out_op(s, opc32);
        tcg_wasm_out_op(s, OPC_END);
        tcg_wasm_out_op(s, OPC_I64_EXTEND_I32_U);
        tcg_wasm_out_op_idx(s, OPC_GLOBAL_SET, REG_IDX(ret));
        break;
    case TCG_TYPE_I64:
        tcg_wasm_out_op_idx(s, OPC_GLOBAL_GET, REG_IDX(arg1));
        tcg_wasm_out_op(s, OPC_I64_EQZ);
        tcg_wasm_out_op_block(s, OPC_IF, BLOCK_I64);
        tcg_wasm_out_op_idx(s, OPC_GLOBAL_GET, REG_IDX(arg2));
        tcg_wasm_out_op(s, OPC_ELSE);
        tcg_wasm_out_op_idx(s, OPC_GLOBAL_GET, REG_IDX(arg1));
        tcg_wasm_out_op(s, opc64);
        tcg_wasm_out_op(s, OPC_END);
        tcg_wasm_out_op_idx(s, OPC_GLOBAL_SET, REG_IDX(ret));
        break;
    default:
        g_assert_not_reached();
    }
}

typedef struct LabelInfo {
    int label;
    int block;
    QSIMPLEQ_ENTRY(LabelInfo) entry;
} LabelInfo;

static __thread QSIMPLEQ_HEAD(, LabelInfo) label_info;

static void init_label_info(void)
{
    QSIMPLEQ_INIT(&label_info);
}

static void add_label(int label, int block)
{
    LabelInfo *e = tcg_malloc(sizeof(LabelInfo));
    e->label = label;
    e->block = block;
    QSIMPLEQ_INSERT_TAIL(&label_info, e, entry);
}

typedef struct BlockPlaceholder {
    int label;
    int pos;
    QSIMPLEQ_ENTRY(BlockPlaceholder) entry;
} BlockPlaceholder;

static __thread QSIMPLEQ_HEAD(, BlockPlaceholder) block_placeholder;
static __thread int64_t cur_block_idx;

static void init_blocks(void)
{
    QSIMPLEQ_INIT(&block_placeholder);
    cur_block_idx = 0;
}

static void add_block_placeholder(int label, int pos)
{
    BlockPlaceholder *e = tcg_malloc(sizeof(BlockPlaceholder));
    e->label = label;
    e->pos = pos;
    QSIMPLEQ_INSERT_TAIL(&block_placeholder, e, entry);
}

static int get_block_of_label(int label)
{
    LabelInfo *e;
    QSIMPLEQ_FOREACH(e, &label_info, entry) {
        if (e->label == label) {
            return e->block;
        }
    }
    return -1;
}

static void tcg_wasm_out_new_block(TCGContext *s)
{
    tcg_wasm_out_op(s, OPC_END); /* close this block */

    /* next block */
    tcg_wasm_out_op_idx(s, OPC_GLOBAL_GET, BLOCK_IDX);
    tcg_wasm_out_op_const(s, OPC_I64_CONST, ++cur_block_idx);
    tcg_wasm_out_op(s, OPC_I64_LE_U);
    tcg_wasm_out_op_block(s, OPC_IF, BLOCK_NORET);
}

static void tcg_out_label_cb(TCGContext *s, TCGLabel *l)
{
    add_label(l->id, cur_block_idx + 1);
    tcg_wasm_out_new_block(s);
}

static void tcg_wasm_out_br_to_label(TCGContext *s, TCGLabel *l, bool br_if)
{
    int toploop_depth = 1;
    if (br_if) {
        tcg_wasm_out_op_block(s, OPC_IF, BLOCK_NORET);
        toploop_depth++;
    }
    tcg_wasm_out8(s, OPC_I64_CONST);

    add_block_placeholder(l->id, sub_buf_len());

    tcg_wasm_out8(s, 0x80); /* placeholder for the target block idx */
    tcg_wasm_out8(s, 0x80);
    tcg_wasm_out8(s, 0x80);
    tcg_wasm_out8(s, 0x80);
    tcg_wasm_out8(s, 0x00);
    tcg_wasm_out_op_idx(s, OPC_GLOBAL_SET, BLOCK_IDX);
    if (get_block_of_label(l->id) != -1) {
        /*
         * The label is placed before this br, branch to the top of loop
         */
        tcg_wasm_out_op_idx(s, OPC_BR, toploop_depth);
    } else {
        /*
         * The label will be generated after this br,
         * branch to the end of the current block
         */
        tcg_wasm_out_op_idx(s, OPC_BR, toploop_depth - 1);
    }
    if (br_if) {
        tcg_wasm_out_op(s, OPC_END);
    }
}

static void tcg_wasm_out_br(TCGContext *s, TCGLabel *l)
{
    tcg_wasm_out_br_to_label(s, l, false);
}

static void tcg_wasm_out_brcond(TCGContext *s, TCGType type,
                                TCGReg arg1, TCGReg arg2,
                                TCGCond cond, TCGLabel *l)
{
    tcg_wasm_out_cond(s, type, cond, arg1, arg2);
    tcg_wasm_out_br_to_label(s, l, true);
}

#define CTX_OFFSET(f) offsetof(struct WasmContext, f)

static intptr_t tcg_wasm_out_get_ctx(TCGContext *s, intptr_t off)
{
    tcg_wasm_out_op_idx(s, OPC_LOCAL_GET, CTX_IDX);
    return tcg_wasm_out_norm_ptr(s, off);
}

static void tcg_wasm_out_exit_tb(TCGContext *s, uintptr_t arg)
{
    intptr_t ofs;

    /* Store ctx.tb_ptr = 0 which indicates there is no next TB */
    ofs = tcg_wasm_out_get_ctx(s, CTX_OFFSET(tb_ptr));
    tcg_wasm_out_op_const(s, OPC_I64_CONST, 0);
    tcg_wasm_out_op_ldst(s, OPC_I64_STORE, 0, ofs);

    /* Return the control to the caller */
    tcg_wasm_out_op_const(s, OPC_I64_CONST, arg);
    tcg_wasm_out_op(s, OPC_RETURN);
}

static void tcg_wasm_out_goto(TCGContext *s, TCGReg target, int block_depth)
{
    intptr_t ofs;

    /* Check if the target TB is the same as the current TB */
    tcg_wasm_out_op_idx(s, OPC_GLOBAL_GET, REG_IDX(target));
    ofs = tcg_wasm_out_get_ctx(s, CTX_OFFSET(tb_ptr));
    tcg_wasm_out_op_ldst(s, OPC_I64_LOAD, 0, ofs);
    tcg_wasm_out_op(s, OPC_I64_EQ);

    /*
     * If the target TB is the same as the current TB, no need to return to the
     * caller. Just branch to the top of the current TB.
     */
    tcg_wasm_out_op_block(s, OPC_IF, BLOCK_NORET);
    tcg_wasm_out_op_const(s, OPC_I64_CONST, 0);
    tcg_wasm_out_op_idx(s, OPC_GLOBAL_SET, BLOCK_IDX);
    tcg_wasm_out_op_idx(s, OPC_BR, block_depth); /* br to the top of loop */
    tcg_wasm_out_op(s, OPC_END);

    /* Store the target TB to ctx.tb_ptr and return */
    ofs = tcg_wasm_out_get_ctx(s, CTX_OFFSET(tb_ptr));
    tcg_wasm_out_op_idx(s, OPC_GLOBAL_GET, REG_IDX(target));
    tcg_wasm_out_op_ldst(s, OPC_I64_STORE, 0, ofs);
    tcg_wasm_out_op_const(s, OPC_I64_CONST, 0);
    tcg_wasm_out_op(s, OPC_RETURN);
}

static void tcg_wasm_out_goto_ptr(TCGContext *s, TCGReg arg)
{
    tcg_wasm_out_goto(s, arg, 2);
}

static void tcg_wasm_out_goto_tb(
    TCGContext *s, int which, uintptr_t cur_reset_ptr)
{
    intptr_t ofs;

    /* Set the target TB in the tmp variable. */
    tcg_wasm_out_op_const(s, OPC_I64_CONST, get_jmp_target_addr(s, which));
    ofs = tcg_wasm_out_norm_ptr(s, 0);
    tcg_wasm_out_op_ldst(s, OPC_I64_LOAD, 0, ofs);
    tcg_wasm_out_op_idx(s, OPC_GLOBAL_SET, REG_IDX(TCG_REG_TMP));

    /* Goto the target TB if it's registered. */
    tcg_wasm_out_op_idx(s, OPC_GLOBAL_GET, REG_IDX(TCG_REG_TMP));
    tcg_wasm_out_op_const(s, OPC_I64_CONST, cur_reset_ptr);
    tcg_wasm_out_op(s, OPC_I64_NE);
    tcg_wasm_out_op_block(s, OPC_IF, BLOCK_NORET);
    tcg_wasm_out_goto(s, TCG_REG_TMP, 3);
    tcg_wasm_out_op(s, OPC_END);
}

static void push_arg_i64(TCGContext *s, int *stack_offset)
{
    intptr_t ofs;
    tcg_wasm_out_op_idx(s, OPC_GLOBAL_GET, REG_IDX(TCG_REG_CALL_STACK));
    ofs = tcg_wasm_out_norm_ptr(s, *stack_offset);
    tcg_wasm_out_op_ldst(s, OPC_I64_LOAD, 0, ofs);
    *stack_offset = *stack_offset + 8;
}

static void gen_call(TCGContext *s,
                     const TCGHelperInfo *info, uint32_t func_idx)
{
    unsigned typemask = info->typemask;
    int rettype = typemask & 7;
    int stack_offset = 0;
    intptr_t ofs;

    if (rettype ==  dh_typecode_i128) {
        /* receive 128bit return value via the buffer */
        ofs = tcg_wasm_out_get_ctx(s, CTX_OFFSET(buf128));
        tcg_wasm_out_op_ldst(s, OPC_I64_LOAD, 0, ofs);
    }

    for (typemask >>= 3; typemask; typemask >>= 3) {
        switch (typemask & 7) {
        case dh_typecode_void:
            break;
        case dh_typecode_i32:
        case dh_typecode_s32:
            push_arg_i64(s, &stack_offset);
            tcg_wasm_out_op(s, OPC_I32_WRAP_I64);
            break;
        case dh_typecode_i64:
        case dh_typecode_s64:
            push_arg_i64(s, &stack_offset);
            break;
        case dh_typecode_i128:
            tcg_wasm_out_op_idx(s, OPC_GLOBAL_GET, REG_IDX(TCG_REG_CALL_STACK));
            tcg_wasm_out_op_const(s, OPC_I64_CONST, stack_offset);
            tcg_wasm_out_op(s, OPC_I64_ADD);
            stack_offset += 16;
            break;
        case dh_typecode_ptr:
            push_arg_i64(s, &stack_offset);
            break;
        default:
            g_assert_not_reached();
        }
    }

    tcg_wasm_out_op_idx(s, OPC_CALL, func_idx);

    switch (rettype) {
    case dh_typecode_void:
        break;
    case dh_typecode_i32:
    case dh_typecode_s32:
        tcg_wasm_out_op(s, OPC_I64_EXTEND_I32_S);
        tcg_wasm_out_op_idx(s, OPC_GLOBAL_SET, REG_IDX(TCG_REG_R0));
        break;
    case dh_typecode_i64:
    case dh_typecode_s64:
        tcg_wasm_out_op_idx(s, OPC_GLOBAL_SET, REG_IDX(TCG_REG_R0));
        break;
    case dh_typecode_i128:
        ofs = tcg_wasm_out_get_ctx(s, CTX_OFFSET(buf128));
        tcg_wasm_out_op_ldst(s, OPC_I64_LOAD, 0, ofs);
        ofs = tcg_wasm_out_norm_ptr(s, 0);
        tcg_wasm_out_op_ldst(s, OPC_I64_LOAD, 0, ofs);
        tcg_wasm_out_op_idx(s, OPC_GLOBAL_SET, REG_IDX(TCG_REG_R0));
        ofs = tcg_wasm_out_get_ctx(s, CTX_OFFSET(buf128));
        tcg_wasm_out_op_ldst(s, OPC_I64_LOAD, 0, ofs);
        ofs = tcg_wasm_out_norm_ptr(s, 8);
        tcg_wasm_out_op_ldst(s, OPC_I64_LOAD, 0, ofs);
        tcg_wasm_out_op_idx(s, OPC_GLOBAL_SET, REG_IDX(TCG_REG_R1));
        break;
    case dh_typecode_ptr:
        tcg_wasm_out_op_idx(s, OPC_GLOBAL_SET, REG_IDX(TCG_REG_R0));
        break;
    default:
        g_assert_not_reached();
    }
}

static __thread LinkedBuf types_buf;

static void init_types_buf(void)
{
    QSIMPLEQ_INIT(&types_buf);
}

static void types_buf_out8(uint8_t v)
{
    linked_buf_out8(&types_buf, v);
}

static void gen_func_type_call(TCGContext *s, const TCGHelperInfo *info)
{
    unsigned typemask = info->typemask;
    int rettype = typemask & 7;
    uint32_t vec_size = 0;

    if (rettype == dh_typecode_i128) {
        vec_size++;
    }
    for (int m = typemask >> 3; m; m >>= 3) {
        if ((m & 7) != dh_typecode_void) {
            vec_size++;
        }
    }

    types_buf_out8(0x60);
    linked_buf_out_leb128(&types_buf, vec_size);

    if (rettype == dh_typecode_i128) {
        types_buf_out8(PTR_TYPE);
    }

    for (int m = typemask >> 3; m; m >>= 3) {
        switch (m & 7) {
        case dh_typecode_void:
            break;
        case dh_typecode_i32:
        case dh_typecode_s32:
            types_buf_out8(0x7f);
            break;
        case dh_typecode_i64:
        case dh_typecode_s64:
            types_buf_out8(0x7e);
            break;
        case dh_typecode_i128:
            types_buf_out8(PTR_TYPE);
            break;
        case dh_typecode_ptr:
            types_buf_out8(PTR_TYPE);
            break;
        default:
            g_assert_not_reached();
        }
    }

    switch (rettype) {
    case dh_typecode_void:
    case dh_typecode_i128:
        types_buf_out8(0x0);
        break;
    case dh_typecode_i32:
    case dh_typecode_s32:
        types_buf_out8(0x1);
        types_buf_out8(0x7f);
        break;
    case dh_typecode_i64:
    case dh_typecode_s64:
        types_buf_out8(0x1);
        types_buf_out8(0x7e);
        break;
    case dh_typecode_ptr:
        types_buf_out8(0x1);
        types_buf_out8(PTR_TYPE);
        break;
    default:
        g_assert_not_reached();
    }
}

static __thread LinkedBuf imports_buf;

static void init_imports_buf(void)
{
    QSIMPLEQ_INIT(&imports_buf);
}

static void imports_buf_out8(uint8_t v)
{
    linked_buf_out8(&imports_buf, v);
}

typedef struct HelperInfo {
    intptr_t idx_on_qemu;
    QSIMPLEQ_ENTRY(HelperInfo) entry;
} HelperInfo;

static __thread QSIMPLEQ_HEAD(, HelperInfo) helpers;
__thread uint32_t helper_idx;

static void init_helpers(void)
{
    QSIMPLEQ_INIT(&helpers);
    helper_idx = HELPER_IDX_START;
}

static uint32_t register_helper(TCGContext *s, intptr_t helper_idx_on_qemu)
{
    uint32_t typeidx = helper_idx + 1;
    char buf[11]; /* enough for decimal int max + NULL*/
    int n = snprintf(buf, sizeof(buf), "%d", helper_idx - HELPER_IDX_START);

    tcg_debug_assert(helper_idx_on_qemu >= 0);

    HelperInfo *e = tcg_malloc(sizeof(HelperInfo));
    e->idx_on_qemu = helper_idx_on_qemu;
    QSIMPLEQ_INSERT_TAIL(&helpers, e, entry);

    tcg_debug_assert(n < sizeof(buf));
    imports_buf_out8(6); /* helper */
    imports_buf_out8(0x68);
    imports_buf_out8(0x65);
    imports_buf_out8(0x6c);
    imports_buf_out8(0x70);
    imports_buf_out8(0x65);
    imports_buf_out8(0x72);
    linked_buf_out_leb128(&imports_buf, (uint32_t)n);
    for (int i = 0; i < n; i++) {
        imports_buf_out8(buf[i]);
    }
    imports_buf_out8(0); /* type(0) */
    linked_buf_out_leb128(&imports_buf, typeidx);

    return helper_idx++;
}

static int helpers_len(void)
{
    int n = 0;
    HelperInfo *e;

    QSIMPLEQ_FOREACH(e, &helpers, entry) {
        n++;
    }
    return n;
}

static int helpers_write_to_array(intptr_t *dst)
{
    intptr_t *start = dst;
    HelperInfo *e;

    QSIMPLEQ_FOREACH(e, &helpers, entry) {
        *dst++ = e->idx_on_qemu;
    }
    return (intptr_t)dst - (intptr_t)start;
}

static int64_t get_helper_idx(TCGContext *s, intptr_t helper_idx_on_qemu)
{
    uint32_t idx = HELPER_IDX_START;
    HelperInfo *e;

    QSIMPLEQ_FOREACH(e, &helpers, entry) {
        if (e->idx_on_qemu == helper_idx_on_qemu) {
            return idx;
        }
        idx++;
    }
    return -1;
}

static void tcg_wasm_out_handle_unwinding(TCGContext *s)
{
    tcg_wasm_out_op_idx(s, OPC_CALL, CHECK_UNWINDING_IDX);
    tcg_wasm_out_op(s, OPC_I32_EQZ);
    tcg_wasm_out_op_block(s, OPC_IF, BLOCK_NORET);
    tcg_wasm_out_op_const(s, OPC_I64_CONST, 0);
    /* returns if unwinding */
    tcg_wasm_out_op(s, OPC_RETURN);
    tcg_wasm_out_op(s, OPC_END);
}

static void tcg_wasm_out_call(TCGContext *s, intptr_t func,
                              const TCGHelperInfo *info)
{
    intptr_t ofs;
    int64_t func_idx = get_helper_idx(s, func);
    if (func_idx < 0) {
        func_idx = register_helper(s, func);
        gen_func_type_call(s, info);
    }

    ofs = tcg_wasm_out_get_ctx(s, CTX_OFFSET(tci_tb_ptr));
    tcg_wasm_out_op_ldst(s, OPC_I64_LOAD, 0, ofs);
    ofs = tcg_wasm_out_norm_ptr(s, 0);
    tcg_wasm_out_op_const(s, OPC_I64_CONST, (uint64_t)s->code_ptr);
    tcg_wasm_out_op_ldst(s, OPC_I64_STORE, 0, ofs);

    /*
     * update the block index so that the possible rewinding will
     * skip this block
     */
    tcg_wasm_out_op_const(s, OPC_I64_CONST, cur_block_idx + 1);
    tcg_wasm_out_op_idx(s, OPC_GLOBAL_SET, BLOCK_IDX);
    tcg_wasm_out_new_block(s);

    gen_call(s, info, func_idx);
    tcg_wasm_out_handle_unwinding(s);
}

static void gen_func_type_qemu_ld(TCGContext *s, uint32_t oi)
{
    types_buf_out8(0x60);
    types_buf_out8(0x4);
    types_buf_out8(PTR_TYPE);
    types_buf_out8(0x7e);
    types_buf_out8(0x7f);
    types_buf_out8(PTR_TYPE);
    types_buf_out8(0x1);
    types_buf_out8(0x7e);
}

static void gen_func_type_qemu_st(TCGContext *s, uint32_t oi)
{
    MemOp mop = get_memop(oi);

    types_buf_out8(0x60);
    types_buf_out8(0x5);
    types_buf_out8(PTR_TYPE);
    types_buf_out8(0x7e);
    switch (mop & MO_SSIZE) {
    case MO_UQ:
        types_buf_out8(0x7e);
        break;
    default:
        types_buf_out8(0x7f);
        break;
    }
    types_buf_out8(0x7f);
    types_buf_out8(PTR_TYPE);
    types_buf_out8(0x0);
}

static void *qemu_ld_helper_ptr(uint32_t oi)
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

static void tcg_wasm_out_qemu_ld(TCGContext *s, TCGReg data_reg,
                                 TCGReg addr_reg, MemOpIdx oi)
{
    intptr_t helper_idx;
    int64_t func_idx;

    helper_idx = (intptr_t)qemu_ld_helper_ptr(oi);
    func_idx = get_helper_idx(s, helper_idx);
    if (func_idx < 0) {
        func_idx = register_helper(s, helper_idx);
        gen_func_type_qemu_ld(s, oi);
    }

    /*
     * update the block index so that the possible rewinding will
     * skip this block
     */
    tcg_wasm_out_op_const(s, OPC_I64_CONST, cur_block_idx + 1);
    tcg_wasm_out_op_idx(s, OPC_GLOBAL_SET, BLOCK_IDX);
    tcg_wasm_out_new_block(s);

    /* call the target helper */
    tcg_wasm_out_op_idx(s, OPC_GLOBAL_GET, REG_IDX(TCG_AREG0));
    tcg_wasm_out_op_idx(s, OPC_GLOBAL_GET, REG_IDX(addr_reg));
    tcg_wasm_out_op_const(s, OPC_I32_CONST, oi);
    tcg_wasm_out_op_const(s, OPC_I64_CONST, (intptr_t)s->code_ptr);

    tcg_wasm_out_op_idx(s, OPC_CALL, func_idx);
    tcg_wasm_out_op_idx(s, OPC_GLOBAL_SET, REG_IDX(data_reg));
    tcg_wasm_out_handle_unwinding(s);
}

static void *qemu_st_helper_ptr(uint32_t oi)
{
    MemOp mop = get_memop(oi);
    switch (mop & MO_SIZE) {
    case MO_8:
        return helper_stb_mmu;
    case MO_16:
        return helper_stw_mmu;
    case MO_32:
        return helper_stl_mmu;
    case MO_64:
        return helper_stq_mmu;
    default:
        g_assert_not_reached();
    }
}

static void tcg_wasm_out_qemu_st(TCGContext *s, TCGReg data_reg,
                                 TCGReg addr_reg, MemOpIdx oi)
{
    intptr_t helper_idx;
    int64_t func_idx;
    MemOp mop = get_memop(oi);

    helper_idx = (intptr_t)qemu_st_helper_ptr(oi);
    func_idx = get_helper_idx(s, helper_idx);
    if (func_idx < 0) {
        func_idx = register_helper(s, helper_idx);
        gen_func_type_qemu_st(s, oi);
    }

    /*
     * update the block index so that the possible rewinding will
     * skip this block
     */
    tcg_wasm_out_op_const(s, OPC_I64_CONST, cur_block_idx + 1);
    tcg_wasm_out_op_idx(s, OPC_GLOBAL_SET, BLOCK_IDX);
    tcg_wasm_out_new_block(s);

    /* call the target helper */
    tcg_wasm_out_op_idx(s, OPC_GLOBAL_GET, REG_IDX(TCG_AREG0));
    tcg_wasm_out_op_idx(s, OPC_GLOBAL_GET, REG_IDX(addr_reg));
    switch (mop & MO_SSIZE) {
    case MO_UQ:
        tcg_wasm_out_op_idx(s, OPC_GLOBAL_GET, REG_IDX(data_reg));
        break;
    default:
        tcg_wasm_out_op_idx(s, OPC_GLOBAL_GET, REG_IDX(data_reg));
        tcg_wasm_out_op(s, OPC_I32_WRAP_I64);
        break;
    }
    tcg_wasm_out_op_const(s, OPC_I32_CONST, oi);
    tcg_wasm_out_op_const(s, OPC_I64_CONST, (intptr_t)s->code_ptr);

    tcg_wasm_out_op_idx(s, OPC_CALL, func_idx);
    tcg_wasm_out_handle_unwinding(s);
}

static void tcg_out_op_l(TCGContext *s, TCGOpcode op, TCGLabel *l0)
{
    tcg_insn_unit_tci insn = 0;

    tcg_out_reloc(s, s->code_ptr, 20, l0, 0);
    insn = deposit32(insn, 0, 8, op);
    tcg_out32(s, insn);
}

static void tcg_out_op_p(TCGContext *s, TCGOpcode op, void *p0)
{
    tcg_insn_unit_tci insn = 0;
    intptr_t diff;

    /* Special case for exit_tb: map null -> 0. */
    if (p0 == NULL) {
        diff = 0;
    } else {
        diff = p0 - (void *)(s->code_ptr + 4);
        tcg_debug_assert(diff != 0);
        if (diff != sextract32(diff, 0, 20)) {
            tcg_raise_tb_overflow(s);
        }
    }
    insn = deposit32(insn, 0, 8, op);
    insn = deposit32(insn, 12, 20, diff);
    tcg_out32(s, insn);
}

static void tcg_out_op_r(TCGContext *s, TCGOpcode op, TCGReg r0)
{
    tcg_insn_unit_tci insn = 0;

    insn = deposit32(insn, 0, 8, op);
    insn = deposit32(insn, 8, 4, r0);
    tcg_out32(s, insn);
}

static void tcg_out_op_v(TCGContext *s, TCGOpcode op)
{
    tcg_out32(s, (uint8_t)op);
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

static void tcg_out_op_rl(TCGContext *s, TCGOpcode op, TCGReg r0, TCGLabel *l1)
{
    tcg_insn_unit_tci insn = 0;

    tcg_out_reloc(s, s->code_ptr, 20, l1, 0);
    insn = deposit32(insn, 0, 8, op);
    insn = deposit32(insn, 8, 4, r0);
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

static void tcg_out_op_rrm(TCGContext *s, TCGOpcode op,
                           TCGReg r0, TCGReg r1, TCGArg m2)
{
    tcg_insn_unit_tci insn = 0;

    tcg_debug_assert(m2 == extract32(m2, 0, 16));
    insn = deposit32(insn, 0, 8, op);
    insn = deposit32(insn, 8, 4, r0);
    insn = deposit32(insn, 12, 4, r1);
    insn = deposit32(insn, 16, 16, m2);
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

static void tgen_neg(TCGContext *s, TCGType type, TCGReg a0, TCGReg a1)
{
    tcg_out_op_rr(s, INDEX_op_neg, a0, a1);
    tcg_wasm_out_neg(s, a0, a1);
}

static const TCGOutOpUnary outop_neg = {
    .base.static_constraint = C_O1_I1(r, r),
    .out_rr = tgen_neg,
};

static void tgen_ctpop(TCGContext *s, TCGType type, TCGReg a0, TCGReg a1)
{
    tcg_out_op_rr(s, INDEX_op_ctpop, a0, a1);
    tcg_wasm_out_ctpop64(s, a0, a1);
}

static TCGConstraintSetIndex cset_ctpop(TCGType type, unsigned flags)
{
    return type == TCG_TYPE_REG ? C_O1_I1(r, r) : C_NotImplemented;
}

static const TCGOutOpUnary outop_ctpop = {
    .base.static_constraint = C_Dynamic,
    .base.dynamic_constraint = cset_ctpop,
    .out_rr = tgen_ctpop,
};

static void tgen_rotl(TCGContext *s, TCGType type,
                     TCGReg a0, TCGReg a1, TCGReg a2)
{
    TCGOpcode opc = (type == TCG_TYPE_I32
                     ? INDEX_op_tci_rotl32
                     : INDEX_op_rotl);
    tcg_out_op_rrr(s, opc, a0, a1, a2);
    tcg_wasm_out_o1_i2_type(s, type, OPC_I32_ROTL, OPC_I64_ROTL, a0, a1, a2);
}

static const TCGOutOpBinary outop_rotl = {
    .base.static_constraint = C_O1_I2(r, r, r),
    .out_rrr = tgen_rotl,
};

static void tgen_rotr(TCGContext *s, TCGType type,
                     TCGReg a0, TCGReg a1, TCGReg a2)
{
    TCGOpcode opc = (type == TCG_TYPE_I32
                     ? INDEX_op_tci_rotr32
                     : INDEX_op_rotr);
    tcg_out_op_rrr(s, opc, a0, a1, a2);
    tcg_wasm_out_o1_i2_type(s, type, OPC_I32_ROTR, OPC_I64_ROTR, a0, a1, a2);
}

static const TCGOutOpBinary outop_rotr = {
    .base.static_constraint = C_O1_I2(r, r, r),
    .out_rrr = tgen_rotr,
};

static void tgen_clz(TCGContext *s, TCGType type,
                      TCGReg a0, TCGReg a1, TCGReg a2)
{
    TCGOpcode opc = (type == TCG_TYPE_I32
                     ? INDEX_op_tci_clz32
                     : INDEX_op_clz);
    tcg_out_op_rrr(s, opc, a0, a1, a2);
    tcg_wasm_out_cz(s, type, OPC_I32_CLZ, OPC_I64_CLZ, a0, a1, a2);
}

static const TCGOutOpBinary outop_clz = {
    .base.static_constraint = C_O1_I2(r, r, r),
    .out_rrr = tgen_clz,
};

static void tgen_ctz(TCGContext *s, TCGType type,
                      TCGReg a0, TCGReg a1, TCGReg a2)
{
    TCGOpcode opc = (type == TCG_TYPE_I32
                     ? INDEX_op_tci_ctz32
                     : INDEX_op_ctz);
    tcg_out_op_rrr(s, opc, a0, a1, a2);
    tcg_wasm_out_cz(s, type, OPC_I32_CTZ, OPC_I64_CTZ, a0, a1, a2);
}

static const TCGOutOpBinary outop_ctz = {
    .base.static_constraint = C_O1_I2(r, r, r),
    .out_rrr = tgen_ctz,
};

static void tgen_brcond(TCGContext *s, TCGType type, TCGCond cond,
                        TCGReg arg0, TCGReg arg1, TCGLabel *l)
{
    tgen_setcond_tci(s, type, cond, TCG_REG_TMP, arg0, arg1);
    tcg_out_op_rl(s, INDEX_op_brcond, TCG_REG_TMP, l);
    tcg_wasm_out_brcond(s, type, arg0, arg1, cond, l);
}

static const TCGOutOpBrcond outop_brcond = {
    .base.static_constraint = C_O0_I2(r, r),
    .out_rr = tgen_brcond,
};

static void tcg_out_br(TCGContext *s, TCGLabel *l)
{
    tcg_out_op_l(s, INDEX_op_br, l);
    tcg_wasm_out_br(s, l);
}

static void tcg_out_exit_tb(TCGContext *s, uintptr_t arg)
{
    tcg_out_op_p(s, INDEX_op_exit_tb, (void *)arg);
    tcg_wasm_out_exit_tb(s, arg);
}

static void tcg_out_goto_tb(TCGContext *s, int which)
{
    /* indirect jump method. */
    tcg_out_op_p(s, INDEX_op_goto_tb, (void *)get_jmp_target_addr(s, which));
    set_jmp_reset_offset(s, which);
    tcg_wasm_out_goto_tb(s, which, (intptr_t)s->code_ptr);
}

static void tcg_out_goto_ptr(TCGContext *s, TCGReg a0)
{
    tcg_out_op_r(s, INDEX_op_goto_ptr, a0);
    tcg_wasm_out_goto_ptr(s, a0);
}

void tb_target_set_jmp_target(const TranslationBlock *tb, int n,
                              uintptr_t jmp_rx, uintptr_t jmp_rw)
{
    /* Always indirect, nothing to do */
}

static void tcg_out_addi_ptr(TCGContext *s, TCGReg rd, TCGReg rs,
                             tcg_target_long imm)
{
    /* This function is only used for passing structs by reference. */
    g_assert_not_reached();
}

static void tcg_out_call(TCGContext *s, const tcg_insn_unit *func,
                         const TCGHelperInfo *info)
{
    ffi_cif *cif = info->cif;
    tcg_insn_unit_tci insn = 0;
    uint8_t which;

    if (cif->rtype == &ffi_type_void) {
        which = 0;
    } else {
        tcg_debug_assert(cif->rtype->size == 4 ||
                         cif->rtype->size == 8 ||
                         cif->rtype->size == 16);
        which = ctz32(cif->rtype->size) - 1;
    }
    new_pool_l2(s, 20, s->code_ptr, 0, (uintptr_t)func, (uintptr_t)cif);
    insn = deposit32(insn, 0, 8, INDEX_op_call);
    insn = deposit32(insn, 8, 4, which);
    tcg_out32(s, insn);
    tcg_wasm_out_call(s, (intptr_t)func, info);
}

static void tgen_qemu_ld(TCGContext *s, TCGType type, TCGReg data,
                         TCGReg addr, MemOpIdx oi)
{
    tcg_out_op_rrm(s, INDEX_op_qemu_ld, data, addr, oi);
    tcg_wasm_out_qemu_ld(s, data, addr, oi);
}

static const TCGOutOpQemuLdSt outop_qemu_ld = {
    .base.static_constraint = C_O1_I1(r, r),
    .out = tgen_qemu_ld,
};

static void tgen_qemu_st(TCGContext *s, TCGType type, TCGReg data,
                         TCGReg addr, MemOpIdx oi)
{
    tcg_out_op_rrm(s, INDEX_op_qemu_st, data, addr, oi);
    tcg_wasm_out_qemu_st(s, data, addr, oi);
}

static const TCGOutOpQemuLdSt outop_qemu_st = {
    .base.static_constraint = C_O0_I2(r, r),
    .out = tgen_qemu_st,
};

bool tcg_target_has_memory_bswap(MemOp memop)
{
    return true;
}

static bool tcg_out_qemu_ld_slow_path(TCGContext *s, TCGLabelQemuLdst *l)
{
    g_assert_not_reached();
}

static bool tcg_out_qemu_st_slow_path(TCGContext *s, TCGLabelQemuLdst *l)
{
    g_assert_not_reached();
}

static void tcg_out_mb(TCGContext *s, unsigned a0)
{
    tcg_out_op_v(s, INDEX_op_mb);

    /*
     * Wasm's threading proposal provides atomic.fence instruction as the fence
     * operator.
     * https://webassembly.github.io/threads/core/syntax/instructions.html#atomic-memory-instructions
     */
    tcg_wasm_out8(s, 0xfe);
    tcg_wasm_out8(s, 0x03);
    tcg_wasm_out8(s, 0x00);
}

static const TCGOutOpDeposit outop_deposit = {
    .base.static_constraint = C_NotImplemented,
};

static const TCGOutOpExtract outop_extract = {
    .base.static_constraint = C_NotImplemented,
};

static const TCGOutOpExtract2 outop_extract2 = {
    .base.static_constraint = C_NotImplemented,
};

static const TCGOutOpBinary outop_addco = {
    .base.static_constraint = C_NotImplemented,
};

static const TCGOutOpAddSubCarry outop_addci = {
    .base.static_constraint = C_NotImplemented,
};

static const TCGOutOpBinary outop_addcio = {
    .base.static_constraint = C_NotImplemented,
};

static const TCGOutOpAddSubCarry outop_subbo = {
    .base.static_constraint = C_NotImplemented,
};

static const TCGOutOpAddSubCarry outop_subbi = {
    .base.static_constraint = C_NotImplemented,
};

static const TCGOutOpAddSubCarry outop_subbio = {
    .base.static_constraint = C_NotImplemented,
};

static const TCGOutOpUnary outop_not = {
    .base.static_constraint = C_NotImplemented,
};

static const TCGOutOpBinary outop_andc = {
    .base.static_constraint = C_NotImplemented,
};

static const TCGOutOpBinary outop_eqv = {
    .base.static_constraint = C_NotImplemented,
};

static const TCGOutOpBinary outop_nand = {
    .base.static_constraint = C_NotImplemented,
};

static const TCGOutOpBinary outop_nor = {
    .base.static_constraint = C_NotImplemented,
};

static const TCGOutOpBinary outop_orc = {
    .base.static_constraint = C_NotImplemented,
};

static const TCGOutOpDivRem outop_divs2 = {
    .base.static_constraint = C_NotImplemented,
};

static const TCGOutOpDivRem outop_divu2 = {
    .base.static_constraint = C_NotImplemented,
};

static const TCGOutOpMul2 outop_muls2 = {
    .base.static_constraint = C_NotImplemented,
};

static const TCGOutOpBinary outop_mulsh = {
    .base.static_constraint = C_NotImplemented,
};

static const TCGOutOpMul2 outop_mulu2 = {
    .base.static_constraint = C_NotImplemented,
};

static const TCGOutOpBinary outop_muluh = {
    .base.static_constraint = C_NotImplemented,
};

static const TCGOutOpBswap outop_bswap16 = {
    .base.static_constraint = C_NotImplemented,
};

static const TCGOutOpBswap outop_bswap32 = {
    .base.static_constraint = C_NotImplemented,
};

static const TCGOutOpUnary outop_bswap64 = {
    .base.static_constraint = C_NotImplemented,
};

static const TCGOutOpQemuLdSt2 outop_qemu_ld2 = {
    .base.static_constraint = C_NotImplemented,
};

static const TCGOutOpQemuLdSt2 outop_qemu_st2 = {
    .base.static_constraint = C_NotImplemented,
};

static bool tcg_out_xchg(TCGContext *s, TCGType type, TCGReg r1, TCGReg r2)
{
    return false;
}

static void tcg_out_set_borrow(TCGContext *s)
{
    g_assert_not_reached();
}

static void tcg_out_set_carry(TCGContext *s)
{
    g_assert_not_reached();
}

/* Generate global QEMU prologue and epilogue code. */
static inline void tcg_target_qemu_prologue(TCGContext *s)
{
}

static const uint8_t mod_1[] = {
    0x0, 0x61, 0x73, 0x6d, /* magic */
    0x01, 0x0, 0x0, 0x0,   /* version */

    0x01,                         /* type section */
    0x80, 0x80, 0x80, 0x80, 0x00, /* placehodler for size */
    0x80, 0x80, 0x80, 0x80, 0x00, /* placehodler for num of types vec */
    0x60,                         /* 0: Type of "start" function */
    0x01, PTR_TYPE,               /* arg: ctx pointer */
    0x01, PTR_TYPE,               /* return: res */
    0x60,                         /* 1: Type of the asyncify helper */
    0x0,                          /* no argument */
    0x01, 0x7f,                   /* return: res (i32) */
};

#define MOD_1_PH_TYPE_SECTION_SIZE_OFF 9
#define MOD_1_PH_TYPE_VEC_NUM_OFF 14

static const uint8_t mod_2[] = {
    0x02,                                     /* import section */
    0x80, 0x80, 0x80, 0x80, 0x00,             /* placehodler for size */
    0x80, 0x80, 0x80, 0x80, 0x00,             /* placehodler for imports num */
    0x03, 0x65, 0x6e, 0x76,                   /* module: "env" */
    0x06, 0x6d, 0x65, 0x6d, 0x6f, 0x72, 0x79, /* name: "memory" */
#if defined(WASM64_MEMORY64_2)
    /* 32bit memory is used for Emscripten's "-sMEMORY64=2" configuration. */
    0x02, 0x03,                               /* shared mem */
    0x00, 0x80, 0x80, 0x04,                   /* min: 0, max: 65536 pages */
#else
    /*
     * 64bit memory is used for Emscripten's "-sMEMORY64=1" configuration.
     * Note: the maximum 64bit memory size of the engine implementations is
     * limited to 262144 pages(16GiB)
     * https://webassembly.github.io/memory64/js-api/#limits
     */
    0x02, 0x07,                               /* shared mem(64bit) */
    0x00, 0x80, 0x80, 0x10,                   /* min: 0, max: 262144 pages */
#endif
    0x06, 0x68, 0x65, 0x6c, 0x70, 0x65, 0x72, /* module: "helper" */
    0x01, 0x75,                               /* name: "u" */
    0x00, 0x01,                               /* func type 1 */
};

#define MOD_2_PH_IMPORT_SECTION_SIZE_OFF 1
#define MOD_2_PH_IMPORT_VEC_NUM_OFF 6

static const uint8_t mod_3[] = {
    0x03,       /* function section */
    2, 1, 0x00, /* function type 0 */

    0x06,                         /* global section */
    86,                           /* section size */
    17,                           /* num of global vars */
    0x7e, 0x01, 0x42, 0x00, 0x0b, /* 0-cleared 64bit var */
    0x7e, 0x01, 0x42, 0x00, 0x0b, /* 0-cleared 64bit var */
    0x7e, 0x01, 0x42, 0x00, 0x0b, /* 0-cleared 64bit var */
    0x7e, 0x01, 0x42, 0x00, 0x0b, /* 0-cleared 64bit var */
    0x7e, 0x01, 0x42, 0x00, 0x0b, /* 0-cleared 64bit var */
    0x7e, 0x01, 0x42, 0x00, 0x0b, /* 0-cleared 64bit var */
    0x7e, 0x01, 0x42, 0x00, 0x0b, /* 0-cleared 64bit var */
    0x7e, 0x01, 0x42, 0x00, 0x0b, /* 0-cleared 64bit var */
    0x7e, 0x01, 0x42, 0x00, 0x0b, /* 0-cleared 64bit var */
    0x7e, 0x01, 0x42, 0x00, 0x0b, /* 0-cleared 64bit var */
    0x7e, 0x01, 0x42, 0x00, 0x0b, /* 0-cleared 64bit var */
    0x7e, 0x01, 0x42, 0x00, 0x0b, /* 0-cleared 64bit var */
    0x7e, 0x01, 0x42, 0x00, 0x0b, /* 0-cleared 64bit var */
    0x7e, 0x01, 0x42, 0x00, 0x0b, /* 0-cleared 64bit var */
    0x7e, 0x01, 0x42, 0x00, 0x0b, /* 0-cleared 64bit var */
    0x7e, 0x01, 0x42, 0x00, 0x0b, /* 0-cleared 64bit var */
    0x7e, 0x01, 0x42, 0x00, 0x0b, /* 0-cleared 64bit var */

    0x07,                               /* export section */
    13,                                 /* size of section */
    1,                                  /* num of funcs */
    0x05, 0x73, 0x74, 0x61, 0x72, 0x74, /* "start" function */
    0x00, 0x80, 0x80, 0x80, 0x80, 0x00, /* placeholder for func index*/

    0x0a,                         /* code section */
    0x80, 0x80, 0x80, 0x80, 0x00, /* placeholder for section size*/
    1,                            /* num of codes */
    0x80, 0x80, 0x80, 0x80, 0x00, /* placeholder for code size */
    0x0,                          /* local variables (none) */
};

#define MOD_3_PH_EXPORT_START_FUNC_IDX 102
#define MOD_3_PH_CODE_SECTION_SIZE_OFF 108
#define MOD_3_PH_CODE_SIZE_OFF 114
#define MOD_3_VARIABLES_SIZE 5
#define MOD_3_CODE_SECTION_SIZE_ADD 11

static void fill_uint32_leb128(uint8_t *b, uint32_t v)
{
    do {
        *b |= v & 0x7f;
        v >>= 7;
        b++;
    } while (v != 0);
}

typedef struct FillValueU32 {
    int64_t offset;
    uint32_t value;
} FillValueU32;

static int write_mod(TCGContext *s, const uint8_t mod[], int len,
                     FillValueU32 values[], int values_len)
{
    void *base = s->code_ptr;

    if (unlikely(((void *)s->code_ptr + len)
                 > s->code_gen_highwater)) {
        return -1;
    }

    memcpy(s->code_ptr, mod, len);
    s->code_ptr += len;

    for (int i = 0; i < values_len; i++) {
        fill_uint32_leb128(base + values[i].offset, values[i].value);
    }

    return 0;
}

static int write_mod_code(TCGContext *s)
{
    void *base = s->code_ptr;
    int code_size = sub_buf_len();
    BlockPlaceholder *e;

    if (unlikely(((void *)s->code_ptr + code_size) > s->code_gen_highwater)) {
        return -1;
    }
    linked_buf_write(&sub_buf, s->code_ptr);
    s->code_ptr += code_size;

    QSIMPLEQ_FOREACH(e, &block_placeholder, entry) {
        uint8_t *ph = e->pos + base;
        int blk = get_block_of_label(e->label);
        tcg_debug_assert(blk >= 0);
        fill_uint32_leb128(ph, blk);
    }

    return 0;
}

static void tcg_out_tb_start(TCGContext *s)
{
    int size;
    intptr_t ofs;
    struct WasmTBHeader *h;

    init_sub_buf();
    init_blocks();
    init_label_info();
    init_helpers();
    init_types_buf();
    init_imports_buf();

    /* TB starts from a header */
    h = (struct WasmTBHeader *)(s->code_ptr);
    s->code_ptr += sizeof(struct WasmTBHeader);

    /* locate counters */
    h->counter_ptr = (int32_t *)s->code_ptr;
    size = tcg_max_ctxs * sizeof(int32_t);
    memset(s->code_ptr, 0, size);
    s->code_ptr += size;

    /* locate the instance information */
    h->info_ptr = (struct WasmInstanceInfo **)s->code_ptr;
    size = tcg_max_ctxs * sizeof(void *);
    memset(s->code_ptr, 0, size);
    s->code_ptr += size;

    /* Followed by TCI code */
    h->tci_ptr = s->code_ptr;

    /* Initialize fundamental registers */
    tcg_wasm_out_op_idx(s, OPC_GLOBAL_GET, REG_IDX(TCG_AREG0));
    tcg_wasm_out_op(s, OPC_I64_EQZ);
    tcg_wasm_out_op_block(s, OPC_IF, BLOCK_NORET);

    ofs = tcg_wasm_out_get_ctx(s, CTX_OFFSET(env));
    tcg_wasm_out_op_ldst(s, OPC_I64_LOAD, 0, ofs);
    tcg_wasm_out_op_idx(s, OPC_GLOBAL_SET, REG_IDX(TCG_AREG0));

    ofs = tcg_wasm_out_get_ctx(s, CTX_OFFSET(stack));
    tcg_wasm_out_op_ldst(s, OPC_I64_LOAD, 0, ofs);
    tcg_wasm_out_op_idx(s, OPC_GLOBAL_SET, REG_IDX(TCG_REG_CALL_STACK));
    tcg_wasm_out_op(s, OPC_END);

    ofs = tcg_wasm_out_get_ctx(s, CTX_OFFSET(do_init));
    tcg_wasm_out_op_ldst(s, OPC_I32_LOAD, 0, ofs);
    tcg_wasm_out_op_const(s, OPC_I32_CONST, 0);
    tcg_wasm_out_op(s, OPC_I32_NE);
    tcg_wasm_out_op_block(s, OPC_IF, BLOCK_NORET);
    tcg_wasm_out_op_const(s, OPC_I64_CONST, 0);
    tcg_wasm_out_op_idx(s, OPC_GLOBAL_SET, BLOCK_IDX);
    ofs = tcg_wasm_out_get_ctx(s, CTX_OFFSET(do_init));
    tcg_wasm_out_op_const(s, OPC_I32_CONST, 0);
    tcg_wasm_out_op_ldst(s, OPC_I32_STORE, 0, ofs);
    tcg_wasm_out_op(s, OPC_END);

    tcg_wasm_out_op_block(s, OPC_LOOP, BLOCK_NORET);
    tcg_wasm_out_op_idx(s, OPC_GLOBAL_GET, BLOCK_IDX);
    tcg_wasm_out_op(s, OPC_I64_EQZ);
    tcg_wasm_out_op_block(s, OPC_IF, BLOCK_NORET);
}

static int tcg_out_tb_end(TCGContext *s)
{
    int res;
    struct WasmTBHeader *h = (struct WasmTBHeader *)(s->code_buf);

    tcg_wasm_out_op(s, OPC_END); /* end if */
    tcg_wasm_out_op(s, OPC_END); /* end loop */
    tcg_wasm_out_op(s, OPC_UNREACHABLE);
    tcg_wasm_out_op(s, OPC_END); /* end func */

    /* write wasm blob */
    h->wasm_ptr = s->code_ptr;

    res = write_mod(s, mod_1, sizeof(mod_1), (FillValueU32[]) {
            {
                MOD_1_PH_TYPE_SECTION_SIZE_OFF,
                linked_buf_len(&types_buf) +
                sizeof(mod_1) - MOD_1_PH_TYPE_VEC_NUM_OFF
            },
            {
                MOD_1_PH_TYPE_VEC_NUM_OFF,
                HELPER_IDX_START + helpers_len() + 1/* start */
            },
    }, 2);
    if (res < 0) {
        return res;
    }
    s->code_ptr += linked_buf_write(&types_buf, s->code_ptr);

    res = write_mod(s, mod_2, sizeof(mod_2), (FillValueU32[]) {
            {
                MOD_2_PH_IMPORT_SECTION_SIZE_OFF,
                linked_buf_len(&imports_buf) +
                sizeof(mod_2) - MOD_2_PH_IMPORT_VEC_NUM_OFF
            },
            {
                MOD_2_PH_IMPORT_VEC_NUM_OFF,
                HELPER_IDX_START + helpers_len() + 1/* memory */
            },
    }, 2);
    if (res < 0) {
        return res;
    }
    s->code_ptr += linked_buf_write(&imports_buf, s->code_ptr);

    res = write_mod(s, mod_3, sizeof(mod_3), (FillValueU32[]) {
            {
                MOD_3_PH_EXPORT_START_FUNC_IDX,
                HELPER_IDX_START + helpers_len()
            },
            {
                MOD_3_PH_CODE_SECTION_SIZE_OFF,
                sub_buf_len() + MOD_3_CODE_SECTION_SIZE_ADD
            },
            {
                MOD_3_PH_CODE_SIZE_OFF,
                sub_buf_len() + MOD_3_VARIABLES_SIZE
            },
    }, 3);
    if (res < 0) {
        return res;
    }

    res = write_mod_code(s);
    if (res < 0) {
        return res;
    }
    h->wasm_size = (intptr_t)s->code_ptr - (intptr_t)h->wasm_ptr;

    /* record imported helper functions */
    if (unlikely(((void *)s->code_ptr + helpers_len() * 4)
                 > s->code_gen_highwater)) {
        return -1;
    }
    h->import_ptr = s->code_ptr;
    s->code_ptr += helpers_write_to_array((intptr_t *)s->code_ptr);
    h->import_size = (intptr_t)s->code_ptr - (intptr_t)h->import_ptr;

    return 0;
}
