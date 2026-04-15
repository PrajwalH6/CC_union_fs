#ifndef UTILS_H
#define UTILS_H

#include <stddef.h>   // for size_t

/* Build full path */
void build_path(char *dest, const char *base, const char *path);

/* Resolve path (used by cache) */
int resolve_path(const char *path, char *resolved, size_t size);

#endif
