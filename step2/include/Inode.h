/**
 * Inode.h
 *
 * Inode struct and functions of inode operations
 */

#ifndef INODE_H
#define INODE_H

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <stdbool.h>

#include "FileSystem.h"

#define BLOCK_ENTRIES 64
#define INODE_DIRECT 7
#define INODE_INDIRECT 71     // 7 + 256 / 4
#define INODE_DINDIRECT 4167  // 7 + 256 / 4 + 256 / 4 * 256 / 4

#define INODE_UNUSED 0
#define INODE_FILE 1
#define INODE_DIR 2

typedef struct Inode {
    u_int8_t i_mode;        // File mode, 0 = unused, 1 = file, 2 = directory
    u_int8_t i_nlink;       // Links count
    u_int16_t i_uid;        // User ID
    u_int16_t i_prem;       // Permissions
    u_int16_t i_parent;     // Parent inode
    u_int16_t i_blocks;     // Blocks count
    u_int16_t i_idx;        // Inode index
    u_int32_t i_size;       // File size
    u_int32_t i_atime;      // Access time
    u_int32_t i_mtime;      // Modification time
    u_int32_t i_ctime;      // Creation time
    u_int32_t i_direct[7];  // Pointers to data blocks
    u_int32_t i_indirect;   // Pointer to indirect block
    u_int32_t i_dindirect;  // Pointer to double indirect block
} Inode;                    // Total = 64 bytes

/**
 * @brief Allocate an inode
 * @return int >=0 inode index, -1 if failed
 */
int allocate_inode(Volume* vol);

/**
 * @brief Free an inode
 * @return int 0 if success, -1 if failed
 */
int free_inode(Volume* vol, int inode_idx);

/**
 * @brief Read an inode from disk
 * @param vol Volume struct: the formatted disk
 * @param inode Inode struct: must contain valid i_idx
 */
int read_inode(Volume* vol, Inode* inode);

/**
 * @brief Write an inode to disk
 * @param vol Volume struct: the formatted disk
 * @param inode Inode struct: the inode to be written
 */
int write_inode(Volume* vol, Inode* inode);

/**
 * @brief Helper function to allocate blocks for a file
 * @param vol Volume struct: the formatted disk
 * @param inodeptr Inode struct: the inode of the file, will be updated
 * @param block_cnt u_int32_t: total block count of the file
 * @param indirect u_int32_t*: indirect block entry buffer
 * @param dindirect u_int32_t*: double indirect block entry buffer
 * @return int 0 if success, -1 if failed
 */
int _allocate_file_blocks(Volume* vol, Inode* inodeptr, u_int32_t block_cnt, u_int32_t* indirect, u_int32_t* dindirect);

/**
 * @brief Helper function to free blocks of a file
 * @param vol Volume struct: the formatted disk
 * @param inodeptr Inode struct: the inode of the file, will be updated
 * @param block_cnt u_int32_t: total block count of the file
 * @param indirect u_int32_t*: indirect block entry buffer
 * @param dindirect u_int32_t*: double indirect block entry buffer
 * @return int 0 if success, -1 if failed
 */
int _free_file_blocks(Volume* vol, Inode* inodeptr, u_int32_t block_cnt, u_int32_t* indirect, u_int32_t* dindirect);

/**
 * @brief Print inode information
 * @param vol Volume struct: the formatted disk, contains the socket file descriptor
 * @param inode Inode struct: the inode to be printed
 */
void print_inode(Volume* vol, Inode* inode, char** data, int* len);

#endif