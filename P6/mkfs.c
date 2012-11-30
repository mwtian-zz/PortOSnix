#include <string.h>

#include "minithread.h"
#include "minifile_private.h"
#include "minifile_diskutil.h"

static blocknum_t total_blocks;

int mkfs(int *arg)
{
    disk_t disk;
    return minifile_mkfs(&disk, "minidisk", total_blocks);
}

int main(int argc, char** argv)
{
    if (argc != 2) {
        return -1;
    }
    total_blocks = atoi(argv[1]);

    minithread_system_initialize(mkfs, NULL);

    return 0;
}
