#include "operations.h"
#include "cow.h"

/* ── getattr ── */
int unionfs_getattr(const char *path, struct stat *stbuf,
                    struct fuse_file_info *fi)
{
    char resolved[1024];
    (void) fi;

    memset(stbuf, 0, sizeof(struct stat));

    int ret = resolve_path(path, resolved, sizeof(resolved));
    if (ret != 0) return ret;

    if (lstat(resolved, stbuf) == -1) return -errno;

    LOG("getattr: %s -> %s", path, resolved);
    return 0;
}

/* ── readdir (YOUR FEATURE) ── */
int unionfs_readdir(const char *path, void *buf, fuse_fill_dir_t filler,
                    off_t offset, struct fuse_file_info *fi,
                    enum fuse_readdir_flags flags)
{
    (void) offset; (void) fi; (void) flags;

    char upper_path[1024], lower_path[1024];

    if (build_path(upper_path, sizeof(upper_path), STATE->upper, path) < 0)
        return -ENAMETOOLONG;
    if (build_path(lower_path, sizeof(lower_path), STATE->lower, path) < 0)
        return -ENAMETOOLONG;

    filler(buf, ".",  NULL, 0, 0);
    filler(buf, "..", NULL, 0, 0);

    /* ── Pass 1: scan upper directory ── */
    DIR *upper_dir = opendir(upper_path);
    if (upper_dir) {
        struct dirent *de;
        while ((de = readdir(upper_dir)) != NULL) {
            if (strcmp(de->d_name, ".") == 0 ||
                strcmp(de->d_name, "..") == 0)
                continue;

            /* Never expose .wh.* markers */
            if (is_whiteout(de->d_name))
                continue;

            filler(buf, de->d_name, NULL, 0, 0);
        }
        closedir(upper_dir);
    }

    /* ── Pass 2: scan lower directory ── */
    DIR *lower_dir = opendir(lower_path);
    if (lower_dir) {
        struct dirent *de;
        while ((de = readdir(lower_dir)) != NULL) {
            if (strcmp(de->d_name, ".") == 0 ||
                strcmp(de->d_name, "..") == 0)
                continue;

            /* Skip if whiteout exists for this name in upper */
            if (whiteout_exists(upper_path, de->d_name))
                continue;

            /* Skip if already added from upper (upper takes precedence) */
            char candidate_upper[1024];
            snprintf(candidate_upper, sizeof(candidate_upper),
                     "%s/%s", upper_path, de->d_name);
            if (access(candidate_upper, F_OK) == 0)
                continue;

            filler(buf, de->d_name, NULL, 0, 0);
        }
        closedir(lower_dir);
    }

    LOG("readdir: %s", path);
    return 0;
}

/* ── create (YOUR FEATURE) ── */
int unionfs_create(const char *path, mode_t mode,
                   struct fuse_file_info *fi)
{
    char upper_path[1024];

    if (build_path(upper_path, sizeof(upper_path), STATE->upper, path) < 0)
        return -ENAMETOOLONG;

    int fd = open(upper_path, O_CREAT | O_WRONLY | O_TRUNC, mode);
    if (fd == -1) return -errno;

    fi->fh = (uint64_t) fd;
    LOG("create: %s", upper_path);
    return 0;
}

/* ── open ── */
int unionfs_open(const char *path, struct fuse_file_info *fi)
{
    char upper_path[1024];

    if (build_path(upper_path, sizeof(upper_path), STATE->upper, path) < 0)
        return -ENAMETOOLONG;

    /* If opening for write and file is only in lower → CoW now */
    if ((fi->flags & O_ACCMODE) != O_RDONLY) {
        if (access(upper_path, F_OK) != 0) {
            int ret = copy_to_upper(path);
            if (ret != 0) return ret;
        }
    }

    char resolved[1024];
    int ret = resolve_path(path, resolved, sizeof(resolved));
    if (ret != 0) return ret;

    int fd = open(resolved, fi->flags);
    if (fd == -1) return -errno;

    fi->fh = (uint64_t) fd;
    return 0;
}

/* ── read ── */
int unionfs_read(const char *path, char *buf, size_t size,
                 off_t offset, struct fuse_file_info *fi)
{
    (void) path;

    int fd = (int) fi->fh;
    if (fd == -1) return -EBADF;

    int res = pread(fd, buf, size, offset);
    if (res == -1) return -errno;
    return res;
}

/* ── write ── */
int unionfs_write(const char *path, const char *buf, size_t size,
                  off_t offset, struct fuse_file_info *fi)
{
    (void) path;

    int fd = (int) fi->fh;
    if (fd == -1) return -EBADF;

    int res = pwrite(fd, buf, size, offset);
    if (res == -1) return -errno;
    return res;
}

/* ── release ── */
static int unionfs_release(const char *path, struct fuse_file_info *fi)
{
    (void) path;
    if (fi->fh) close((int) fi->fh);
    return 0;
}

/* ── Stubs for teammate features (filled in by TM2 and TM3) ── */
int unionfs_unlink(const char *path) { (void) path; return -ENOSYS; }
int unionfs_mkdir (const char *path, mode_t mode) { (void)path; (void)mode; return -ENOSYS; }
int unionfs_rmdir (const char *path) { (void) path; return -ENOSYS; }

/* ── FUSE operations table ── */
struct fuse_operations unionfs_oper = {
    .getattr  = unionfs_getattr,
    .readdir  = unionfs_readdir,
    .create   = unionfs_create,
    .open     = unionfs_open,
    .read     = unionfs_read,
    .write    = unionfs_write,
    .release  = unionfs_release,
    .unlink   = unionfs_unlink,
    .mkdir    = unionfs_mkdir,
    .rmdir    = unionfs_rmdir,
};