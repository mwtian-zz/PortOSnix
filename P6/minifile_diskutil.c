#include <stdlib.h>

#include "disk.h"
#include "minifile_diskutil.h"
#include "minifile_private.h"

/* Make a file system with the specified number of blocks */
int
minifile_mkfs(disk_t* disk, const char* fs_name, blocknum_t fs_size)
{
    struct sblock sb_;
    sblock_t sb = &sb_;
    struct disk_inode inode_;
    disk_inode_t inode = &inode_;
    struct disk_freeblock freeblock_;
    disk_freeblock_t freeblock = &freeblock_;
    blocknum_t i;

    /* Make a new disk */
    use_existing_disk = 0;
    disk_name = fs_name;
    disk_flags = DISK_READWRITE;
    disk_size = fs_size;
    disk_initialize(disk);

    /* Initialize superblock */
    sb->total_blocks = fs_size;
    sb->total_inodes = fs_size / 10;
    sb->free_ilist_head = 2;
    sb->free_ilist_tail = sb->total_inodes;
    sb->free_inodes = sb->total_inodes - 1;
    sb->free_blist_head = sb->total_inodes + 1;
    sb->free_blist_tail = fs_size - 1;
    sb->free_blocks = sb->free_blist_tail - sb->free_blist_head + 1;
    disk_send_request(disk, 0, (char*)sb, DISK_WRITE);

    /* Initialize root inode */
    inode->type = MINIDIRECTORY;
    inode->size = 0;
    disk_send_request(disk, 1, (char*)inode, DISK_WRITE);

    /* Initialize free inode list */
    inode->type = INODE_EMPTY;
    for (i = sb->free_ilist_head; i < sb->free_ilist_tail; ++i) {
        inode->next = i + 1;
        disk_send_request(disk, i, (char*)inode, DISK_WRITE);
    }
    inode->next = 0;
    disk_send_request(disk, sb->free_ilist_tail, (char*)inode, DISK_WRITE);

    /* Initialize free block list */
    for (i = sb->free_blist_head; i < sb->free_blist_tail; ++i) {
        freeblock->next = i + 1;
        disk_send_request(disk, i, (char*)freeblock, DISK_WRITE);
    }
    freeblock->next = 0;
    disk_send_request(disk, sb->free_blist_tail, (char*)freeblock, DISK_WRITE);

    return 0;
}

/* Check the consistency of the file system on disk */
int
minifile_fsck(disk_t* disk)
{
    return 0;
}
