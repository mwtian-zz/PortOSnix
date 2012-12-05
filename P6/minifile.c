#include "minifile.h"
#include "minifile_path.h"
#include "minifile_inode.h"
#include "minithread.h"
#include "minifile_util.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/*
 * struct minifile:
 *     This is the structure that keeps the information about
 *     the opened file like the position of the cursor, etc.
 */

struct minifile {
    /* add members here */
    int dummy;
};

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
    return 0;
}

int minifile_stat(char *path)
{
    return 0;
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
	/* Not a directory */
	if (new_dir->type != MINIDIRECTORY) {
		iput(new_dir);
		return -1;
	}
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
