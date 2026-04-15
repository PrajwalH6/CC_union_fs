#include "operations.h"
#include "cow.h"
#include "cache.h"
#include <sys/xattr.h>


/* ── getattr ── */
#ifdef __APPLE__
int unionfs_getattr(const char *path, struct stat *stbuf)
{
#else
int unionfs_getattr(const char *path, struct stat *stbuf,
                    struct fuse_file_info *fi)
{
    (void) fi;
#endif

    memset(stbuf, 0, sizeof(struct stat));

    char resolved[1024];
    int ret = resolve_path(path, resolved, sizeof(resolved));
    if (ret != 0) return ret;

    if (lstat(resolved, stbuf) == -1) return -errno;

    LOG("getattr: %s -> %s", path, resolved);
    return 0;
}

/* ── readdir (YOUR FEATURE) ── */
#ifdef __APPLE__
int unionfs_readdir(const char *path, void *buf, fuse_fill_dir_t filler,
                    off_t offset, struct fuse_file_info *fi)
{
    (void) offset; (void) fi;
#else
int unionfs_readdir(const char *path, void *buf, fuse_fill_dir_t filler,
                    off_t offset, struct fuse_file_info *fi,
                    enum fuse_readdir_flags flags)
{
    (void) offset; (void) fi; (void) flags;
#endif

    char upper_path[1024], lower_path[1024];

    if (build_path(upper_path, sizeof(upper_path), STATE->upper, path) < 0)
        return -ENAMETOOLONG;
    if (build_path(lower_path, sizeof(lower_path), STATE->lower, path) < 0)
        return -ENAMETOOLONG;

#ifdef __APPLE__
    filler(buf, ".",  NULL, 0);
    filler(buf, "..", NULL, 0);
#else
    filler(buf, ".",  NULL, 0, 0);
    filler(buf, "..", NULL, 0, 0);
#endif

    /* ── Pass 1: scan upper directory ── */
    DIR *upper_dir = opendir(upper_path);
    if (upper_dir) {
        struct dirent *de;
        while ((de = readdir(upper_dir)) != NULL) {
            if (strcmp(de->d_name, ".") == 0 ||
                strcmp(de->d_name, "..") == 0)
                continue;

            

#ifdef __APPLE__
            filler(buf, de->d_name, NULL, 0);
#else
            filler(buf, de->d_name, NULL, 0, 0);
#endif
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

            if (whiteout_exists(upper_path, de->d_name))
                continue;

            char candidate_upper[1024];
            snprintf(candidate_upper, sizeof(candidate_upper),
                     "%s/%s", upper_path, de->d_name);
            if (access(candidate_upper, F_OK) == 0)
                continue;

#ifdef __APPLE__
            filler(buf, de->d_name, NULL, 0);
#else
            filler(buf, de->d_name, NULL, 0, 0);
#endif
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
/* ── write ── */
int unionfs_write(const char *path, const char *buf, size_t size,
                  off_t offset, struct fuse_file_info *fi)
{
    int fd;
    char upper_path[1024];
    char resolved[1024];

    /* Build upper path */
    if (build_path(upper_path, sizeof(upper_path), STATE->upper, path) < 0)
        return -ENAMETOOLONG;

    /* 🔥 CoW: ensure file exists in upper */
    if (access(upper_path, F_OK) != 0) {
        int ret = resolve_path(path, resolved, sizeof(resolved));
        if (ret == 0) {
            ret = copy_to_upper(path);
            if (ret != 0) return ret;
        } else {
            return -ENOENT;
        }
    }

    /* Open upper file for writing */
    fd = open(upper_path, O_WRONLY);
    if (fd == -1)
        return -errno;

    /* Perform write */
    int res = pwrite(fd, buf, size, offset);
    if (res == -1) {
        close(fd);
        return -errno;
    }

    close(fd);

    /* 🔥 Invalidate cache after modification */
    cache_invalidate(path);

    return res;
}

/* ── release ── */
static int unionfs_release(const char *path, struct fuse_file_info *fi)
{
    (void) path;
    if (fi->fh) close((int) fi->fh);
    return 0;
}

int unionfs_unlink(const char *path)
{
    char upper_path[1024];
    char whiteout_path[1024];

    if (!path || !STATE)
        return -EINVAL;

    /* Build upper path */
    if (build_path(upper_path, sizeof(upper_path),
                   STATE->upper, path) < 0)
        return -ENAMETOOLONG;

    /* Extract filename */
    const char *filename = strrchr(path, '/');
    filename = filename ? filename + 1 : path;

    /* Whiteout path in upper root */
    snprintf(whiteout_path, sizeof(whiteout_path),
             "%s/.wh.%s", STATE->upper, filename);

    /* Delete from upper if exists */
    if (access(upper_path, F_OK) == 0) {
        if (unlink(upper_path) == -1)
            return -errno;
    }

    /* 🔥 Create whiteout (no silent failure) */
    int fd = open(whiteout_path, O_CREAT | O_WRONLY | O_TRUNC, 0644);
    if (fd == -1) {
        perror("WHITEOUT ERROR");   // important debug
        return -errno;
    }
    close(fd);

    cache_invalidate(path);

    return 0;
}

int unionfs_mkdir(const char *path, mode_t mode)
{
    char upper_path[1024];
    
    /* Input validation */
    if (path == NULL || strlen(path) == 0)
        return -EINVAL;
    
    if (build_path(upper_path, sizeof(upper_path), STATE->upper, path) < 0)
        return -ENAMETOOLONG;
    
    /* Create directory in upper layer */
    if (mkdir(upper_path, mode) == -1)
        return -errno;
    
    LOG("mkdir: %s (mode: %o)", upper_path, mode);
    return 0;
}

/* ── rmdir (DIRECTORY REMOVAL WITH WHITEOUT) ── */
int unionfs_rmdir(const char *path)
{
    char upper_path[1024], lower_path[1024];
    char whiteout_path[1024];

    if (!path || strlen(path) == 0)
        return -EINVAL;

    if (build_path(upper_path, sizeof(upper_path), STATE->upper, path) < 0)
        return -ENAMETOOLONG;
    if (build_path(lower_path, sizeof(lower_path), STATE->lower, path) < 0)
        return -ENAMETOOLONG;

    DIR *dir;
    struct dirent *de;

    /* Check upper */
    dir = opendir(upper_path);
    if (dir) {
        while ((de = readdir(dir)) != NULL) {
            if (strcmp(de->d_name, ".") && strcmp(de->d_name, "..")) {
                closedir(dir);
                return -ENOTEMPTY;
            }
        }
        closedir(dir);
    }

    /* Check lower */
    dir = opendir(lower_path);
    if (dir) {
        while ((de = readdir(dir)) != NULL) {
            if (strcmp(de->d_name, ".") && strcmp(de->d_name, "..")) {
                if (whiteout_exists(upper_path, de->d_name))
                    continue;
                closedir(dir);
                return -ENOTEMPTY;
            }
        }
        closedir(dir);
    }

    int upper_exists = (access(upper_path, F_OK) == 0);
    int lower_exists = (access(lower_path, F_OK) == 0);

    const char *filename = strrchr(path, '/');
    filename = filename ? filename + 1 : path;

    char wh_name[1024];
    snprintf(wh_name, sizeof(wh_name), "/.wh.%s", filename);

    if (build_path(whiteout_path, sizeof(whiteout_path),
                   STATE->upper, wh_name) < 0)
        return -ENAMETOOLONG;

    if (upper_exists) {
        if (rmdir(upper_path) == -1)
            return -errno;
    }

    if (lower_exists) {
        int fd = open(whiteout_path, O_CREAT | O_WRONLY | O_TRUNC, 0644);
        if (fd == -1)
            return -errno;
        close(fd);
    }

    return 0;
}

/* ── setxattr (SET EXTENDED ATTRIBUTES) ── */
int unionfs_setxattr(const char *path, const char *name,
                     const char *value, size_t size, int flags)
{
    char upper_path[1024];
    char resolved[1024];

    if (path == NULL || name == NULL || value == NULL)
        return -EINVAL;

    if (build_path(upper_path, sizeof(upper_path), STATE->upper, path) < 0)
        return -ENAMETOOLONG;

    if (access(upper_path, F_OK) != 0) {
        int ret = resolve_path(path, resolved, sizeof(resolved));
        if (ret == 0) {
            ret = copy_to_upper(path);
            if (ret != 0) return ret;
        } else {
            return -ENOENT;
        }
    }

#ifdef __APPLE__
    if (setxattr(upper_path, name, value, size, 0, flags) == -1)
#else
    if (setxattr(upper_path, name, value, size, flags) == -1)
#endif
        return -errno;

    LOG("setxattr: %s -> %s", path, name);
    return 0;
}

/* ── getxattr (GET EXTENDED ATTRIBUTES) ── */
int unionfs_getxattr(const char *path, const char *name,
                     char *value, size_t size)
{
    char resolved[1024];

    if (path == NULL || name == NULL)
        return -EINVAL;

    int ret = resolve_path(path, resolved, sizeof(resolved));
    if (ret != 0) return ret;

#ifdef __APPLE__
    ssize_t res = getxattr(resolved, name, value, size, 0, 0);
#else
    ssize_t res = getxattr(resolved, name, value, size);
#endif

    if (res == -1)
        return -errno;

    LOG("getxattr: %s -> %s", path, name);
    return (int)res;
}

/* ── listxattr (LIST EXTENDED ATTRIBUTES) ── */
int unionfs_listxattr(const char *path, char *list, size_t size)
{
    char resolved[1024];

    if (path == NULL)
        return -EINVAL;

    int ret = resolve_path(path, resolved, sizeof(resolved));
    if (ret != 0) return ret;

#ifdef __APPLE__
    ssize_t res = listxattr(resolved, list, size, 0);
#else
    ssize_t res = listxattr(resolved, list, size);
#endif

    if (res == -1)
        return -errno;

    LOG("listxattr: %s", path);
    return (int)res;
}

/* ── removexattr (REMOVE EXTENDED ATTRIBUTES) ── */
int unionfs_removexattr(const char *path, const char *name)
{
    char upper_path[1024];
    char resolved[1024];

    if (path == NULL || name == NULL)
        return -EINVAL;

    if (build_path(upper_path, sizeof(upper_path), STATE->upper, path) < 0)
        return -ENAMETOOLONG;

    if (access(upper_path, F_OK) != 0) {
        int ret = resolve_path(path, resolved, sizeof(resolved));
        if (ret == 0) {
            ret = copy_to_upper(path);
            if (ret != 0) return ret;
        } else {
            return -ENOENT;
        }
    }

#ifdef __APPLE__
    if (removexattr(upper_path, name, 0) == -1)
#else
    if (removexattr(upper_path, name) == -1)
#endif
        return -errno;

    LOG("removexattr: %s -> %s", path, name);
    return 0;
}

/* ── truncate (TRUNCATE FILE SIZE) ── */
int unionfs_truncate(const char *path, off_t size,
                     struct fuse_file_info *fi)
{
    (void) fi;

    char upper_path[1024];
    char resolved[1024];

    if (path == NULL)
        return -EINVAL;

    if (build_path(upper_path, sizeof(upper_path), STATE->upper, path) < 0)
        return -ENAMETOOLONG;

    if (access(upper_path, F_OK) != 0) {
        int ret = resolve_path(path, resolved, sizeof(resolved));
        if (ret == 0) {
            ret = copy_to_upper(path);
            if (ret != 0) return ret;
        } else {
            return -ENOENT;
        }
    }

    if (truncate(upper_path, size) == -1)
        return -errno;

    LOG("truncate: %s to %lld bytes", path, (long long)size);
    return 0;
}


// Add this section before the FUSE operations table (around line 230):

/* ── readlink (READ SYMBOLIC LINK TARGET) ── */
int unionfs_readlink(const char *path, char *buf, size_t size)
{
    char resolved[1024];
    
    /* Input validation */
    if (path == NULL || buf == NULL || size == 0)
        return -EINVAL;
    
    /* Resolve to actual file location */
    int ret = resolve_path(path, resolved, sizeof(resolved));
    if (ret != 0) return ret;
    
    /* Read the symlink */
    ssize_t len = readlink(resolved, buf, size - 1);
    if (len == -1)
        return -errno;
    
    /* Null-terminate the buffer */
    buf[len] = '\0';
    
    LOG("readlink: %s -> %s", path, buf);
    return 0;
}

/* ── symlink (CREATE SYMBOLIC LINK) ── */
int unionfs_symlink(const char *target, const char *linkpath)
{
    char upper_link[1024];
    
    /* Input validation */
    if (target == NULL || linkpath == NULL)
        return -EINVAL;
    
    if (build_path(upper_link, sizeof(upper_link),
                   STATE->upper, linkpath) < 0)
        return -ENAMETOOLONG;
    
    /* Ensure parent directory exists */
    char *parent = strdup(upper_link);
    if (!parent) return -ENOMEM;
    
    char *last_slash = strrchr(parent, '/');
    if (last_slash) {
        *last_slash = '\0';
        struct stat st;
        if (stat(parent, &st) == -1) {
            if (mkdir(parent, 0755) == -1 && errno != EEXIST) {
                free(parent);
                return -errno;
            }
        }
    }
    free(parent);
    
    /* Create the symlink in upper layer */
    if (symlink(target, upper_link) == -1)
        return -errno;
    
    LOG("symlink: %s -> %s", linkpath, target);
    return 0;
}

/* ── rename (RENAME/MOVE FILES AND DIRECTORIES) ── */
int unionfs_rename(const char *oldpath, const char *newpath, unsigned int flags)
{
    (void) flags;
    
    char old_upper[1024], new_upper[1024];
    char old_resolved[1024], old_lower[1024];
    
    /* Input validation */
    if (oldpath == NULL || newpath == NULL)
        return -EINVAL;
    
    if (build_path(old_upper, sizeof(old_upper), STATE->upper, oldpath) < 0)
        return -ENAMETOOLONG;
    if (build_path(new_upper, sizeof(new_upper), STATE->upper, newpath) < 0)
        return -ENAMETOOLONG;
    if (build_path(old_lower, sizeof(old_lower), STATE->lower, oldpath) < 0)
        return -ENAMETOOLONG;
    
    /* Check if source file exists only in lower - need to CoW first */
    int was_in_lower = 0;
    if (access(old_upper, F_OK) != 0) {
        int ret = resolve_path(oldpath, old_resolved, sizeof(old_resolved));
        if (ret == 0) {
            /* Source exists in lower - copy it to upper */
            ret = copy_to_upper(oldpath);
            if (ret != 0) return ret;
            was_in_lower = 1;
        } else {
            /* Source doesn't exist anywhere */
            return -ENOENT;
        }
    }
    
    /* Ensure destination parent directory exists */
    char *parent = strdup(new_upper);
    if (!parent) return -ENOMEM;
    
    char *last_slash = strrchr(parent, '/');
    if (last_slash) {
        *last_slash = '\0';
        struct stat st;
        if (stat(parent, &st) == -1) {
            if (mkdir(parent, 0755) == -1 && errno != EEXIST) {
                free(parent);
                return -errno;
            }
        }
    }
    free(parent);
    
    /* Perform the rename in upper layer */
    if (rename(old_upper, new_upper) == -1)
        return -errno;
    
    /* If source was in lower, create whiteout for old location */
    if (was_in_lower || access(old_lower, F_OK) == 0) {
        const char *filename = strrchr(oldpath, '/');
        filename = filename ? filename + 1 : oldpath;
        
        char *dir_path = strdup(oldpath);
        if (!dir_path) return -errno;  /* Already renamed, just log error */
        
        char *slash = strrchr(dir_path, '/');
        if (slash && slash != dir_path) {
            *slash = '\0';
        } else {
            strcpy(dir_path, "");
        }
        
        char whiteout[1024];
        snprintf(whiteout, sizeof(whiteout),
                 "%s%s/.wh.%s", STATE->upper, 
                 strlen(dir_path) > 0 ? dir_path : "", filename);
        
        int fd = open(whiteout, O_WRONLY | O_CREAT | O_TRUNC, 0644);
        if (fd != -1) {
            close(fd);
            LOG("rename: created whiteout for old location %s", whiteout);
        }
        
        free(dir_path);
    }
    
    LOG("rename: %s -> %s", oldpath, newpath);
    return 0;
}

/* ── chmod (CHANGE PERMISSIONS) ── */
int unionfs_chmod(const char *path, mode_t mode, struct fuse_file_info *fi)
{
    (void) fi;
    
    char upper_path[1024];
    char resolved[1024];
    
    if (path == NULL)
        return -EINVAL;
    
    if (build_path(upper_path, sizeof(upper_path), STATE->upper, path) < 0)
        return -ENAMETOOLONG;
    
    /* If file exists only in lower → trigger CoW */
    if (access(upper_path, F_OK) != 0) {
        int ret = resolve_path(path, resolved, sizeof(resolved));  // ✅ FIX
        if (ret == 0) {
            ret = copy_to_upper(path);
            if (ret != 0) return ret;
        } else {
            return -ENOENT;
        }
    }
    
    if (chmod(upper_path, mode) == -1)
        return -errno;
    
    cache_invalidate(path);  // ✅ VERY IMPORTANT
    
    LOG("chmod: %s mode=%o", path, mode);
    return 0;
}


/* ── chown (CHANGE OWNERSHIP) ── */
int unionfs_chown(const char *path, uid_t uid, gid_t gid,
                  struct fuse_file_info *fi)
{
    (void) fi;
    
    char upper_path[1024];
    char resolved[1024];
    
    if (path == NULL)
        return -EINVAL;
    
    if (build_path(upper_path, sizeof(upper_path), STATE->upper, path) < 0)
        return -ENAMETOOLONG;
    
    /* If file exists only in lower → trigger CoW */
    if (access(upper_path, F_OK) != 0) {
        int ret = resolve_path(path, resolved, sizeof(resolved));  // ✅ FIX
        if (ret == 0) {
            ret = copy_to_upper(path);
            if (ret != 0) return ret;
        } else {
            return -ENOENT;
        }
    }
    
    if (chown(upper_path, uid, gid) == -1)
        return -errno;
    
    cache_invalidate(path);  // ✅ VERY IMPORTANT
    
    LOG("chown: %s uid=%d gid=%d", path, uid, gid);
    return 0;
}

/* ── FUSE operations table ── */

/* ── FUSE operations table ── */
struct fuse_operations unionfs_oper = {
    .getattr    = unionfs_getattr,
    .readdir    = unionfs_readdir,
    .create     = unionfs_create,
    .open       = unionfs_open,
    .read       = unionfs_read,
    .write      = unionfs_write,
    .release    = unionfs_release,
    .unlink     = unionfs_unlink,
    .mkdir      = unionfs_mkdir,
    .rmdir      = unionfs_rmdir,
    .chmod      = unionfs_chmod,
    .chown      = unionfs_chown,
};
