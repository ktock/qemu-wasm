/* SPDX-License-Identifier: MIT */
#ifndef TCG_TARGET_HAS_H
#define TCG_TARGET_HAS_H

#define TCG_TARGET_HAS_tst 0
#define TCG_TARGET_HAS_extr_i64_i32 0
#define TCG_TARGET_HAS_qemu_ldst_i128 0

#define TCG_TARGET_extract_valid(type, ofs, len) 0
#define TCG_TARGET_sextract_valid(type, ofs, len) \
    ((ofs == 0) && ((len == 8) || (len == 16) || (len == 32)))
#define TCG_TARGET_deposit_valid(type, ofs, len) 0

#endif
