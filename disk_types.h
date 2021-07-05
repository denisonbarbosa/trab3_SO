#define FORMATTED_DISK 1
#define BLOCK 512
#define N_INODES 2048
#define N_DATA_BLOCKS 1787
#define MAX_FILE_HANDLERS 255

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

typedef struct links_block_s
{
    int links[128];
} links_block_t;

typedef struct block_entry_s
{
    int self_inode;
    int parent_inode;
    int self_block;
    int parent_block;
    char *name;
} block_entry_t;

typedef struct opened_file_s
{
    block_entry_t *file;
    int cursor;
    int flag;
} opened_file_t;