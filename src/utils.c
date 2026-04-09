#include <stdio.h>
#include <string.h>
#include "utils.h"

void build_path(char *dest, const char *base, const char *path) {
    snprintf(dest, 1024, "%s%s", base, path);
}