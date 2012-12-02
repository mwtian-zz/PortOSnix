#ifndef __MINIFILE_DISKUTIL_H__
#define __MINIFILE_DISKUTIL_H__

#include "disk.h"
#include "minifile_fs.h"

/* Make a file system with the specified number of blocks */
extern int minifile_remkfs();

/* Check the consistency of the file system on disk */
extern int minifile_fsck(disk_t* disk);

#endif /* __MINIFILE_DISKUTIL_H__ */
