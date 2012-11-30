#ifndef __MINIFILE_PATH_H__
#define __MINIFILE_PATH_H__

#include "minifile_cache.h"
#include "minifile_fs.h"

#define MAX_NAME_LENGTH 255
#define DIR_ENTRY_SIZE sizeof(struct dir_entry)

struct dir_entry {
    char name[MAX_NAME_LENGTH + 1];
    inodenum_t inode_num;
}

#endif /* __MINIFILE_FS_H__ */
