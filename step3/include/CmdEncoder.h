/**
 * CmdEncoder.h
 *
 * The parser and encoder of file-system protocol
 */

#ifndef CMDENCODER_H
#define CMDENCODER_H

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <stdbool.h>
#include <sys/types.h>

typedef struct CmdHeader {
    u_int16_t type;
    u_int16_t name_len;
    u_int32_t data_len;
    u_int32_t position;
    char name[1024];
} CmdHeader;

#define FS_FORMAT 0
#define FS_MK 1
#define FS_MKDIR 2
#define FS_RM 3
#define FS_RMDIR 4
#define FS_CD 5
#define FS_LS 6
#define FS_CAT 7
#define FS_WRITE 8
#define FS_INSERT 9
#define FS_DELETE 10
#define FS_PWD 11
#define FS_STAT 12
#define FS_DU 13
#define FS_USERADD 14
#define FS_USERDEL 15
#define FS_PASSWD 16
#define FS_CHOWN 17
#define FS_CHMOD 18
#define FS_SU 19

#define FS_EXIT -233
#define FS_INVALID 255

#define FS_BUF_SIZE 1024

/**
 * @brief Get a password from stdin
 * @param data_buff char*: the buffer to store the password and result
 * @param confirm bool: whether to confirm the password
 * @return int 0 if success, -1 if failed
 */
int get_passwd(char* data_buff, FILE* input, bool confirm);

/**
 * @brief Parse a command
 * @param cmd char*: the command string
 * @param command CmdHeader*: the command header
 * @param data char**: the data part
 * @return int >0 command length, -1 if failed
 */
int parse_command(char* cmd, CmdHeader* command, char** data, FILE* input);

/**
 * @brief Print help message
 */
void print_help();

#endif