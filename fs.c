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

#define TYPE_EMPTY '0'
#define TYPE_DIR '1'
#define TYPE_FILE '2'

typedef struct inode_s
{
    char type;
    int size;
    int n_links;
    int hard_links[10];
    int secundary_link;
    int tertiary_link;
} inode_t;

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

typedef struct block_entry_s
{
    int self_inode;
    int parent_inode;
    int size;
    int self_block;
    int parent_block;
    char *name;
    char type;
} block_entry_t;

typedef struct opened_file_s
{
    block_entry_t file;
    int cursor;
    int flag;
} opened_file_t;

static disk_t *disk;
static block_entry_t *current_dir;
static opened_file_t open_files[20];
static int n_open_files;

//TODO: fs_init: testing
void fs_init(void)
{
    block_init();

    disk = (disk_t *)malloc(sizeof(disk_t));
    disk->super_block = (super_block_t *)malloc(sizeof(super_block_t));
    disk->inodes = (inode_t *)malloc(N_INODES * sizeof(inode_t));
    disk->bitmap = (char *)malloc(N_DATA_BLOCKS * sizeof(char));

    block_read(0, (char *)disk->super_block);
    if (disk->super_block->magic_number == 0)
    {
        fs_mkfs();
    }
    else
    {
        for (int i = 0; i < 4; i++)
            block_read(i + 1, (char *)(disk->bitmap + i * BLOCK_SIZE));

        for (int i = 0; i < (BLOCK_SIZE / N_INODES); i++)
            block_read((i + 5), (char *)disk->inodes + (8 * i));
    }

    block_read(disk->super_block->first_data_block, (char *)current_dir);

    for (int i = 0; i < 20; i++)
        open_files->flag = -1;
}

//TODO: fs_mkfs: testing
int fs_mkfs(void)
{
    disk->super_block->blocks_size = BLOCK_SIZE;
    disk->super_block->n_inodes = N_INODES;
    disk->super_block->free_inodes = N_INODES;
    disk->super_block->n_data_blocks = N_DATA_BLOCKS;
    disk->super_block->free_data_blocks = N_DATA_BLOCKS;
    disk->super_block->magic_number = FORMATTED_DISK;
    disk->super_block->first_inode = 5;
    disk->super_block->first_data_block = disk->super_block->first_inode + (N_INODES / 8);

    disk->inodes[0].type = TYPE_DIR;
    disk->inodes[0].size = 0;
    disk->inodes[0].n_links = 2;
    disk->inodes[0].hard_links[0] = disk->super_block->first_data_block;
    disk->bitmap[0] = '0';

    block_entry_t *root = (block_entry_t *)malloc(sizeof(block_entry_t));

    root->name = "root";
    root->self_inode = 0;
    root->parent_inode = 0;
    root->self_block = disk->super_block->first_data_block;
    root->parent_block = disk->super_block->first_data_block;
    root->size = 0;
    root->type = TYPE_DIR;

    for (int i = 1; i < N_DATA_BLOCKS; i++)
    {
        if (i < N_INODES)
        {
            disk->inodes[i].type = TYPE_EMPTY;
            disk->inodes[i].size = 0;
            disk->inodes[i].n_links = 0;
        }
        disk->bitmap[i] = '1';
    }

    fs_flush();

    block_write(disk->super_block->first_data_block, (char *)root);

    return 0;
}

//TODO: fs_open: testing
int fs_open(char *fileName, int flags)
{
    inode_t current_dir_inode = disk->inodes[current_dir->self_inode];
    block_entry_t *aux;
    int found = FALSE;

    for (int i = 0; i < current_dir_inode.n_links; i++)
    {
        block_read(current_dir_inode.hard_links[i], (char *)aux);

        if (same_string(aux->name, fileName))
        {
            found = TRUE;
            break;
        }
    }

    if (found)
    {
        if (aux->type == TYPE_DIR && (flags == FS_O_WRONLY || flags == FS_O_RDWR))
            return -1;

        for (int i = 0; i < 20; i++)
        {
            if (open_files[i].flag == -1)
            {
                open_files[i].file = *aux;
                open_files[i].flag = flags;
                open_files[i].cursor = 0;
                return i;
            }
        }
    }
    else
    {
        if (flags == FS_O_RDONLY)
            return -1;

        int i = 0;
        while (i < N_DATA_BLOCKS && disk->bitmap[i] == '0')
        {
            i++;
        }
        if (current_dir_inode.n_links < 10)
        {
            current_dir_inode.hard_links[current_dir_inode.n_links - 1] = i;
            block_entry_t new_file;
            new_file.name = fileName;
            new_file.parent_inode = current_dir->self_inode;
            new_file.size = 0;
            new_file.type = TYPE_FILE;
            new_file.parent_block = current_dir->self_block;

            int j;
            for (j = 0; j < N_INODES; j++)
            {
                if (disk->inodes[j].type == TYPE_EMPTY)
                    break;
            }

            if (j == N_INODES)
                return -1;

            disk->inodes[j].type = FILE_TYPE;
            disk->inodes[j].n_links++;
            disk->inodes[j].hard_links[0] = i;
            new_file.self_inode = j;
            new_file.self_block = i;
            disk->bitmap[i] = '0';

            block_write(i, (char*)&new_file);
            disk->inodes[current_dir->self_inode] = current_dir_inode;
            fs_flush();

            for (int i = 0; i < 20; i++)
            {
                if (open_files[i].flag == -1)
                {
                    open_files[i].file = new_file;
                    open_files[i].flag = flags;
                    open_files[i].cursor = 0;
                    return i;
                }
            }
        }
    }
    return -1;
}

//TODO: fs_close: testing
int fs_close(int fd)
{
    if (fd < 20)
    {
        open_files[fd].flag = -1;
        return 0;
    }
    return -1;
}

//TODO: fs_read: testing
int fs_read(int fd, char *buf, int count)
{
    if (fd > 20 || open_files[fd].flag < 0 || open_files[fd].flag == FS_O_WRONLY)
        return -1;

    if (count == 0)
        return 0;

    int cursor = open_files[fd].cursor;
    if (open_files[fd].file.size - cursor < count)
        count = open_files[fd].file.size - cursor;

    char *block;
    block_read(open_files[fd].file.self_block, block);
    buf = block + cursor;
    open_files[fd].cursor += count;
    return count;
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

//TODO: fs_mkdir: testing
int fs_mkdir(char *fileName)
{
    inode_t current_dir_inode = disk->inodes[current_dir->self_inode];

    block_entry_t *aux;
    for (int i = 0; i < current_dir_inode.n_links; i++)
    {
        block_read(current_dir_inode.hard_links[i], (char *)aux);
        if (same_string(aux->name,fileName))
            return -1;
    }

    if (current_dir_inode.n_links == 10)
        return -1;

    block_entry_t new_dir;

    new_dir.name = fileName;
    new_dir.parent_inode = current_dir->self_inode;
    new_dir.size = 0;
    new_dir.type = TYPE_DIR;
    new_dir.parent_block = current_dir->self_block;

    int i = 0;
    while (i < N_DATA_BLOCKS && disk->bitmap[i] == '0')
        i++;

    if (i >= N_DATA_BLOCKS)
        return -1;

    new_dir.self_block = i;
    disk->bitmap[i] = '0';

    int j;
    while (j < N_INODES && disk->inodes[j].type != TYPE_EMPTY)
        j++;

    new_dir.self_inode = j;
    current_dir_inode.hard_links[current_dir_inode.n_links - 1] = i;

    disk->inodes[current_dir->self_inode] = current_dir_inode;

    block_write(new_dir.self_block, (char *)&new_dir);

    fs_flush();

    return 0;
}

//TODO: fs_rmdir: testing
int fs_rmdir(char *fileName)
{
    inode_t current_dir_inode = disk->inodes[current_dir->self_inode];

    block_entry_t *aux;
    inode_t aux_inode;
    int i;
    for (i = 0; i < current_dir_inode.n_links; i++)
    {
        block_read(current_dir_inode.hard_links[i], (char*)aux);

        if (same_string(aux->name,fileName))
        {
            if (aux->type != TYPE_DIR)
                return -1;
            break;
        }
    }

    if (i == current_dir_inode.n_links)
        return -1;

    aux_inode = disk->inodes[aux->self_inode];
    if (aux_inode.n_links > 0)
        return -1;

    disk->inodes[aux->self_inode].type = TYPE_EMPTY;
    disk->bitmap[current_dir_inode.hard_links[i]] = '1';

    for (int j = i; j < current_dir_inode.n_links-1; j++)
    {
        current_dir_inode.hard_links[j] = current_dir_inode.hard_links[j+1];
    }
    current_dir_inode.n_links--;
    disk->inodes[current_dir->self_inode] = current_dir_inode;

    fs_flush();

    return 0;
}

//TODO: fs_cd: testing
int fs_cd(char *dirName)
{
    inode_t current_dir_inode = disk->inodes[current_dir->self_inode];
    block_entry_t *aux;

    int i;
    for (i = 0; i < current_dir_inode.n_links; i++) 
    {
        block_read(current_dir_inode.hard_links[i], (char*)aux);
        if (same_string(aux->name, dirName))
        {
            break;
        }
    }

    if (i == current_dir_inode.n_links)
        return -1;

    current_dir = aux;
    return 0;
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

int fs_flush()
{
    block_write(0, (char *)disk->super_block);

    for (int i = 0; i < 4; i++)
        block_write(i + 1, (char *)(disk->bitmap + (i * BLOCK_SIZE)));

    for (int i = 0; i < (N_INODES / 8); i++)
        block_write(i + 5, (char *)(disk->inodes + i * 8));

    return 0;
}