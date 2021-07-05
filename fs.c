#include "util.h"
#include "common.h"
#include "block.h"
#include "fs.h"
#include <stdlib.h>
#include "disk_IO.h"

#ifdef FAKE
#include <stdio.h>
#define ERROR_MSG(m) printf m;
#else
#define ERROR_MSG(m)
#endif

static disk_t *disk;
static block_entry_t *current_dir;
static opened_file_t *open_files;
static int n_open_files;
static block_entry_t *new_file;

//TODO: fs_init: testing
void fs_init(void)
{
    disk = (disk_t *)malloc(sizeof(disk_t));
    disk->super_block = (super_block_t *)malloc(sizeof(super_block_t));
    disk->inodes = (inode_t *)malloc(N_INODES * sizeof(inode_t));
    disk->bitmap = (char *)malloc(N_DATA_BLOCKS * sizeof(char));
    open_files = (opened_file_t *)malloc(MAX_FILE_HANDLERS * sizeof(opened_file_t));
    current_dir = (block_entry_t *)malloc(sizeof(block_entry_t));

    init_disk_IO(disk);

    super_read(&disk->super_block);

    if (disk->super_block->magic_number == FORMATTED_DISK)
    {
        bitmap_read(disk->bitmap);

        for (int i = 0; i < N_INODES; i++)
        {
            inode_read(i, &disk->inodes[i]);
        }
        entry_read(0, &current_dir);
    }

    for (int i = 0; i < MAX_FILE_HANDLERS; i++)
        open_files[i].flag = -1;
}

//DONE: fs_mkfs
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
    disk->inodes[0].link_count = 1;
    disk->inodes[0].n_links = 0;
    disk->bitmap[0] = '0';

    for (int i = 1; i < N_INODES; i++)
    {
        if (i < N_DATA_BLOCKS)
            disk->bitmap[i] = '1';

        disk->inodes[i].type = TYPE_EMPTY;
        disk->inodes[i].size = 0;
        disk->inodes[i].link_count = 0;
        disk->inodes[i].n_links = 0;
    }

    fs_flush();

    block_entry_t *root = (block_entry_t *)malloc(sizeof(block_entry_t));

    root->name = "/";
    root->self_inode = 0;
    root->parent_inode = 0;
    root->self_block = 0;
    root->parent_block = 0;

    entry_write(root->self_block, root);

    return 0;
}

//DONE: fs_open
int fs_open(char *fileName, int flags)
{
    inode_t *current_inode = &disk->inodes[current_dir->self_inode];

    block_entry_t *aux = (block_entry_t *)malloc(sizeof(block_entry_t));

    int i = 0;
    for (i = 0; i < current_inode->n_links; i++)
    {
        entry_read(current_inode->hard_links[i], &aux);
        if (same_string(aux->name, fileName))
        {
            break;
        }
    }

    if (i < current_inode->n_links)
    {
        if (disk->inodes[aux->self_inode].type == TYPE_DIR && flags > 1)
            return -1;
        for (int j = 0; j < MAX_FILE_HANDLERS; j++)
        {
            if (open_files[j].flag == -1)
            {
                open_files[j].file = aux;
                open_files[j].flag = flags;
                open_files[j].cursor = 0;
                return j;
            }
        }
        return -1;
    }
    if (flags < 2)
        return -1;

    i = 0;
    while (i < N_DATA_BLOCKS && disk->bitmap[i] != '1')
        i++;

    if (i == N_DATA_BLOCKS)
        return -1;

    disk->bitmap[i] = '0';
    bitmap_write(disk->bitmap);

    int j = 0;
    while (j < N_INODES && disk->inodes[j].type != TYPE_EMPTY)
        j++;

    if (j == N_INODES)
        return -1;

    (new_file) = (block_entry_t *)malloc(sizeof(block_entry_t));
    new_file->name = malloc(MAX_FILE_NAME * sizeof(char));
    bcopy(fileName, new_file->name, strlen(fileName));
    (new_file)->parent_block = current_dir->parent_block;
    (new_file)->parent_inode = current_dir->self_inode;
    (new_file)->self_inode = j;
    (new_file)->self_block = i;

    disk->inodes[j].link_count = 1;
    disk->inodes[j].n_links = 0;
    disk->inodes[j].size = 0;
    disk->inodes[j].type = TYPE_FILE;
    inode_write(j, &disk->inodes[j]);

    current_inode->size++;

    current_inode->hard_links[current_inode->n_links++] = i;
    inode_write(current_dir->self_inode, current_inode);

    entry_write(i, (new_file));

    entry_write(current_dir->self_block, current_dir);

    for (int c = 0; c < MAX_FILE_HANDLERS; c++)
    {
        if (open_files[c].flag == -1)
        {
            open_files[c].file = (new_file);
            open_files[c].cursor = 0;
            open_files[c].flag = flags;
            return c;
        }
    }

    return -1;
}

//DONE: fs_close
int fs_close(int fd)
{
    if (fd > MAX_FILE_HANDLERS || fd < 0)
        return -1;

    if (open_files[fd].flag == -1)
        return -1;

    inode_t *file_inode = &disk->inodes[open_files[fd].file->self_inode];

    open_files[fd].flag = -1;

    if (file_inode->link_count == 0)
        fs_unlink(open_files[fd].file->name);

    return 0;
}

//DONE: fs_read
int fs_read(int fd, char *buf, int count)
{
    int cursor = open_files[fd].cursor;
    int bytes_read = 0;

    inode_t *file_inode = &disk->inodes[open_files[fd].file->self_inode];

    if (fd >= MAX_FILE_HANDLERS || fd < 0)
        return -1;

    if (open_files[fd].flag == FS_O_WRONLY)
        return -1;

    if (file_inode->size < cursor)
        return -1;

    if (file_inode->size - cursor < count)
        count = file_inode->size - cursor;

    int cursor_block = cursor / BLOCK;
    int cursor_offset = cursor % BLOCK;

    int aux = 0;
    char *init_buf = buf;
    if (file_inode->n_links > 0)
    {
        aux = content_read(file_inode->hard_links[cursor_block], buf, cursor_offset, count);
        buf = buf + aux;
        bytes_read += aux;
        count -= aux;
        cursor += aux;
    }
    if (count > 0)
    {
        for (int i = cursor_block + 1; i < file_inode->n_links; i++)
        {
            if (count <= 0)
                break;

            aux = content_read(file_inode->hard_links[i], buf, 0, count);
            buf = buf + aux;
            count -= aux;
            cursor += aux;
            bytes_read += aux;
        }
    }
    open_files[fd].cursor = cursor;
    buf = init_buf;
    return bytes_read;
}

//DONE: fs_write
int fs_write(int fd, char *buf, int count)
{
    int cursor = open_files[fd].cursor;

    if (fd >= MAX_FILE_HANDLERS || fd < 0)
        return -1;

    inode_t *file_inode = &disk->inodes[open_files[fd].file->self_inode];

    if (count > strlen(buf))
        count = strlen(buf);

    int bytes_written = 0;

    // Remove after implementation of secundary and/or tertiary links on inode_t
    if ((10 - file_inode->n_links) * BLOCK_SIZE < count)
        return -1;

    int cursor_block = cursor / BLOCK;
    int cursor_block_offset = cursor % BLOCK;
    int amount, aux;
    int remaining_space = BLOCK_SIZE - cursor_block_offset;
    int buf_size = strlen(buf);

    if (file_inode->n_links > 0)
    {
        amount = buf_size;
        if (amount > remaining_space)
            amount = remaining_space;

        aux = content_write(file_inode->hard_links[cursor_block], buf, cursor_block_offset, amount);
        count -= aux;
        bytes_written += aux;
        buf = buf + aux;
        file_inode->size += aux;
        cursor += aux;
        cursor_block = cursor / BLOCK;
        buf_size -= aux;
    }

    while (count > 0)
    {
        int i = 0;
        while (i < N_DATA_BLOCKS && disk->bitmap[i] != '1')
            i++;

        file_inode->hard_links[file_inode->n_links++] = i;
        disk->bitmap[i] = '0';

        amount = buf_size;
        if (amount > count)
            amount = count;

        aux = content_write(i, buf, 0, amount);
        count -= aux;
        bytes_written += aux;
        buf = buf + aux;
        file_inode->size += aux;
        cursor += aux;
        buf_size -= aux;
    }

    open_files[fd].cursor = cursor;
    inode_write(open_files[fd].file->self_inode, file_inode);
    bitmap_write(disk->bitmap);
    entry_write(open_files[fd].file->self_block, open_files[fd].file);

    return bytes_written;
}

//DONE: fs_lseek
int fs_lseek(int fd, int offset)
{
    if (fd >= MAX_FILE_HANDLERS || fd < 0 || open_files[fd].flag < 0)
        return -1;

    open_files[fd].cursor = offset;
    return open_files[fd].cursor;
}

//DONE: fs_mkdir
int fs_mkdir(char *fileName)
{
    inode_t *current_dir_inode = &disk->inodes[current_dir->self_inode];

    block_entry_t *aux;
    aux = (block_entry_t *)malloc(sizeof(block_entry_t));

    for (int i = 0; i < current_dir_inode->n_links; i++)
    {
        entry_read(current_dir_inode->hard_links[i], &aux);
        if (same_string(aux->name, fileName))
            return -1;
    }

    if (current_dir_inode->n_links == 10)
        return -1;

    int i = 0;
    while (i < N_DATA_BLOCKS && disk->bitmap[i] != '1')
        i++;

    if (i >= N_DATA_BLOCKS)
        return -1;

    disk->bitmap[i] = '0';
    bitmap_write(disk->bitmap);

    int j = 0;
    while (j < N_INODES && disk->inodes[j].type != TYPE_EMPTY)
        j++;

    block_entry_t *new_dir = malloc(sizeof(block_entry_t));
    new_dir->name = malloc(MAX_FILE_NAME * sizeof(char));
    bcopy(fileName, new_dir->name, strlen(fileName));
    new_dir->parent_inode = current_dir->self_inode;
    new_dir->parent_block = current_dir->self_block;
    new_dir->self_block = i;
    new_dir->self_inode = j;

    current_dir_inode->size++;

    disk->inodes[j].type = TYPE_DIR;
    disk->inodes[j].link_count = 1;
    disk->inodes[j].n_links = 0;
    disk->inodes[j].size = 0;
    inode_write(j, &disk->inodes[j]);

    current_dir_inode->hard_links[current_dir_inode->n_links++] = i;

    inode_write(current_dir->self_inode, current_dir_inode);
    entry_write(current_dir->self_block, current_dir);
    entry_write(new_dir->self_block, new_dir);

    return 0;
}

//DONE: fs_rmdir
int fs_rmdir(char *fileName)
{
    inode_t *current_dir_inode = &disk->inodes[current_dir->self_inode];

    block_entry_t *aux;
    aux = (block_entry_t *)malloc(sizeof(block_entry_t));

    inode_t *aux_inode;
    int i;
    for (i = 0; i < current_dir_inode->n_links; i++)
    {
        entry_read(current_dir_inode->hard_links[i], &aux);

        if (same_string(aux->name, fileName))
        {
            if (disk->inodes[aux->self_inode].type != TYPE_DIR)
                return -1;
            break;
        }
    }

    if (i == current_dir_inode->n_links)
        return -1;

    aux_inode = &disk->inodes[aux->self_inode];
    if (aux_inode->n_links > 0)
        return -1;

    disk->inodes[aux->self_inode].type = TYPE_EMPTY;
    disk->bitmap[current_dir_inode->hard_links[i]] = '1';
    bitmap_write(disk->bitmap);

    for (int j = i; j < current_dir_inode->n_links - 1; j++)
    {
        current_dir_inode->hard_links[j] = current_dir_inode->hard_links[j + 1];
    }
    current_dir_inode->n_links--;

    fs_flush();

    return 0;
}

//DONE: fs_cd
int fs_cd(char *dirName)
{
    inode_t *current_dir_inode = &disk->inodes[current_dir->self_inode];
    block_entry_t *aux;
    aux = (block_entry_t *)malloc(sizeof(block_entry_t));

    if (same_string(dirName, "."))
    {
        return 0;
    }

    if (same_string(dirName, ".."))
    {
        entry_read(current_dir->parent_block, &current_dir);
        return 0;
    }

    int i;
    for (i = 0; i < current_dir_inode->n_links; i++)
    {
        entry_read(current_dir_inode->hard_links[i], &aux);
        if (same_string(aux->name, dirName))
        {
            if (disk->inodes[aux->self_inode].type == TYPE_FILE)
                return -1;
            break;
        }
    }

    if (i == current_dir_inode->n_links)
        return -1;

    current_dir = aux;

    fs_flush();
    return 0;
}

//DONE: fs_link
int fs_link(char *old_fileName, char *new_fileName)
{
    inode_t *current_dir_inode = &disk->inodes[current_dir->self_inode];
    block_entry_t *aux = (block_entry_t *)malloc(sizeof(block_entry_t));

    int found = FALSE;
    int i;

    for (i = 0; i < current_dir_inode->n_links; i++)
    {
        entry_read(current_dir_inode->hard_links[i], &aux);

        if (same_string(aux->name, old_fileName))
        {
            found = TRUE;
            break;
        }
    }

    // Remove last condition after implementation of secundary and/or tertiary links on inode_t
    if (found && disk->inodes[aux->self_inode].type == TYPE_FILE && current_dir_inode->n_links < 10)
    {

        block_entry_t *new_file = (block_entry_t *)malloc(sizeof(block_entry_t));
        new_file->name = (char *)malloc(MAX_FILE_NAME * sizeof(char));
        bcopy(new_fileName, new_file->name, strlen(new_fileName));
        new_file->parent_block = aux->parent_block;
        new_file->parent_inode = aux->parent_inode;
        new_file->self_inode = aux->self_inode;
        disk->inodes[new_file->self_inode].link_count++;

        i = 0;
        while (i < N_DATA_BLOCKS && disk->bitmap[i] != '1')
            i++;

        if (i == N_DATA_BLOCKS)
            return -1;

        disk->bitmap[i] = '0';
        bitmap_write(disk->bitmap);
        new_file->self_block = i;
        entry_write(i, new_file);
        current_dir_inode->hard_links[current_dir_inode->n_links++] = i;
        inode_write(current_dir->self_inode, current_dir_inode);
        inode_write(new_file->self_inode, &disk->inodes[new_file->self_inode]);
        return 0;
    }

    return -1;
}

//DONE: fs_unlink
int fs_unlink(char *fileName)
{
    inode_t *current_dir_inode = &disk->inodes[current_dir->self_inode];
    block_entry_t *aux;
    aux = (block_entry_t *)malloc(sizeof(block_entry_t));

    int i;
    for (i = 0; i < current_dir_inode->n_links; i++)
    {
        entry_read(current_dir_inode->hard_links[i], &aux);

        if (same_string(aux->name, fileName))
        {
            break;
        }
    }
    if (i == current_dir_inode->n_links)
        return -1;

    if (disk->inodes[aux->self_inode].type == TYPE_DIR)
        return -1;

    int found = FALSE;
    for (int j = 0; j < MAX_FILE_HANDLERS; j++)
    {
        if (same_string(open_files[j].file->name, aux->name) && open_files[j].flag > -1)
        {
            found = TRUE;
            break;
        }
    }

    if (disk->inodes[aux->self_inode].link_count > 0)
    {
        disk->bitmap[aux->self_block] = '1';
        disk->inodes[aux->self_inode].link_count--;

        for (; i < current_dir_inode->n_links - 1; i++)
            current_dir_inode->hard_links[i] = current_dir_inode->hard_links[i + 1];

        current_dir_inode->n_links--;
    }

    if (found && disk->inodes[aux->self_inode].link_count == 0)
        return 0;

    if (disk->inodes[aux->self_inode].link_count == 0)
    {

        inode_t *file_inode = &disk->inodes[aux->self_inode];
        for (int j = 0; j < file_inode->n_links; j++)
        {
            disk->bitmap[file_inode->hard_links[j]] = '1';
        }
        file_inode->type = TYPE_EMPTY;
    }

    inode_write(current_dir->self_inode, current_dir_inode);
    inode_write(aux->self_inode, &disk->inodes[aux->self_inode]);
    bitmap_write(disk->bitmap);
    return 0;
}

//DONE: fs_stat
int fs_stat(char *fileName, fileStat *buf)
{
    inode_t current_dir_inode = disk->inodes[current_dir->self_inode];
    block_entry_t *aux;
    aux = (block_entry_t *)malloc(sizeof(block_entry_t));

    int found = FALSE;
    int i;

    for (i = 0; i < current_dir_inode.n_links; i++)
    {
        entry_read(current_dir_inode.hard_links[i], &aux);

        if (same_string(aux->name, fileName))
        {
            found = TRUE;
            break;
        }
    }

    if (found == FALSE || disk->inodes[aux->self_inode].type == TYPE_DIR)
        return -1;

    buf->inodeNo = aux->self_inode;
    buf->links = disk->inodes[aux->self_inode].link_count;
    buf->numBlocks = disk->inodes[aux->self_inode].n_links;
    buf->size = disk->inodes[aux->self_inode].size;

    buf->type = atoi(&disk->inodes[aux->self_inode].type);

    return 0;
}

//DONE: fs_ls
int fs_ls()
{
    inode_t *current_dir_inode = &disk->inodes[current_dir->self_inode];
    block_entry_t *aux;
    int col;
    for (int i = 0; i < current_dir_inode->n_links; i++)
    {
        aux = (block_entry_t *)malloc(sizeof(block_entry_t));
        entry_read(current_dir_inode->hard_links[i], &aux);

        printf("%s %c %d %d\n", aux->name, disk->inodes[aux->self_inode].type, aux->self_inode, disk->inodes[aux->self_inode].size);
    }
    return 0;
}

int fs_flush()
{
    super_write(disk->super_block);

    bitmap_write(disk->bitmap);

    for (int i = 0; i < N_INODES; i++)
        inode_write(i, &disk->inodes[i]);

    return 0;
}