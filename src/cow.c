#include <fcntl.h>
#include <unistd.h>
#include "unionfs.h"
#include "utils.h"
#include "cow.h"

void copy_to_upper(const char *path) {
    char lower_path[1024], upper_path[1024];

    build_path(lower_path, STATE->lower, path);
    build_path(upper_path, STATE->upper, path);

    int src = open(lower_path, O_RDONLY);
    int dst = open(upper_path, O_WRONLY | O_CREAT | O_TRUNC, 0644);

    char buf[4096];
    int n;

    while ((n = read(src, buf, sizeof(buf))) > 0)
        write(dst, buf, n);

    close(src);
    close(dst);
}