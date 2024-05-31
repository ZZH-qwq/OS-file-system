#include "FileSystem.h"
#include "BasicDisk.h"

int init_volume(Volume* vol) {
    // Load MetaBlocks
    vol->blockptr->super_block.s_blocks_count = 3;
    load_meta_blocks(vol);
    // Get the volume information
    u_int16_t cmd_info[2] = {CMD_I, 0};
    int n = write(vol->sockfd, (char*)cmd_info, 4);
    if (n < 0) {
        perror("write");
        exit(1);
    }
    char response[20];
    n = read(vol->sockfd, response, 20);
    if (n < 0) {
        perror("read");
        exit(1);
    }
    u_int16_t* info = (u_int16_t*)response;
#ifdef _DEBUG
    printf("Cylinders: %d, Sectors: %d\n", info[0], info[1]);
#endif
    u_int32_t tot_blocks = info[0] * info[1] * SECTOR_SIZE;
    if (vol->blockptr->super_block.s_magic != MAGIC_NUM) {
        printf("Disk not formatted. Please format the disk\n");
        vol->blockptr->super_block.s_total_size = tot_blocks;
        return -1;
    }
    if (vol->blockptr->super_block.s_total_size != tot_blocks) {
        printf("Disk size mismatch. Please format the disk\n");
        vol->blockptr->super_block.s_total_size = tot_blocks;
        return -1;
    }
    return 0;
}

int format_disk(Volume* vol) {
    // Initialize super block
    SuperBlock* sb = &vol->blockptr->super_block;
    sb->s_magic = MAGIC_NUM;
    sb->s_inodes_per_block = SIZE_BLOCK / SIZE_INODE;
    sb->s_block_size = SIZE_BLOCK;
    sb->s_inodes_count = sb->s_total_size / SIZE_BLOCK / 8 * sb->s_inodes_per_block;
    if (sb->s_inodes_count < 1) {
        print_err("Disk size too small");
        return -1;
    } else if (sb->s_inodes_count > INODE_COUNT) {
        sb->s_inodes_count = INODE_COUNT;
    }
    u_int32_t data_blocks = (sb->s_total_size - sizeof(MetaBlocks)) / SIZE_BLOCK - sb->s_inodes_count / sb->s_inodes_per_block;
    if (data_blocks < 1) {
        print_err("Disk size too small");
        return -1;
    } else if (data_blocks > BLOCK_COUNT) {
        data_blocks = BLOCK_COUNT;
    }
    sb->s_blocks_count = data_blocks;
    sb->s_free_blocks_count = data_blocks;
    sb->s_free_inodes_count = sb->s_inodes_count;
    sb->s_first_data_block = sizeof(MetaBlocks) / SIZE_BLOCK + sb->s_inodes_count / sb->s_inodes_per_block;
#ifdef _DEBUG
    printf("Inodes count: %d\n", sb->s_inodes_count);
    printf("Data blocks: %d\n", data_blocks);
    printf("First data block: %d\n", sb->s_first_data_block);
#endif

    // Initialize inode bitmap
    memset(vol->blockptr->inode_bitmap, 0, sizeof(vol->blockptr->inode_bitmap));

    // Initialize block bitmap
    memset(vol->blockptr->block_bitmap, 0, sizeof(vol->blockptr->block_bitmap));

    // Write metadata blocks to disk
    if (save_meta_blocks(vol) < 0) {
        print_err("Failed to save meta blocks");
        return -1;
    }
    return 0;
}

int load_meta_blocks(Volume* vol) {
    char buffer[METABLOCKS_SIZE * SIZE_BLOCK];
    for (int i = 0; i < METABLOCKS_SIZE; i++) {
        if (read_block(vol, i, buffer + i * SIZE_BLOCK) < 0) {
            return -1;
        }
    }
    memcpy(vol->blockptr, buffer, sizeof(MetaBlocks));
    return 0;
}

int save_meta_blocks(Volume* vol) {
    char buffer[METABLOCKS_SIZE * SIZE_BLOCK];
    memcpy(buffer, vol->blockptr, sizeof(MetaBlocks));
    for (int i = 0; i < METABLOCKS_SIZE; i++) {
        if (write_block(vol, i, buffer + i * SIZE_BLOCK) < 0) {
            return -1;
        }
    }
    return 0;
}

int allocate_block(Volume* vol) {
    SuperBlock* sb = &vol->blockptr->super_block;
    if (sb->s_free_blocks_count == 0) {
        return -1;
    }
    for (int i = 0; (u_int32_t)i < sb->s_blocks_count; i++) {
        if ((vol->blockptr->block_bitmap[i / BITMAP_WIDTH] & (1UL << (i % BITMAP_WIDTH))) == 0) {
            vol->blockptr->block_bitmap[i / BITMAP_WIDTH] |= (1UL << (i % BITMAP_WIDTH));
            sb->s_free_blocks_count--;
            return i;
        }
    }
    return -1;
}

int free_block(Volume* vol, int data_block) {
    SuperBlock* sb = &vol->blockptr->super_block;
    if (data_block < 0 || (u_int32_t)data_block >= sb->s_blocks_count) {
        return -1;
    }
    if ((vol->blockptr->block_bitmap[data_block / BITMAP_WIDTH] & (1UL << (data_block % BITMAP_WIDTH))) == 0) {
        return -1;
    }
    vol->blockptr->block_bitmap[data_block / BITMAP_WIDTH] &= ~(1UL << (data_block % BITMAP_WIDTH));
    sb->s_free_blocks_count++;
    return 0;
}

int read_block(Volume* vol, int disk_block, char* buffer) {
    SuperBlock* sb = &vol->blockptr->super_block;
    if (disk_block < 0 || (u_int32_t)disk_block >= sb->s_blocks_count + sb->s_first_data_block) {
        print_err("Invalid block index");
        printf("Disk block: %d\n", disk_block);
        return -1;
    }
    Command cmd;
    cmd.type = CMD_R;
    cmd.len = SIZE_BLOCK;
    cmd.block_id = disk_block;
    int n = write(vol->sockfd, &cmd, SIZE_CMD_BASIC);
    if (n < 0) {
        return -1;
    }
    n = read(vol->sockfd, buffer, SIZE_BLOCK);
    if (n < 200) {
        printf("Read block %d: %d\n", disk_block, n);
        return -1;
    }
    return 0;
}

int read_data(Volume* vol, int data_block, char* buffer) {
    if (data_block < 0) {
        print_err("Invalid data block index");
        return -1;
    }
    return read_block(vol, data_block + vol->blockptr->super_block.s_first_data_block, buffer);
}

int write_block(Volume* vol, int disk_block, char* buffer) {
    SuperBlock* sb = &vol->blockptr->super_block;
    if (disk_block < 0 || (u_int32_t)disk_block >= sb->s_blocks_count + sb->s_first_data_block) {
        print_err("Invalid block index");
        return -1;
    }
    Command cmd;
    cmd.type = CMD_W;
    cmd.len = SIZE_BLOCK;
    cmd.block_id = disk_block;
    memcpy(cmd.data, buffer, SIZE_BLOCK);
    int n = write(vol->sockfd, &cmd, sizeof(Command));
    if (n < 0) {
        return -1;
    }
    char response[SIZE_BLOCK];
    do {
        n = read(vol->sockfd, response, SIZE_BLOCK);
    } while (response[0] != 0 && n > 0);
    return 0;
}

int write_data(Volume* vol, int data_block, char* buffer) {
    if (data_block < 0) {
        print_err("Invalid data block index");
        return -1;
    }
    return write_block(vol, data_block + vol->blockptr->super_block.s_first_data_block, buffer);
}

int confirm_sync(Volume* vol) {
    // Send sync command
    Command cmd;
    cmd.type = CMD_S;
    cmd.len = 0;
    int n = write(vol->sockfd, &cmd, SIZE_CMD_BASIC);
    if (n < 0) {
        print_err("Sync failed");
        return -1;
    }
    // Wait for response
    char response[SIZE_BLOCK];
    do {
        printf("Syncing...\n");
        n = read(vol->sockfd, response, SIZE_BLOCK);
    } while (response[0] != 0 && n > 0);
    if (n < 0) {
        print_err("Sync failed");
        return -1;
    }
    return 0;
}

int print_vol_info(Volume* vol, char** data, int* len) {
    SuperBlock* sb = &vol->blockptr->super_block;
    char* buffer = (char*)malloc(SIZE_BLOCK + sb->s_inodes_count + sb->s_blocks_count);
    char* ptr = buffer;
    ptr += sprintf(ptr, "Volume information:\n");
    ptr += sprintf(ptr, "    Block size: %8d    Inode size: %8d\n", sb->s_block_size, SIZE_INODE);
    ptr += sprintf(ptr, "  Blocks count: %8d  Inodes count: %8d\n", sb->s_blocks_count, sb->s_inodes_count);
    ptr += sprintf(ptr, "   Free blocks: %8d   Free inodes: %8d\n", sb->s_free_blocks_count, sb->s_free_inodes_count);
    double used = (double)(sb->s_free_blocks_count * sb->s_block_size + sb->s_free_inodes_count * SIZE_INODE) / sb->s_total_size * 100;
    ptr += sprintf(ptr, "   Total space: %8d    Used space: %7.2f%%\n", sb->s_total_size, 100 - used);

    static const char* Braille_table[] = {
        "⠀", "⠁", "⠂", "⠃", "⠄", "⠅", "⠆", "⠇", "⡀", "⡁", "⡂", "⡃", "⡄", "⡅", "⡆", "⡇",
        "⠈", "⠉", "⠊", "⠋", "⠌", "⠍", "⠎", "⠏", "⡈", "⡉", "⡊", "⡋", "⡌", "⡍", "⡎", "⡏",
        "⠐", "⠑", "⠒", "⠓", "⠔", "⠕", "⠖", "⠗", "⡐", "⡑", "⡒", "⡓", "⡔", "⡕", "⡖", "⡗",
        "⠘", "⠙", "⠚", "⠛", "⠜", "⠝", "⠞", "⠟", "⡘", "⡙", "⡚", "⡛", "⡜", "⡝", "⡞", "⡟",

        "⠠", "⠡", "⠢", "⠣", "⠤", "⠥", "⠦", "⠧", "⡠", "⡡", "⡢", "⡣", "⡤", "⡥", "⡦", "⡧",
        "⠨", "⠩", "⠪", "⠫", "⠬", "⠭", "⠮", "⠯", "⡨", "⡩", "⡪", "⡫", "⡬", "⡭", "⡮", "⡯",
        "⠰", "⠱", "⠲", "⠳", "⠴", "⠵", "⠶", "⠷", "⡰", "⡱", "⡲", "⡳", "⡴", "⡵", "⡶", "⡷",
        "⠸", "⠹", "⠺", "⠻", "⠼", "⠽", "⠾", "⠿", "⡸", "⡹", "⡺", "⡻", "⡼", "⡽", "⡾", "⡿",

        "⢀", "⢁", "⢂", "⢃", "⢄", "⢅", "⢆", "⢇", "⣀", "⣁", "⣂", "⣃", "⣄", "⣅", "⣆", "⣇",
        "⢈", "⢉", "⢊", "⢋", "⢌", "⢍", "⢎", "⢏", "⣈", "⣉", "⣊", "⣋", "⣌", "⣍", "⣎", "⣏",
        "⢐", "⢑", "⢒", "⢓", "⢔", "⢕", "⢖", "⢗", "⣐", "⣑", "⣒", "⣓", "⣔", "⣕", "⣖", "⣗",
        "⢘", "⢙", "⢚", "⢛", "⢜", "⢝", "⢞", "⢟", "⣘", "⣙", "⣚", "⣛", "⣜", "⣝", "⣞", "⣟",

        "⢠", "⢡", "⢢", "⢣", "⢤", "⢥", "⢦", "⢧", "⣠", "⣡", "⣢", "⣣", "⣤", "⣥", "⣦", "⣧",
        "⢨", "⢩", "⢪", "⢫", "⢬", "⢭", "⢮", "⢯", "⣨", "⣩", "⣪", "⣫", "⣬", "⣭", "⣮", "⣯",
        "⢰", "⢱", "⢲", "⢳", "⢴", "⢵", "⢶", "⢷", "⣰", "⣱", "⣲", "⣳", "⣴", "⣵", "⣶", "⣷",
        "⢸", "⢹", "⢺", "⢻", "⢼", "⢽", "⢾", "⢿", "⣸", "⣹", "⣺", "⣻", "⣼", "⣽", "⣾", "⣿"};

    // Print inode bitmap
    ptr += sprintf(ptr, "\nInode bitmap:");
    int ibytes = (sb->s_inodes_count + 7) / 8;
    u_int8_t* ibitmap = (u_int8_t*)vol->blockptr->inode_bitmap;
    for (int i = 0; i < ibytes; i++) {
        if (i % 32 == 0 && i != ibytes - 1) {
            ptr += sprintf(ptr, "\n0x%04x - 0x%04x: ", i * 8, (i + 32) * 8 - 1);
        }
        ptr += sprintf(ptr, "%s", Braille_table[ibitmap[i]]);
    }
    ptr += sprintf(ptr, "\n");

    // Print block bitmap
    ptr += sprintf(ptr, "\nBlock bitmap:");
    int bbytes = (sb->s_blocks_count + 7) / 8;
    u_int8_t* bbitmap = (u_int8_t*)vol->blockptr->block_bitmap;
    for (int i = 0; i < bbytes; i++) {
        if (i % 32 == 0 && i != bbytes - 1) {
            ptr += sprintf(ptr, "\n0x%04x - 0x%04x: ", i * 8, (i + 32) * 8 - 1);
        }
        ptr += sprintf(ptr, "%s", Braille_table[bbitmap[i]]);
    }
    ptr += sprintf(ptr, "\n");

    if (data != NULL) {
        *data = buffer;
        *len = ptr - buffer;
    } else {
        free(buffer);
    }
    return 0;
}

void print_err(char* msg) {
    fprintf(stderr, "Error: %s\n", msg);
}