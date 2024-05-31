#include "Files.h"

int _read_indirect(Volume* vol, u_int32_t* indirect, char** data, u_int16_t* block_cnt, u_int32_t last_size, int start) {
    if (start < 0) {
        start = 0;
    }
    for (int i = start; i < BLOCK_ENTRIES && *block_cnt > 0; i++) {
        if (*block_cnt > 1) {
            if (read_data(vol, indirect[i], *data) < 0) {
                return -1;
            }
            *data += SIZE_BLOCK;
            (*block_cnt)--;
        } else {
            char buffer[SIZE_BLOCK];
            if (read_data(vol, indirect[i], buffer) < 0) {
                return -1;
            }
            memcpy(*data, buffer, last_size);
            *data += last_size;
            (*block_cnt) = 0;
        }
    }
    return 0;
}

int read_file(Volume* vol, FileType* file) {
    if (file->inodeptr == NULL) {
        print_err("Invalid inode");
        return -1;
    }
    file->inodeptr->i_atime = time(NULL);
    u_int32_t start_block = file->start_block;
    file->size = file->inodeptr->i_size;
    u_int16_t block_cnt = file->inodeptr->i_blocks - start_block;
    if (start_block != 0 && block_cnt <= 0) {
        print_err("Invalid start block");
        return -1;
    }
    file->data = (char*)malloc(file->size - start_block * SIZE_BLOCK);
    char* ptr = file->data;
    char buffer[SIZE_BLOCK + 10];
    u_int32_t last_size = file->inodeptr->i_size % SIZE_BLOCK;
    if (last_size == 0 && block_cnt > 0) {
        last_size = SIZE_BLOCK;
    }

    // Direct blocks
    for (int i = start_block; i < INODE_DIRECT && block_cnt > 0; i++) {
        if (block_cnt > 1) {
            if (read_data(vol, file->inodeptr->i_direct[i], ptr) < 0) {
                print_err("read_data");
                free(file->data);
                return -1;
            }
            ptr += SIZE_BLOCK;
            block_cnt--;
        } else {
            if (read_data(vol, file->inodeptr->i_direct[i], buffer) < 0) {
                print_err("read_data");
                free(file->data);
                return -1;
            }
            memcpy(ptr, buffer, last_size);
            ptr += last_size;
            block_cnt = 0;
        }
    }

    // Indirect block
    u_int32_t indirect[BLOCK_ENTRIES];
    if (block_cnt > 0 && start_block < INODE_INDIRECT) {
        if (read_data(vol, file->inodeptr->i_indirect, (char*)indirect) < 0) {
            print_err("read_data");
            free(file->data);
            return -1;
        }
        int start = start_block - INODE_DIRECT;
        if (_read_indirect(vol, indirect, &ptr, &block_cnt, last_size, start) < 0) {
            print_err("_read_indirect");
            free(file->data);
            return -1;
        }
    }

    // Double indirect block
    u_int32_t dindirect[BLOCK_ENTRIES];
    if (block_cnt > 0) {
        if (read_data(vol, file->inodeptr->i_dindirect, (char*)dindirect) < 0) {
            print_err("read_data");
            free(file->data);
            return -1;
        }
        int indirect_start = (start_block - INODE_INDIRECT) / BLOCK_ENTRIES;
        int start = (start_block - INODE_INDIRECT) % BLOCK_ENTRIES;
        if (start_block < INODE_INDIRECT) {
            indirect_start = 0;
            start = 0;
        }
        for (int i = indirect_start; i < BLOCK_ENTRIES && block_cnt > 0; i++) {
            if (read_data(vol, dindirect[i], (char*)indirect) < 0) {
                print_err("read_data");
                free(file->data);
                return -1;
            }
            if (_read_indirect(vol, indirect, &ptr, &block_cnt, last_size, start) < 0) {
                print_err("_read_indirect");
                free(file->data);
                return -1;
            }
            start = 0;
        }
    }
    // Update inode
    file->inodeptr->i_atime = time(NULL);
    if (write_inode(vol, file->inodeptr) < 0) {
        free(file->data);
        return -1;
    }
#ifdef _DEBUG
    printf("File read: %u bytes\n", file->size);
#endif
    return 0;
}

int _write_indirect(Volume* vol, u_int32_t* indirect, char** data, u_int32_t cnt, u_int32_t last_size, int start) {
    if (start < 0) {
        start = 0;
    }
    for (int i = start; i < BLOCK_ENTRIES && i < (int32_t)cnt; i++) {
        if (i < (int32_t)cnt - 1) {
            if (write_data(vol, indirect[i], *data) < 0) {
                return -1;
            }
            *data += SIZE_BLOCK;
        } else {
            char buffer[SIZE_BLOCK];
            memset(buffer, 0, SIZE_BLOCK);
            memcpy(buffer, *data, last_size);
            if (write_data(vol, indirect[i], buffer) < 0) {
                return -1;
            }
            *data += last_size;
        }
    }
    return 0;
}

int write_file(Volume* vol, FileType* file) {
    if (file->inodeptr == NULL) {
        print_err("Invalid inode");
        return -1;
    }
    char buffer[SIZE_BLOCK + 10];
    u_int32_t start_block = file->start_block;
    u_int16_t original_blocks = file->inodeptr->i_blocks;
    u_int16_t block_cnt = file->size / SIZE_BLOCK + (file->size % SIZE_BLOCK != 0);
    if (start_block != 0 && start_block >= block_cnt) {
        print_err("Invalid start block");
        return -1;
    }
    if (block_cnt >= INODE_DINDIRECT) {
        print_err("File too large");
        return -1;
    }
    u_int32_t indirect[BLOCK_ENTRIES];
    u_int32_t dindirect[BLOCK_ENTRIES];

    // Allocate blocks
    if (block_cnt > original_blocks) {
        if (_allocate_file_blocks(vol, file->inodeptr, block_cnt, indirect, dindirect) < 0) {
            print_err("_allocate_file_blocks");
            return -1;
        }
    }

    // Write data to blocks
    char* ptr = file->data;
    u_int32_t last_size = file->size % SIZE_BLOCK;
    if (last_size == 0 && block_cnt > 0) {
        last_size = SIZE_BLOCK;
    }

    // - Direct blocks
    for (int i = start_block; i < INODE_DIRECT && i < block_cnt; i++) {
        if (i < block_cnt - 1) {
            if (write_data(vol, file->inodeptr->i_direct[i], ptr) < 0) {
                print_err("write_data");
                return -1;
            }
            ptr += SIZE_BLOCK;
        } else {
            memset(buffer, 0, SIZE_BLOCK);
            memcpy(buffer, ptr, last_size);
            if (write_data(vol, file->inodeptr->i_direct[i], buffer) < 0) {
                return -1;
            }
        }
    }

    // - Indirect block
    if (block_cnt > INODE_DIRECT && start_block < INODE_INDIRECT) {
        if (read_data(vol, file->inodeptr->i_indirect, (char*)indirect) < 0) {
            print_err("Read indirect block");
            return -1;
        }
        int start = start_block - INODE_DIRECT;
        if (_write_indirect(vol, indirect, &ptr, block_cnt - INODE_DIRECT, last_size, start) < 0) {
            print_err("_write_indirect");
            return -1;
        }
    }

    // - Double indirect block
    if (block_cnt > INODE_INDIRECT) {
        if (read_data(vol, file->inodeptr->i_dindirect, (char*)dindirect) < 0) {
            print_err("Read double indirect block");
            return -1;
        }
        int start = (start_block - INODE_INDIRECT) % BLOCK_ENTRIES;
        int indirect_start = (start_block - INODE_INDIRECT) / BLOCK_ENTRIES;
        int indirect_blocks = (block_cnt - INODE_INDIRECT) / BLOCK_ENTRIES + ((block_cnt - INODE_INDIRECT) % BLOCK_ENTRIES != 0);
        if (start_block < INODE_INDIRECT) {
            start = 0;
            indirect_start = 0;
        }
        for (int i = indirect_start; i < BLOCK_ENTRIES && i < indirect_blocks; i++) {
            if (read_data(vol, dindirect[i], (char*)indirect) < 0) {
                print_err("Read indirect block");
                return -1;
            }
            int blocks_left = block_cnt - INODE_INDIRECT - i * BLOCK_ENTRIES;
            if (_write_indirect(vol, indirect, &ptr, blocks_left, last_size, start) < 0) {
                return -1;
            }
            start = 0;
        }
    }

    // Free blocks
    if (block_cnt < original_blocks) {
        if (_free_file_blocks(vol, file->inodeptr, block_cnt, indirect, dindirect) < 0) {
            return -1;
        }
    }

    if (block_cnt != original_blocks) {
        // Update superblock
        if (save_meta_blocks(vol) < 0) {
            return -1;
        }
    }

    // Update inode
    // @todo update timestamp
    file->inodeptr->i_blocks = block_cnt;
    file->inodeptr->i_size = file->size;
    file->inodeptr->i_mtime = time(NULL);
    file->inodeptr->i_atime = time(NULL);
    if (write_inode(vol, file->inodeptr) < 0) {
        return -1;
    }

#ifdef _DEBUG
    printf("File written: %u bytes\n", file->size);
#endif
    return 0;
}

int parse_dir(FileType* file, DirType* dir) {
    /**
     * Structure of a directory file:
     * [DirSize:2]
     * [DirEntry1]:
     *  |  [InodeMode:2]
     *  |  [InodeIdx:2]
     *  |  [FilenameLen:2]
     *  |- [Filename:FilenameLen]
     * [DirEntry2]...
     */
    if (file->inodeptr == NULL || file->data == NULL) {
        return -1;
    }
    u_int16_t dir_size = *(u_int16_t*)file->data;
    dir->inodeptr = file->inodeptr;
    dir->orig_data = file->data;
    dir->dir_size = dir_size;
    dir->dir_entries = (DirEntry*)(malloc(sizeof(DirEntry) * dir_size));
    DirEntry* entries = dir->dir_entries;
    char* ptr = file->data + DIR_ENTEY_OFFSET;
    u_int16_t tot_size = DIR_ENTEY_OFFSET;

    for (int i = 0; i < dir_size; i++) {
        entries[i].inode_mode = *(u_int16_t*)ptr;
        ptr += 2;
        entries[i].inode_idx = *(u_int16_t*)ptr;
        ptr += 2;
        entries[i].filename_len = *(u_int16_t*)ptr;
        ptr += 2;
        entries[i].filename = ptr;
        ptr += entries[i].filename_len;
        tot_size += 6 + entries[i].filename_len;
    }
    dir->tot_size = tot_size;
    return 0;
}

int init_dir(FileType* file, bool is_root) {
    if (file->inodeptr == NULL) {
        return -1;
    }
    u_int16_t inode_idx = file->inodeptr->i_idx;
    file->inodeptr->i_mode = INODE_DIR;
    file->size = is_root ? DIR_INIT_SIZE - 8 : DIR_INIT_SIZE;
    file->inodeptr->i_size = 0;
    file->inodeptr->i_blocks = 0;
    file->data = (char*)malloc(file->size);
    file->start_block = 0;
    *(u_int16_t*)file->data = is_root ? 1 : 2;
    *(u_int16_t*)(file->data + 2) = INODE_DIR;
    *(u_int16_t*)(file->data + 4) = inode_idx;
    *(u_int16_t*)(file->data + 6) = 1;
    file->data[8] = '.';
    if (!is_root) {
        *(u_int16_t*)(file->data + 9) = INODE_DIR;
        *(u_int16_t*)(file->data + 11) = file->inodeptr->i_parent;
        *(u_int16_t*)(file->data + 13) = 2;
        file->data[15] = '.';
        file->data[16] = '.';
    }
    return 0;
}

int encode_dir(DirType* dir, FileType* file) {
    if (dir->inodeptr == NULL || dir->dir_entries == NULL) {
        return -1;
    }
    u_int16_t dir_size = dir->dir_size;
    u_int16_t tot_size = dir->tot_size;
    file->inodeptr = dir->inodeptr;
    file->size = tot_size;
    file->data = (char*)malloc(tot_size);
    char* ptr = file->data;
    *(u_int16_t*)ptr = dir_size;
    ptr += 2;

    DirEntry* entries = dir->dir_entries;
    for (int i = 0; i < dir_size; i++) {
        *(u_int16_t*)ptr = entries[i].inode_mode;
        ptr += 2;
        *(u_int16_t*)ptr = entries[i].inode_idx;
        ptr += 2;
        *(u_int16_t*)ptr = entries[i].filename_len;
        ptr += 2;
        memcpy(ptr, entries[i].filename, entries[i].filename_len);
        ptr += entries[i].filename_len;
    }
    if (ptr - file->data != tot_size) {
        printf("%ld %d\n", ptr - file->data, tot_size);
        print_err("encode_dir: Invalid size");
        return -1;
    }
    return 0;
}

void free_dir(DirType* dir) {
    free(dir->dir_entries);
    free(dir->orig_data);
    dir->dir_entries = NULL;
    dir->orig_data = NULL;
}

void print_dir(DirType* dir) {
    if (dir->dir_entries == NULL) {
        return;
    }
    char buffer[256];
    DirEntry* entries = dir->dir_entries;
    printf("Directory:  Size: %u,\tTotal: %u\n", dir->dir_size, dir->tot_size);
    for (int i = 0; i < dir->dir_size; i++) {
        memcpy(buffer, entries[i].filename, entries[i].filename_len);
        printf("            Inode: %u,\t Mode: %u,\tLen: %u,\tName: %.*s\n", entries[i].inode_idx, entries[i].inode_mode, entries[i].filename_len, entries[i].filename_len, buffer);
    }
}