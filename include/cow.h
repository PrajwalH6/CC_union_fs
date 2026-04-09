#ifndef COW_H
#define COW_H

#include "unionfs.h"

/* Returns 0 on success, -errno on failure */
int copy_to_upper(const char *path);

/* Returns 0 if name is a whiteout marker (.wh.<name>) */
int is_whiteout(const char *name);

/* Returns 0 if a whiteout for `name` exists in upper_dir */
int whiteout_exists(const char *dir_upper, const char *name);

/* Builds "base + path" safely. Returns -ENAMETOOLONG on overflow. */
int build_path(char *dest, size_t dest_size,
               const char *base, const char *path);

/* Resolves path to real path on disk. Returns 0 and fills resolved,
   or -ENOENT if not found in either layer. */
int resolve_path(const char *path, char *resolved, size_t size);

#endif /* COW_H */