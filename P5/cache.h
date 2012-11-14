#ifndef __CACHE_H__
#define __CACHE_H__

#include "cache_private.h"
#include "miniroute.h"

/* Create a cache with size as table size, return NULL if fails */
extern cache_t cache_new(size_t size);

/* 
 * Get an item from cache
 * Return -1 if the destination is not found
 * Return 0 if the destination is found and put it in item
 */
extern int cache_get_dest(cache_t cache, char dest[], cache_item_t *item);

/*
 * Get an item with network address from cache
 * Return -1 if not found
 * Return 0 if found and put it in item
 */
extern int cache_get_by_addr(cache_t cache, network_address_t addr, cache_item_t *item);

/*
 * Put an item into cache
 * If the cache is full, find a victim to evict 
 * Return 0 on success, -1 on failure
 */
extern int cache_put_item(cache_t cache, cache_item_t item);

/* Set the maximum item number of cache */
extern void cache_set_max_num(cache_t cache, int num);

/* If a item has expired. Return -1 if not, 0 if yes */
extern int cache_is_expired(cache_item_t item);

/* Destroy a cache */
extern void cache_destroy(cache_t cache);

/* Construct a new item from a header*/
extern cache_item_t item_new(struct routing_header header);


#endif