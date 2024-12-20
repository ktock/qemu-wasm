#include "qemu/osdep.h"

ssize_t fgetxattrat_nofollow(int dirfd, const char *path, const char *name,
                             void *value, size_t size)
{
    return -1;
}

ssize_t flistxattrat_nofollow(int dirfd, const char *filename,
                              char *list, size_t size)
{
    return -1;
}

ssize_t fremovexattrat_nofollow(int dirfd, const char *filename,
                                const char *name)
{
    return -1;
}

int fsetxattrat_nofollow(int dirfd, const char *path, const char *name,
                         void *value, size_t size, int flags)
{
    return -1;

}

int qemu_mknodat(int dirfd, const char *filename, mode_t mode, dev_t dev)
{
    return -1;
}

ssize_t fgetxattr(int fd, const char *name, void *value, size_t size)
{
    return -1;
}
