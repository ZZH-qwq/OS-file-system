/*
 * BasicDisc.h
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

// Sector size = 256 bytes
#define SECTOR_SIZE 256

typedef struct {
    int n_cylinders;
    int n_sectors;
    int track_to_track_delay;
    int fd;
    int current_track;
    char *diskfile;
    int socket_fd;
} BasicDisk;

void init_disk(char *filename, BasicDisk *disk);

void write_to_socket(BasicDisk *disk, char *response);

void info(BasicDisk *disk);

int read_sector(BasicDisk *disk, int cylinder, int sector);

int write_sector(BasicDisk *disk, int cylinder, int sector, int length, char *data);

void dystroy_disk(BasicDisk *disk);

#endif