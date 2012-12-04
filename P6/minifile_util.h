#ifndef __MINIFILE_UTIL_H__
#define __MINIFILE_UTIL_H__

#include "minifile_cache.h"
#include "minifile_inode.h"

/* Byte offset within inode to disk block number */
extern blocknum_t bytemap(disk_t* disk, mem_inode_t ino, size_t byte_offset); 
/* Block offset within inode to disk block number */
extern blocknum_t blockmap(disk_t* disk, mem_inode_t ino, size_t block_offset); 


#endif /* __MINIFILE_UTIL_H__ */