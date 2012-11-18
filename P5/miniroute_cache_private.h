#ifndef __MINIROUTE_CACHE_PRIVATE__
#define __MINIROUTE_CACHE_PRIVATE__

#include "miniroute.h"
#include "miniroute_cache.h"

struct miniroute_path {
	network_address_t addr;          /* Destinatioin going to find */
	unsigned int path_len;                /* Length of the path to destination */
	char path[MAX_ROUTE_LENGTH][8];  /* Path along to destination */

	struct miniroute_path *list_next;          /* Next item in eviction list */
	struct miniroute_path *list_prev;          /* Previous item in eviction list */
	struct miniroute_path *hash_next;          /* Next item in hash table entry */
	struct miniroute_path *hash_prev;          /* Previous item in hash table entry */

	long exp_time;                   /* Expiration time */
};


/* Maybe we can use void* to make it more generic... */
struct miniroute_cache {
	miniroute_path_t *items;                   /* Hash table, using chaining */
	miniroute_path_t list_head;                /* Eviction list head */
	miniroute_path_t list_tail;                /* Eviction list tail */
	int item_num;                    /* Number of items in cache */
	int table_size;                  /* Hash table size */
	int max_item_num;                /* Maximum item which can be in the cached */
};


#endif /* __MINIROUTE_CACHE_PRIVATE__ */
