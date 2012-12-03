#include "defs.h"

#include "disk.h"
#include "minifile_cache.h"
#include "minithread.h"
#include "synch.h"

static blocknum_t total_blocks = 128;
static semaphore_t sig;
static int shift[10];

int cache_multithread_test(int *arg)
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

int cache_test(int *arg)
{
    buf_block_t buf;
    blocknum_t i, j;
    blocknum_t* block;

    bread(maindisk, 0, &buf);
    block = (blocknum_t*) buf->data;
    for (i = 0; i < DISK_BLOCK_SIZE / sizeof(blocknum_t); ++i)
        block[i] = i;
    bwrite(buf);

    bread(maindisk, 127, &buf);
    block = (blocknum_t*) buf->data;
    for (i = 0; i < DISK_BLOCK_SIZE / sizeof(blocknum_t); ++i)
        block[i] = i + 127;
    bwrite(buf);

    printf("Checking block 0... ");
    bread(maindisk, 0, &buf);
    block = (blocknum_t*) buf->data;
    for (i = 0; i < DISK_BLOCK_SIZE / sizeof(blocknum_t); ++i) {
        if (block[i] != i) {
            printf("Error in write %ld!\n", i);
            break;
        }
    }
    brelse(buf);
    printf("Check finished\n");

    printf("Checking block 127... ");
    bread(maindisk, 127, &buf);
    block = (blocknum_t*) buf->data;
    for (i = 0; i < DISK_BLOCK_SIZE / sizeof(blocknum_t); ++i) {
        if (block[i] != (i + 127)) {
            printf("Error in write %ld!\n", i);
            break;
        }
    }
    brelse(buf);
    printf("Check finished\n");

    sig = semaphore_new(0);
    for (j = 0; j < 10; ++j) {
        shift[j] = j + 11;
        minithread_fork(cache_multithread_test, &(shift[j]));
    }
    for (j = 0; j < 10; ++j) {
        semaphore_P(sig);
    }

    printf("Checking block 0 after multithreaded operation... ");
    bread(maindisk, 0, &buf);
    block = (blocknum_t*) buf->data;
    printf("block[0]: %ld\n", block[0]);
    for (i = 1; i < DISK_BLOCK_SIZE / sizeof(blocknum_t); ++i) {
        if (block[i] != i + block[0]) {
            printf("Error in write %ld!\n", i);
            break;
        }
    }
    brelse(buf);
    printf("Check finished\n");

    return 0;
}

int main(int argc, char** argv)
{
    use_existing_disk = 0;
    disk_name = "minidisk";
    disk_flags = DISK_READWRITE;
    disk_size = total_blocks;

    minithread_system_initialize(cache_test, NULL);

    return 0;
}
