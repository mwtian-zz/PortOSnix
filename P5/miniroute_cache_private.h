#ifndef __MINIROUTE_CACHE_PRIVATE__
#define __MINIROUTE_CACHE_PRIVATE__

#include "miniroute.h"
#include "miniroute_cache.h"

struct miniroute_item {
	struct miniroute_item *list_next;          /* Next item in eviction list */
	struct miniroute_item *list_prev;          /* Previous item in eviction list */
	struct miniroute_item *hash_next;          /* Next item in hash table entry */
	struct miniroute_item *hash_prev;          /* Previous item in hash table entry */
    network_address_t addr;          /* Address used as the key */
    long exp_time;                   /* Expiration time */
};

struct miniroute_path {
	struct miniroute_path *list_next;          /* Next item in eviction list */
	struct miniroute_path *list_prev;          /* Previous item in eviction list */
	struct miniroute_path *hash_next;          /* Next item in hash table entry */
	struct miniroute_path *hash_prev;          /* Previous item in hash table entry */
    network_address_t addr;          /* Address used as the key */
    long exp_time;                   /* Expiration time */

	unsigned int path_len;                /* Length of the path to destination */
	network_address_t hop[MAX_ROUTE_LENGTH];  /* Path along to destination */
};

struct miniroute_disc_hist {
	struct miniroute_path *list_next;          /* Next item in eviction list */
	struct miniroute_path *list_prev;          /* Previous item in eviction list */
	struct miniroute_path *hash_next;          /* Next item in hash table entry */
	struct miniroute_path *hash_prev;          /* Previous item in hash table entry */
    network_address_t addr;          /* Address used as the key */
    long exp_time;                   /* Expiration time */

	unsigned int id;                /* Discovery packet id */
};

/* Maybe we can use void* to make it more generic... */
struct miniroute_cache {
	miniroute_item_t *items;                   /* Hash table, using chaining */
	miniroute_item_t list_head;                /* Eviction list head */
	miniroute_item_t list_tail;                /* Eviction list tail */
	int item_num;                    /* Number of items in cache */
	int table_size;                  /* Hash table size */
	int max_item_num;                /* Maximum item which can be in the cached */
	long exp_length;
};


#endif /* __MINIROUTE_CACHE_PRIVATE__ */
