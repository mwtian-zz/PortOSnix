#include <stdlib.h>

#include "disk.h"
#include "minifile_cache.h"
#include "minifile_diskutil.h"
#include "minifile_fs.h"

/* Make a file system with the specified number of blocks */
int
minifile_mkfs(disk_t* disk, const char* fs_name, blocknum_t fs_size)
{
    buf_block_t buf;
    sblock_t sb;
    disk_inode_t inode;
    freeblock_t freeblock;
    blocknum_t i;

    /* Make a new disk */
    use_existing_disk = 0;
    disk_name = fs_name;
    disk_flags = DISK_READWRITE;
    disk_size = fs_size;
    disk_initialize(disk);

    /* Initialize superblock */
    bread(disk, 0, &buf);
    sb = (sblock_t) buf->data;
    sb->total_blocks = fs_size;
    sb->total_inodes = fs_size / 10;
    sb->free_ilist_head = 2;
    sb->free_ilist_tail = sb->total_inodes;
    sb->free_inodes = sb->total_inodes - 1;
    sb->free_blist_head = sb->total_inodes + 1;
    sb->free_blist_tail = fs_size - 1;
    sb->free_blocks = sb->free_blist_tail - sb->free_blist_head + 1;
    bwrite(buf);


    /* Initialize root inode */
    bread(disk, 1, &buf);
    inode = (disk_inode_t) buf->data;
    inode->type = MINIDIRECTORY;
    inode->size = 0;
    bwrite(buf);

    /* Initialize free inode list */
    for (i = sb->free_ilist_head; i < sb->free_ilist_tail; ++i) {
        bread(disk, i, &buf);
        freeblock = (freeblock_t) buf->data;
        freeblock->next = i + 1;
        bwrite(buf);
    }
    bread(disk, sb->free_ilist_tail, &buf);
    freeblock = (freeblock_t) buf->data;
    freeblock->next = 0;
    bwrite(buf);

    /* Initialize free block list */
    for (i = sb->free_blist_head; i < sb->free_blist_tail; ++i) {
        bread(disk, i, &buf);
        freeblock = (freeblock_t) buf->data;
        freeblock->next = i + 1;
        bwrite(buf);
    }
    bread(disk, sb->free_blist_tail, &buf);
    freeblock = (freeblock_t) buf->data;
    freeblock->next = 0;
    bwrite(buf);

    printf("minifile system started at '%s'.\n", fs_name);

    return 0;
}

/* Check the consistency of the file system on disk */
int
minifile_fsck(disk_t* disk)
{
    return 0;
}
