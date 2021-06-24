#include "util.h"
#include "common.h"
#include "block.h"
#include "fs.h"

#ifdef FAKE
#include <stdio.h>
#define ERROR_MSG(m) printf m;
#else
#define ERROR_MSG(m)
#endif

#define FORMATTED_DISK "0xf0"

//TODO: disk_t: needed?
typedef struct
{

} disk_t;

//TODO: super_block_t: created correctly?
typedef struct
{
    int block_number;
    int blocks_size;
    int n_inodes;
    int n_data_blocks;
    char *first_inode;
    char *first_data_block;
    char *magic_number;
} super_block_t;

//TODO: inode_t: created correctly?
typedef struct
{
    char *type;
    char *n_links;
    char *size;
    char *links;
} inode_t;

//TODO: data_block_t: created correctly?
typedef struct
{

} data_block_t;

static super_block_t *super_block;

//TODO: fs_init: implementation
void fs_init(void)
{
    block_init();
    /* More code HERE */
    char *buffer;
    block_read(0, buffer);
    if (buffer != NULL)
    {
    }
}

//TODO: fs_mkfs: implementation
int fs_mkfs(void)
{
    return -1;
}

//TODO: fs_open: implementation
int fs_open(char *fileName, int flags)
{
    return -1;
}

//TODO: fs_close: implementation
int fs_close(int fd)
{
    return -1;
}

//TODO: fs_read: implementation
int fs_read(int fd, char *buf, int count)
{
    return -1;
}

//TODO: fs_write: implementation
int fs_write(int fd, char *buf, int count)
{
    return -1;
}

//TODO: fs_lseek: implementation
int fs_lseek(int fd, int offset)
{
    return -1;
}

//TODO: fs_mkdir: implementation
int fs_mkdir(char *fileName)
{
    return -1;
}

//TODO: fs_rmdir: implementation
int fs_rmdir(char *fileName)
{
    return -1;
}

//TODO: fs_cd: implementation
int fs_cd(char *dirName)
{
    return -1;
}

//TODO: fs_link: implementation
int fs_link(char *old_fileName, char *new_fileName)
{
    return -1;
}

//TODO: fs_unlink: implementation
int fs_unlink(char *fileName)
{
    return -1;
}

//TODO: fs_stat: implementation
int fs_stat(char *fileName, fileStat *buf)
{
    return -1;
}
