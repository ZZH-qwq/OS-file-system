/**
 * Files.h
 *
 * Definition of FileType and DirType struct,
 * and functions of file operations
 */

#ifndef FILES_H
#define FILES_H

#include "Inode.h"

#define DIR_ENTEY_OFFSET 2
#define DIR_INIT_SIZE 17  // 2 + (2 * 3 + 1) + (2 * 3 + 2)

typedef struct FileType {
    Inode* inodeptr;
    char* data;
    u_int32_t size;
    u_int32_t start_block;
} FileType;

typedef struct DirEntry {
    u_int16_t inode_mode;
    u_int16_t inode_idx;
    u_int16_t filename_len;
    char* filename;
} DirEntry;

typedef struct DirType {
    Inode* inodeptr;
    char* orig_data;
    DirEntry* dir_entries;
    u_int16_t dir_size;
    u_int16_t tot_size;
} DirType;

/**
 * @brief Read a file from disk
 * @param vol Volume struct: the formatted disk
 * @param file FileType struct: must contain valid inodeptr
 * @return int 0 if success, -1 if failed. file->data will be allocated
 */
int read_file(Volume* vol, FileType* file);

/**
 * @brief Write a file to disk
 * @param vol Volume struct: the formatted disk
 * @param file FileType struct: must contain valid inodeptr and data. inodeptr contains old attributes
 * @return int 0 if success, -1 if failed. content of inode will be updated, and will allocate or free blocks if needed
 */
int write_file(Volume* vol, FileType* file);

/**
 * @brief Parse a directory from a file
 * @param file FileType struct: the original file to be parsed
 * @return int 0 if success, -1 if failed. dir will be updated & allocated
 */
int parse_dir(FileType* file, DirType* dir);

/**
 * @brief Initialize a directory
 * @param file FileType struct: the target file to be initialized, must contain valid inodeptr
 * @param is_root bool: true if root directory
 * @return int 0 if success, -1 if failed
 */
int init_dir(FileType* file, bool is_root);

/**
 * @brief Encode a directory to a file
 * @param dir DirType struct: must contain valid inodeptr and dir_entries
 * @param file FileType struct: target file to be encoded
 * @return int 0 if success, -1 if failed
 */
int encode_dir(DirType* dir, FileType* file);

/**
 * @brief Free a directory
 * @param dir DirType struct: the directory to be freed
 */
void free_dir(DirType* dir);

/**
 * @brief Print a directory
 * @param dir DirType struct: the directory to be printed
 */
void print_dir(DirType* dir);

#endif