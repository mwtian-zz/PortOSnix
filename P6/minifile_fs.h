#ifndef __MINIFILE_FS_H__
#define __MINIFILE_FS_H__

#include "disk.h"
#include "minifile_cache.h"
#include "minifile_inode.h"

#define MINIFS_MAGIC_NUMBER 0xbadb01

#define INODE_START_BLOCK 1 /* Inode starts from block 1 */
#define INODE_SIZE 128    /* Size of inode in bytes on disk */
#define INODE_PER_BLOCK (DISK_BLOCK_SIZE / INODE_SIZE)   /* Number of inodes in one block */

#define BITS_PER_BLOCK (8 * (DISK_BLOCK_SIZE))

/* Magic number is four bytes */
typedef uint32_t magicnum_t;

/* Bitmap for free inodes and blocks */
typedef unsigned char* bitmap_t;

/* Super block on disk */
typedef struct sblock {
    magicnum_t magic_number;
    blocknum_t disk_num_blocks;

    inodenum_t total_inodes;
    inodenum_t first_inode_block;
    inodenum_t root_inum;

    blocknum_t inode_bitmap_first;
    blocknum_t inode_bitmap_last;

    blocknum_t block_bitmap_first;
    blocknum_t block_bitmap_last;

    blocknum_t total_data_blocks;
    blocknum_t first_data_block;
} *sblock_t;

/* Super block in memory */
typedef struct mem_sblock {
    magicnum_t magic_number;
    blocknum_t disk_num_blocks;

    inodenum_t total_inodes;
    inodenum_t first_inode_block;
    inodenum_t root_inum;

    blocknum_t inode_bitmap_first;
    blocknum_t inode_bitmap_last;

    blocknum_t block_bitmap_first;
    blocknum_t block_bitmap_last;

    blocknum_t total_data_blocks;
    blocknum_t first_data_block;

    disk_t* disk;
    blocknum_t pos;

    buf_block_t sb_buf;

    bitmap_t block_bitmap;
    bitmap_t inode_bitmap;
    blocknum_t free_blocks;
    blocknum_t free_inodes;

    char init;
    semaphore_t filesys_lock;
} *mem_sblock_t;

struct mem_sblock sb_table[8];
mem_sblock_t mainsb;
semaphore_t sb_lock;

/* free block on disk */
typedef struct freenode {
    blocknum_t next;
} *freenode_t;


/* Super block management */
extern int sblock_get(disk_t* disk, mem_sblock_t sbp);
extern int sblock_update(mem_sblock_t sbp);
extern void sblock_put(mem_sblock_t sbp);
extern int sblock_format(mem_sblock_t sbp, blocknum_t disk_num_blocks);
extern int sblock_isvalid(mem_sblock_t sbp);
extern void sblock_print(mem_sblock_t sbp);

/* File system management */
int
fs_format(mem_sblock_t sbp);
int
fs_init(mem_sblock_t sbp);
void
fs_lock(mem_sblock_t sbp);
void
fs_unlock(mem_sblock_t sbp);

/* Disk space management functions. Explained before implementations. */
extern buf_block_t balloc(disk_t* disk);
extern void bfree(buf_block_t block);
extern int blist_check(mem_sblock_t sbp);

extern mem_inode_t ialloc(disk_t* disk);
extern void ifree(mem_inode_t inode);
int ilist_check(mem_sblock_t sbp);

extern int iread(disk_t* disk, inodenum_t n, mem_inode_t *inop);
extern int iwrite(mem_inode_t ino);
extern void irelse(mem_inode_t ino);

#endif /* __MINIFILE_FS_H__ */
