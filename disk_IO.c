#include "disk_IO.h"

static FILE *util_fd;
static disk_t *util_disk;

int init_disk_IO(disk_t *disk)
{
    util_disk = disk;
    util_fd = fopen("./disk", "r+");

    if (util_fd == NULL)
        util_fd = fopen("./disk", "w+");

    if (util_fd == NULL)
        return -1;

    return 0;
}

int super_write(super_block_t *super_block)
{
    fseek(util_fd, 0, SEEK_SET);

    return fwrite(super_block, sizeof(super_block_t), 1, util_fd);
}

int inode_write(int n, inode_t *inode)
{
    int block = n / 8;
    block += util_disk->super_block->first_inode;

    int offset = n % 8;

    fseek(util_fd, ((block * BLOCK) + (offset * sizeof(inode_t))), SEEK_SET);

    return fwrite(inode, sizeof(inode_t), 1, util_fd);
}

int entry_write(int n, block_entry_t *entry)
{
    int block = entry->self_block;
    block += util_disk->super_block->first_data_block;

    fseek(util_fd, (block * BLOCK), SEEK_SET);

    return fwrite(entry, sizeof(block_entry_t), 1, util_fd);
}

int bitmap_write(char *bitmap)
{
    fseek(util_fd, BLOCK, SEEK_SET);

    return fwrite(bitmap, N_DATA_BLOCKS * sizeof(char), 1, util_fd);
}

int content_write(int block, char *buffer, int offset, int count)
{
    block += util_disk->super_block->first_data_block;

    fseek(util_fd, (block * BLOCK) + offset , SEEK_SET);

    return fwrite(buffer, 1, count, util_fd);
}

int super_read(super_block_t **super_block)
{
    fseek(util_fd, 0, SEEK_SET);

    return fread(*super_block, sizeof(super_block_t), 1, util_fd);
}

int inode_read(int n, inode_t *inode)
{
    int block = n / 8;
    block += util_disk->super_block->first_inode;

    int offset = n % 8;

    fseek(util_fd, ((block * BLOCK) + (offset * sizeof(inode_t))), SEEK_SET);

    return fread(inode, sizeof(inode_t), 1, util_fd);
}

int entry_read(int n, block_entry_t **entry)
{
    int block = n;
    block += util_disk->super_block->first_data_block;

    fseek(util_fd, (block * BLOCK), SEEK_SET);

    fread(*entry, sizeof(block_entry_t), 1, util_fd);

    return 1;
}

int bitmap_read(char **bitmap)
{
    fseek(util_fd, BLOCK, SEEK_SET);

    return fread(*bitmap, N_DATA_BLOCKS * sizeof(char), 1, util_fd);
}

int content_read(int block, char **buffer, int offset, int count)
{
    block += util_disk->super_block->first_data_block;

    fseek(util_fd, (block * BLOCK) + offset, SEEK_SET);

    return fread(buffer, 1, count, util_fd);
}