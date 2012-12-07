#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "defs.h"

#include "minifile.h"
#include "minifile_private.h"
#include "minifile_path.h"
#include "minifile_inode.h"
#include "minithread.h"

minifile_t minifile_creat(char *filename)
{
    return NULL;
}

minifile_t minifile_open(char *filename, char *mode)
{

    return NULL;
}

int minifile_read(minifile_t file, char *data, int maxlen)
{
    return 0;
}

int minifile_write(minifile_t file, char *data, int len)
{
    return 0;
}

int minifile_close(minifile_t file)
{
    return 0;
}

int minifile_unlink(char *filename)
{
    return 0;
}

int minifile_mkdir(char *dirname)
{
	return 0;
}

int minifile_rmdir(char *dirname)
{
    inodenum_t inodenum, parent_inodenum;
	mem_inode_t ino, parent_ino;

	if (strcmp(dirname, "/") == 0) {
		printf("Forbide you remove root!!!\n");
		return -1;
	}

	inodenum = namei(dirname);
	if (inodenum == 0) {
		return -1;
	}
	if (iget(maindisk, inodenum, &ino) != 0) {
		return -1;
	}

	parent_inodenum = nameinode("..", ino);
	if (iget(maindisk, parent_inodenum, &parent_ino) != 0) {
		iput(ino);
		return -1;
	}

	/* When want to create file in this dirctory by other process
	 * they first grab lock and check if this directory is deleted
	 * If delete, should return path not found to user
	 * If this functions grabs lock later, the directory is not empty then
	 */
	semaphore_P(ino->inode_lock);
	if (ino->size > 0) {
		semaphore_V(ino->inode_lock);
		iput(parent_ino);
		iput(ino);
		return -1;
	}
	ino->status = TO_DELETE;
	semaphore_V(ino->inode_lock);
	iput(ino);

	/* Remove this inodenum from parent inode */
	semaphore_P(parent_ino->inode_lock);
	if (idelete_from_dir(parent_ino, inodenum) != 0) {
		semaphore_V(parent_ino->inode_lock);
		iput(parent_ino);
		return -1;
	}
	parent_ino->size--; /* Should update inode to disk after this */
	semaphore_V(parent_ino->inode_lock);
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
	semaphore_P(new_dir->inode_lock);
	if (new_dir->type != MINIDIRECTORY || new_dir->status == TO_DELETE) {
		semaphore_V(new_dir->inode_lock);
		iput(new_dir);
		return -1;
	}
	semaphore_V(new_dir->inode_lock);

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
		filename = pathtofile(path);
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
	entries = malloc(entries_size * sizeof(char*));
	for (i = 0; i < entries_size; i++) {
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
