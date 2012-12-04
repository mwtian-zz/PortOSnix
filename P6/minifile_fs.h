#ifndef __MINIFILE_FS_H__
#define __MINIFILE_FS_H__

#include "disk.h"
#include "minifile_cache.h"

#define FS_MAGIC_NUMBER 0xbada55

#define INODE_START_BLOCK 1 /* Inode starts from block 1 */
#define INODE_SIZE 128    /* Size of inode in bytes on disk */
#define INODE_PER_BLOCK (DISK_BLOCK_SIZE / INODE_SIZE)   /* Number of inodes in one block */

/* Translate inode number to block number. Inode starts from 1 which is root directory */
#define INODE_TO_BLOCK(num) ((((num) - 1) / INODE_PER_BLOCK) + INODE_START_BLOCK)
/* Inode offset within a data block */
#define INODE_OFFSET(num) ((((num) - 1) % INODE_PER_BLOCK) * INODE_SIZE)

#define POINTER_PER_BLOCK 512
#define INODE_DIRECT_BLOCKS 11
#define INODE_INDIRECT_BLOCKS (512)
#define INODE_DOUBLE_BLOCKS (512 * 512)
#define INODE_TRIPLE_BLOCKS (512 * 512 * 512)
#define INODE_MAX_FILE_BLOCKS (INODE_DIRECT_BLOCKS + INODE_INDIRECT_BLOCKS + INODE_DOUBLE_BLOCKS + INODE_TRIPLE_BLOCKS)

/* Size of inode table */
#define INODE_TABLE_SIZE 128

/* Address space of inodes */
typedef uint64_t inodenum_t;

/* Magic number is four bytes */
typedef uint32_t magicnum_t;

/* Types of inodes */
typedef enum inode_type {
    MINIFILE,
    MINIDIRECTORY,
    INODE_EMPTY
} itype_t;

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

/* inode on disk */
typedef struct inode {
        itype_t type;
        size_t size;
        blocknum_t direct[INODE_DIRECT_BLOCKS];
        blocknum_t indirect;
        blocknum_t double_indirect;
        blocknum_t triple_indirect;
} *inode_t;

/* indoe in memory */
typedef struct mem_inode {
    itype_t type;
    size_t size;
    blocknum_t direct[INODE_DIRECT_BLOCKS];
    blocknum_t indirect;
    blocknum_t double_indirect;
    blocknum_t triple_indirect;

    disk_t* disk;
    inodenum_t num;
    buf_block_t buf;
    blocknum_t size_blocks;
    size_t ref_count;
    /* semaphore_t lock; */
} *mem_inode_t;

struct inode _root_inode;
mem_inode_t root_inode;

/*
struct mem_inode inode[INODE_TABLE_SIZE];
*/

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
extern mem_inode_t ialloc(disk_t* disk);
extern void ifree(disk_t* disk, inodenum_t n);
extern int iclear(disk_t* disk, inodenum_t n);
extern int iget(disk_t* disk, inodenum_t n, mem_inode_t *inop);
extern void iput(mem_inode_t ino);
extern int iupdate(mem_inode_t ino);
extern blocknum_t bytemap(disk_t* disk, mem_inode_t ino, size_t byte_offset); /* Byte offset to block number */
extern blocknum_t blockmap(disk_t* disk, mem_inode_t ino, size_t block_offset); /* Block offset to block number */

#endif /* __MINIFILE_FS_H__ */
