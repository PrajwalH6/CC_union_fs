#include <string.h>
#include <fcntl.h>
#include <unistd.h>
#include <errno.h>
#include <sys/stat.h>

#include "unionfs.h"
#include "path.h"
#include "cow.h"

static int unionfs_getattr(const char *path, struct stat *stbuf,
                           struct fuse_file_info *fi) {
    char resolved[1024];
    memset(stbuf, 0, sizeof(struct stat));

    if (resolve_path(path, resolved) != 0)
        return -ENOENT;

    if (lstat(resolved, stbuf) == -1)
        return -errno;

    return 0;
}

static int unionfs_read(const char *path, char *buf, size_t size,
                        off_t offset, struct fuse_file_info *fi) {
    char resolved[1024];

    if (resolve_path(path, resolved) != 0)
        return -ENOENT;

    int fd = open(resolved, O_RDONLY);
    if (fd == -1)
        return -errno;

    int res = pread(fd, buf, size, offset);
    close(fd);

    return res;
}

static int unionfs_write(const char *path, const char *buf,
                         size_t size, off_t offset,
                         struct fuse_file_info *fi) {
    char upper_path[1024], resolved[1024];

    build_path(upper_path, STATE->upper, path);

    if (access(upper_path, F_OK) != 0) {
        if (resolve_path(path, resolved) == 0)
            copy_to_upper(path);
    }

    int fd = open(upper_path, O_WRONLY);
    if (fd == -1)
        return -errno;

    int res = pwrite(fd, buf, size, offset);
    close(fd);

    return res;
}

struct fuse_operations unionfs_oper = {
    .getattr = unionfs_getattr,
    .read    = unionfs_read,
    .write   = unionfs_write,
};