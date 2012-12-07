#ifndef __MINIFILE_UTIL_H__
#define __MINIFILE_UTIL_H__

#include "minifile_cache.h"
#include "minifile_inode.h"

/* Translate inode number to block number. Inode starts from 1 which is root directory */
#define INODE_TO_BLOCK(num) (((num) / INODE_PER_BLOCK) + INODE_START_BLOCK)
/* Inode offset within a data block */
#define INODE_OFFSET(num) (((num) % INODE_PER_BLOCK) * INODE_SIZE)

#define POINTER_PER_BLOCK 512
#define INODE_INDIRECT_BLOCKS (512)
#define INODE_DOUBLE_BLOCKS (512 * 512)
#define INODE_TRIPLE_BLOCKS (512 * 512 * 512)
#define INODE_MAX_FILE_BLOCKS (INODE_DIRECT_BLOCKS + INODE_INDIRECT_BLOCKS + INODE_DOUBLE_BLOCKS + INODE_TRIPLE_BLOCKS)


/* Byte offset within inode to disk block number */
extern blocknum_t bytemap(disk_t* disk, mem_inode_t ino, size_t byte_offset);
/* Block offset within inode to disk block number */
extern blocknum_t blockmap(disk_t* disk, mem_inode_t ino, size_t block_offset);
/* Find file name from path */
extern char* pathtofile(char* path);


#endif /* __MINIFILE_UTIL_H__ */
