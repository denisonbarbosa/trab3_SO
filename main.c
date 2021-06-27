#include "block.h"
#include "fs.c"

int main() 
{
    fs_init();

    block_write(0, (char*)disk->super_block);

    return 0;
}