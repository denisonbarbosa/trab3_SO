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
// static block_entry_t *aux;
static block_entry_t *new_file;

//TODO: fs_init: testing
void fs_init(void)
{
    disk = (disk_t *)malloc(sizeof(disk_t));
    disk->super_block = (super_block_t *)malloc(sizeof(super_block_t));
    disk->inodes = (inode_t *)malloc(N_INODES * sizeof(inode_t));
    disk->bitmap = (char *)malloc(N_DATA_BLOCKS * sizeof(char));
    open_files = (opened_file_t *)malloc(20 * sizeof(opened_file_t));
    current_dir = (block_entry_t *)malloc(sizeof(block_entry_t));

    init_disk_IO(disk);

    super_read(&disk->super_block);
    printf("superblock read\n");

    if (disk->super_block->magic_number == 0)
    {
        fs_mkfs();
    }
    else
    {
        bitmap_read(&disk->bitmap);
        printf("bitmap read\n");

        for (int i = 0; i < N_INODES; i++)
        {
            inode_read(i, &disk->inodes[i]);
        }
        printf("inodes read\n");
    }

    entry_read(0, &current_dir);
    printf("root_dir read\n");

    for (int i = 0; i < 20; i++)
        open_files[i].flag = -1;
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
    disk->inodes[0].link_count = 1;
    disk->inodes[0].n_links = 0;
    disk->bitmap[0] = '0';

    for (int i = 1; i < N_DATA_BLOCKS; i++)
    {
        if (i < N_INODES)
        {
            disk->inodes[i].type = TYPE_EMPTY;
            disk->inodes[i].size = 0;
            disk->inodes[i].link_count = 0;
            disk->inodes[i].n_links = 0;
        }
        disk->bitmap[i] = '1';
    }

    fs_flush();

    block_entry_t *root = (block_entry_t *)malloc(sizeof(block_entry_t));

    root->name = "/";
    root->self_inode = 0;
    root->parent_inode = 0;
    root->self_block = 0;
    root->parent_block = 0;
    root->size = 0;
    root->type = TYPE_DIR;

    return 0;
}

int fs_open(char *fileName, int flags)
{
    // VER SE O ARQUIVO JA EXISTE
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

    // SE SIM, RETORNA DESCRITOR PRA ELE
    if (i < current_inode->n_links)
    {
        if (aux->type == TYPE_DIR && flags > 1)
            return -1;
        for (int j = 0; j < 20; j++)
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
    // SE NAO, CRIA OUTRO ARQUIVO
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
    (new_file)->size = 0;
    (new_file)->type = TYPE_FILE;

    disk->inodes[j].link_count = 1;
    disk->inodes[j].n_links = 0;
    disk->inodes[j].size = 0;
    disk->inodes[j].type = TYPE_FILE;
    inode_write(j, &disk->inodes[j]);

    current_dir->size++;
    current_inode->size++;

    current_inode->hard_links[current_inode->n_links++] = i;
    inode_write(current_dir->self_inode, current_inode);

    entry_write(i, (new_file));

    entry_write(current_dir->self_block, &current_dir);

    // RETORNA DESCRITOR DO NOVO ARQUIVO
    for (int c = 0; c < 20; c++)
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
    int cursor = open_files[fd].cursor;

    if (fd > 20 || open_files[fd].flag < 0 || open_files[fd].flag == FS_O_WRONLY || open_files[fd].file->size < cursor)
        return -1;

    int i = 0;

    if (open_files[fd].file->size - cursor < count)
        count = open_files[fd].file->size - cursor;

    if (count == 0)
        return 0;
    // printf("file size: %d && cursor: %d && count: %d\n", open_files[fd].file->size, cursor, count);

    char *block = (char *)malloc(sizeof(char) * BLOCK_SIZE);
    buf = (char *)malloc(sizeof(char) * count);

    // PEGA O INODE DO FD
    inode_t *file_inode = &disk->inodes[open_files[fd].file->self_inode];

    // ACHA QUAL BLOCO TÁ O CURSOR
    int block_index = cursor / BLOCK_SIZE;
    int offset = cursor % BLOCK_SIZE;
    // LE
    content_read(file_inode->hard_links[block_index], &block);
    // printf("bloco: %d | offset: %d | count: %d \n", block_index, offset, count);
    {
        bcopy((block + offset), buf, BLOCK_SIZE - offset);
        for (i = 1; (i + 1) * BLOCK_SIZE - offset < count; i++)
        {
            content_read(file_inode->hard_links[block_index + i], &block);
            bcopy(block, buf + i * BLOCK_SIZE, BLOCK_SIZE);
        }
    }
    // printf("i: %d\n", i);
    content_read(file_inode->hard_links[block_index + i], &block);
    // printf("block: %s\n", block);

    bcopy(block, buf + i * BLOCK_SIZE, BLOCK_SIZE - count % BLOCK_SIZE);
}

//TODO: fs_write: implementation
int fs_write(int fd, char *buf, int count)
{
    if (fd > 20)
        return -1;

    inode_t file_inode = disk->inodes[open_files[fd].file->self_inode];

    if (count > strlen(buf))
        count = strlen(buf);

    int bytes_written = 0;

    // Remove after implementation of secundary and/or tertiary links on inode_t
    if ((10 - file_inode.n_links) * BLOCK_SIZE < count)
        return -1;

    // VE SE TEM ESPAÇO NO ULTIMO ALOCADO
    if (open_files[fd].file->size % BLOCK_SIZE > 0)
    {
        char *aux_string = (char*) malloc((open_files[fd].file->size % BLOCK_SIZE) * sizeof(char));
        char *block = (char*) malloc(BLOCK * sizeof(char));

        int remaining_space = BLOCK_SIZE - (open_files[fd].file->size % BLOCK_SIZE);

        // ESCREVE NO ULTIMO BLOCO ALOCADO SE TIVER ESPAÇO
        content_read(file_inode.hard_links[file_inode.n_links - 1], &aux_string);

        bcopy(aux_string, block, strlen(aux_string));

        bcopy(buf, (block + (open_files[fd].file->size % BLOCK_SIZE)), remaining_space);

        content_write(file_inode.hard_links[file_inode.n_links - 1], block);

        count -= remaining_space;
        bytes_written += remaining_space;
        buf = buf + remaining_space;
        open_files[fd].file->size += remaining_space;
    }

    while (count > 0)
    {
        // ALOCA BLOCO
        int i = 0;
        while (i < N_DATA_BLOCKS && disk->bitmap[i] != '1')
            i++;

        file_inode.hard_links[file_inode.n_links++] = i;
        disk->bitmap[i] = '0';

        // ESCREVE NO BLOCO ALOCADO

        content_write(i, buf);

        if (count > BLOCK_SIZE)
        {
            count -= BLOCK_SIZE;
            bytes_written += BLOCK_SIZE;
            buf = buf + BLOCK_SIZE;
            open_files[fd].file->size += BLOCK_SIZE;
        }
        else
        {
            bytes_written += count;
            open_files[fd].file->size += count;
            count = 0;
        }
    }
    disk->inodes[open_files[fd].file->self_inode] = file_inode;

    entry_write(open_files[fd].file->self_block, open_files[fd].file);
    fs_flush();
    return bytes_written;
}

//TODO: fs_lseek: testing
int fs_lseek(int fd, int offset)
{
    if (fd > 20 || fd < 0 || open_files[fd].flag < 0)
        return -1;

    open_files[fd].cursor = offset;
    return open_files[fd].cursor;
}

//TODO: fs_mkdir: testing
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
    new_dir->size = 0;
    new_dir->type = TYPE_DIR;
    new_dir->self_block = i;
    new_dir->self_inode = j;

    current_dir_inode->size++;
    current_dir->size++;

    disk->inodes[j].type = TYPE_DIR;
    disk->inodes[j].link_count = 1;
    disk->inodes[j].n_links = 0;
    disk->inodes[j].size = 0;
    inode_write(j, &disk->inodes[j]);

    current_dir_inode->hard_links[current_dir_inode->n_links++] = i;

    inode_write(current_dir->self_inode, current_dir_inode);
    entry_write(current_dir->self_block, &current_dir);
    entry_write(new_dir->self_block, new_dir);

    return 0;
}

//TODO: fs_rmdir: testing
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
            if (aux->type != TYPE_DIR)
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

//TODO: fs_cd: testing
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
            break;
        }
    }

    if (i == current_dir_inode->n_links)
        return -1;

    current_dir = aux;

    fs_flush();
    return 0;
}

//TODO: fs_link: testing
int fs_link(char *old_fileName, char *new_fileName)
{
    inode_t current_dir_inode = disk->inodes[current_dir->self_inode];
    block_entry_t *aux;
    aux = (block_entry_t *)malloc(sizeof(block_entry_t));

    int found = FALSE;
    int i;

    for (i = 0; i < current_dir_inode.n_links; i++)
    {
        block_read(current_dir_inode.hard_links[i] + disk->super_block->first_data_block, (char *)aux);

        if (same_string(aux->name, old_fileName))
        {
            found = TRUE;
            break;
        }
    }

    // Remove last condition after implementation of secundary and/or tertiary links on inode_t
    if (found && aux->type == TYPE_FILE && current_dir_inode.n_links < 10)
    {
        block_entry_t *new_file = (block_entry_t *)malloc(sizeof(block_entry_t));
        new_file->name = new_fileName;
        new_file->parent_block = aux->parent_block;
        new_file->parent_inode = aux->parent_inode;
        new_file->self_block = aux->self_block;
        new_file->self_inode = aux->self_inode;
        new_file->size = aux->size;
        new_file->type = aux->type;

        for (i = disk->super_block->first_data_block, found = FALSE; i < N_DATA_BLOCKS; i++)
        {
            if (disk->bitmap[i] == '1')
            {
                found = TRUE;
                break;
            }
        }

        if (found)
        {
            block_write(i + disk->super_block->first_data_block, (char *)new_file);
            current_dir_inode.hard_links[current_dir_inode.n_links++] = i;
            fs_flush();
            return 0;
        }
    }

    return -1;
}

//TODO: fs_unlink: implementation
int fs_unlink(char *fileName)
{
    inode_t current_dir_inode = disk->inodes[current_dir->self_inode];
    block_entry_t *aux;
    aux = (block_entry_t *)malloc(sizeof(block_entry_t));

    int found = FALSE;
    int i;

    for (i = 0; i < current_dir_inode.n_links; i++)
    {
        block_read(current_dir_inode.hard_links[i] + disk->super_block->first_data_block, (char *)aux);

        if (same_string(aux->name, fileName))
        {
            found = TRUE;
            break;
        }
    }

    if (found == FALSE || aux->type == TYPE_DIR)
        return -1;

    // TODO
}

//TODO: fs_stat: testing
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

    if (found == FALSE || aux->type == TYPE_DIR)
        return -1;

    buf->inodeNo = aux->self_inode;
    buf->links = disk->inodes[aux->self_inode].link_count;
    buf->numBlocks = disk->inodes[aux->self_inode].n_links;
    buf->size = aux->size;

    buf->type = atoi(&aux->type);

    return 0;
}

int fs_ls()
{
    inode_t *current_dir_inode = &disk->inodes[current_dir->self_inode];
    block_entry_t *aux;
    int col;
    for (int i = 0; i < current_dir_inode->n_links; i++)
    {
        aux = (block_entry_t *)malloc(sizeof(block_entry_t));
        entry_read(current_dir_inode->hard_links[i], &aux);

        printf("%s %c %d %d\n", aux->name, aux->type, aux->self_inode, aux->size);
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