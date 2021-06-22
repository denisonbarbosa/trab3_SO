#ifndef FS_INCLUDED
#define FS_INCLUDED

#define FS_SIZE 2048

void fs_init( void);

/**
 * @brief Disk recognition mecanism 
 * 1  - Format the disk (brute or previoulsy formatted)
 * 2 - Mount on root directory
 * 3 - disk size == FS_SIZE in fs.h
 * 4 - Put magic number on super block to recognize as formatted disk
 * 
 * @return int 0 if successful, -1 if failed
 */
int fs_mkfs( void);

/**
 * @brief Returns file descriptor for usage in system calls.
 *
 * @param fileName Name of the file to be opened
 * @param flags Acess mode of the file (RDONLY = 1, WRONLY = 2, RDWR = 3)
 *
 * @return int file descriptor if successful || int -1 if failed
 * @note 
 * fails if: 
 *  - File does not exist and RDONLY
 *  - Trying to open directory with other than RDONLY
 */
int fs_open( char *fileName, int flags);

/**
 * @brief Closes a file descriptor
 * 
 * @param fd File descriptor to be closed
 * @return int 0 if successful || int -1 if failed
 * 
 * @note If fd was the last reference to a previously unlinked file,
 *       file should be deleted.
 */
int fs_close( int fd);

/**
 * @brief Reads {count} bytes of the file descriptor.
 * 
 * @param fd File descriptor
 * @param buf Buffer to be read
 * @param count Number of bytes to read
 * @return int number of bytes read successfully || -1 if failed
 * 
 * @note Buffer needs to move accordingly 
 */
int fs_read( int fd, char *buf, int count);

/**
 * @brief Writes {count} bytes of buffer into file
 * 
 * @param fd File descriptor
 * @param buf Buffer to be written
 * @param count Number of bytes to write
 * @return int number of bytes written || -1 if failed
 * 
 * @note Buffer needs to move accordingly
 * @note Error if trying to write beyond max file size
 * @note Error if count > 0, but no byte was written
 */
int fs_write( int fd, char *buf, int count);

/**
 * @brief 
 * 
 * @param fd 
 * @param offset 
 * @return int 
 */
int fs_lseek( int fd, int offset);
int fs_mkdir( char *fileName);
int fs_rmdir( char *fileName);
int fs_cd( char *dirName);
int fs_link( char *old_fileName, char *new_fileName);
int fs_unlink( char *fileName);
int fs_stat( char *fileName, fileStat *buf);

#define MAX_FILE_NAME 32
#define MAX_PATH_NAME 256  // This is the maximum supported "full" path len, eg: /foo/bar/test.txt, rather than the maximum individual filename len.
#endif
