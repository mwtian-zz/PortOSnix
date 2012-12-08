#ifndef __MINIFILE_DISKUTIL_H__
#define __MINIFILE_DISKUTIL_H__

#include "disk.h"
#include "minifile_fs.h"

/* Make a file system with the specified number of blocks */
extern int minifile_remkfs(int *arg);

/* Check the consistency of the file system on disk */
extern int minifile_fsck(int *arg);

#endif /* __MINIFILE_DISKUTIL_H__ */
