#define FUSE_USE_VERSION 31

#include <fuse3/fuse.h>
#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <fcntl.h>
#include <unistd.h>
#include <dirent.h>
#include <sys/stat.h>
#include <stdlib.h>

struct mini_unionfs_state {
    char lower[512];
    char upper[512];
};

#define STATE ((struct mini_unionfs_state *) fuse_get_context()->private_data)

void build_path(char *dest, const char *base, const char *path) {
    sprintf(dest, "%s%s", base, path);
}

void get_whiteout_path(const char *path, char *whiteout) {
    const char *filename = strrchr(path, '/');

    if (filename)
        sprintf(whiteout, "%s/.wh.%s", STATE->upper, filename + 1);
    else
        sprintf(whiteout, "%s/.wh.%s", STATE->upper, path);
}

int resolve_path(const char *path, char *resolved) {
    char upper_path[1024], lower_path[1024], whiteout[1024];

    build_path(upper_path, STATE->upper, path);
    build_path(lower_path, STATE->lower, path);
    get_whiteout_path(path, whiteout);

    if (access(whiteout, F_OK) == 0)
        return -ENOENT;

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

/*
 * ensure_upper_dirs: walks every component of `path` and calls mkdir()
 * for each directory level under STATE->upper, so that a nested file
 * like /subdir/file.txt can be created as upper/subdir/file.txt without
 * the open() failing because upper/subdir/ does not exist yet.
 */
void ensure_upper_dirs(const char *path) {
    char tmp[1024];
    build_path(tmp, STATE->upper, path);

    /* start just past the upper prefix so we don't try to mkdir upper itself */
    for (char *p = tmp + strlen(STATE->upper) + 1; *p; p++) {
        if (*p == '/') {
            *p = '\0';
            mkdir(tmp, 0755);   /* silently ignore EEXIST */
            *p = '/';
        }
    }
}

/*
 * copy_to_upper: copies a file from lower to upper for Copy-on-Write.
 * Changes from original:
 *   1. Calls ensure_upper_dirs() so parent directories always exist.
 *   2. Reads source permissions via stat() and preserves them on the copy.
 *   3. Opens dst with O_TRUNC so stale content is never left behind.
 */
void copy_to_upper(const char *path) {
    char lower_path[1024], upper_path[1024];
    build_path(lower_path, STATE->lower, path);
    build_path(upper_path, STATE->upper, path);

    ensure_upper_dirs(path);

    struct stat st;
    if (stat(lower_path, &st) == -1)
        return;

    int src = open(lower_path, O_RDONLY);
    if (src == -1)
        return;

    int dst = open(upper_path, O_WRONLY | O_CREAT | O_TRUNC, st.st_mode);
    if (dst == -1) {
        close(src);
        return;
    }

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

static int unionfs_readdir(const char *path, void *buf, fuse_fill_dir_t filler,
                           off_t offset, struct fuse_file_info *fi,
                           enum fuse_readdir_flags flags) {

    DIR *dp;
    struct dirent *de;
    char dirpath[1024];

    filler(buf, ".", NULL, 0, 0);
    filler(buf, "..", NULL, 0, 0);

    char seen[256][256];
    int seen_count = 0;

    /* ---------- UPPER ---------- */
    build_path(dirpath, STATE->upper, path);
    dp = opendir(dirpath);

    if (dp) {
        while ((de = readdir(dp)) != NULL) {
            if (strncmp(de->d_name, ".wh.", 4) == 0)
                continue;
            strcpy(seen[seen_count++], de->d_name);
            filler(buf, de->d_name, NULL, 0, 0);
        }
        closedir(dp);
    }

    /* ---------- LOWER ---------- */
    build_path(dirpath, STATE->lower, path);
    dp = opendir(dirpath);

    if (dp) {
        while ((de = readdir(dp)) != NULL) {

            int skip = 0;

            for (int i = 0; i < seen_count; i++) {
                if (strcmp(seen[i], de->d_name) == 0) {
                    skip = 1;
                    break;
                }
            }

            char whiteout[1024];
            get_whiteout_path(de->d_name, whiteout);
            if (access(whiteout, F_OK) == 0)
                skip = 1;

            if (!skip)
                filler(buf, de->d_name, NULL, 0, 0);
        }
        closedir(dp);
    }

    return 0;
}

/*
 * unionfs_open: called by FUSE before every read or write.
 *
 * For write-mode opens (O_WRONLY / O_RDWR), we trigger CoW here — before
 * any write() call arrives — so that write() always finds the file already
 * present in upper and never has to think about lower.
 *
 * For read-only opens we just verify the file is visible (not whiteouted).
 * This is the correct place for CoW rather than lazily inside write(),
 * because open() is guaranteed to fire first and only once per file handle.
 */
static int unionfs_open(const char *path, struct fuse_file_info *fi) {
    char upper_path[1024], resolved[1024];

    build_path(upper_path, STATE->upper, path);

    if (fi->flags & (O_WRONLY | O_RDWR)) {
        /* Write path: bring file into upper if it is lower-only */
        if (access(upper_path, F_OK) != 0) {
            if (resolve_path(path, resolved) == 0)
                copy_to_upper(path);
            else
                return -ENOENT;
        }
    } else {
        /* Read path: just confirm the file exists and is not whiteouted */
        if (resolve_path(path, resolved) != 0)
            return -ENOENT;
    }

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

static int unionfs_unlink(const char *path) {
    char upper_path[1024], lower_path[1024], whiteout[1024];

    build_path(upper_path, STATE->upper, path);
    build_path(lower_path, STATE->lower, path);
    get_whiteout_path(path, whiteout);

    if (access(upper_path, F_OK) == 0)
        return unlink(upper_path);

    if (access(lower_path, F_OK) == 0) {
        int fd = open(whiteout, O_CREAT, 0644);
        close(fd);
        return 0;
    }

    return -ENOENT;
}

static int unionfs_create(const char *path, mode_t mode, struct fuse_file_info *fi) {
    char upper_path[1024];
    build_path(upper_path, STATE->upper, path);

    int fd = open(upper_path, fi->flags, mode);
    if (fd == -1)
        return -errno;

    close(fd);
    return 0;
}

static int unionfs_mkdir(const char *path, mode_t mode) {
    char upper_path[1024];
    build_path(upper_path, STATE->upper, path);

    int res = mkdir(upper_path, mode);
    if (res == -1)
        return -errno;

    return 0;
}

static int unionfs_rmdir(const char *path) {
    char upper_path[1024];
    build_path(upper_path, STATE->upper, path);

    int res = rmdir(upper_path);
    if (res == -1)
        return -errno;

    return 0;
}

static struct fuse_operations unionfs_oper = {
    .getattr = unionfs_getattr,
    .readdir = unionfs_readdir,
    .open    = unionfs_open,
    .read    = unionfs_read,
    .write   = unionfs_write,
    .unlink  = unionfs_unlink,
    .create  = unionfs_create,
    .mkdir   = unionfs_mkdir,
    .rmdir   = unionfs_rmdir,
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