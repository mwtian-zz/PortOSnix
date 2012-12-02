#include "defs.h"

#include "minifile.h"
#include "minifile_cache.h"


/*********************************************************************
 * Semantics of using a block:
 * (1) Create a buf_block_t pointer.
 *     Know the block num to use (from inode, or allocated by balloc).
 * (2) Use bread to get the block.
 * (3) Use bwrite/bdwrite/bawrite/brelse to return the block,
 *     and schedule/immediately write to disk.
 * (+) Before the block is returned, no other thread can use it. <- To do
 ********************************************************************/


/* Functions on hash list */
static buf_block_t hash_find(disk_t* disk, blocknum_t n, blocknum_t bhash);
static void hash_add(buf_block_t block, blocknum_t bhash);
static void hash_remove(buf_block_t block, blocknum_t bhash);
static buf_block_t get_blk();

/* Initialize buffer cache */
int
minifile_buf_cache_init()
{
    int i;

    disk_lim = semaphore_new(MAX_PENDING_DISK_REQUESTS);
    bc = malloc(sizeof(struct buf_cache));
    if (NULL == disk_lim || NULL == bc)
        goto err1;

    bc->locked = queue_new();
    bc->lru = queue_new();
    for (i = 0; i < BUFFER_CACHE_HASH; ++i) {
        bc->block_lock[i] = semaphore_new(1);
        bc->block_sig[i] = semaphore_new(0);
    }
    bc->cache_lock = semaphore_new(1);

    if (NULL == bc->locked || NULL == bc->lru || NULL == bc->cache_lock) {
        goto err2;
    }
    for (i = 0; i < BUFFER_CACHE_HASH; ++i) {
        if (NULL == bc->block_sig[i] || NULL == bc->block_lock[i]) {
            goto err2;
        }
    }

    return 0;

err2:
    queue_free(bc->locked);
    queue_free(bc->lru);
    semaphore_destroy(bc->cache_lock);
    for (i = 0; i < BUFFER_CACHE_HASH; ++i) {
        semaphore_destroy(bc->block_lock[i]);
        semaphore_destroy(bc->block_sig[i]);
    }
err1:
    semaphore_destroy(disk_lim);
    free(bc);
    return -1;
}

static buf_block_t
hash_find(disk_t* disk, blocknum_t n, blocknum_t bhash)
{
    buf_block_t p_block = bc->hash[bhash];
    while (p_block != NULL) {
        if (p_block->disk == disk && p_block->num == n)
            break;
        p_block = p_block->hash_next;
    }
    return p_block;
}

static void
hash_add(buf_block_t block, blocknum_t bhash)
{
    block->hash_prev = NULL;
    block->hash_next = bc->hash[bhash];
    if (block->hash_next != NULL) {
        block->hash_next->hash_prev = block;
    }
    bc->hash[bhash] = block;
}

static void
hash_remove(buf_block_t block, blocknum_t bhash)
{
    bc->hash[bhash] = block->hash_next;
    if (block->hash_prev != NULL) {
        block->hash_prev->hash_next = block->hash_next;
    }
    if (block->hash_next != NULL) {
        block->hash_next->hash_prev = block->hash_prev;
    }
    block->hash_prev = NULL;
    block->hash_next = NULL;
}

static buf_block_t
get_blk()
{
    buf_block_t block;
    queue_dequeue(bc->lru, (void**)&block);
    if (NULL == block)
        block = malloc(sizeof(struct buf_block));
    return block;
}

/* For block n, get a pointer to its block buffer */
int
bread(disk_t* disk, blocknum_t n, buf_block_t *bufp)
{
    blocknum_t bhash = BUF_HASH(n);

    if (NULL == bufp || NULL == disk || disk->layout.size < n) {
        return -1;
    }

    semaphore_P(bc->block_lock[bhash]);
    semaphore_P(bc->cache_lock);

    *bufp = hash_find(disk, n, bhash);
    if (NULL != *bufp) {
        queue_delete(bc->lru, (void**)bufp);
        queue_append(bc->locked, *bufp);
    } else {
        *bufp = get_blk();
        if (NULL == *bufp) {
            return -1;
        }
        (*bufp)->disk = disk;
        (*bufp)->num = n;

        semaphore_V(bc->cache_lock);

        semaphore_P(disk_lim);
        disk_read_block(disk, n, (*bufp)->data);
        semaphore_P(bc->block_sig[bhash]);
        semaphore_V(disk_lim);

        semaphore_P(bc->cache_lock);

        hash_add(*bufp, bhash);
        queue_append(bc->locked, *bufp);
    }

    semaphore_V(bc->cache_lock);

    return 0;
}


/* Only release the buffer, no write scheduled */
int
brelse(buf_block_t buf)
{
    semaphore_P(bc->cache_lock);
    queue_delete(bc->locked, (void**)&buf);
    queue_append(bc->lru, buf);
    semaphore_V(bc->cache_lock);
    semaphore_V(bc->block_lock[BUF_HASH(buf->num)]);
    return 0;
}


/* Immediately write buffer back to disk, block until write finishes */
int bwrite(buf_block_t buf)
{
    semaphore_P(disk_lim);
    disk_write_block(buf->disk, buf->num, buf->data);
    semaphore_P(bc->block_sig[BUF_HASH(buf->num)]);
    semaphore_V(disk_lim);
    brelse(buf);
    return 0;
}


/* Schedule write immediately, but do not block */
void bawrite(buf_block_t buf)
{
    bwrite(buf);
}

/* Only mark buffer dirty, no write scheduled */
void bdwrite(buf_block_t buf)
{
    bwrite(buf);
}



///* Create a cache with size as table size, return NULL if fails */
//minifile_cache_t
//minifile_cache_new(int size, int max_num_entry)
//{
//    minifile_cache_t new_cache;
//
//    if (size <= 0) {
//        return NULL;
//    }
//
//    new_cache = malloc(sizeof(struct minifile_cache));
//    if (new_cache == NULL) {
//        return NULL;
//    }
//    new_cache->items = malloc(sizeof(minifile_item_t) * size);
//    if (new_cache->items == NULL) {
//        free(new_cache);
//        return NULL;
//    }
//    memset(new_cache->items, 0, sizeof(minifile_item_t) * size);
//    new_cache->item_num = 0;
//    new_cache->table_size = size;
//    new_cache->list_head = NULL;
//    new_cache->list_tail = NULL;
//    new_cache->max_item_num = max_num_entry;
//    new_cache->exp_length = exp_length;
//    return new_cache;
//}
//
//
///*
// * Put an item into cache
// * If the cache is full, find a victim to evict
// * Return 0 on success, -1 on failure
// */
//int
//minifile_cache_put_item(minifile_cache_t cache, void *it)
//{
//    minifile_item_t item = it;
//    minifile_item_t item_to_evict;
//    minifile_item_t exist_item;
//    int hash_num;
//
//    if (minifile_cache_get_by_addr(cache, item->addr, (void**)&exist_item) == 0) {
//        minifile_cache_delete_item(cache, exist_item);
//    }
//
//    if (cache->item_num >= cache->max_item_num) {
//        item_to_evict = cache->list_head;
//        /* This shouldn't happen */
//        if (item_to_evict == NULL) {
//            return -1;
//        }
//        minifile_cache_delete_item(cache, item_to_evict);
//    }
//
//    hash_num = hash_address(item->addr) % cache->table_size;
//    item->hash_next = cache->items[hash_num];
//    if (cache->items[hash_num] != NULL) {
//        cache->items[hash_num]->hash_prev = item;
//    }
//    cache->items[hash_num] = item;
//    if (cache->list_tail != NULL) {
//        cache->list_tail->list_next = item;
//        item->list_prev = cache->list_tail;
//        cache->list_tail = item;
//    } else {
//        cache->list_tail = item;
//        cache->list_head = item;
//    }
//    item->exp_time = ticks + cache->exp_length;
//
//    cache->item_num++;
//    return 0;
//}
//
///*
// * Delete an item from cache
// * Return 0 on success, -1 on failure
// */
//int
//minifile_cache_delete_item(minifile_cache_t cache, void *it)
//{
//    int hash_num;
//    minifile_item_t item = it;
//
//    if (item == NULL) {
//        return -1;
//    }
//    if (item == cache->list_head) {
//        cache->list_head = item->list_next;
//    }
//    if (item == cache->list_tail) {
//        cache->list_tail = item->list_prev;
//    }
//    if (item->list_next != NULL) {
//        item->list_next->list_prev = item->list_prev;
//    }
//    if (item->list_prev != NULL) {
//        item->list_prev->list_next = item->list_next;
//    }
//    hash_num = hash_address(item->addr) % cache->table_size;
//    if (cache->items[hash_num] == item) {
//        cache->items[hash_num] = item->hash_next;
//    }
//    if (item->hash_next != NULL) {
//        item->hash_next->hash_prev = item->hash_prev;
//    }
//    if (item->hash_prev != NULL) {
//        item->hash_prev->hash_next = item->hash_next;
//    }
//    free(item);
//    cache->item_num--;
//    return 0;
//}
//
///* Set the maximum item number of cache */
//void
//minifile_cache_set_max_num(minifile_cache_t cache, int num)
//{
//    if (num <= 0) {
//        return;
//    }
//    cache->max_item_num = num;
//}
//
///* Destroy a cache */
//void
//minifile_cache_destroy(minifile_cache_t cache)
//{
//    free(cache->items);
//    free(cache);
//}
//
