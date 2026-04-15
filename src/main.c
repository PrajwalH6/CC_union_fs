#include "unionfs.h"
#include "operations.h"
#include "cache.h"

extern struct fuse_operations unionfs_oper;

int main(int argc, char *argv[])
{
    if (argc < 4) {
        fprintf(stderr, "Usage: %s <lower> <upper> <mountpoint> [fuse opts]\n",
                argv[0]);
        return 1;
    }

    struct mini_unionfs_state *state = calloc(1, sizeof(*state));
    if (!state) {
        perror("calloc");
        return 1;
    }

    if (!realpath(argv[1], state->lower)) {
        perror("lower");
        free(state);
        return 1;
    }

    if (!realpath(argv[2], state->upper)) {
        perror("upper");
        free(state);
        return 1;
    }

    /* Prepare FUSE arguments (skip lower/upper) */
    int fuse_argc = argc - 2;
    char **fuse_argv = malloc((size_t)(fuse_argc + 1) * sizeof(char *));
    if (!fuse_argv) {
        perror("malloc");
        free(state);
        return 1;
    }

    fuse_argv[0] = argv[0];
    for (int i = 1; i < fuse_argc; i++)
        fuse_argv[i] = argv[i + 2];
    fuse_argv[fuse_argc] = NULL;

    /* 🔥 Initialize cache BEFORE mounting */
    cache_init();

    /* Mount filesystem */
    int ret = fuse_main(fuse_argc, fuse_argv, &unionfs_oper, state);

    /* 🔥 Destroy cache AFTER unmount */
    cache_destroy();

    free(fuse_argv);
    free(state);

    return ret;
}