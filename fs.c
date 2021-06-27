#include "util.h"
// #include "common.h"
#include "block.h"
#include "fs.h"
#include <stdlib.h>

#ifdef FAKE
#include <stdio.h>
#define ERROR_MSG(m) printf m;
#else
#define ERROR_MSG(m)
#endif

#define FORMATTED_DISK "0xf"
#define N_INODES 510
#define N_DATA_BLOCKS 1536

//TODO: inode_t: created correctly?
typedef struct inode_s
{
    char status;
    char *type;
    int size;
    int n_links;
    char *links;
} inode_t;

//TODO: super_block_t: created correctly?
typedef struct super_block_s
{
    int blocks_size;
    int n_inodes;
    int n_data_blocks;
    int first_inode;
    int first_data_block;
    char *magic_number;
} super_block_t;

//TODO: data_block_t: created correctly?
typedef struct data_block_s
{
    char *content;

} data_block_t;

typedef struct disk_s
{
    super_block_t *super_block;
    inode_t *inodes;
    char *bitmap; // [i] = 1 if block i is free, 0 otherwise
} disk_t;

static disk_t *disk;

//TODO: fs_init: implementation
void fs_init(void)
{
    block_init();

    disk = (disk_t *)malloc(sizeof(disk_t));
    disk->super_block = (super_block_t *)malloc(sizeof(super_block_t));
    disk->inodes = (inode_t*)malloc(N_INODES * sizeof(inode_t));
    disk->bitmap = (char*)malloc(N_DATA_BLOCKS * sizeof(char));

    block_read(0, (char*)disk->super_block);
    if (disk->super_block->magic_number == NULL)
    {
        fs_mkfs();
    }
    else
    {

    }
}

//TODO: fs_mkfs: implementation
int fs_mkfs(void)
{
    disk->super_block->blocks_size = BLOCK_SIZE;
    disk->super_block->n_inodes = N_INODES;
    disk->super_block->n_data_blocks = N_DATA_BLOCKS;
    disk->super_block->magic_number = FORMATTED_DISK;
    disk->super_block->first_inode = (N_DATA_BLOCKS/BLOCK_SIZE) + 2;
    disk->super_block->first_data_block = disk->super_block->first_inode + N_INODES;
    
    for (int i = 0; i < N_DATA_BLOCKS; i++)
    {
        if (i < N_INODES)
        {
            disk->inodes[i].status = '1';
            disk->inodes[i].type = NULL;
            disk->inodes[i].size = 0;
            disk->inodes[i].n_links = 0;
            disk->inodes[i].links = NULL;
        }
        disk->bitmap[i] = '1';
    }
    
    char *buffer;
    block_read(0, buffer);
    if (buffer == NULL)
        return -1;

    block_write(0, (char*)disk->super_block);
    return 0;
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

// int fs_stat(char *fileName, fileStat *buf)
// {
//     return -1;
// }
