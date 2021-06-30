#include "util.h"
#include "common.h"
#include "block.h"
#include "fs.h"
#include <stdlib.h>

#ifdef FAKE
#include <stdio.h>
#define ERROR_MSG(m) printf m;
#else
#define ERROR_MSG(m)
#endif

#define FORMATTED_DISK 1
#define N_INODES 1032
#define N_DATA_BLOCKS 1914

#define INODE_EMPTY '0'
#define INODE_DIR '1'
#define INODE_FILE '2'

//TODO: inode_t: created correctly?
typedef struct inode_s
{
    char type; 
    int size; 
    int n_links;
    int hard_links[10];
    int secundary_link;
    int tertiary_link;
} inode_t;

//TODO: super_block_t: created correctly?
typedef struct super_block_s
{
    int blocks_size;
    int n_inodes;
    int n_data_blocks;
    int free_inodes;
    int free_data_blocks;
    int first_inode;
    int first_data_block;
    int magic_number;
} super_block_t;

typedef struct disk_s
{
    super_block_t *super_block;
    inode_t *inodes;
    char *bitmap; // [i] = 1 if block i is free, 0 otherwise
} disk_t;

typedef struct dir_entry_s
{
    int self_inode;
    int parent_inode;
    int size;
    int self_block;
    int parent_block;
    char *name;
} dir_entry_t;

typedef struct file_entry_s
{
    int size;
    int block_number;
    char name[MAX_FILE_NAME];
} file_entry_t;

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
    if (disk->super_block->magic_number == 0)
    {
        fs_mkfs();
    }
    else
    {
        for (int i = 0; i < 4; i ++)
            block_read(i+1, (char*)(disk->bitmap + i * BLOCK_SIZE));
        
        for (int i = 0; i < (BLOCK_SIZE/N_INODES); i++)
            block_read((i+5), (char*)disk->inodes+(8*i));
    }
}

//TODO: fs_mkfs: implementation
int fs_mkfs(void)
{
    disk->super_block->blocks_size = BLOCK_SIZE;
    disk->super_block->n_inodes = N_INODES;
    disk->super_block->free_inodes = N_INODES;
    disk->super_block->n_data_blocks = N_DATA_BLOCKS;
    disk->super_block->free_data_blocks = N_DATA_BLOCKS;
    disk->super_block->magic_number = FORMATTED_DISK;
    disk->super_block->first_inode = 5;
    disk->super_block->first_data_block = disk->super_block->first_inode + (N_INODES/8);
      
    disk->inodes[0].type = INODE_DIR;
    disk->inodes[0].size = 0;
    disk->inodes[0].n_links = 2;
    disk->inodes[0].hard_links[0] = disk->super_block->first_data_block;
    disk->bitmap[0] = '0';

    dir_entry_t* root = (dir_entry_t*)malloc(sizeof(dir_entry_t));

    root->name = "root";
    root->self_inode = 5;
    root->parent_inode = 5;
    root->self_block = disk->super_block->first_data_block;
    root->parent_block = disk->super_block->first_data_block;
    root->size = 0;

    
    for (int i = 1; i < N_DATA_BLOCKS; i++)
    {
        if (i < N_INODES)
        {
            disk->inodes[i].type = INODE_EMPTY;
            disk->inodes[i].size = 0;
            disk->inodes[i].n_links = 0;
        }
        disk->bitmap[i] = '1';
    }
    
    block_write(0, (char*)disk->super_block);

    for (int i = 0; i < 4; i++)
        block_write(i+1, (char*)(disk->bitmap + (i * BLOCK_SIZE)));

    for (int i = 0; i < (N_INODES/8); i++)
        block_write(i+5, (char*)(disk->inodes + i * 8));

    block_write(disk->super_block->first_data_block, (char*)root);
    
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

int fs_stat(char *fileName, fileStat *buf)
{
    return -1;
}
