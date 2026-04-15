#ifndef OPERATIONS_H
#define OPERATIONS_H

#include "unionfs.h"

#ifdef __APPLE__
int unionfs_getattr (const char *, struct stat *);
#else
int unionfs_getattr (const char *, struct stat *, struct fuse_file_info *);
#endif
int unionfs_create  (const char *, mode_t, struct fuse_file_info *);
int unionfs_open    (const char *, struct fuse_file_info *);
int unionfs_read    (const char *, char *, size_t, off_t, struct fuse_file_info *);
int unionfs_write   (const char *, const char *, size_t, off_t, struct fuse_file_info *);
int unionfs_unlink  (const char *);
int unionfs_mkdir   (const char *, mode_t);
int unionfs_rmdir   (const char *);

#ifdef __APPLE__
int unionfs_readdir(const char *, void *, fuse_fill_dir_t,
                    off_t, struct fuse_file_info *);
#else
int unionfs_readdir(const char *, void *, fuse_fill_dir_t,
                    off_t, struct fuse_file_info *, enum fuse_readdir_flags);
#endif

int unionfs_setxattr    (const char *, const char *, const char *, size_t, int);
int unionfs_getxattr    (const char *, const char *, char *, size_t);
int unionfs_listxattr   (const char *, char *, size_t);
int unionfs_removexattr (const char *, const char *);
int unionfs_truncate    (const char *, off_t, struct fuse_file_info *);


int unionfs_readlink (const char *, char *, size_t);
int unionfs_symlink  (const char *, const char *);
int unionfs_rename   (const char *, const char *, unsigned int);

int unionfs_chmod (const char *, mode_t, struct fuse_file_info *);
int unionfs_chown (const char *, uid_t, gid_t, struct fuse_file_info *);

#endif /* OPERATIONS_H */
