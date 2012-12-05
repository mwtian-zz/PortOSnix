#include "minifile.h"
#include "minifile_path.h"
#include "minifile_inode.h"
#include "minithread.h"

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
    return 0;
}

char **minifile_ls(char *path)
{
    return 0;
}

char* minifile_pwd(void)
{
	inodenum_t cur_inodenum, parent_inodenum;
	mem_inode_t cur_directory;
	char* pwd;
	char** path;
	int path_len = 0, pwd_len = 2, i, entry_size;
	dir_entry_t* entries;
	
	cur_inodenum = minithread_wd();
	cur_directory = minithread_wd_inode();
	/* Current directory is root directory */
	if (cur_inodenum == sb->root) {
		pwd = malloc(2);
		strcpy(pwd, "/");
		return pwd;
	}
	
	parent_inodenum = nameinode("..", cur_directory);
	while (cur_inodenum != sb->root) {
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
		iput(maindisk, cur_directory);
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
