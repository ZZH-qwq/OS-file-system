/*
 * Parser.h
 *
 * Parse the command and extract the arguments
 */

#ifndef PARSER_H
#define PARSER_H

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define CMD_I 0  // Information request
#define CMD_R 1  // Read a sector
#define CMD_W 2  // Write data to sectors
#define CMD_E 3  // Exit

#define INVALID_COMMAND -1

#define MAX_BUF_SIZE 1024

int parse_command(char* command, int* args, char* data_buffer);

#endif