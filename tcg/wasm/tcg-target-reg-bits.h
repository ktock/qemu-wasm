/* SPDX-License-Identifier: MIT */

#ifndef TCG_TARGET_REG_BITS_H
#define TCG_TARGET_REG_BITS_H

#if UINTPTR_MAX != UINT64_MAX
# error Unsupported pointer size for TCG target
#endif
#define TCG_TARGET_REG_BITS 64

#endif
