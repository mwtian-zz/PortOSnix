#ifndef __MINIFILE_PRIVATE_H__
#define __MINIFILE_PRIVATE_H__

#include "minifile_cache.h"
#include "minifile_fs.h"

/*
 * struct minifile:
 *     This is the structure that keeps the information about
 *     the opened file like the position of the cursor, etc.
 */

struct minifile {
    mem_inode_t inode;
    inodenum_t inode_num;
    int block_cursor;
    int byte_cursor;
    int byte_in_block;
    char mode[3];
};

#endif /* __MINIFILE_PRIVATE_H__ */
