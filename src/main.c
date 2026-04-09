#define FUSE_USE_VERSION 31
#include <fuse3/fuse.h>
#include <stdio.h>
#include <stdlib.h>
#include <limits.h>

#include "unionfs.h"
#include "operations.h"

int main(int argc, char *argv[]) {
    if (argc < 4) {
        printf("Usage: %s <lower> <upper> <mountpoint>\n", argv[0]);
        return 1;
    }

    struct mini_unionfs_state *state = malloc(sizeof(struct mini_unionfs_state));

    realpath(argv[1], state->lower);
    realpath(argv[2], state->upper);

    char *fuse_argv[] = { argv[0], argv[3], "-f" };
    int fuse_argc = 3;

    return fuse_main(fuse_argc, fuse_argv, &unionfs_oper, state);
}