/**
 * FileSystem.h
 *
 * Basic definitions and functions for the file system
 * including SuperBlock, MetaBlocks, Volume
 */

#ifndef FILESYSTEM_H
#define FILESYSTEM_H

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <time.h>
#include <sys/types.h>

#define METABLOCKS_SIZE 3
#define INODE_BITMAP_SIZE 32
#define BLOCK_BITMAP_SIZE 128
#define BITMAP_WIDTH 32

#define INODE_COUNT 1024
#define BLOCK_COUNT 4096

#define SIZE_INODE 64
#define SIZE_BLOCK 256

#define MAGIC_NUM 0x2333

typedef struct SuperBlock {
    u_int16_t s_magic;
    u_int16_t s_inodes_per_block;
    u_int32_t s_block_size;
    u_int32_t s_blocks_count;
    u_int32_t s_inodes_count;
    u_int32_t s_free_blocks_count;
    u_int32_t s_free_inodes_count;
    u_int32_t s_first_data_block;
    u_int32_t s_total_size;
} SuperBlock;  // Total = 32 bytes

typedef struct MetaBlocks {
    SuperBlock super_block;
    u_int32_t pad[(128 - sizeof(SuperBlock)) / sizeof(u_int32_t)];
    u_int32_t inode_bitmap[INODE_BITMAP_SIZE];
    u_int32_t block_bitmap[BLOCK_BITMAP_SIZE];
} MetaBlocks;  // Blk0[ superblock: 32 | padding: 96 | inode_bitmap: 128 ] Blk1-2[ block_bitmap:512 ]

typedef struct Volume {
    MetaBlocks* blockptr;
    int sockfd;
} Volume;

/**
 * @brief Initialize the volume
 * @param vol Volume struct: must contain valid sockfd
 * @return int 0 if valid, -1 if need to format
 */
int init_volume(Volume* vol);

/**
 * @brief Format the disk by creating metadata blocks
 * @param vol Volume struct: must contain valid sockfd and blockptr->s_total_size
 * @return int 0 if success, -1 if failed. vol->blockptr will be updated
 */
int format_disk(Volume* vol);

/**
 * @brief Load metadata blocks from disk
 * @param vol Volume struct: must contain valid sockfd
 * @return int 0 if success, -1 if failed. vol->blockptr will be updated
 */
int load_meta_blocks(Volume* vol);

/**
 * @brief Save metadata blocks to disk
 * @param vol Volume struct: must be a valid disk
 * @return int 0 if success, -1 if failed
 */
int save_meta_blocks(Volume* vol);

/**
 * @brief Allocate a block
 * @return int >=0 block index, -1 if failed
 */
int allocate_block(Volume* vol);

/**
 * @brief Free a block
 * @param vol Volume struct: a valid disk
 * @param data_block int: block idx of data block
 * @return int 0 if success, -1 if failed
 */
int free_block(Volume* vol, int data_block);

/**
 * @brief Read a block from disk
 * @param vol Volume struct: a valid disk
 * @param disk_block int: block idx of disk (not data block idx)
 * @param buffer char*: buffer to store data
 * @return int 0 if success, -1 if failed. buffer will be updated
 */
int read_block(Volume* vol, int disk_block, char* buffer);

/**
 * @brief Read a data block from disk
 * @param vol Volume struct: a valid disk
 * @param data_block int: block idx of data block
 * @param buffer char*: buffer to store data
 * @return int 0 if success, -1 if failed. buffer will be updated
 */
int read_data(Volume* vol, int data_block, char* buffer);

/**
 * @brief Write a block to disk
 * @param vol Volume struct: a valid disk
 * @param disk_block int: block idx of disk (not data block idx)
 * @param buffer char*: data to write
 * @return int 0 if success, -1 if failed
 */
int write_block(Volume* vol, int disk_block, char* buffer);

/**
 * @brief Write a data block to disk
 * @param vol Volume struct: a valid disk
 * @param data_block int: block idx of data block
 * @param buffer char*: data to write
 * @return int 0 if success, -1 if failed
 */
int write_data(Volume* vol, int data_block, char* buffer);

/**
 * @brief Confirm operation and sync to disk
 * @param vol Volume struct: a valid disk
 * @return int 0 if success, -1 if failed
 */
int confirm_sync(Volume* vol);

/**
 * @brief Print volume information
 * @param vol Volume struct: the volume to be printed
 * @param data char**: the data to be printed
 * @param len int*: the length of data
 * @return int 0 if success, -1 if failed. data will be allocated
 */
int print_vol_info(Volume* vol, char** data, int* len);

/**
 * @brief Print error message
 * @param msg char*: error message
 */
void print_err(char* msg);

#endif