/* SPDX-License-Identifier: GPL-2.0-or-later */
#include "qemu/osdep.h"
/*
 * emscripten exposes copy_file_range declaration but doesn't provide the
 * implementation in the final link. Define the stub here but avoid type
 * conflict with the emscripten's header.
 */
ssize_t copy_file_range(int in_fd, off_t *in_off, int out_fd,
                             off_t *out_off, size_t len, unsigned int flags)
{
    errno = ENOSYS;
    return -1;
}
