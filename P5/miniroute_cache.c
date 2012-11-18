#include "miniroute_cache.h"
#include "miniroute.h"
#include "network.h"
#include "interrupts.h"
#include "miniheader.h"
#include <stdlib.h>

extern long ticks;

/* Create a cache with size as table size, return NULL if fails */
miniroute_cache_t
miniroute_cache_new(size_t size)
{
    miniroute_cache_t new_cache;

    if (size <= 0) {
        return NULL;
    }

    new_cache = malloc(sizeof(struct miniroute_cache));
    if (new_cache == NULL) {
        return NULL;
    }
    new_cache->items = malloc(sizeof(miniroute_item_t) * size);
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
miniroute_cache_get_by_dest(miniroute_cache_t cache, char dest[], void **it)
{
    network_address_t addr;
    unpack_address(dest, addr);
    return miniroute_cache_get_by_addr(cache, addr, it);
}

/*
 * Get an item with network address from cache
 * Return -1 if not found
 * Return 0 if found and put it in item
 */
int
miniroute_cache_get_by_addr(miniroute_cache_t cache, network_address_t addr, void **it)
{
    int hash_num;
    miniroute_item_t head;
    miniroute_item_t *item = (miniroute_item_t*) it;
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

    *item = NULL;
    return -1;
}
/*
 * Put an item into cache
 * If the cache is full, find a victim to evict
 * Return 0 on success, -1 on failure
 */
int
miniroute_cache_put_item(miniroute_cache_t cache, void *it)
{
    miniroute_item_t item = it;
    miniroute_item_t item_to_evict;
    int hash_num;

    if (cache->item_num >= cache->max_item_num) {
        item_to_evict = cache->list_head;
        /* This shouldn't happen */
        if (item_to_evict == NULL) {
            return -1;
        }
        miniroute_cache_delete_item(cache, item_to_evict);
    }

    hash_num = hash_address(item->addr) % cache->table_size;
    item->hash_next = cache->items[hash_num];
    if (cache->items[hash_num] != NULL) {
        cache->items[hash_num]->hash_prev = item;
    }
    cache->items[hash_num] = item;
    if (cache->list_tail != NULL) {
        cache->list_tail->list_next = item;
        item->list_prev = cache->list_tail;
        cache->list_tail = item;
    } else {
        cache->list_tail = item;
        cache->list_head = item;
    }
    cache->item_num++;

    return 0;
}

/*
 * Put an item into cache by header
 * If the cache is full, find a victim to evict
 * Return 0 on success, -1 on failure
 */
int
miniroute_cache_put_path_from_hdr(miniroute_cache_t cache, miniroute_header_t header)
{
    miniroute_item_t item;
    int val;

    item = (miniroute_item_t) miniroute_path_from_hdr(header);
    if (item == NULL) {
        return -1;
    }
    val = miniroute_cache_put_item(cache, item);
    if (val == -1) {
        free(item);
    }
    return val;
}

/*
 * Delete an item from cache
 * Return 0 on success, -1 on failure
 */
int
miniroute_cache_delete_item(miniroute_cache_t cache, void *it)
{
    int hash_num;
    miniroute_item_t item = it;

    if (item == NULL) {
        return -1;
    }
    if (item == cache->list_head) {
        cache->list_head = item->list_next;
    }
    if (item == cache->list_tail) {
        cache->list_tail = item->list_prev;
    }
    if (item->list_next != NULL) {
        item->list_next->list_prev = item->list_prev;
    }
    if (item->list_prev != NULL) {
        item->list_prev->list_next = item->list_next;
    }
    hash_num = hash_address(item->addr) % cache->table_size;
    if (cache->items[hash_num] == item) {
        cache->items[hash_num] = item->hash_next;
    }
    if (item->hash_next != NULL) {
        item->hash_next->hash_prev = item->hash_prev;
    }
    if (item->hash_prev != NULL) {
        item->hash_prev->hash_next = item->hash_next;
    }
    free(item);
    cache->item_num--;
    return 0;
}

/* Set the maximum item number of cache */
void
miniroute_cache_set_max_num(miniroute_cache_t cache, int num)
{
    if (num <= 0) {
        return;
    }
    cache->max_item_num = num;
}

/* If a item has expired. Return -1 if not, 0 if yes */
int
miniroute_cache_is_expired(miniroute_item_t item)
{
    return item->exp_time >= ticks ? 0 : -1;
}

/* Print whole cache, for debugging */
void
miniroute_cache_print_path(miniroute_cache_t cache)
{
    miniroute_path_t head;
    int i;

    printf("Total number of item: %d\n", cache->item_num);
    printf("Table size: %d\n", cache->table_size);
    printf("Maximum number of items: %d\n", cache->max_item_num);
    printf("Hash table is:\n");

    for (i = 0; i < cache->table_size; i++) {
        head = (miniroute_path_t) cache->items[i];
        if (head) {
            printf("Row %d: ", i);
            while (head) {
                network_printaddr(head->addr);
                printf(" ");
                head = head->hash_next;
            }
            printf("\n");
        }
    }

    printf("List is:\n");
    head = (miniroute_path_t) cache->list_head;
    while (head) {
        network_printaddr(head->addr);
        printf(" ");
        head = head->list_next;
    }
    printf("\n\n");
}

/*
 * Construct a new miniroute cache item from a header.
 * The cached route reverses the path in the header.
 */
miniroute_path_t
miniroute_path_from_hdr(miniroute_header_t header)
{
    miniroute_path_t item;
    network_address_t addr;
    int i;

    item = malloc(sizeof(struct miniroute_path));
    if (item == NULL) {
        return NULL;
    }
    unpack_address(header->destination, addr);
    network_address_copy(addr, item->addr);
    item->path_len = unpack_unsigned_int(header->path_len);

    /* Reverse path in header */
    for (i = 0; i < item->path_len; i++) {
        unpack_address(header->path[item->path_len - i - 1], item->hop[i]);
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
miniroute_cache_destroy(miniroute_cache_t cache)
{
    free(cache->items);
    free(cache);
}
