#include "cow.h"

int copy_to_upper(const char *path)
{
    char lower_path[1024], upper_path[1024];
    struct stat st;
    char buf[4096];
    ssize_t n;
    int ret = 0;

    if (build_path(lower_path, sizeof(lower_path), STATE->lower, path) < 0)
        return -ENAMETOOLONG;
    if (build_path(upper_path, sizeof(upper_path), STATE->upper, path) < 0)
        return -ENAMETOOLONG;

    /* Get source metadata FIRST */
    if (lstat(lower_path, &st) == -1)
        return -errno;

    int src = open(lower_path, O_RDONLY);
    if (src == -1) return -errno;

    int dst = open(upper_path, O_WRONLY | O_CREAT | O_TRUNC, st.st_mode);
    if (dst == -1) { ret = -errno; goto close_src; }

    while ((n = read(src, buf, sizeof(buf))) > 0) {
        if (write(dst, buf, (size_t)n) != n) {
            ret = -errno;
            goto close_both;
        }
    }
    if (n < 0) { ret = -errno; goto close_both; }

    /* Preserve permissions, ownership, timestamps */
    fchown(dst, st.st_uid, st.st_gid);
    fchmod(dst, st.st_mode);

    struct timespec times[2] = { st.st_atim, st.st_mtim };
    futimens(dst, times);

    LOG("CoW: copied %s -> %s", lower_path, upper_path);

close_both:
    close(dst);
close_src:
    close(src);
    return ret;
}