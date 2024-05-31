#include "BasicDisk.h"

// Create the diskfile
void init_disk(char *filename, BasicDisk *disk) {
    int fd = open(filename, O_RDWR | O_CREAT, 0666);
    if (fd < 0) {
        perror("open");
        exit(-1);
    }

    long disk_size = disk->n_cylinders * disk->n_sectors * SECTOR_SIZE;

    // Stretch the file to the size of the disk
    int res = lseek(fd, disk_size - 1, SEEK_SET);
    if (res < 0) {
        perror("Error calling lseek() to stretch the file");
        close(fd);
        exit(-1);
    }

    // Check if the diskfile have free space
    res = write(fd, "", 1);
    if (res != 1) {
        perror("Error writing last byte of the file");
        close(fd);
        exit(-1);
    }

    // Use mmap to map the diskfile to memory
    char *disk_map = mmap(NULL, disk_size, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
    if (disk_map == MAP_FAILED) {
        perror("Error mapping the diskfile to memory");
        close(fd);
        exit(-1);
    }

    disk->fd = fd;
    disk->diskfile = disk_map;

    close(fd);
}

// Update track and delay
void update_track(BasicDisk *disk, int cylinder) {
    int track_diff = abs(disk->current_track - cylinder);
    disk->current_track = cylinder;
    usleep(track_diff * disk->track_to_track_delay);
}

// Write to the socket of the disk
void write_to_socket(BasicDisk *disk, char *response, size_t len) {
    int nbytes = write(disk->socket_fd, response, len);
    if (nbytes < 0) {
        perror("Error writing to socket");
    }
}

/*
 * Command: I
 * Information request
 */
void info(BasicDisk *disk) {
    static u_int16_t response[2];
    response[0] = disk->n_cylinders;
    response[1] = disk->n_sectors;
    fprintf(stdout, "%d %d\n", disk->n_cylinders, disk->n_sectors);
    write_to_socket(disk, (char *)response, 4);
}

/*
 * Command: R <cylinder> <sector>
 * Read a sector
 */
int read_sector(BasicDisk *disk, int cylinder, int sector) {
    // Check if the cylinder and sector are valid
    if (cylinder < 0 || cylinder >= disk->n_cylinders ||
        sector < 0 || sector >= disk->n_sectors) {
        fprintf(stderr, "Invalid cylinder or sector\n");
        write_to_socket(disk, "No - Invalid cylinder or sector\n", 33);
        return -1;
    }
    update_track(disk, cylinder);

    int offset = (cylinder * disk->n_sectors + sector) * SECTOR_SIZE;
    char *sector_data = disk->diskfile + offset;

    write_to_socket(disk, sector_data, SECTOR_SIZE);
    // fprintf(stdout, "%.*s\n", SECTOR_SIZE, sector_data);
    return 0;
}

/*
 * Command: W <cylinder> <sector> <length> <data>
 * Write data to a sector
 */
int write_sector(BasicDisk *disk, int cylinder, int sector, int length, char *data) {
    // Check if the cylinder and sector are valid
    if (cylinder < 0 || cylinder >= disk->n_cylinders ||
        sector < 0 || sector >= disk->n_sectors) {
        fprintf(stderr, "Invalid cylinder or sector\n");
        write_to_socket(disk, "No - Invalid cylinder or sector\n", 33);
        return -1;
    }
    if (length < 0 || length > SECTOR_SIZE) {
        fprintf(stderr, "Invalid length\n");
        write_to_socket(disk, "No - Invalid length\n", 21);
        return -1;
    }
    update_track(disk, cylinder);

    int offset = (cylinder * disk->n_sectors + sector) * SECTOR_SIZE;
    char *sector_data = disk->diskfile + offset;

    // Write the data to the sector
    memcpy(sector_data, data, length);

    // Fill the rest of the sector with 0
    memset(sector_data + length, 0, SECTOR_SIZE - length);

    // write_to_socket(disk, "Yes\n");
    write_to_socket(disk, "", 1);
    fprintf(stdout, "Yes\n");
    return 0;

    // int end_sector = sector + length / SECTOR_SIZE;
    // int end_cylinder = cylinder + end_sector / disk->n_sectors;
    // if (cylinder < 0 || end_cylinder >= disk->n_cylinders ||
    //     sector < 0 || sector >= disk->n_sectors) {
    //     fprintf(disk->stderr_fd, "Invalid cylinder or sector\n");
    //     return;
    // }

    // int offset = (cylinder * disk->n_sectors + sector) * SECTOR_SIZE;
    // char *sector_data = disk->diskfile + offset;

    // // Write the data to the sector
    // write(disk->socket_fd, data, length);
}

void dystroy_disk(BasicDisk *disk) {
    if (munmap(disk->diskfile, disk->n_cylinders * disk->n_sectors * SECTOR_SIZE) < 0) {
        perror("Error unmapping the diskfile from memory");
        exit(-1);
    }
    if (close(disk->fd) < 0) {
        perror("Error closing the diskfile");
        exit(-1);
    }
}