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
    char upper_path[1024];
    char lower_path[1024];
    char whiteout_path[1024];

    /* Build paths */
    if (build_path(upper_path, sizeof(upper_path), STATE->upper, path) < 0)
        return -ENAMETOOLONG;
    if (build_path(lower_path, sizeof(lower_path), STATE->lower, path) < 0)
        return -ENAMETOOLONG;

    /* Extract filename */
    const char *filename = strrchr(path, '/');
    filename = filename ? filename + 1 : path;

    /* Extract directory path */
    char dir_path[1024];
    strncpy(dir_path, path, sizeof(dir_path));
    dir_path[sizeof(dir_path) - 1] = '\0';

    char *last_slash = strrchr(dir_path, '/');
    if (last_slash && last_slash != dir_path) {
        *last_slash = '\0';
    } else {
        strcpy(dir_path, "");
    }

    /* Build correct whiteout path */
    snprintf(whiteout_path, sizeof(whiteout_path),
             "%s%s/.wh.%s",
             STATE->upper,
             strlen(dir_path) > 0 ? dir_path : "",
             filename);

    /* Check whiteout FIRST */
    if (access(whiteout_path, F_OK) == 0) {
        return -ENOENT;   // file is logically deleted
    }

    /* Check upper layer */
    if (access(upper_path, F_OK) == 0) {
        strncpy(resolved, upper_path, size - 1);
        resolved[size - 1] = '\0';
        return 0;
    }

    /* Check lower layer */
    if (access(lower_path, F_OK) == 0) {
        strncpy(resolved, lower_path, size - 1);
        resolved[size - 1] = '\0';
        return 0;
    }

    return -ENOENT;
}