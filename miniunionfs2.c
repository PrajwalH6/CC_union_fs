#define FUSE_USE_VERSION 31

#include <fuse3/fuse.h>
#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/stat.h>
#include <stdlib.h>

// Same state struct 
struct mini_unionfs_state {
    char lower[512];
    char upper[512];
};

#define STATE ((struct mini_unionfs_state *) fuse_get_context()->private_data)

// Helper function 
void build_path(char *dest, const char *base, const char *path) {
    snprintf(dest, 1024, "%s%s", base, path);
}

// ================== FEATURE 1: WHITEOUT ===================== //

// Implements unlink with whiteout logic (CORE REQUIREMENT)

static int unionfs_unlink(const char *path) {
    char upper_path[1024], lower_path[1024], whiteout_path[1024];

    build_path(upper_path, STATE->upper, path);
    build_path(lower_path, STATE->lower, path);

    // Case 1: File exists in upper → delete normally
    if (access(upper_path, F_OK) == 0) {
        if (unlink(upper_path) == -1)
            return -errno;
        return 0;
    }

    // Case 2: File exists only in lower → create whiteout
    if (access(lower_path, F_OK) == 0) {
        char *filename = strrchr(path, '/');
        filename = filename ? filename + 1 : (char *)path;

        // Create .wh.<filename> in upper directory
        snprintf(whiteout_path, sizeof(whiteout_path),
                 "%s/.wh.%s", STATE->upper, filename);

        int fd = open(whiteout_path, O_CREAT | O_WRONLY, 0644);
        if (fd == -1)
            return -errno;

        close(fd);
        return 0;
    }

    return -ENOENT;
}

// ================== FEATURE 2: MKDIR ========================= //
// Creates directory ONLY in upper layer (easy feature)

static int unionfs_mkdir(const char *path, mode_t mode) {
    char upper_path[1024];

    build_path(upper_path, STATE->upper, path);

    if (mkdir(upper_path, mode) == -1)
        return -errno;

    return 0;
}

// ================== FEATURE 3: RMDIR ========================= //
// Removes directory ONLY from upper layer (easy feature)
static int unionfs_rmdir(const char *path) {
    char upper_path[1024];

    build_path(upper_path, STATE->upper, path);

    if (rmdir(upper_path) == -1)
        return -errno;

    return 0;
}


struct fuse_operations unionfs_extra_oper = {
    .unlink = unionfs_unlink,   // WHITEOUT FEATURE
    .mkdir  = unionfs_mkdir,    // CREATE DIRECTORY
    .rmdir  = unionfs_rmdir,    // REMOVE DIRECTORY
};
