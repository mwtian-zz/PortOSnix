#ifndef __MINIFILE_BLOCK_H__
#define __MINIFILE_BLOCK_H__

#include "synch.h"
#include "minifile_private.h"

/* Sempahores used for locking the disk and signaling response */
extern semaphore_t disk_lock;
extern semaphore_t block_sig;

/* Block operations */
extern blocknum_t alloc_blk();
extern void free_blk(blocknum_t blocknum);
extern int get_blk(char* buffer, blocknum_t blocknum);
extern int put_blk(char* buffer, blocknum_t blocknum);

#endif /* __MINIFILE_BLOCK_H__ */
