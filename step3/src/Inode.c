#include "Inode.h"

int allocate_inode(Volume* vol) {
    SuperBlock* sb = &vol->blockptr->super_block;
    if (sb->s_free_inodes_count == 0) {
        return -1;
    }
    for (int i = 0; (u_int32_t)i < sb->s_inodes_count; i++) {
        if ((vol->blockptr->inode_bitmap[i / BITMAP_WIDTH] & (1UL << (i % BITMAP_WIDTH))) == 0) {
            vol->blockptr->inode_bitmap[i / BITMAP_WIDTH] |= (1UL << (i % BITMAP_WIDTH));
            sb->s_free_inodes_count--;
            return i;
        }
    }
    return -1;
}

int free_inode(Volume* vol, int inode_idx) {
    SuperBlock* sb = &vol->blockptr->super_block;
    if (inode_idx < 0 || (u_int32_t)inode_idx >= sb->s_inodes_count) {
        return -1;
    }
    if ((vol->blockptr->inode_bitmap[inode_idx / BITMAP_WIDTH] & (1UL << (inode_idx % BITMAP_WIDTH))) == 0) {
        return -1;
    }
    vol->blockptr->inode_bitmap[inode_idx / BITMAP_WIDTH] &= ~(1UL << (inode_idx % BITMAP_WIDTH));
    sb->s_free_inodes_count++;
    return 0;
}

int read_inode(Volume* vol, Inode* inode) {
    SuperBlock* sb = &vol->blockptr->super_block;
    u_int16_t inode_idx = inode->i_idx;
    if (inode_idx >= sb->s_inodes_count) {
        return -1;
    }
    int inode_blk = METABLOCKS_SIZE + inode_idx / sb->s_inodes_per_block;
    int inode_off = inode_idx % sb->s_inodes_per_block;
    char buffer[SIZE_BLOCK];
    if (read_block(vol, inode_blk, buffer) < 0) {
        return -1;
    }
    memcpy(inode, buffer + inode_off * sizeof(Inode), sizeof(Inode));
    return 0;
}

int write_inode(Volume* vol, Inode* inode) {
    SuperBlock* sb = &vol->blockptr->super_block;
    u_int16_t inode_idx = inode->i_idx;
    if (inode_idx >= sb->s_inodes_count) {
        return -1;
    }
    int inode_blk = METABLOCKS_SIZE + inode_idx / sb->s_inodes_per_block;
    int inode_off = inode_idx % sb->s_inodes_per_block;
    char buffer[SIZE_BLOCK];
    if (read_block(vol, inode_blk, buffer) < 0) {
        return -1;
    }
    memcpy(buffer + inode_off * sizeof(Inode), inode, sizeof(Inode));
    if (write_block(vol, inode_blk, buffer) < 0) {
        return -1;
    }
    return 0;
}

int _allocate_file_blocks(Volume* vol, Inode* inodeptr, u_int32_t block_cnt, u_int32_t* indirect, u_int32_t* dindirect) {
    bool read_indirect = true;
    bool read_dindirect = true;
    for (u_int32_t i = inodeptr->i_blocks; i < block_cnt; i++) {
        int data_block = allocate_block(vol);
#ifdef _DEBUG
        printf("Allocated data block: %d\n", data_block);
#endif
        if (data_block < 0) {
            return -1;
        }
        if (i < INODE_DIRECT) {
            // Direct blocks
            inodeptr->i_direct[i] = data_block;
        } else if (i < INODE_INDIRECT) {
            // Single indirect block

            // - Step1: Allocate or read indirect block
            if (i == INODE_DIRECT) {
                // Allocate indirect block
                int res = allocate_block(vol);
#ifdef _DEBUG
                printf("Allocated indirect block: %d\n", res);
#endif
                if (res < 0) {
                    return -1;
                }
                inodeptr->i_indirect = res;
                memset(indirect, 0, SIZE_BLOCK);
                read_indirect = false;
            } else if (read_indirect) {
                if (read_data(vol, inodeptr->i_indirect, (char*)indirect) < 0) {
                    return -1;
                }
                read_indirect = false;
            }

            // - Step2: Write data block to indirect block
            indirect[i - INODE_DIRECT] = data_block;

            // - Step3: Write indirect block to disk
            if (i == block_cnt - 1 || i == INODE_INDIRECT - 1) {
                if (write_data(vol, inodeptr->i_indirect, (char*)indirect) < 0) {
                    return -1;
                }
                read_indirect = true;
            }
        } else {
            // Double indirect block

            // - Step1: Allocate or read double indirect block
            if (i == INODE_INDIRECT) {
                // Allocate double indirect block
                int res = allocate_block(vol);
#ifdef _DEBUG
                printf("Allocated double indirect block: %d\n", res);
#endif
                if (res < 0) {
                    return -1;
                }
                inodeptr->i_dindirect = res;
                memset(dindirect, 0, SIZE_BLOCK);
                read_dindirect = false;
            } else if (read_dindirect) {
                if (read_data(vol, inodeptr->i_dindirect, (char*)dindirect) < 0) {
                    return -1;
                }
                read_dindirect = false;
            }

            // - Step2: Allocate or read indirect block
            if ((i - INODE_INDIRECT) % BLOCK_ENTRIES == 0) {
                // Allocate indirect block
                int res = allocate_block(vol);
#ifdef _DEBUG
                printf("Allocated indirect block: %d\n", res);
#endif
                if (res < 0) {
                    return -1;
                }
                dindirect[(i - INODE_INDIRECT) / BLOCK_ENTRIES] = res;
                memset(indirect, 0, SIZE_BLOCK);
                read_indirect = false;
            } else if (read_indirect) {
                if (read_data(vol, dindirect[(i - INODE_INDIRECT) / BLOCK_ENTRIES], (char*)indirect) < 0) {
                    return -1;
                }
                read_indirect = false;
            }

            // - Step3: Write data block to indirect block
            indirect[(i - INODE_INDIRECT) % BLOCK_ENTRIES] = data_block;

            // - Step4: Write indirect block to disk
            if (i == block_cnt - 1 || (i - INODE_INDIRECT) % BLOCK_ENTRIES == BLOCK_ENTRIES - 1 || i == INODE_DINDIRECT - 1) {
                if (write_data(vol, dindirect[(i - INODE_INDIRECT) / BLOCK_ENTRIES], (char*)indirect) < 0) {
                    return -1;
                }
                read_indirect = true;
            }

            // - Step5: Write double indirect block to disk
            if (i == block_cnt - 1 || i == INODE_DINDIRECT - 1) {
                if (write_data(vol, inodeptr->i_dindirect, (char*)dindirect) < 0) {
                    return -1;
                }
                read_dindirect = true;
            }
        }
    }
    return 0;
}

int _free_file_blocks(Volume* vol, Inode* inodeptr, u_int32_t block_cnt, u_int32_t* indirect, u_int32_t* dindirect) {
    bool read_indirect = true;
    bool read_dindirect = true;
    for (int i = inodeptr->i_blocks - 1; i >= (int32_t)block_cnt; i--) {
        if (i >= INODE_INDIRECT) {
            // Double indirect block

            // - Step1: Read double indirect block
            if (read_dindirect) {
                if (read_data(vol, inodeptr->i_dindirect, (char*)dindirect) < 0) {
                    return -1;
                }
                read_dindirect = false;
            }

            // - Step2: Read indirect block
            if (read_indirect) {
                if (read_data(vol, dindirect[(i - INODE_INDIRECT) / BLOCK_ENTRIES], (char*)indirect) < 0) {
                    return -1;
                }
                read_indirect = false;
            }

            // - Step3: Free the block
#ifdef _DEBUG
            printf("Free data block: %d\n", indirect[(i - INODE_INDIRECT) % BLOCK_ENTRIES]);
#endif
            if (free_block(vol, indirect[(i - INODE_INDIRECT) % BLOCK_ENTRIES]) < 0) {
                return -1;
            }
            indirect[(i - INODE_INDIRECT) % BLOCK_ENTRIES] = 0;

            // - Step4: Free or write indirect block to the disk
            if (i == (int32_t)block_cnt || (i - INODE_INDIRECT) % BLOCK_ENTRIES == 0) {
                if ((i - INODE_INDIRECT) % BLOCK_ENTRIES == 0) {
                    // Free the indirect block
#ifdef _DEBUG
                    printf("Free indirect block: %d\n", dindirect[(i - INODE_INDIRECT) / BLOCK_ENTRIES]);
#endif
                    if (free_block(vol, dindirect[(i - INODE_INDIRECT) / BLOCK_ENTRIES]) < 0) {
                        return -1;
                    }
                    dindirect[(i - INODE_INDIRECT) / BLOCK_ENTRIES] = 0;
                } else {
                    if (write_data(vol, dindirect[(i - INODE_INDIRECT) / BLOCK_ENTRIES], (char*)indirect) < 0) {
                        return -1;
                    }
                }
                read_indirect = true;
            }

            // - Step5: Free or write double indirect block to the disk
            if (i == (int32_t)block_cnt || i == INODE_INDIRECT) {
                if (i == INODE_INDIRECT) {
                    // Free the double indirect block
#ifdef _DEBUG
                    printf("Free double indirect block: %d\n", inodeptr->i_dindirect);
#endif
                    if (free_block(vol, inodeptr->i_dindirect) < 0) {
                        return -1;
                    }
                    inodeptr->i_dindirect = 0;
                } else {
                    if (write_data(vol, inodeptr->i_dindirect, (char*)dindirect) < 0) {
                        return -1;
                    }
                }
                read_dindirect = true;
            }
        } else if (i >= INODE_DIRECT) {
            // Single indirect block

            // - Step1: Read indirect block
            if (read_indirect) {
                if (read_data(vol, inodeptr->i_indirect, (char*)indirect) < 0) {
                    return -1;
                }
                read_indirect = false;
            }

            // - Step2: Free the block
#ifdef _DEBUG
            printf("Free data block: %d\n", indirect[i - INODE_DIRECT]);
#endif
            if (free_block(vol, indirect[i - INODE_DIRECT]) < 0) {
                return -1;
            }
            indirect[i - INODE_DIRECT] = 0;

            // - Step3: Free or write indirect block to the disk
            if (i == (int32_t)block_cnt || i == INODE_DIRECT) {
                if (i == INODE_DIRECT) {
                    // Free the indirect block
#ifdef _DEBUG
                    printf("Free indirect block: %d\n", inodeptr->i_indirect);
#endif
                    if (free_block(vol, inodeptr->i_indirect) < 0) {
                        return -1;
                    }
                    inodeptr->i_indirect = 0;
                } else {
                    if (write_data(vol, inodeptr->i_indirect, (char*)indirect) < 0) {
                        return -1;
                    }
                }
                read_indirect = true;
            }
        } else {
            // Direct blocks
#ifdef _DEBUG
            printf("Free data block: %d\n", inodeptr->i_direct[i]);
#endif
            free_block(vol, inodeptr->i_direct[i]);
            inodeptr->i_direct[i] = 0;
        }
    }
    return 0;
}

char* _print_indirect(u_int32_t* indirect, int size, char* ptr, int depth) {
    for (int i = 0; i < depth; i++) {
        ptr += sprintf(ptr, "        ");
    }
    for (int i = 0; i < size && i < BLOCK_ENTRIES; i++) {
        ptr += sprintf(ptr, "%8u", indirect[i]);
        if (i % 8 == 7 && i != size - 1 && i != BLOCK_ENTRIES - 1) {
            ptr += sprintf(ptr, "\n");
            for (int j = 0; j < depth; j++) {
                ptr += sprintf(ptr, "        ");
            }
        }
    }
    ptr += sprintf(ptr, "\n");
    return ptr;
}

void print_inode(Volume* vol, Inode* inode, char** data, int* len) {
    if (inode == NULL) {
        return;
    }
    char* buffer = (char*)malloc(inode->i_blocks * 12 + SIZE_BLOCK);
    char* ptr = buffer;
    // Basic information
    char* mode = inode->i_mode == INODE_FILE ? "  File" : "Directory";
    ptr += sprintf(ptr, "  Size: %-16uBlocks: %-16u%s\n", inode->i_size, inode->i_blocks, mode);
    ptr += sprintf(ptr, " Inode: %-16uParent: %-16u Links: %u\n", inode->i_idx, inode->i_parent, inode->i_nlink);
    time_t i_atime = inode->i_atime, i_mtime = inode->i_mtime, i_ctime = inode->i_ctime;
    char* catime = ctime(&i_atime);
    catime[strlen(catime) - 1] = '\0';
    ptr += sprintf(ptr, "Access: %s\n", catime);
    char* cmtime = ctime(&i_mtime);
    cmtime[strlen(cmtime) - 1] = '\0';
    ptr += sprintf(ptr, "Modify: %s\n", cmtime);
    char* cctime = ctime(&i_ctime);
    cctime[strlen(cctime) - 1] = '\0';
    ptr += sprintf(ptr, "Create: %s\n\n", cctime);

    // Direct blocks
    ptr += sprintf(ptr, "Direct blocks:\t");
    for (int i = 0; i < INODE_DIRECT && i < inode->i_blocks; i++) {
        ptr += sprintf(ptr, "%8u", inode->i_direct[i]);
    }
    ptr += sprintf(ptr, "\n");

    // Indirect blocks
    if (inode->i_blocks > INODE_DIRECT) {
        ptr += sprintf(ptr, "Indirect blocks:\t%d\n", inode->i_indirect);
        u_int32_t indirect[BLOCK_ENTRIES];
        if (read_data(vol, inode->i_indirect, (char*)indirect) < 0) {
            free(buffer);
            return;
        }
        ptr = _print_indirect(indirect, inode->i_blocks - INODE_DIRECT, ptr, 1);
    }

    // Double indirect blocks
    if (inode->i_blocks > INODE_INDIRECT) {
        ptr += sprintf(ptr, "Double indirect:\t%d\n", inode->i_dindirect);
        u_int32_t dindirect[BLOCK_ENTRIES];
        if (read_data(vol, inode->i_dindirect, (char*)dindirect) < 0) {
            free(buffer);
            return;
        }
        int indirect_blocks = (inode->i_blocks - INODE_INDIRECT) / BLOCK_ENTRIES + ((inode->i_blocks - INODE_INDIRECT) % BLOCK_ENTRIES != 0);
        for (int i = 0; i < BLOCK_ENTRIES && i < indirect_blocks; i++) {
            if (dindirect[i] != 0) {
                ptr += sprintf(ptr, "        Indirect blocks:\t%d\n", dindirect[i]);
                u_int32_t indirect[BLOCK_ENTRIES];
                if (read_data(vol, dindirect[i], (char*)indirect) < 0) {
                    free(buffer);
                    return;
                }
                ptr = _print_indirect(indirect, inode->i_blocks - INODE_INDIRECT - i * BLOCK_ENTRIES, ptr, 2);
            }
        }
    }

    // Print buffer
    if (data == NULL) {
        printf("%s", buffer);
        free(buffer);
    } else {
        *data = buffer;
        *len = ptr - buffer;
    }
}