/* Define vaddr.  */

#ifndef VADDR_H
#define VADDR_H

/**
 * vaddr:
 * Type wide enough to contain any #target_ulong virtual address.
 */
#if TCG_VADDR_BITS == 32
typedef uint32_t vaddr;
#define VADDR_PRId PRId32
#define VADDR_PRIu PRIu32
#define VADDR_PRIo PRIo32
#define VADDR_PRIx PRIx32
#define VADDR_PRIX PRIX32
#define VADDR_MAX UINT32_MAX
#elif TCG_VADDR_BITS == 64
typedef uint64_t vaddr;
#define VADDR_PRId PRId64
#define VADDR_PRIu PRIu64
#define VADDR_PRIo PRIo64
#define VADDR_PRIx PRIx64
#define VADDR_PRIX PRIX64
#define VADDR_MAX UINT64_MAX
#else
#error Unknown pointer size
#endif

#endif
