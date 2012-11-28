#include "minifile.h"

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

}

minifile_t minifile_open(char *filename, char *mode)
{

}

int minifile_read(minifile_t file, char *data, int maxlen)
{

}

int minifile_write(minifile_t file, char *data, int len)
{

}

int minifile_close(minifile_t file)
{

}

int minifile_unlink(char *filename)
{

}

int minifile_mkdir(char *dirname)
{

}

int minifile_rmdir(char *dirname)
{

}

int minifile_stat(char *path)
{

}

int minifile_cd(char *path)
{

}

char **minifile_ls(char *path)
{

}

char* minifile_pwd(void)
{

}
