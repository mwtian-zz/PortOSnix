#include <string.h>

#include "minifile_private.h"
#include "minifile_diskutil.h"

int main(int argc, char** argv)
{
    disk_t disk;
    blocknum_t fs_size;

    if (argc != 2) {
        return -1;
    }
    fs_size = atoi(argv[1]);

    return minifile_mkfs(&disk, "minidisk", fs_size);
}
