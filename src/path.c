#include <string.h>
#include <unistd.h>
#include <errno.h>
#include "unionfs.h"
#include "utils.h"
#include "path.h"

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