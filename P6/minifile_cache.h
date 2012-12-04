#ifndef __MINIFILE_CACHE_H__
#define __MINIFILE_CACHE_H__

#include <stdint.h>

#include "disk.h"
#include "queue.h"
#include "queue_private.h"
#include "synch.h"

/* Buffer cache hashing */
#define BUFFER_CACHE_HASH_VALUE 1024
#define BLOCK_NUM_HASH(n) ((n) & 1023)

/* Supported disk address space */
typedef uint64_t blocknum_t;

/* Disk table. Only the first entry, main disk, is used. */
disk_t disk_table[8];
disk_t *maindisk;

/* Used for communication with interrupt handler */
semaphore_t disk_lim;

/* Data structures for buffer cache and cached items */
typedef enum block_state block_state_t;
typedef struct buf_block *buf_block_t;
typedef struct buf_cache *buf_cache_t;

enum block_state {
    BLOCK_EMPTY,
    BLOCK_CLEAN,
    BLOCK_DIRTY
};

struct buf_block {
    struct node node;
    buf_block_t hash_prev;          /* Previous item in hash table entry */
    buf_block_t hash_next;          /* Next item in hash table entry */
    char data[DISK_BLOCK_SIZE];
    disk_t* disk;
    blocknum_t num;
    block_state_t state;
};

struct buf_cache {
    size_t hash_val;
    size_t num_blocks;
    semaphore_t cache_lock;
    semaphore_t block_lock[BUFFER_CACHE_HASH_VALUE];
    semaphore_t block_sig[BUFFER_CACHE_HASH_VALUE];
    buf_block_t hash[BUFFER_CACHE_HASH_VALUE];
    disk_reply_t reply[BUFFER_CACHE_HASH_VALUE];
    queue_t locked;
    queue_t lru;
};

/* Global cache */
buf_cache_t bc;

/* Buffer cache interface, explained before implementations */
extern int minifile_buf_cache_init();
extern int bread(disk_t* disk, blocknum_t n, buf_block_t *bufp);
extern int brelse(buf_block_t buf);
extern int bwrite(buf_block_t buf);
extern void bawrite(buf_block_t buf);
extern void bdwrite(buf_block_t buf);

#endif /* __MINIFILE_CACHE_H__ */

