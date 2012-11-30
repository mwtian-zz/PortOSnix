#ifndef __MINIFILE_CACHE_H__
#define __MINIFILE_CACHE_H__

#include "disk.h"
#include "synch.h"
#include "minifile.h"
#include "minifile_private.h"

/* Data structures for buffer cache */
typedef struct buf_cache *buf_cache_t;


/* Buffer cache item structure */
typedef struct buf_block *buf_block_t;

struct buf_block {
    buf_block_t list_next;          /* Next item in LRU */
	buf_block_t list_prev;          /* Previous item in LRU */
	buf_block_t hash_next;          /* Next item in hash table entry */
	buf_block_t hash_prev;          /* Previous item in hash table entry */
    char data[DISK_BLOCK_SIZE];
    char mod;
    blocknum_t num;
};

/* Sempahores used for locking the disk and signaling response */
extern semaphore_t disk_lock;
extern semaphore_t block_sig;

/* Buffer cache interface, explained before implementations */
extern int minifile_buf_cache_init();
extern int bread(blocknum_t n, buf_block_t *buffer);
extern int brelse(buf_block_t buffer);
extern int bwrite(buf_block_t buffer);
extern void bawrite(buf_block_t buffer);
extern void bdwrite(buf_block_t buffer);

#endif /* __MINIFILE_CACHE_H__ */

