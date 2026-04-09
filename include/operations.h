#ifndef OPERATIONS_H
#define OPERATIONS_H

#include "unionfs.h"

/* Declared in operations.c — implemented by all teammates */
int unionfs_getattr (const char *, struct stat *, struct fuse_file_info *);
int unionfs_readdir (const char *, void *, fuse_fill_dir_t,
                     off_t, struct fuse_file_info *, enum fuse_readdir_flags);
int unionfs_create  (const char *, mode_t, struct fuse_file_info *);
int unionfs_open    (const char *, struct fuse_file_info *);
int unionfs_read    (const char *, char *, size_t, off_t,
                     struct fuse_file_info *);
int unionfs_write   (const char *, const char *, size_t, off_t,
                     struct fuse_file_info *);
int unionfs_unlink  (const char *);
int unionfs_mkdir   (const char *, mode_t);
int unionfs_rmdir   (const char *);

#endif /* OPERATIONS_H */