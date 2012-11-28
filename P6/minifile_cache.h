#ifndef __CACHE_H__
#define __minifile_cache_H__

#include "minifile_cache_private.h"
#include "minifile.h"

/* Data structure to record a path */
typedef struct minifile_item *minifile_item_t;
typedef struct minifile_cache *minifile_cache_t;

/* Create a LRU cache with size as table size, return NULL if fails */
extern minifile_cache_t minifile_cache_new(int size, int max_num);

/*
 * Get an item from cache
 * Return -1 if the destination is not found
 * Return 0 if the destination is found and put it in item
 */
extern int minifile_cache_get_by_dest(minifile_cache_t cache, char dest[],
                                       void **it);

/*
 * Get an item with network address from cache
 * Return -1 if not found
 * Return 0 if found and put it in item
 */
extern int minifile_cache_get_by_addr(minifile_cache_t cache,
                                       network_address_t addr, void **it);

/*
 * Put an item into cache
 * If the cache is full, find a victim to evict
 * Return 0 on success, -1 on failure
 */
extern int minifile_cache_put_item(minifile_cache_t cache, void *item);

/*
 * Put an item into cache by header
 * If the cache is full, find a victim to evict
 * Return 0 on success, -1 on failure
 */
extern int minifile_cache_put_path_from_hdr(minifile_cache_t cache,
        minifile_header_t header);

/*
 * Delete an item from cache
 * Return 0 on success, -1 on failure
 */
extern int minifile_cache_delete_item(minifile_cache_t cache, void *item);

/* Set the maximum item number of cache */
extern void minifile_cache_set_max_num(minifile_cache_t cache, int num);

/* If a item has expired. Return -1 if not, 0 if yes */
extern int minifile_cache_is_expired(minifile_item_t item);

/* Destroy a cache */
extern void minifile_cache_destroy(minifile_cache_t cache);

/*
 * Construct a new minifile path from a header.
 * The cached route reverses the path in the header.
 */
extern minifile_path_t minifile_path_from_hdr(minifile_header_t header);

/* Construct a new discovery history item from header */
extern minifile_disc_hist_t minifile_dischist_from_hdr(minifile_header_t header);

/* Print whole cache with path as items, for debugging */
extern void minifile_cache_print_path(minifile_cache_t cache);

#endif

