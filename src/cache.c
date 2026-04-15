#include "cache.h"
#include "utils.h"
#include <string.h>
#include <stdio.h>
#include <time.h>
#include <pthread.h>
#include <errno.h>

#define LOG(fmt, ...) printf("[CACHE] " fmt "\n", ##__VA_ARGS__)

/* Global cache instance */
static struct path_cache g_cache;

/* Initialize cache (call this in main.c after fuse_main setup) */
void cache_init(void)
{
    pthread_mutex_init(&g_cache.lock, NULL);
    memset(&g_cache.entries, 0, sizeof(g_cache.entries));
    g_cache.hits = 0;
    g_cache.misses = 0;
    g_cache.invalidations = 0;
    LOG("Cache initialized with %d entries", CACHE_SIZE);
}

/* Destroy cache (call this on unmount) */
void cache_destroy(void)
{
    pthread_mutex_destroy(&g_cache.lock);
    LOG("Cache stats: hits=%lu misses=%lu invalidations=%lu",
        g_cache.hits, g_cache.misses, g_cache.invalidations);
    
    if (g_cache.hits + g_cache.misses > 0) {
        LOG("Cache hit rate: %.1f%%",
            g_cache.hits * 100.0 / (g_cache.hits + g_cache.misses));
    }
}

/* Simple hash function for path */
static unsigned int hash_path(const char *path)
{
    unsigned int hash = 5381;
    int c;
    
    while ((c = *path++))
        hash = ((hash << 5) + hash) + c;
    
    return hash % CACHE_SIZE;
}

/* Lookup path in cache */
int cache_lookup(const char *vpath, char *rpath, size_t rpath_size)
{
    pthread_mutex_lock(&g_cache.lock);
    
    unsigned int idx = hash_path(vpath);
    struct cache_entry *entry = &g_cache.entries[idx];
    
    /* Check if entry is valid and matches */
    if (entry->valid &&
        strcmp(entry->vpath, vpath) == 0 &&
        (time(NULL) - entry->timestamp) < CACHE_TTL_SECONDS) {
        
        if (entry->is_whiteout) {
            pthread_mutex_unlock(&g_cache.lock);
            g_cache.hits++;
            return -ENOENT;  /* Cached whiteout */
        }
        
        strncpy(rpath, entry->rpath, rpath_size - 1);
        rpath[rpath_size - 1] = '\0';
        g_cache.hits++;
        pthread_mutex_unlock(&g_cache.lock);
        LOG("Cache HIT: %s -> %s", vpath, rpath);
        return 0;
    }
    
    g_cache.misses++;
    pthread_mutex_unlock(&g_cache.lock);
    LOG("Cache MISS: %s", vpath);
    return -1;  /* Not found or expired */
}

/* Insert path into cache */
void cache_insert(const char *vpath, const char *rpath, int is_whiteout)
{
    pthread_mutex_lock(&g_cache.lock);
    
    unsigned int idx = hash_path(vpath);
    struct cache_entry *entry = &g_cache.entries[idx];
    
    strncpy(entry->vpath, vpath, sizeof(entry->vpath) - 1);
    entry->vpath[sizeof(entry->vpath) - 1] = '\0';
    
    if (rpath && !is_whiteout) {
        strncpy(entry->rpath, rpath, sizeof(entry->rpath) - 1);
        entry->rpath[sizeof(entry->rpath) - 1] = '\0';
    }
    
    entry->timestamp = time(NULL);
    entry->valid = 1;
    entry->is_whiteout = is_whiteout;
    
    pthread_mutex_unlock(&g_cache.lock);
    LOG("Cache INSERT: %s -> %s", vpath, 
        is_whiteout ? "[WHITEOUT]" : (rpath ? rpath : ""));
}

/* Invalidate cache entry (call on write/delete) */
void cache_invalidate(const char *vpath)
{
    pthread_mutex_lock(&g_cache.lock);
    
    unsigned int idx = hash_path(vpath);
    struct cache_entry *entry = &g_cache.entries[idx];
    
    if (entry->valid && strcmp(entry->vpath, vpath) == 0) {
        entry->valid = 0;
        g_cache.invalidations++;
        LOG("Cache INVALIDATE: %s", vpath);
    }
    
    pthread_mutex_unlock(&g_cache.lock);
}

/* Invalidate entire cache (call after bulk operations) */
void cache_clear(void)
{
    pthread_mutex_lock(&g_cache.lock);
    
    for (int i = 0; i < CACHE_SIZE; i++) {
        g_cache.entries[i].valid = 0;
    }
    g_cache.invalidations++;
    
    pthread_mutex_unlock(&g_cache.lock);
    LOG("Cache CLEARED");
}

/* Wrapper around resolve_path that uses cache */
int resolve_path_cached(const char *path, char *resolved, size_t size)
{
    /* Try cache first */
    int ret = cache_lookup(path, resolved, size);
    if (ret == 0 || ret == -ENOENT)
        return ret;  /* Cache hit or cached whiteout */
    
    /* Cache miss - resolve normally */
    ret = resolve_path(path, resolved, size);
    
    /* Insert into cache */
    if (ret == 0) {
        cache_insert(path, resolved, 0);
    } else if (ret == -ENOENT) {
        cache_insert(path, NULL, 1);  /* Cache whiteout */
    }
    
    return ret;
}
