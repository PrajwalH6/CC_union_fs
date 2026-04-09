#define FUSE_USE_VERSION 31

#include <fuse3/fuse.h>
#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/stat.h>
#include <stdlib.h>

struct mini_unionfs_state {
    char lower[512];
    char upper[512];
};

#define STATE ((struct mini_unionfs_state *) fuse_get_context()->private_data)

void build_path(char *dest, const char *base, const char *path) {
    snprintf(dest, 1024, "%s%s", base, path);
}

int resolve_path(const char *path, char *resolved) {
    char upper_path[1024], lower_path[1024];

    build_path(upper_path, STATE->upper, path);
    build_path(lower_path, STATE->lower, path);

    if (access(upper_path, F_OK) == 0) {
        strcpy(resolved, upper_path);
        return 0;
    }

    if (access(lower_path, F_OK) == 0) {
        strcpy(resolved, lower_path);
        return 0;
    }

    return -ENOENT;
}

void copy_to_upper(const char *path) {
    char lower_path[1024], upper_path[1024];
    build_path(lower_path, STATE->lower, path);
    build_path(upper_path, STATE->upper, path);

    int src = open(lower_path, O_RDONLY);
    int dst = open(upper_path, O_WRONLY | O_CREAT | O_TRUNC, 0644);

    char buf[4096];
    int n;

    while ((n = read(src, buf, sizeof(buf))) > 0)
        write(dst, buf, n);

    close(src);
    close(dst);
}

static int unionfs_getattr(const char *path, struct stat *stbuf, struct fuse_file_info *fi) {
    char resolved[1024];
    memset(stbuf, 0, sizeof(struct stat));

    if (resolve_path(path, resolved) != 0)
        return -ENOENT;

    if (lstat(resolved, stbuf) == -1)
        return -errno;

    return 0;
}

static int unionfs_read(const char *path, char *buf, size_t size, off_t offset,
                        struct fuse_file_info *fi) {
    char resolved[1024];
    int fd;

    if (resolve_path(path, resolved) != 0)
        return -ENOENT;

    fd = open(resolved, O_RDONLY);
    if (fd == -1)
        return -errno;

    int res = pread(fd, buf, size, offset);
    close(fd);

    return res;
}

static int unionfs_write(const char *path, const char *buf, size_t size,
                         off_t offset, struct fuse_file_info *fi) {
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

static struct fuse_operations unionfs_oper = {
    .getattr = unionfs_getattr,
    .read = unionfs_read,
    .write = unionfs_write,
};

int main(int argc, char *argv[]) {
    if (argc < 4) {
        printf("Usage: %s <lower> <upper> <mountpoint>\n", argv[0]);
        return 1;
    }

    struct mini_unionfs_state state;
    realpath(argv[1], state.lower);
    realpath(argv[2], state.upper);

    char *fuse_argv[] = { argv[0], argv[3], "-f" };
    int fuse_argc = 3;

    return fuse_main(fuse_argc, fuse_argv, &unionfs_oper, &state);
}

