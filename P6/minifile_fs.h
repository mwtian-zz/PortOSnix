#ifndef __MINIFILE_FS_H__
#define __MINIFILE_FS_H__

#include "disk.h"
#include "minifile_cache.h"
#include "minifile_inode.h"

#define FS_MAGIC_NUMBER 0xbada55


/* Magic number is four bytes */
typedef uint32_t magicnum_t;

/* Super block on disk */
typedef struct sblock {
    magicnum_t magic_number;
    blocknum_t total_blocks;
    blocknum_t free_blist_head;
    blocknum_t free_blist_tail;
    blocknum_t free_blocks;
    inodenum_t total_inodes;
    inodenum_t free_ilist_head;
    inodenum_t free_ilist_tail;
    inodenum_t free_inodes;
    inodenum_t root;
} *sblock_t;

/* Super block in memory */
typedef struct mem_sblock {
    magicnum_t magic_number;
    blocknum_t total_blocks;
    blocknum_t free_blist_head;
    blocknum_t free_blist_tail;
    blocknum_t free_blocks;
    inodenum_t total_inodes;
    inodenum_t free_ilist_head;
    inodenum_t free_ilist_tail;
    inodenum_t free_inodes;
    inodenum_t root;

    disk_t* disk;
    blocknum_t pos;
    buf_block_t buf;
    semaphore_t lock;
    char init;
} *mem_sblock_t;

struct mem_sblock sb_table[8];
mem_sblock_t sb;
semaphore_t sb_lock;

/* free block on disk */
typedef struct freespace {
    blocknum_t next;
} *freespace_t;


/* Super block management */
int sblock_get(disk_t* disk, mem_sblock_t sbp);
void sblock_put(mem_sblock_t sbp);
int sblock_update(mem_sblock_t sbp);

/* Disk space management functions. Explained before implementations. */
extern blocknum_t balloc(disk_t* disk);
extern void bfree(disk_t* disk, blocknum_t n);


#endif /* __MINIFILE_FS_H__ */
