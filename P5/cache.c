#include "cache.h"
#include "miniroute.h"
#include "network.h"
#include "interrupts.h"
#include "miniheader.h"
#include <stdlib.h>

extern long ticks;

/* Create a cache with size as table size, return NULL if fails */
cache_t
cache_new(size_t size) {
	cache_t new_cache;

	if (size <= 0) {
		return NULL;
	}
	
	new_cache = malloc(sizeof(struct cache));
	if (new_cache == NULL) {
		return NULL;
	}
	new_cache->items = malloc(sizeof(cache_item_t) * size);
	if (new_cache->items == NULL) {
		free(new_cache);
		return NULL;
	}
	memset(new_cache->items, 0, size);
	new_cache->item_num = 0;
	new_cache->table_size = size;
	new_cache->list_head = NULL;
	new_cache->list_tail = NULL;
	new_cache->max_item_num = SIZE_OF_ROUTE_CACHE;
	return new_cache;
}

/* 
 * Get an item with char destination from cache
 * Return -1 if the destination is not found
 * Return 0 if the destination is found and put it in item
 */
int 
cache_get_by_dest(cache_t cache, char dest[], cache_item_t *item) {
	network_address_t addr;
	unpack_address(dest, addr);
	return cache_get_by_addr(cache, addr, item);
}

/*
 * Get an item with network address from cache
 * Return -1 if not found
 * Return 0 if found and put it in item
 */
int
cache_get_by_addr(cache_t cache, network_address_t addr, cache_item_t *item) {
	int hash_num;
	cache_item_t head;
	
	if (item == NULL || cache == NULL) {
		return -1;
	}
	hash_num = hash_address(addr) % cache->table_size;
	head = cache->items[hash_num];
	while (head != NULL) {
		if (network_address_same(addr, head->addr)) {
			*item = head;
			return 0;
		}
		head = head->hash_next;
	}
	return -1;
}
/*
 * Put an item into cache
 * If the cache is full, find a victim to evict 
 * Return 0 on success, -1 on failure
 */
int 
cache_put_item(cache_t cache, cache_item_t item) {
	cache_item_t item_to_evict;
	int hash_num;
	
	if (cache->item_num >= cache->max_item_num) {
		item_to_evict = cache->list_tail;
		/* This shouldn't happen */
		if (item_to_evict == NULL) {
			return -1;
		}
		cache->list_tail = item_to_evict->list_prev;
		if (cache->list_head == item_to_evict) {
			cache->list_head = NULL;
		}
		hash_num = hash_address(item_to_evict->addr) % cache->table_size;
		if (cache->items[hash_num] == item_to_evict) {
			cache->items[hash_num] = item_to_evict->hash_next;
		}
		if (item_to_evict->hash_next != NULL) {
			item_to_evict->hash_next->hash_prev = item_to_evict->hash_prev;
		}
		if (item_to_evict->hash_prev != NULL) {
			item_to_evict->hash_prev->hash_next = item_to_evict->hash_next;
		}
		free(item_to_evict);
		cache->item_num--;
	}
	
	hash_num = hash_address(item->addr) % cache->table_size;
	item->hash_next = cache->items[hash_num];
    cache->items[hash_num] = item;
	if (cache->list_tail != NULL) {
		cache->list_tail->list_next = item;
		item->list_prev = cache->list_tail;
	} else {
		cache->list_tail = item;
		cache->list_head = item;
	}
	cache->item_num++;
	
	return 0;
}


/* Set the maximum item number of cache */
void 
cache_set_max_num(cache_t cache, int num) {
	if (num <= 0) {
		return;
	}
	cache->max_item_num = num;
}

/* If a item has expired. Return -1 if not, 0 if yes */
int 
cache_is_expired(cache_item_t item) {
	return item->exp_time >= ticks ? 0 : -1;
}

/* Construct a new item from a header*/
cache_item_t
item_new(struct routing_header header) {
	cache_item_t item;
	network_address_t addr;
	unsigned int len;
	int i;
	
	item = malloc(sizeof(struct cache_item));
	if (item == NULL) {
		return NULL;
	}
	unpack_address(header.destination, addr);
	network_address_copy(addr, item->addr);
	memcpy(item->path_len, header.path_len, 4);
	len = unpack_unsigned_int(header.path_len);
	for (i = 0; i < len; i++) {
		memcpy(item->path[i], header.path[i], 8);
	}
	item->hash_next = NULL;
	item->hash_prev = NULL;
	item->list_next = NULL;
	item->list_prev = NULL;
	item->exp_time = ticks + (3 * (SECOND / PERIOD));
	return item;
}

/* Destroy a cache */
void 
cache_destroy(cache_t cache) {
	free(cache->items);
	free(cache);
}