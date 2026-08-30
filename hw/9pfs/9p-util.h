/*
 * 9p utilities
 *
 * Copyright IBM, Corp. 2017
 *
 * Authors:
 *  Greg Kurz <groug@kaod.org>
 *
 * This work is licensed under the terms of the GNU GPL, version 2 or later.
 * See the COPYING file in the top-level directory.
 */

#ifndef QEMU_9P_UTIL_H
#define QEMU_9P_UTIL_H

#include "qemu/error-report.h"

#ifdef O_PATH
#define O_PATH_9P_UTIL O_PATH
#else
#define O_PATH_9P_UTIL 0
#endif

#if !defined(CONFIG_LINUX)

/*
 * Generates a Linux device number (a.k.a. dev_t) for given device major
 * and minor numbers.
 *
 * To be more precise: it generates a device number in glibc's format
 * (MMMM_Mmmm_mmmM_MMmm, 64 bits) actually, which is compatible with
 * Linux's format (mmmM_MMmm, 32 bits), as described in <bits/sysmacros.h>.
 */
static inline uint64_t makedev_dotl(uint32_t dev_major, uint32_t dev_minor)
{
    uint64_t dev;

    // from glibc sysmacros.h:
    dev  = (((uint64_t) (dev_major & 0x00000fffu)) <<  8);
    dev |= (((uint64_t) (dev_major & 0xfffff000u)) << 32);
    dev |= (((uint64_t) (dev_minor & 0x000000ffu)) <<  0);
    dev |= (((uint64_t) (dev_minor & 0xffffff00u)) << 12);
    return dev;
}

#endif

/*
 * Converts given device number from host's device number format to Linux
 * device number format. As both the size of type dev_t and encoding of
 * dev_t is system dependent, we have to convert them for Linux guests if
 * host is not running Linux.
 */
static inline uint64_t host_dev_to_dotl_dev(dev_t dev)
{
#ifdef CONFIG_LINUX
    return dev;
#else
    return makedev_dotl(major(dev), minor(dev));
#endif
}

/* Translates errno from host -> Linux if needed */
static inline int errno_to_dotl(int err) {
#if defined(CONFIG_LINUX)
    /* nothing to translate (Linux -> Linux) */
#elif defined(EMSCRIPTEN)
    /*
     * Emscripten's libc uses WASI errno numbering (see <wasi/api.h>), which
     * differs from Linux for every single code (WASI numbers are assigned
     * alphabetically). Translate to Linux errnos for the 9p2000.L guest;
     * fall back to EIO for anything unrecognized.
     */
    switch (err) {
    case 0:
        break;
    case E2BIG: err = 7; break;
    case EACCES: err = 13; break;
    case EADDRINUSE: err = 98; break;
    case EADDRNOTAVAIL: err = 99; break;
    case EAFNOSUPPORT: err = 97; break;
    case EAGAIN: err = 11; break;
    case EALREADY: err = 114; break;
    case EBADF: err = 9; break;
    case EBADMSG: err = 74; break;
    case EBUSY: err = 16; break;
    case ECANCELED: err = 125; break;
    case ECHILD: err = 10; break;
    case ECONNABORTED: err = 103; break;
    case ECONNREFUSED: err = 111; break;
    case ECONNRESET: err = 104; break;
    case EDEADLK: err = 35; break;
    case EDESTADDRREQ: err = 89; break;
    case EDOM: err = 33; break;
    case EDQUOT: err = 122; break;
    case EEXIST: err = 17; break;
    case EFAULT: err = 14; break;
    case EFBIG: err = 27; break;
    case EHOSTUNREACH: err = 113; break;
    case EIDRM: err = 43; break;
    case EILSEQ: err = 84; break;
    case EINPROGRESS: err = 115; break;
    case EINTR: err = 4; break;
    case EINVAL: err = 22; break;
    case EIO: err = 5; break;
    case EISCONN: err = 106; break;
    case EISDIR: err = 21; break;
    case ELOOP: err = 40; break;
    case EMFILE: err = 24; break;
    case EMLINK: err = 31; break;
    case EMSGSIZE: err = 90; break;
    case EMULTIHOP: err = 72; break;
    case ENAMETOOLONG: err = 36; break;
    case ENETDOWN: err = 100; break;
    case ENETRESET: err = 102; break;
    case ENETUNREACH: err = 101; break;
    case ENFILE: err = 23; break;
    case ENOBUFS: err = 105; break;
    case ENODEV: err = 19; break;
    case ENOENT: err = 2; break;
    case ENOEXEC: err = 8; break;
    case ENOLCK: err = 37; break;
    case ENOLINK: err = 67; break;
    case ENOMEM: err = 12; break;
    case ENOMSG: err = 42; break;
    case ENOPROTOOPT: err = 92; break;
    case ENOSPC: err = 28; break;
    case ENOSYS: err = 38; break;
    case ENOTCONN: err = 107; break;
    case ENOTDIR: err = 20; break;
    case ENOTEMPTY: err = 39; break;
    case ENOTRECOVERABLE: err = 131; break;
    case ENOTSOCK: err = 88; break;
    case ENOTSUP: err = 95; break;
    case ENOTTY: err = 25; break;
    case ENXIO: err = 6; break;
    case EOVERFLOW: err = 75; break;
    case EOWNERDEAD: err = 130; break;
    case EPERM: err = 1; break;
    case EPIPE: err = 32; break;
    case EPROTO: err = 71; break;
    case EPROTONOSUPPORT: err = 93; break;
    case EPROTOTYPE: err = 91; break;
    case ERANGE: err = 34; break;
    case EROFS: err = 30; break;
    case ESPIPE: err = 29; break;
    case ESRCH: err = 3; break;
    case ESTALE: err = 116; break;
    case ETIMEDOUT: err = 110; break;
    case ETXTBSY: err = 26; break;
    case EXDEV: err = 18; break;
    default:
        err = 5; /* ==EIO on Linux */
        break;
    }
#elif defined(CONFIG_DARWIN)
    /*
     * translation mandatory for macOS hosts
     *
     * FIXME: Only most important errnos translated here yet, this should be
     * extended to as many errnos being translated as possible in future.
     */
    if (err == ENAMETOOLONG) {
        err = 36; /* ==ENAMETOOLONG on Linux */
    } else if (err == ENOTEMPTY) {
        err = 39; /* ==ENOTEMPTY on Linux */
    } else if (err == ELOOP) {
        err = 40; /* ==ELOOP on Linux */
    } else if (err == ENOATTR) {
        err = 61; /* ==ENODATA on Linux */
    } else if (err == ENOTSUP) {
        err = 95; /* ==EOPNOTSUPP on Linux */
    } else if (err == EOPNOTSUPP) {
        err = 95; /* ==EOPNOTSUPP on Linux */
    }
#else
#error Missing errno translation to Linux for this host system
#endif
    return err;
}

#ifdef CONFIG_DARWIN
#define qemu_fgetxattr(...) fgetxattr(__VA_ARGS__, 0, 0)
#else
#define qemu_fgetxattr fgetxattr
#endif

#define qemu_openat     openat
#define qemu_fstat      fstat
#define qemu_fstatat    fstatat
#define qemu_mkdirat    mkdirat
#define qemu_renameat   renameat
#define qemu_utimensat  utimensat
#define qemu_unlinkat   unlinkat

static inline void close_preserve_errno(int fd)
{
    int serrno = errno;
    close(fd);
    errno = serrno;
}

/**
 * close_if_special_file() - Close @fd if neither regular file nor directory.
 *
 * @fd: file descriptor of open file
 * Return: 0 on regular file or directory, -1 otherwise
 *
 * CVE-2023-2861: Prohibit opening any special file directly on host
 * (especially device files), as a compromised client could potentially gain
 * access outside exported tree under certain, unsafe setups. We expect
 * client to handle I/O on special files exclusively on guest side.
 */
static inline int close_if_special_file(int fd)
{
    struct stat stbuf;

    if (qemu_fstat(fd, &stbuf) < 0) {
        close_preserve_errno(fd);
        return -1;
    }
    if (!S_ISREG(stbuf.st_mode) && !S_ISDIR(stbuf.st_mode)) {
        error_report_once(
            "9p: broken or compromised client detected; attempt to open "
            "special file (i.e. neither regular file, nor directory)"
        );
        close(fd);
        errno = ENXIO;
        return -1;
    }

    return 0;
}

static inline int openat_dir(int dirfd, const char *name)
{
    return qemu_openat(dirfd, name,
                       O_DIRECTORY | O_RDONLY | O_NOFOLLOW | O_PATH_9P_UTIL);
}

static inline int openat_file(int dirfd, const char *name, int flags,
                              mode_t mode)
{
    int fd, serrno, ret;

#ifndef CONFIG_DARWIN
again:
#endif
    fd = qemu_openat(dirfd, name, flags | O_NOFOLLOW | O_NOCTTY | O_NONBLOCK,
                     mode);
    if (fd == -1) {
#ifndef CONFIG_DARWIN
        if (errno == EPERM && (flags & O_NOATIME)) {
            /*
             * The client passed O_NOATIME but we lack permissions to honor it.
             * Rather than failing the open, fall back without O_NOATIME. This
             * doesn't break the semantics on the client side, as the Linux
             * open(2) man page notes that O_NOATIME "may not be effective on
             * all filesystems". In particular, NFS and other network
             * filesystems ignore it entirely.
             */
            flags &= ~O_NOATIME;
            goto again;
        }
#endif
        return -1;
    }

    if (close_if_special_file(fd) < 0) {
        return -1;
    }

    serrno = errno;
    /* O_NONBLOCK was only needed to open the file. Let's drop it. We don't
     * do that with O_PATH since fcntl(F_SETFL) isn't supported, and openat()
     * ignored it anyway.
     */
    if (!(flags & O_PATH_9P_UTIL)) {
        ret = fcntl(fd, F_SETFL, flags);
        assert(!ret);
    }
    errno = serrno;
    return fd;
}

ssize_t fgetxattrat_nofollow(int dirfd, const char *path, const char *name,
                             void *value, size_t size);
int fsetxattrat_nofollow(int dirfd, const char *path, const char *name,
                         void *value, size_t size, int flags);
ssize_t flistxattrat_nofollow(int dirfd, const char *filename,
                              char *list, size_t size);
ssize_t fremovexattrat_nofollow(int dirfd, const char *filename,
                                const char *name);

/*
 * Darwin has d_seekoff, which appears to function similarly to d_off.
 * However, it does not appear to be supported on all file systems,
 * so ensure it is manually injected earlier and call here when
 * needed.
 */
static inline off_t qemu_dirent_off(struct dirent *dent)
{
#ifdef CONFIG_DARWIN
    return dent->d_seekoff;
#else
    return dent->d_off;
#endif
}

/**
 * qemu_dirent_dup() - Duplicate directory entry @dent.
 *
 * @dent: original directory entry to be duplicated
 * Return: duplicated directory entry which should be freed with g_free()
 *
 * It is highly recommended to use this function instead of open coding
 * duplication of dirent objects, because the actual struct dirent
 * size may be bigger or shorter than sizeof(struct dirent) and correct
 * handling is platform specific (see gitlab issue #841).
 */
static inline struct dirent *qemu_dirent_dup(struct dirent *dent)
{
    size_t sz = 0;
#if defined _DIRENT_HAVE_D_RECLEN
    /* Avoid use of strlen() if platform supports d_reclen. */
    sz = dent->d_reclen;
#endif
    /*
     * Test sz for zero even if d_reclen is available
     * because some drivers may set d_reclen to zero.
     */
    if (sz == 0) {
        /* Fallback to the most portable way. */
        sz = offsetof(struct dirent, d_name) +
                      strlen(dent->d_name) + 1;
    }
    return g_memdup(dent, sz);
}

/*
 * As long as mknodat is not available on macOS, this workaround
 * using pthread_fchdir_np is needed. qemu_mknodat is defined in
 * os-posix.c. pthread_fchdir_np is weakly linked here as a guard
 * in case it disappears in future macOS versions, because it is
 * is a private API.
 */
#if defined CONFIG_DARWIN && defined CONFIG_PTHREAD_FCHDIR_NP
int pthread_fchdir_np(int fd) __attribute__((weak_import));
#endif
int qemu_mknodat(int dirfd, const char *filename, mode_t mode, dev_t dev);

#endif
