#include <string.h>

#include "minithread.h"
#include "minifile_fs.h"
#include "minifile_diskutil.h"

static blocknum_t total_blocks;

int mkfs(int *arg)
{
    disk_t disk;
    return minifile_remkfs();
}

int main(int argc, char** argv)
{
    if (argc != 2) {
        return -1;
    }
    total_blocks = atoi(argv[1]);

    use_existing_disk = 0;
    disk_name = "minidisk";
    disk_flags = DISK_READWRITE;
    disk_size = total_blocks;

    minithread_system_initialize(mkfs, NULL);

    return 0;
}
