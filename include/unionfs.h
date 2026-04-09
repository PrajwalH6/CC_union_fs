#ifndef UNIONFS_H
#define UNIONFS_H

/* Auto-detect FUSE version per platform */
#ifdef __APPLE__
  #define FUSE_USE_VERSION 26
  #include <fuse.h>
#else
  #define FUSE_USE_VERSION 31
  #include <fuse3/fuse.h>
#endif

#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/stat.h>
#include <stdlib.h>
#include <dirent.h>

/* ── State ── */
struct mini_unionfs_state {
    char lower[512];
    char upper[512];
};

#define STATE \
    ((struct mini_unionfs_state *) fuse_get_context()->private_data)

/* ── Debug logging ── */
#ifdef DEBUG
#define LOG(fmt, ...) \
    fprintf(stderr, "[UnionFS] " fmt "\n", ##__VA_ARGS__)
#else
#define LOG(fmt, ...) do {} while (0)
#endif

#endif /* UNIONFS_H */