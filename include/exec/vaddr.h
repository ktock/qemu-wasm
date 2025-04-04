/* Define vaddr.  */

#ifndef VADDR_H
#define VADDR_H

/**
 * vaddr:
 * Type wide enough to contain any #target_ulong virtual address.
 * We do not support 64-bit guest on 32-host and detect at configure time.
 * Therefore, a host pointer width will always fit a guest pointer.
 */
#ifndef EMSCRIPTEN
typedef uintptr_t vaddr;
#define VADDR_PRId PRIdPTR
#define VADDR_PRIu PRIuPTR
#define VADDR_PRIo PRIoPTR
#define VADDR_PRIx PRIxPTR
#define VADDR_PRIX PRIXPTR
#define VADDR_MAX UINTPTR_MAX
#else
/* Explicitly define this as 64bit on emscripten */
typedef uint64_t vaddr;
#define VADDR_PRId PRId64
#define VADDR_PRIu PRIu64
#define VADDR_PRIo PRIo64
#define VADDR_PRIx PRIx64
#define VADDR_PRIX PRIX64
#define VADDR_MAX UINT64_MAX
#endif

#endif
