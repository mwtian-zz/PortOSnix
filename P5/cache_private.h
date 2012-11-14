#ifndef __CACHE_PRIVATE__
#define __CACHE_PRIVATE__

#include "miniroute.h"

struct cache_item {
	network_address_t addr;          /* Destinatioin going to find */
	char path_len[4];                /* Length of the path to destination */
	char path[MAX_ROUTE_LENGTH][8];  /* Path along to destination */
	
	struct cache_item *list_next;          /* Next item in eviction list */
	struct cache_item *list_prev;          /* Previous item in eviction list */
	struct cache_item *hash_next;          /* Next item in hash table entry */
	struct cache_item *hash_prev;          /* Previous item in hash table entry */
	
	long exp_time;                   /* Expiration time */
};

typedef struct cache_item *cache_item_t;

/* Maybe we can use void* to make it more generic... */
struct cache {
	cache_item_t *items;                   /* Hash table, using chaining */
	cache_item_t list_head;                /* Eviction list head */
	cache_item_t list_tail;                /* Eviction list tail */
	int item_num;                    /* Number of items in cache */
	int table_size;                  /* Hash table size */
	int max_item_num;                /* Maximum item which can be in the cached */
};

typedef struct cache *cache_t;

#endif