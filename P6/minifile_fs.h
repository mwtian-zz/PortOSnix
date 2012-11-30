#ifndef __MINIFILE_FS_H__
#define __MINIFILE_FS_H__

#include "disk.h"
#include "minifile_cache.h"


typedef enum inode_type {
    MINIFILE,
    MINIDIRECTORY,
    INODE_EMPTY
} itype_t;

typedef struct sblock {
    blocknum_t total_blocks;
    blocknum_t total_inodes;
    blocknum_t free_ilist_head;
    blocknum_t free_ilist_tail;
    blocknum_t free_inodes;
    blocknum_t free_blist_head;
    blocknum_t free_blist_tail;
    blocknum_t free_blocks;
} *sblock_t;

typedef struct mem_sblock {
    blocknum_t total_blocks;
    blocknum_t total_inodes;
    blocknum_t free_ilist_head;
    blocknum_t free_ilist_tail;
    blocknum_t free_inodes;
    blocknum_t free_blist_head;
    blocknum_t free_blist_tail;
    blocknum_t free_blocks;

    disk_t* disk;
    blocknum_t pos;
    buf_block_t buf;
} *mem_sblock_t;


typedef struct inode {
        itype_t type;
        size_t size;
        blocknum_t direct[12];
        blocknum_t indirect1;
        blocknum_t indirect2;
} *inode_t;

typedef struct mem_inode {
    itype_t type;
    size_t size_bytes;
    blocknum_t direct[12];
    blocknum_t indirect1;
    blocknum_t indirect2;

    blocknum_t num;
    blocknum_t size_blocks;
} *mem_inode_t;

typedef struct freeblock {
    blocknum_t next;
} *freeblock_t;

/* Disk space management functions. Explained before implementations. */
extern blocknum_t balloc(disk_t* disk);
extern void bfree(disk_t* disk, blocknum_t n);
extern blocknum_t ialloc(disk_t* disk);
extern void ifree(disk_t* disk, blocknum_t n);
extern int iclear(disk_t* disk, blocknum_t n);
extern int iget(disk_t* disk, blocknum_t n, mem_inode_t *inodep);
extern void iput(mem_inode_t inode);
extern int iupdate(mem_inode_t inode);

#endif /* __MINIFILE_FS_H__ */
