#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <assert.h>

#include "minifile.h"
#include "minithread.h"
#include "read.h"
#include "synch.h"

#define COPY_BUFFER_SIZE 1024

#define IDstring "PortOS filesystem v1.0"
#define BUFFER_SIZE 256

//move file from NT to our file system
int importfile(char *fname,char *ntfname)
{
    FILE *f;
    int len;
    char *buf;
    minifile_t file;
    if((f = fopen(ntfname,"rb")) == NULL) {
        printf("%s not found\n",ntfname);
        return -1;
    }
    fseek(f,0,SEEK_END);
    len = ftell(f);
    fseek(f,0,SEEK_SET);
    buf = malloc(len);
    assert(fread(buf,len,1,f) == 1);
    fclose(f);
    file = minifile_creat(fname);
    if(file == NULL) {
        printf("Couldn't create file %s in import!\n",fname);
        return -1;
    }
    if(minifile_write(file,buf,len) == -1) {
        printf("did not complete importing :(.\n");
        minifile_close(file);
        minifile_unlink(fname);
    } else minifile_close(file);
    free(buf);
    return 0;
}

//move file from our file system to NT
int exportfile(char *fname,char *ntfname)
{
    FILE *f;
    char *buf;
    minifile_t file;
    file = minifile_open(fname,"r");
    if(file == NULL) {
        printf("export: cannot open %s\n",fname);
        return -1;
    }
    buf = malloc(minifile_stat(fname));
    minifile_read(file,buf,minifile_stat(fname));
    minifile_close(file);
    if((f = fopen(ntfname,"wb")) == NULL) {
        printf("export: cannot open %s\n",ntfname);
        return -1;
    }
    fwrite(buf,minifile_stat(fname),1,f);
    fclose(f);
    free(buf);
    return 0;
}

//print a file on the screen
int typefile(char *fname)
{
    minifile_t file;
    char buf[COPY_BUFFER_SIZE+1];
    int i,len;
    buf[COPY_BUFFER_SIZE] = '\0';
    file = minifile_open(fname,"r");
    if(file == NULL) {
        printf("type: couldn't open %s!\n",fname);
        return -1;
    }
    len = minifile_stat(fname);
    for(i=COPY_BUFFER_SIZE; i<len; i+=COPY_BUFFER_SIZE) {
        minifile_read(file,buf,COPY_BUFFER_SIZE);
        printf("%s",buf);
    }
    if(i>len) {
        minifile_read(file,buf,COPY_BUFFER_SIZE-i+len);
        memset(buf+COPY_BUFFER_SIZE-i+len,'\0',i-len);
        printf("%s",buf);
    }

    printf("\nEOF\n");
    minifile_close(file);
    return 0;
}

//copy a file within our FS
int copy(char *fname,char *fname2)
{
    minifile_t src, dest;
    char buf[COPY_BUFFER_SIZE];
    int i,len;
    if(strcmp(fname,"")==0||strcmp(fname2,"")==0) {
        printf("Usage: cp [source_path] [dest_path]\n");
        return -1;
    }
    src = minifile_open(fname,"r");
    if(src == NULL) {
        printf("cp: couldn't open source %s!\n",fname);
        return -1;
    }
    dest = minifile_creat(fname2);
    if(dest == NULL) {
        printf("cp: couldn't create destination %s!\n",fname2);
        return -1;
    }
    len = minifile_stat(fname);
    for(i=COPY_BUFFER_SIZE; i<len; i+=COPY_BUFFER_SIZE) {
        minifile_read(src,buf,COPY_BUFFER_SIZE);
        minifile_write(dest,buf,COPY_BUFFER_SIZE);
    }
    if(i>len) {
        minifile_read(src,buf,COPY_BUFFER_SIZE-i+len);
        minifile_write(dest,buf,COPY_BUFFER_SIZE-i+len);
    }
    printf("\n");
    minifile_close(src);
    minifile_close(dest);
    return 0;
}

//this is terribly, extremely inefficient, please don't write real code that
//does this!
int move(char *fname,char *fname2)
{
    if(copy(fname,fname2) != -1)
        minifile_unlink(fname);
    return 0;
}

//input a file from stdin, input terminated with a "^D" input on a new blank line
//(not ctrl-D, but just two characters ^D.
int inputfile(char *fname)
{
    char str[BUFFER_SIZE];
    minifile_t f = minifile_creat(fname);
    if(f == NULL) {
        printf("input: Error creating file %s\n",fname);
        return -1;
    }
    printf("---inputting %s: input \"ctrl-D,ENTER\" on a new line to exit---\n",fname);
    do {
        memset(str,'\0',BUFFER_SIZE);
        gets(str);
        if(str[0] == 4) //that's what ctrl-D puts in ((char)4)
            break;
        str[strlen(str)] = '\n';
        minifile_write(f,str,strlen(str));
    } while(1);
    minifile_close(f);
    return 0;
}


void help_screen()
{
    printf("Supported commands:\n");
    printf(" cd path  - switch to a new path\n");
    printf(" ls [path] - list contents of current directory, or path if given\n");
    printf(" pwd - tell current directory\n");
    printf(" mkdir path - create a new directory\n");
    printf(" rmdir path - remove a directory\n");
    printf(" rm (del) path - remove a file\n");
    printf(" import localpath NTpath - import a file from Windows file system\n");
    printf(" export localpath NTpath - export a local file to Windows\n");
    printf(" type path - print given file on the screen\n");
    printf(" input path - input a text file from standard input\n");
    printf(" cp (copy) src dest - copy src file to dest file\n");
    printf(" mv (move) src dest - move src file to dest file\n");
    printf(" whoami - print your identity\n");
    printf(" help - show this screen\n");
    printf(" exit - exit shell\n");
    printf("\n");
}

void put_prompt()
{
    printf("thread%d@localhost", minithread_id());
    printf(": %s %% ", minifile_pwd());
    fflush(stdout);
}

int shell(int *g)
{
    char command[BUFFER_SIZE];
    char func[BUFFER_SIZE],arg1[BUFFER_SIZE],arg2[BUFFER_SIZE];
    int i;

    minifile_cd("/"); //cd to root (will also initialize the system if necessary)
    printf("%s\n", IDstring);

    while(1) {
        memset(command,'\0',BUFFER_SIZE);
        memset(func,'\0',BUFFER_SIZE);
        memset(arg1,'\0',BUFFER_SIZE);
        memset(arg2,'\0',BUFFER_SIZE);
        put_prompt();
        //gets(command);
        miniterm_read(command, BUFFER_SIZE);
        //extract first three strings in command (delimited by spaces)
        sscanf(command,"%s %s %s",func,arg1,arg2);
        if(strcmp(func,"help") == 0)
            help_screen();
        else if(strcmp(func,"cd") == 0)
            minifile_cd(arg1);
        else if(strcmp(func,"ls") == 0 || strcmp(func,"dir") == 0) {
            char **files = minifile_ls(arg1);
            printf("File listing for %s\n", arg1);
            for(i = 0; files != NULL && files[i] != NULL; ++i) {
                printf("\t%s\n",files[i]);
                free(files[i]);
            }
            free(files);
        } else if(strcmp(func,"pwd") == 0)
            printf("%s\n", minifile_pwd());
        else if(strcmp(func,"mkdir") == 0)
            minifile_mkdir(arg1);
        else if(strcmp(func,"rmdir") == 0)
            minifile_rmdir(arg1);
        else if(strcmp(func,"rm") == 0 || strcmp(func,"del") == 0)
            minifile_unlink(arg1);
        else if(strcmp(func,"import") == 0)
            importfile(arg1,arg2);
        else if(strcmp(func,"export") == 0)
            exportfile(arg1,arg2);
        else if(strcmp(func,"type") == 0)
            typefile(arg1);
        else if(strcmp(func,"input") == 0)
            inputfile(arg1);
        else if(strcmp(func,"cp") == 0 || strcmp(func,"copy") == 0)
            copy(arg1,arg2);
        else if(strcmp(func,"mv") == 0 || strcmp(func,"move") == 0)
            move(arg1,arg2);
        else if(strcmp(func,"whoami") == 0)
            printf("You are minithread %d, running our shell\n",minithread_id());
        else if(strcmp(func,"exit") == 0)
            break;
        else if(strcmp(func,"doscmd") == 0)
            system(command+7);
        else if(strcmp(func,"exec") == 0) { //this is not efficient -- just for fun!!!
            char cmdline[BUFFER_SIZE];
            memset(cmdline,'\0',BUFFER_SIZE);
            strcpy(cmdline,"tmp0000~.exe ");
            strcpy(cmdline+13,command+5+strlen(arg1));
            exportfile(arg1,"tmp0000~.exe");
            system(cmdline);
            system("rm tmp0000~.exe");
        } else printf("%s: Command not found\n",func);
    }
    printf("Good-bye :-)\n");
    return 0;
}

int
main(int argc, char** argv)
{
    minithread_system_initialize(shell, NULL);
    return 0;
}
