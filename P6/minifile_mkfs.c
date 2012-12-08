#include <string.h>

#include "minithread.h"
#include "minifile_fs.h"
#include "minifile_diskutil.h"

int main(int argc, char** argv)
{
    if (argc != 2) {
        return -1;
    }

    use_existing_disk = 0;
    disk_name = "minidisk";
    disk_flags = DISK_READWRITE;
    disk_size = atoi(argv[1]);

    minithread_system_initialize(minifile_remkfs, NULL);

    return 0;
}
