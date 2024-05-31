/**
 * BasicDisk.h
 *
 * Basic disk functions
 */

#ifndef BASICDISK_H
#define BASICDISK_H

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <stdbool.h>
#include <fcntl.h>
#include <sys/mman.h>
#include <sys/types.h>

// Sector size = 256 bytes
#define SECTOR_SIZE 256

typedef struct BasicDisk {
    int n_cylinders;
    int n_sectors;
    int track_to_track_delay;
    int fd;
    int current_track;
    char *diskfile;
    int socket_fd;
} BasicDisk;

#define CMD_I 0  // Information request
#define CMD_R 1  // Read a sector
#define CMD_W 2  // Write data to sectors
#define CMD_E 3  // Exit
#define CMD_S 4  // Sync
#define INVALID_COMMAND -1

#define MAX_BUF_SIZE 8192
#define SIZE_CMD_BASIC 8

typedef struct Command {
    u_int16_t type;
    u_int16_t len;
    u_int32_t block_id;
    char data[SECTOR_SIZE];
} Command;

void init_disk(char *filename, BasicDisk *disk);

void write_to_socket(BasicDisk *disk, char *response, size_t len);

void info(BasicDisk *disk);

int read_sector(BasicDisk *disk, int cylinder, int sector);

int write_sector(BasicDisk *disk, int cylinder, int sector, int length, char *data);

void dystroy_disk(BasicDisk *disk);

#endif