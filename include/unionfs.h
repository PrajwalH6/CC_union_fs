#ifndef UNIONFS_H
#define UNIONFS_H

#include <fuse3/fuse.h>

struct mini_unionfs_state {
    char lower[512];
    char upper[512];
};

#define STATE ((struct mini_unionfs_state *) fuse_get_context()->private_data)

#endif