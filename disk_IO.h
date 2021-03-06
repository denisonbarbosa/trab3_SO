#ifndef DISK_IO_H
#define DISK_IO_H

#include <stdio.h>
#include "disk_types.h"

int init_disk_IO();

int inode_write(int n, inode_t* inode);

int entry_write(int n, block_entry_t *entry);

int bitmap_write(char *bitmap);

int links_write(int block, int *links);

int content_write(int block, char* buffer, int offset, int count);

int inode_read(int n, inode_t *inode);

int entry_read(int n, block_entry_t **entry);

int bitmap_read(char **bitmap);

int links_read(int block, int **links);

int content_read(int block, char **buffer, int offset, int count);

#endif
