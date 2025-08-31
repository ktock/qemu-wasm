/* SPDX-License-Identifier: MIT */
/*
 * Define Wasm target-specific operand constraints.
 *
 * Based on tci/tcg-target-con-str.h
 *
 * Copyright (c) 2021 Linaro
 */

/*
 * Define constraint letters for register sets:
 * REGS(letter, register_mask)
 */
REGS('r', MAKE_64BIT_MASK(0, TCG_TARGET_NB_REGS))
