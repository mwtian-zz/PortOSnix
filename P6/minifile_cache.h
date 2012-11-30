#ifndef __MINIFILE_CACHE_H__
#define __MINIFILE_CACHE_H__

#include <stdint.h>

#include "disk.h"
#include "synch.h"


/* Data structures for buffer cache */
typedef struct buf_cache *buf_cache_t;

/* Buffer cache item structure */
typedef struct buf_block *buf_block_t;

/* Supported disk address space */
typedef uint64_t blocknum_t;

struct buf_block {
    buf_block_t list_next;          /* Next item in LRU */
    buf_block_t list_prev;          /* Previous item in LRU */
    buf_block_t hash_next;          /* Next item in hash table entry */
    buf_block_t hash_prev;          /* Previous item in hash table entry */
    char data[DISK_BLOCK_SIZE];
    char mod;
    disk_t* disk;
    blocknum_t num;
};

/* Sempahores used for locking the disk and signaling response */
extern semaphore_t disk_lock;
extern semaphore_t block_sig;

/* Buffer cache interface, explained before implementations */
extern int minifile_buf_cache_init(interrupt_handler_t disk_handler);
extern int bread(disk_t* disk, blocknum_t n, buf_block_t *bufp);
extern int brelse(buf_block_t buf);
extern int bwrite(buf_block_t buf);
extern void bawrite(buf_block_t buf);
extern void bdwrite(buf_block_t buf);

#endif /* __MINIFILE_CACHE_H__ */

