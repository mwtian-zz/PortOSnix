#ifndef __CACHE_H__
#define __minifile_cache_H__

#include "disk.h"
#include "minifile.h"

/* Data structures for buffer cache */

typedef struct buf_cache *buf_cache_t;


/* Create a LRU cache with size as table size, return NULL if fails */
extern buf_cache_t minifile_cache_new(int size, int max_num);

/*
 * Put an item into cache
 * If the cache is full, find a victim to evict
 * Return 0 on success, -1 on failure
 */
extern int minifile_cache_put_item(buf_cache_t cache, buf_block_t item);

/*
 * Delete an item from cache
 * Return 0 on success, -1 on failure
 */
extern int minifile_cache_delete_item(buf_cache_t cache, buf_block_t item);

/* Set the maximum item number of cache */
extern void minifile_cache_set_max_num(buf_cache_t cache, int num);

/* Destroy a cache */
extern void minifile_cache_destroy(buf_cache_t cache);


#endif

