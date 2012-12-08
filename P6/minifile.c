#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "defs.h"

#include "minifile.h"
#include "minifile_private.h"
#include "minifile_path.h"
#include "minifile_inode.h"
#include "minithread.h"

static void minifile_cursor_shift(minifile_t file, int shift);

minifile_t minifile_creat(char *filename)
{
    minifile_t file = NULL;
    inodenum_t inum, parent_inum;
    mem_inode_t inode, parent_inode;
	char* name, *parent;

	if (filename == NULL || strlen(filename) == 0 || filename[strlen(filename) - 1] == '/') {
		return NULL;
	}

    file = malloc(sizeof(struct minifile));
    if (NULL == file) {
        return NULL;
    }

    inum = namei(filename);
    if (0 == inum) {
		parent = get_path(filename);
		name = get_filename(filename);
		if (parent == NULL) {
			parent_inum = minithread_wd();
		} else {
			parent_inum = namei(parent);
		}
		if (parent_inum == 0) {
			free(name);
			free(parent);
			free(file);
			return NULL;
		}
		iget(maindisk, parent_inum, &parent_inode);
		if (parent_inode->type != MINIDIRECTORY) {
			iput(parent_inode);
			free(name);
			free(parent);
			free(file);
			return NULL;
		}
        inum = ialloc(maindisk);
		ilock(parent_inode);
		iadd_to_dir(parent_inode, name, inum);
		parent_inode->size++;
		iupdate(parent_inode);
		iunlock(parent_inode);
		iput(parent_inode);
        iget(maindisk, inum, &inode);
        ilock(inode);
        izero(inode);
        inode->type = MINIFILE;
        inode->size = 0;
        iunlock(inode);
        iupdate(inode);
    } else {
        iget(maindisk, inum, &inode);
		ilock(inode);
		iclear(inode);
		iunlock(inode);
    }

    file->inode = inode;
    file->inode_num = inum;
    file->block_cursor = 0;
    file->byte_cursor = 0;
    file->byte_in_block = 0;
    file->mode[0] = 'w';
    file->mode[1] = '\0';
    return file;
}

minifile_t minifile_open(char *filename, char *mode)
{
    minifile_t file = NULL;
    inodenum_t inum;
    mem_inode_t inode;

    inum = namei(filename);
    if (0 == inum) {
        /* When file is not found, return NULL if 'r' or 'r+' */
        if ('r' == mode[0]) {
            return NULL;
        } else {
            if ((file = minifile_creat(filename)) != 0) {
                inode = file->inode;
            }
        }
    } else {
        /* When file is found */
        file = malloc(sizeof(struct minifile));
        iget(maindisk, inum, &inode);
        if (NULL == file || NULL == inode) {
            free(file);
            iput(inode);
            return NULL;
        }
        ilock(inode);
        file->inode = inode;
        file->inode_num = inum;
        /* Empty the content if 'w' or 'w+' */
        if ('w' == mode[0]) {
            iclear(inode);
        }
        iunlock(inode);
    }

    /* Set modes */
    file->mode[0] = mode[0];
    file->mode[1] = '\0';
    file->mode[2] = '\0';
    file->block_cursor = 0;
    file->byte_cursor = 0;
    file->byte_in_block = 0;
    if ('a' == mode[0]) {
        /* Position cursor to the end */
        minifile_cursor_shift(file, inode->size);
    }
    if ('+' == mode[1]) {
        file->mode[1] = '+';
    }

    return file;
}

int minifile_read(minifile_t file, char *data, int maxlen)
{
    int count = 0;
    int left_byte = 0;
    int disk_block = 0;
    buf_block_t buf;
    int step = 0;
    if (('w' == file->mode[0] || 'a' == file->mode[0])
            && ('\0' == file->mode[1])) {
        return -1;
    }

    left_byte = file->inode->size - file->byte_cursor;
    if (left_byte < maxlen) {
        maxlen = left_byte;
    }

    while (maxlen > 0 && left_byte > 0) {
        /* Get step size */
        if (file->byte_in_block + maxlen - 1 > DISK_BLOCK_SIZE) {
            step = DISK_BLOCK_SIZE - file->byte_in_block;
        } else {
            step = maxlen;
        }
        /* Get disk block number from block cursor */
        ilock(file->inode);
        disk_block = blockmap(maindisk, file->inode, file->block_cursor);
        iunlock(file->inode);
        /* Copy disk block */
        if (bread(maindisk, disk_block, &buf) != 0)
            return count;
        memcpy(data, buf->data + file->byte_in_block, step);
        brelse(buf);
        /* Update upon success */
        minifile_cursor_shift(file, step);
        data += step;
        maxlen -= step;
        count += step;
    }

    return count;
}

int minifile_write(minifile_t file, char *data, int len)
{
    blocknum_t blocknum = 0;
    buf_block_t buf;
    int disk_block = 0;
    int step = 0;
    if ('r' == file->mode[0] && '\0' == file->mode[1]) {
        return -1;
    }

    while (len > 0) {
        blocknum = 0;
        /* Allocate block if needed */
        if (file->block_cursor * DISK_BLOCK_SIZE < file->byte_cursor + 1) {
            blocknum = balloc(maindisk);
            /* Update disk inode block list */
            ilock(file->inode);
            if (iadd_block(file->inode, blocknum) == 0)
                file->inode->size_blocks++;
            iupdate(file->inode);
            iunlock(file->inode);
        }

        /* Get step size */
        if (file->byte_in_block + len - 1 > DISK_BLOCK_SIZE) {
            step = DISK_BLOCK_SIZE - file->byte_in_block;
        } else {
            step = len;
        }
        /* Get disk block number from block cursor */
        ilock(file->inode);
        disk_block = blockmap(maindisk, file->inode, file->block_cursor);
        iunlock(file->inode);
        /* Copy disk block */
        if (bread(maindisk, disk_block, &buf) != 0)
            return -1;
        memcpy(buf->data + file->byte_in_block, data, step);
        bwrite(buf);
        /* Update upon success */
        minifile_cursor_shift(file, step);
        len -= step;
        /* Update disk inode size */
        ilock(file->inode);
        file->inode->size += step;
        iupdate(file->inode);
        iunlock(file->inode);
    }

    return 0;
}

int minifile_close(minifile_t file)
{
	if (file == NULL) {
		return -1;
	}
    iput(file->inode);
	free(file);
	return 0;
}

int minifile_unlink(char *filename)
{
    char* parent;
	mem_inode_t parent_inode, inode;
	inodenum_t parent_inum, inum;

	if (filename == NULL) {
		return -1;
	}
	inum = namei(filename);
	if (inum == 0) {
		return -1;
	}
	parent = get_path(filename);
	parent_inum = namei(parent);
	if (parent_inum == 0) {
		free(parent);
		return -1;
	}
	iget(maindisk, inum, &inode);
	if (inode->type != MINIFILE) {
		iput(inode);
		free(parent);
		return -1;
	}
	iget(maindisk, parent_inum, &parent_inode);
	if (parent_inode->type != MINIDIRECTORY) {
		iput(inode);
		iput(parent_inode);
		free(parent);
		return -1;
	}
	ilock(inode);
	inode->status = TO_DELETE;
	iunlock(inode);
	iput(inode);
	ilock(parent_inode);
	idelete_from_dir(parent_inode, inum);
	parent_inode->size--;
	iupdate(parent_inode);
	iunlock(parent_inode);
	iput(parent_inode);
	return 0;
}

int minifile_mkdir(char *dirname)
{
    buf_block_t buf;
    blocknum_t blocknum;
    inodenum_t inum, parent_inum;
    mem_inode_t inode, parent_inode;
    dir_entry_t dir;
	char* parent, *name;

	if (dirname == NULL || strlen(dirname) == 0) {
		return -1;
	}

	parent = get_path(dirname);
	name = get_filename(dirname);

	if (parent == NULL) {
		parent_inum = minithread_wd();
	} else {
		parent_inum = namei(parent);
	}

	if (parent_inum == 0) {
		free(parent);
		free(name);
		return -1;
	}
	iget(maindisk, parent_inum, &parent_inode);

    /* Do not create dir if duplicate path and name exists */
    inum = namei(dirname);
    if (0 != inum) {
        return -1;
    } else {
        inum = ialloc(maindisk);
        if (0 == inum || -1 == inum) {
            return -1;
        }
    }

    iget(maindisk, inum, &inode);
    ilock(inode);
    inode->type = MINIDIRECTORY;
    inode->size = 2;
    blocknum = balloc(maindisk);
    iadd_block(inode, blocknum);
    iupdate(inode);
    iunlock(inode);
    if (bread(maindisk, blocknum, &buf) != 0)
        return -1;
    dir = (dir_entry_t) buf->data;
    strcpy(dir[0].name, ".");
    dir[0].inode_num = inum;
    strcpy(dir[1].name, "..");
	dir[1].inode_num = parent_inum;
	bwrite(buf);
	iadd_block(inode, blocknum);
	iupdate(inode);
	iunlock(inode);
	iput(inode);
	ilock(parent_inode);
	iadd_to_dir(parent_inode, name, inum);
	parent_inode->size++;
	iupdate(parent_inode);
	iput(parent_inode);

	free(name);
	free(parent);
	return 0;
}

int minifile_rmdir(char *dirname)
{
    inodenum_t inodenum, parent_inodenum;
	mem_inode_t ino, parent_ino;
    char* parent;

	/* Need further changes to handle parent and name */
	if (strcmp(dirname, "/") == 0) {
		printf("Remove root: permission denied. \n");
		return -1;
	}

    parent = get_path(dirname);

    if (parent == NULL) {
		parent_inodenum = minithread_wd();
	} else {
		parent_inodenum = namei(parent);
	}
	free(parent);
	
	inodenum = namei(dirname);
	if (parent_inodenum == 0 || inodenum == 0) {
		return -1;
	}
	iget(maindisk, parent_inodenum, &parent_ino);
	
	printf("Inode to delete is %ld\n", inodenum);
    iget(maindisk, inodenum, &ino);

	/* When want to create file in this dirctory by other process
	 * they first grab lock and check if this directory is deleted
	 * If delete, should return path not found to user
	 * If this functions grabs lock later, the directory is not empty then
	 */
	ilock(ino);
	if (ino->type != MINIDIRECTORY || ino->size > 2) {
		iunlock(ino);
		iput(parent_ino);
		iput(ino);
		return -1;
	}
	ino->status = TO_DELETE;
	iunlock(ino);
	iput(ino);

	/* Remove this inodenum from parent inode */
	ilock(parent_ino);
	if (idelete_from_dir(parent_ino, inodenum) != 0) {
		iunlock(parent_ino);
		iput(parent_ino);
		return -1;
	}
	parent_ino->size--; /* Should update inode to disk after this */
	iupdate(parent_ino);
	iunlock(parent_ino);
	iput(ino);

	return 0;
}

int minifile_stat(char *path)
{
	inodenum_t inodenum;
    mem_inode_t ino;
	int retval;

	inodenum = namei(path);
	if (inodenum == 0) {
		return -1;
	}
	if (iget(maindisk, inodenum, &ino) != 0) {
		return -1;
	}
	if (ino->type == MINIFILE) {
		retval = ino->size;
	} else if (ino->type == MINIDIRECTORY) {
		retval = -2;
	} else {
		retval = -1;
	}
	iput(ino);
	return retval;
}

int minifile_cd(char *path)
{
	int cur_inodenum, new_inodenum;
	mem_inode_t cur_dir, new_dir;

	cur_inodenum = minithread_wd();
	cur_dir = minithread_wd_inode();
	new_inodenum = namei(path);
	/* Invalid path */
	if (new_inodenum == 0) {
		return -1;
	}
	if (iget(maindisk, new_inodenum, &new_dir) != 0) {
		return -1;
	}
	/* Not a directory or the dirctory is mark deleted. Can't create file there since can't delete non-emptry dir */
	ilock(new_dir);
	if (new_dir->type != MINIDIRECTORY || new_dir->status == TO_DELETE) {
		iunlock(new_dir);
		iput(new_dir);
		return -1;
	}
	iunlock(new_dir);

	/* Release previous directory inode if not root */
	if (cur_inodenum != mainsb->root_inum) {
		iput(cur_dir);
	}
	minithread_set_wd(new_inodenum);
	minithread_set_wd_inode(new_dir);
	return 0;
}

char **minifile_ls(char *path)
{
    char** entries;
	mem_inode_t dir;
	inodenum_t inodenum;
	char* filename;
	dir_entry_t* dir_entries;
	int entries_size, i, count = 0;

	/* If path is NULL or len is 0, ls current working directory */
	if (path == NULL || strlen(path) == 0) {
		inodenum = minithread_wd();
	} else {
		inodenum = namei(path);
	}

	/* Not valid path */
	if (inodenum == 0) {
		return NULL;
	}
	if (iget(maindisk, inodenum, &dir) != 0) {
		return NULL;
	}
	if (dir->type == MINIFILE) {
		entries = malloc(2 * sizeof(char*));
		filename = get_filename(path);
		if (filename == NULL) {
			return NULL;
		}
		entries[0] = malloc(strlen(filename) + 1);
		strcpy(entries[0], filename);
		free(filename);
		entries[1] = NULL;
		return entries;
	}

	semaphore_P(dir->inode_lock);
	dir_entries = get_directory_entry(maindisk, dir, &entries_size);
	semaphore_V(dir->inode_lock);

	iput(dir);
	entries = malloc((entries_size + 1) * sizeof(char*));

	for (i = 0; i < entries_size; i++) {
	    printf("dir name: %s\n", dir_entries[i]->name);
	    printf("dir inum: %ld\n", dir_entries[i]->inode_num);

		if (dir_entries[i] != NULL) {
			entries[count] = malloc(strlen(dir_entries[i]->name) + 1);
			strcpy(entries[count], dir_entries[i]->name);
			count++;
			free(dir_entries[i]);
		}
	}
	entries[count] = NULL;
	free(dir_entries);

	return entries;
}

char* minifile_pwd(void)
{
	inodenum_t cur_inodenum, parent_inodenum;
	mem_inode_t cur_directory;
	char* pwd = NULL;
	char** path = NULL;
	int path_len = 0, pwd_len = 2, i, entry_size;
	dir_entry_t* entries;

	cur_inodenum = minithread_wd();
	cur_directory = minithread_wd_inode();
	/* Current directory is root directory */
	if (cur_inodenum == mainsb->root_inum) {
		pwd = malloc(2);
		strcpy(pwd, "/");
		return pwd;
	}

	parent_inodenum = nameinode("..", cur_directory);
	while (cur_inodenum != mainsb->root_inum) {
		if (iget(maindisk, parent_inodenum, &cur_directory) != 0) {
			printf("Get inode error!\n");
		}
		semaphore_P(cur_directory->inode_lock);
		entries = get_directory_entry(maindisk, cur_directory, &entry_size);
		semaphore_V(cur_directory->inode_lock);
		for (i = 0; i < entry_size; i++) {
			if (entries[i]->inode_num == cur_inodenum) {
				break;
			}
		}
		if (i == entry_size) {
			printf("Can't happen!\n");
			return "Can't find path!!!";
		}
		path_len++;
		path = realloc(path, path_len * sizeof(char*));
		path[path_len - 1] = malloc(strlen(entries[i]->name) + 1);
		strcpy(path[path_len - 1], entries[i]->name);
		pwd_len += (strlen(entries[i]->name) + 1);
		cur_inodenum = parent_inodenum;

		parent_inodenum = nameinode("..", cur_directory);
		iput(cur_directory);
	}
	pwd = malloc(pwd_len);
	strcpy(pwd, "/");
	for (i = path_len - 1; i >= 0; i--) {
		strcat(pwd, path[i]);
		strcat(pwd, "/");
		free(path[i]);
	}
	free(path);

	return pwd;
}

void
minifile_cursor_shift(minifile_t file, int shift)
{
    file->byte_cursor += shift;
    file->block_cursor = file->byte_cursor / DISK_BLOCK_SIZE;
    file->byte_in_block = file->byte_cursor % DISK_BLOCK_SIZE;
}
