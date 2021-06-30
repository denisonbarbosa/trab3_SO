#include <stdio.h>
#include <stdlib.h>
#include <assert.h>

#define BLOCK_SIZE 512
#define FORMATTED_DISK "0xf"
#define N_INODES 510
#define N_DATA_BLOCKS 1536

static FILE *fd;


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

void bzero_block( char *block) {
	int i;

	for ( i = 0; i < BLOCK_SIZE; i++)
		block[i] = 0;
}

void block_init( void) {
	int ret;

	fd = fopen( "./disk", "r+");
	if ( fd == NULL) 
		fd = fopen( "./disk", "w+");
	assert( fd);

	ret = fseek( fd, 0, SEEK_SET);
	assert( ret == 0);
}

void block_read( int block, char *mem) {
	int ret;

	ret = fseek( fd, block * BLOCK_SIZE, SEEK_SET);
	assert( ret == 0);
    
	ret = fread( mem, 1, BLOCK_SIZE, fd);
	if ( ret == 0) { /* End of file */
		ret = BLOCK_SIZE;
		bzero_block( mem);
	}
	assert( ret == BLOCK_SIZE);
}

void block_write( int block, char *mem) {
	int ret;
    
	ret = fseek( fd, block * BLOCK_SIZE, SEEK_SET);
	assert( ret == 0);
    
	ret = fwrite( mem, 1, BLOCK_SIZE, fd);
	assert( ret == BLOCK_SIZE);
}

void print_disk(disk_t *disk)
{
    printf("DISK\n %d\n %d\n %d\n %s\n %d\n %d\n", 
    disk->super_block->blocks_size,
    disk->super_block->n_inodes,
    disk->super_block->n_data_blocks,
    disk->super_block->magic_number,
    disk->super_block->first_inode,
    disk->super_block->first_data_block
    );
}


int fs_mkfs(void)
{
    disk->super_block->blocks_size = BLOCK_SIZE;
    disk->super_block->n_inodes = N_INODES;
    disk->super_block->n_data_blocks = N_DATA_BLOCKS;
    disk->super_block->magic_number = FORMATTED_DISK;
    disk->super_block->first_inode = (N_DATA_BLOCKS/BLOCK_SIZE) + 2;
    disk->super_block->first_data_block = disk->super_block->first_inode + N_INODES;
    
    print_disk(disk);
    
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


//TODO: fs_init: implementation
void fs_init(void)
{
    block_init();

    disk = (disk_t *)malloc(sizeof(disk_t));
    disk->super_block = (super_block_t *)malloc(sizeof(super_block_t));
    disk->inodes = (inode_t*)malloc(N_INODES * sizeof(inode_t));
    disk->bitmap = (char*)malloc(N_DATA_BLOCKS * sizeof(char));

    block_read(0, (char*)disk->super_block);
    
    printf("The problem is inside the if\n");
    
    if (disk->super_block->magic_number == NULL)
    {
        fs_mkfs();
    }
    else
    {

    }
}

//TODO: fs_mkfs: implementation


static disk_t *disk;

int main() 
{
    fs_init();

    //block_write(0, (char*)disk->super_block);

    return 0;
}