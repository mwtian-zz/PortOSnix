#ifndef __MINIFILE_BLOCK_H__
#define __MINIFILE_BLOCK_H__

#include "synch.h"
#include "minifile_private.h"


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
}

/* Sempahores used for locking the disk and signaling response */
extern semaphore_t disk_lim;
extern semaphore_t block_sig;

/* For block n, get a pointer to its block buffer */
extern int bread(blocknum_t n, buf_block_t *buffer);
/* Only release the buffer, no write scheduled */
extern int brelse(buf_block_t buffer);
/* Immediately write buffer back to disk, block until write finishes */
extern int bwrite(buf_block_t buffer);
/* Schedule write immediately, but do not block */
extern void bawrite(buf_block_t buffer);
/* Only mark buffer dirty, no write scheduled */
extern void bdwrite(buf_block_t buffer);

#endif /* __MINIFILE_BLOCK_H__ */
