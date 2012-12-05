#ifndef __MINIFILE_PATH_H__
#define __MINIFILE_PATH_H__

#include "minifile_cache.h"
#include "minifile_fs.h"

#define MAX_NAME_LENGTH 255
#define DIR_ENTRY_SIZE sizeof(struct dir_entry)
#define ENTRY_NUM_PER_BLOCK (DISK_BLOCK_SIZE / DIR_ENTRY_SIZE)

typedef struct dir_entry {
    char name[MAX_NAME_LENGTH + 1];
    inodenum_t inode_num;
} *dir_entry_t;

/* Translate path to inode number base on working directory, return 0 on failure*/
extern inodenum_t namei(char* path); 
/* Translate path to inode number base on given inode, return 0 on failure*/
extern inodenum_t nameinode(char* path, mem_inode_t ino);
/* Get all the directory entries in directory inode, return NULL if no entries */
extern dir_entry_t* get_directory_entry(disk_t* disk, mem_inode_t ino, int* entry_size);
#endif /* __MINIFILE_FS_H__ */
