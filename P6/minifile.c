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
    return 0;
}
