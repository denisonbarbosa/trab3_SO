#include "util.h"
#include "common.h"
#include "block.h"
#include "fs.h"
#include <stdlib.h>

#define FAKE 1
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
    int link_count;
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

typedef struct data_block_s
{
    char *content;
} data_block_t;

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
static opened_file_t *open_files;
static int n_open_files;
static block_entry_t *aux;

//TODO: fs_init: testing
void fs_init(void)
{
    block_init();
    disk = (disk_t *)malloc(sizeof(disk_t));
    disk->super_block = (super_block_t *)malloc(sizeof(super_block_t));
    disk->inodes = (inode_t *)malloc(N_INODES * sizeof(inode_t));
    disk->bitmap = (char *)malloc(N_DATA_BLOCKS * sizeof(char));
    open_files = (opened_file_t *)malloc(20 * sizeof(opened_file_t));
    current_dir = (block_entry_t *)malloc(sizeof(block_entry_t));

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
        open_files[i].flag = -1;

    printf("after possible malloc error\n");
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

    block_write(disk->super_block->first_data_block, (char *)root);

    printf("Filesystem made\n");

    return 0;
}

//TODO: fs_open: testing
int fs_open(char *fileName, int flags)
{
    inode_t current_dir_inode = disk->inodes[current_dir->self_inode];
    int found = FALSE;
    block_entry_t *aux;
    aux = (block_entry_t *)malloc(sizeof(block_entry_t));
    printf("AQUI 1\n");

    for (int i = 0; i < current_dir_inode.n_links; i++)
    {
        printf("AQUI 2\n");
        block_read(current_dir_inode.hard_links[i] + disk->super_block->first_data_block, aux);
        printf("AQUI 3\n");

        printf("%s\n", aux->name);

        if (aux == NULL)
            continue;

        if (same_string(aux->name, fileName))
        {
            found = TRUE;
            break;
        }
    }
    printf("AQUI 4\n");
    if (found)
    {
        if (aux->type == TYPE_DIR && (flags == FS_O_WRONLY || flags == FS_O_RDWR))
            return -1;
        printf("AQUI 5\n");
        for (int i = 0; i < 20; i++)
        {
            if (open_files[i].flag == -1)
            {
                open_files[i].file = *aux;
                open_files[i].flag = flags;
                open_files[i].cursor = 0;
                printf("file descriptor: %d\n", i);
                return i;
            }
        }
        printf("AQUI 6\n");
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
            current_dir_inode.hard_links[current_dir_inode.n_links++] = i;
            block_entry_t *new_file = (block_entry_t*)malloc(sizeof(block_entry_t));
            new_file->name = fileName;
            new_file->parent_inode = current_dir->self_inode;
            new_file->size = 0;
            new_file->type = TYPE_FILE;
            new_file->parent_block = current_dir->self_block;

            int j;
            for (j = 0; j < N_INODES; j++)
            {
                if (disk->inodes[j].type == TYPE_EMPTY)
                    break;
            }

            if (j == N_INODES)
                return -1;

            disk->inodes[j].type = FILE_TYPE;
            // disk->inodes[j].n_links++;
            disk->inodes[j].link_count++;
            disk->inodes[j].hard_links[0] = i;
            new_file->self_inode = j;
            new_file->self_block = i;
            disk->bitmap[i] = '0';

            printf("Inode allocated for new file: %d\n", new_file->self_inode);

            block_write(i + disk->super_block->first_data_block, (char*)new_file);
            printf("File:\n %s\n %s\n", new_file->name, new_file);
            
            disk->inodes[current_dir->self_inode] = current_dir_inode;
            fs_flush();

            block_entry_t *file_written = malloc(BLOCK_SIZE * sizeof(char));
            block_read(i + disk->super_block->first_data_block, file_written);
            printf("File:\n %s\n %s\n", file_written->name ,file_written);

            for (int i = 0; i < 20; i++)
            {
                if (open_files[i].flag == -1)
                {
                    open_files[i].file = *new_file;
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
    printf("file size: %d && cursor: %d && count: %d\n", open_files[fd].file.size, cursor, count);
    if (open_files[fd].file.size - cursor < count)
        count = open_files[fd].file.size - cursor;

    char *block = (char*)malloc(sizeof(char)*BLOCK_SIZE);
    
    // PEGA O INODE DO FD
    inode_t file_inode = disk->inodes[open_files[fd].file.self_inode];

    // ACHA QUAL BLOCO TÁ O CURSOR

    // LE 

    block_read(file_inode.hard_links[0] + disk->super_block->first_data_block, block);
    bcopy((block + cursor), buf, count);
    open_files[fd].cursor += count;
    return count;
}

//TODO: fs_write: implementation
int fs_write(int fd, char *buf, int count)
{
    if (fd > 20)
        return -1;

    inode_t file_inode = disk->inodes[open_files[fd].file.self_inode];

    if (count > strlen(buf))
        count = strlen(buf);

    // printf("file: %s file_inode: %d", open_files[fd].file.name, open_files[fd].file.self_inode);

    int bytes_written = 0;

    // Remove after implementation of secundary and/or tertiary links on inode_t
    if ((10 - file_inode.n_links) * BLOCK_SIZE < count)
        return -1;

    char *aux_string = malloc(BLOCK_SIZE * sizeof(char));

    // VE SE TEM ESPAÇO NO ULTIMO ALOCADO
    if (open_files[fd].file.size % BLOCK_SIZE > 0)
    {
        int remaining_space = BLOCK_SIZE - (open_files[fd].file.size % BLOCK_SIZE);

        // ESCREVE NO ULTIMO BLOCO ALOCADO SE TIVER ESPAÇO
        block_read(file_inode.hard_links[file_inode.n_links - 1] + disk->super_block->first_data_block, aux_string);

        bcopy(buf, (aux_string + (open_files[fd].file.size % BLOCK_SIZE)), remaining_space);

        block_write(file_inode.hard_links[file_inode.n_links - 1] + disk->super_block->first_data_block, aux_string);

        printf("block where buf was written: %d\n",file_inode.hard_links[file_inode.n_links - 1]);

        count -= remaining_space;
        bytes_written += remaining_space;
        buf = buf + remaining_space;
        open_files[fd].file.size += remaining_space;
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
        printf("block where buf was written: %d\n", i);

        block_write(i + disk->super_block->first_data_block, buf);

        if (count > BLOCK_SIZE)
        {
            count -= BLOCK_SIZE;
            bytes_written += BLOCK_SIZE;
            buf = buf + BLOCK_SIZE;
            open_files[fd].file.size += BLOCK_SIZE;
        }
        else
        {
            bytes_written += count;
            open_files[fd].file.size += count;
            count = 0;
        }
    }
    disk->inodes[open_files[fd].file.self_inode] = file_inode;
    block_write(open_files[fd].file.self_block + disk->super_block->first_data_block, &open_files[fd].file);
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
    inode_t current_dir_inode = disk->inodes[current_dir->self_inode];

    block_entry_t *aux;
    aux = (block_entry_t *)malloc(sizeof(block_entry_t));

    for (int i = 0; i < current_dir_inode.n_links; i++)
    {
        block_read(current_dir_inode.hard_links[i] + disk->super_block->first_data_block, (char *)aux);
        if (same_string(aux->name, fileName))
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

    disk->inodes[j].type = TYPE_DIR;
    disk->inodes[j].link_count = 1;
    disk->inodes[j].n_links = 0;
    disk->inodes[j].size = 0;

    current_dir_inode.hard_links[current_dir_inode.n_links - 1] = i;

    disk->inodes[current_dir->self_inode] = current_dir_inode;

    block_write(new_dir.self_block + disk->super_block->first_data_block, (char *)&new_dir);

    fs_flush();

    return 0;
}

//TODO: fs_rmdir: testing
int fs_rmdir(char *fileName)
{
    inode_t current_dir_inode = disk->inodes[current_dir->self_inode];

    block_entry_t *aux;
    aux = (block_entry_t *)malloc(sizeof(block_entry_t));

    inode_t aux_inode;
    int i;
    for (i = 0; i < current_dir_inode.n_links; i++)
    {
        block_read(current_dir_inode.hard_links[i] + disk->super_block->first_data_block, (char *)aux);

        if (same_string(aux->name, fileName))
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

    for (int j = i; j < current_dir_inode.n_links - 1; j++)
    {
        current_dir_inode.hard_links[j] = current_dir_inode.hard_links[j + 1];
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
    aux = (block_entry_t *)malloc(sizeof(block_entry_t));

    if (same_string(dirName, "."))
    {
        return 0;
    }

    if (same_string(dirName, ".."))
    {
        block_read(current_dir->parent_block + disk->super_block->first_data_block, current_dir);
        return 0;
    }

    int i;
    for (i = 0; i < current_dir_inode.n_links; i++)
    {
        block_read(current_dir_inode.hard_links[i] + disk->super_block->first_data_block, (char *)aux);
        if (same_string(aux->name, dirName))
        {
            break;
        }
    }

    if (i == current_dir_inode.n_links)
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
        block_read(current_dir_inode.hard_links[i] + disk->super_block->first_data_block, (char *)aux);

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
    buf->numBlocks = aux->size / BLOCK_SIZE;
    buf->size = aux->size;
    buf->type = aux->type;

    return 0;
}

int fs_ls()
{
    inode_t current_dir_inode = disk->inodes[current_dir->self_inode];
    block_entry_t *aux;
    aux = (block_entry_t *)malloc(sizeof(block_entry_t));

    int col;
    for (int i = 0; i < current_dir_inode.n_links; i++)
    {
        block_read(current_dir_inode.hard_links[i] + disk->super_block->first_data_block, (char *)aux);
        col = 0;
        print_str(i, col, aux->name);
        col += (strlen(aux->name) + 1);
        print_char(i, col, aux->type);
        col += 2;
        print_int(i, col, aux->self_inode);
        col += 5;
        print_int(i, col, aux->size);
    }
    return 0;
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