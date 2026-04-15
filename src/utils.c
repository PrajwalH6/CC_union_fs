#include "cow.h"

int build_path(char *dest, size_t dest_size,
               const char *base, const char *path)
{
    if (strlen(base) + strlen(path) + 1 >= dest_size)
        return -ENAMETOOLONG;
    snprintf(dest, dest_size, "%s%s", base, path);
    return 0;
}

int is_whiteout(const char *name)
{
    return strncmp(name, ".wh.", 4) == 0;
}

int whiteout_exists(const char *dir_upper, const char *name)
{
    char wh_path[1024];
    snprintf(wh_path, sizeof(wh_path), "%s/.wh.%s", dir_upper, name);
    return access(wh_path, F_OK) == 0;
}

int resolve_path(const char *path, char *resolved, size_t size)
{
    char upper_path[1024], lower_path[1024];

    if (build_path(upper_path, sizeof(upper_path), STATE->upper, path) < 0)
        return -ENAMETOOLONG;
    if (build_path(lower_path, sizeof(lower_path), STATE->lower, path) < 0)
        return -ENAMETOOLONG;

    if (access(upper_path, F_OK) == 0) {
        snprintf(resolved, size, "%s", upper_path);
        return 0;
    }
    if (access(lower_path, F_OK) == 0) {
        snprintf(resolved, size, "%s", lower_path);
        return 0;
    }
    return -ENOENT;
}