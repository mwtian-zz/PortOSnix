#include <string.h>

#include "minithread.h"
#include "minifile_fs.h"
#include "minifile_diskutil.h"

int main(int argc, char** argv)
{
    use_existing_disk = 1;
    disk_name = "minidisk";
    disk_flags = DISK_READWRITE;

    minithread_system_initialize(minifile_fsck, NULL);

    return 0;
}

