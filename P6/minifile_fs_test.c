#include "defs.h"

#include "disk.h"
#include "minifile_cache.h"
#include "minifile_fs.h"
#include "minifile_inode.h"

#include "minithread.h"
#include "synch.h"

static blocknum_t total_blocks = 128;
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
    buf_block_t block;
    blocknum_t i, j;
    freenode_t freenode;
//    blocknum_t* block;
//    blocknum_t inodes_blocks = total_blocks / INODE_PER_BLOCK;

    /* Test super block read/write */
    sblock_get(maindisk, mainsb);
    sblock_init(mainsb, total_blocks);
    sblock_update(mainsb);
    sblock_get(maindisk, mainsb);
    sblock_print(mainsb);
    sblock_put(mainsb);

    /* Initialize free block list */
    sblock_get(maindisk, mainsb);
    for (i = mainsb->free_blist_head; i < mainsb->free_blist_tail; ++i) {
        bread(maindisk, i, &block);
        freenode = (freenode_t) block->data;
        freenode->next = i + 1;
        bwrite(block);
    }
    bread(maindisk, mainsb->free_blist_tail, &block);
    freenode = (freenode_t) block->data;
    freenode->next = 0;
    bwrite(block);
    sblock_put(mainsb);
    printf("Free block list initialized.\n");
    if (0 == blist_check(mainsb))
        printf("Free block list correct.\n");
    else
        printf("Free block list incorrect.\n");

    /* Test block allocation/free */
    for (i = 0; i < mainsb->free_blocks; ++i) {
        block = balloc(maindisk);
        block->data[0] = 1;
        bfree(block);
    }
    sblock_get(maindisk, mainsb);
    printf("Allocated and free all blocks. Free block left: %ld.\n", mainsb->free_blocks);
    sblock_put(mainsb);
    if (0 == blist_check(mainsb))
        printf("Free block list correct.\n");
    else
        printf("Free block list incorrect.\n");

//    bread(maindisk, 0, &buf);
//    block = (blocknum_t*) buf->data;
//    for (i = 0; i < DISK_BLOCK_SIZE / sizeof(blocknum_t); ++i)
//        block[i] = i;
//    bwrite(buf);
//
//    bread(maindisk, 127, &buf);
//    block = (blocknum_t*) buf->data;
//    for (i = 0; i < DISK_BLOCK_SIZE / sizeof(blocknum_t); ++i)
//        block[i] = i + 127;
//    bwrite(buf);
//
//    printf("Checking block 0... ");
//    bread(maindisk, 0, &buf);
//    block = (blocknum_t*) buf->data;
//    for (i = 0; i < DISK_BLOCK_SIZE / sizeof(blocknum_t); ++i) {
//        if (block[i] != i) {
//            printf("Error in write %ld!\n", i);
//            break;
//        }
//    }
//    brelse(buf);
//    printf("Check finished\n");
//
//    printf("Checking block 127... ");
//    bread(maindisk, 127, &buf);
//    block = (blocknum_t*) buf->data;
//    for (i = 0; i < DISK_BLOCK_SIZE / sizeof(blocknum_t); ++i) {
//        if (block[i] != (i + 127)) {
//            printf("Error in write %ld!\n", i);
//            break;
//        }
//    }
//    brelse(buf);
//    printf("Check finished\n");
//
//    sig = semaphore_new(0);
//    for (j = 0; j < 10; ++j) {
//        shift[j] = j + 11;
//        minithread_fork(cache_multithread_test, &(shift[j]));
//    }
//    for (j = 0; j < 10; ++j) {
//        semaphore_P(sig);
//    }
//
//    printf("Checking block 0 after multithreaded operation... ");
//    bread(maindisk, 0, &buf);
//    block = (blocknum_t*) buf->data;
//    printf("block[0]: %ld\n", block[0]);
//    for (i = 1; i < DISK_BLOCK_SIZE / sizeof(blocknum_t); ++i) {
//        if (block[i] != i + block[0]) {
//            printf("Error in write %ld!\n", i);
//            break;
//        }
//    }
//    brelse(buf);
//    printf("Check finished\n");

    return 0;
}

int main(int argc, char** argv)
{
    use_existing_disk = 0;
    disk_name = "minidisk";
    disk_flags = DISK_READWRITE;
    disk_size = total_blocks;

    minithread_system_initialize(fs_test, NULL);

    return 0;
}

