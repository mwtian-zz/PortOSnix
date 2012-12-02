#include "defs.h"

#include "disk.h"
#include "minifile_cache.h"
#include "minithread.h"

static blocknum_t total_blocks = 128;

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

    bread(maindisk, 0, &buf);
    block = (blocknum_t*) buf->data;
    for (i = 0; i < DISK_BLOCK_SIZE / sizeof(blocknum_t); ++i) {
        printf("%ld\n", block[i]);
        if (block[i] != i) {
            printf("Error in write %ld!\n", i);
            break;
        }
    }
    brelse(buf);

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
