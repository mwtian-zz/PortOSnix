#include "defs.h"

#include "disk.h"
#include "minifile_cache.h"
#include "minifile_fs.h"
#include "minifile_inode.h"

#include "minithread.h"
#include "synch.h"

static blocknum_t disk_num_blocks = 32;
static semaphore_t sig;
//static int shift[10];

int fs_multithread_test(int *arg)
{
    buf_block_t buf;
    blocknum_t i;
    blocknum_t* block;

    bread(maindisk, 0, &buf);
    block = (blocknum_t*) buf->data;
    for (i = 0; i < DISK_BLOCK_SIZE / sizeof(blocknum_t); ++i)
        block[i] = i + *arg;
    printf("Multi threaded write with block[0] = %ld\n", block[0]);
    bwrite(buf);

    semaphore_V(sig);
    return 0;
}

int fs_test(int *arg)
{
    buf_block_t block[disk_num_blocks];
    mem_inode_t inode[disk_num_blocks];
    blocknum_t i, j;
    freenode_t freenode;
//    blocknum_t* block;
//    blocknum_t inodes_blocks = disk_num_blocks / INODE_PER_BLOCK;
printf("In file system test.\n");
    /* Test super block read/write */
    fs_format(mainsb);
    sblock_print(mainsb);

    /* Test block allocation/free */
    i = 0;
    while (mainsb->free_blocks > 0) {
        block[i] = balloc(maindisk);
        block[i]->data[0] = 1;
        //printf("Free blocks left: %ld.\n", mainsb->free_blocks);
        i++;
    }
    printf("Allocated all blocks. Free blocks left: %ld.\n", mainsb->free_blocks);

    i = 0;
    while (mainsb->free_blocks < mainsb->total_data_blocks) {
        bfree(block[i]);
        //printf("Free blocks left: %ld.\n", mainsb->free_blocks);
        i++;
    }
    printf("Freed all blocks. Free block left: %ld.\n", mainsb->free_blocks);

    /* Test inode allocation/free */
    i = 0;
    while (mainsb->free_inodes > 0) {
printf("Allocating\n");
        inode[i] = ialloc(maindisk);
printf("Got inode %ld\n", inode[i]->num);
        iclear(inode[i]);
        iput(inode[i]);
        i++;
    }
    printf("Allocated all inodes. Free inodes left: %ld.\n", mainsb->free_inodes);

    i = 0;
    while (mainsb->free_inodes < mainsb->total_inodes) {
printf("Freeing %ld\n", i);
        iget(maindisk, i, &inode[i]);
        ifree(inode[i]);
        i++;
    }

    printf("Freed all inodes. Free inode left: %ld.\n", mainsb->free_inodes);

    return 0;
}

int main(int argc, char** argv)
{
    use_existing_disk = 0;
    disk_name = "minidisk";
    disk_flags = DISK_READWRITE;
    disk_size = disk_num_blocks;

    minithread_system_initialize(fs_test, NULL);

    return 0;
}
