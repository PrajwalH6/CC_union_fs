#ifndef CACHE_H
#define CACHE_H

#include <time.h>
#include <pthread.h>

#define CACHE_SIZE 256
#define CACHE_TTL_SECONDS 30

/* Cache entry structure */
struct cache_entry {
    char vpath[512];        /* Virtual path (key) */
    char rpath[1024];       /* Resolved real path (value) */
    time_t timestamp;       /* When entry was created */
    int valid;              /* Is this entry valid? */
    int is_whiteout;        /* Is this path whiteout? */
};

/* Cache structure */
struct path_cache {
    struct cache_entry entries[CACHE_SIZE];
    pthread_mutex_t lock;   /* Thread safety */
    unsigned long hits;     /* Statistics */
    unsigned long misses;
    unsigned long invalidations;
};

/* Function declarations */
void cache_init(void);
void cache_destroy(void);
void cache_invalidate(const char *vpath);
void cache_clear(void);
int cache_lookup(const char *vpath, char *rpath, size_t rpath_size);
void cache_insert(const char *vpath, const char *rpath, int is_whiteout);
int resolve_path_cached(const char *path, char *resolved, size_t size);

#endif /* CACHE_H */
