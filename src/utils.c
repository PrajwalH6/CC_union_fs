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
    build_path(upper_path, sizeof(upper_path), STATE->upper, path);
    build_path(lower_path, sizeof(lower_path), STATE->lower, path);

    /* Extract filename */
    const char *filename = strrchr(path, '/');
    filename = filename ? filename + 1 : path;

    /* 🔥 Check whiteout FIRST */
    snprintf(whiteout_path, sizeof(whiteout_path),
             "%s/.wh.%s", STATE->upper, filename);

    if (access(whiteout_path, F_OK) == 0) {
        return -ENOENT;   // 🔥 hide file
    }

    /* Check upper */
    if (access(upper_path, F_OK) == 0) {
        strncpy(resolved, upper_path, size);
        return 0;
    }

    /* Check lower */
    if (access(lower_path, F_OK) == 0) {
        strncpy(resolved, lower_path, size);
        return 0;
    }

    return -ENOENT;
}