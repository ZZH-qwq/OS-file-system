#include "CmdEncoder.h"
#include "UserAuth.h"
#include <stdio.h>

int _cmd_type(char* cmd) {
    switch (cmd[0]) {
        case 'f':
            return FS_FORMAT;
        case 'm':
            if (strncmp(cmd, "mkdir", 5) == 0) {
                return FS_MKDIR;
            } else {
                return FS_MK;
            }
        case 'r':
            if (strncmp(cmd, "rmdir", 5) == 0) {
                return FS_RMDIR;
            } else {
                return FS_RM;
            }
        case 'c':
            if (strncmp(cmd, "cd", 2) == 0) {
                return FS_CD;
            } else if (strncmp(cmd, "cat", 3) == 0) {
                return FS_CAT;
            } else if (strncmp(cmd, "chmod", 5) == 0) {
                return FS_CHMOD;
            } else if (strncmp(cmd, "chown", 5) == 0) {
                return FS_CHOWN;
            } else {
                return FS_INVALID;
            }
        case 'l':
            if (strncmp(cmd, "ls", 2) == 0) {
                return FS_LS;
            } else {
                return FS_INVALID;
            }
        case 'w':
            return FS_WRITE;
        case 'i':
            return FS_INSERT;
        case 'd':
            if (strncmp(cmd, "du", 2) == 0) {
                return FS_DU;
            } else {
                return FS_DELETE;
            }
        case 'p':
            if (strncmp(cmd, "pwd", 3) == 0) {
                return FS_PWD;
            } else if (strncmp(cmd, "passwd", 6) == 0) {
                return FS_PASSWD;
            } else {
                return FS_INVALID;
            }
        case 's':
            if (strncmp(cmd, "stat", 4) == 0) {
                return FS_STAT;
            } else if (strncmp(cmd, "su", 2) == 0) {
                return FS_SU;
            } else {
                return FS_INVALID;
            }
        case 'e':
            return FS_EXIT;
        case 'u':
            if (strncmp(cmd, "useradd", 7) == 0) {
                return FS_USERADD;
            } else if (strncmp(cmd, "userdel", 7) == 0) {
                return FS_USERDEL;
            } else {
                return FS_INVALID;
            }

        default:
            return FS_INVALID;
    }
}

int _single_arg_cmd(char* cmd, char* type, CmdHeader* command) {
    char data_buffer[1024];
    char type_buffer[256];
    static const char* alnum = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789";
    int res = sscanf(cmd, "%256s %1024s %[^\n]", type_buffer, command->name, data_buffer);
    if (res < 2 || (res > 2 && strpbrk(data_buffer, alnum) != NULL)) {
        return -2;
    }
    if (strcmp(type_buffer, type) != 0) {
        return -1;
    }
    command->name_len = strlen(command->name);
    return 0;
}

int _parse_escape(char* data, u_int32_t data_len) {
    // "\xx" -> char(16-based) e.g. "\0a" -> '\n'
    char* ptr = strchr(data, '\\');
    char* parse_end = ptr;
    size_t orig_len = strlen(data);
    if (data[orig_len - 1] == '\n') {
        orig_len--;
        data[orig_len] = '\0';
    }
    int processed = 0;
    while (ptr != NULL && parse_end - data < data_len) {
        int escape;
        if (sscanf(ptr, "\\%2x", &escape) == 1) {
            *parse_end = escape;
            parse_end++;
            processed++;
            char* next = strchr(ptr + 1, '\\');
            if (next == NULL) {
                memmove(parse_end, ptr + 3, data_len + 1 - (parse_end - data));
                break;
            } else {
                memmove(parse_end, ptr + 3, next - ptr - 3);
                parse_end = parse_end + (next - ptr - 3);
                ptr = next;
            }
        } else {
            return -1;
        }
    }
    // Return actual length of the data
    return orig_len - 2 * processed;
}

int get_passwd(char* data_buff, FILE* input, bool confirm) {
    printf("Enter password: ");
    if (fgets(data_buff, 256, input) == NULL) {
        return -1;
    }
    if (input != stdin) {
        printf("%s", data_buff);
    }
    char* ch = strpbrk(data_buff, "- \t");
    if (ch != NULL) {
        printf("Invalid character in password\n");
        return -1;
    }
    char* newline = strchr(data_buff, '\n');
    if (newline != NULL) {
        *newline = '\0';
    }
    char* passwd = data_buff;
    data_buff += strlen(passwd) + 1;
    if (confirm) {
        // Get password confirmation
        printf("Confirm password: ");
        if (fgets(data_buff, 256, input) == NULL) {
            return -1;
        }
        if (input != stdin) {
            printf("%s", data_buff);
        }
        newline = strchr(data_buff, '\n');
        if (newline != NULL) {
            *newline = '\0';
        }
        if (strcmp(passwd, data_buff) != 0) {
            printf("Password mismatch\n");
            return -1;
        }
    }
    // Hash the password
    AuthUser auth;
    memset(auth.hashed_password, 0, 32);
    if (strlen(passwd) > 0) {
        cal_hash(&auth, (unsigned char*)passwd, strlen(passwd));
    }
    memcpy(passwd, auth.hashed_password, 32);
    return 0;
}

int parse_command(char* cmd, CmdHeader* command, char** data, FILE* input) {
    int res;
    static const char* alnum = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789";
    char data_buffer[1024];
    char type_buffer[256];
    while (*cmd == ' ' || *cmd == '\t') {
        cmd++;
    }
    command->type = FS_INVALID;
    command->name_len = 0;
    command->data_len = 0;
    command->position = 0;
    *data = NULL;
    int pos, data_len;
    switch (_cmd_type(cmd)) {
        case FS_FORMAT:
            if (strpbrk(cmd + 1, alnum) != NULL) {
                return -1;
            }
            command->type = FS_FORMAT;
            return 0;

        case FS_MK:
            res = _single_arg_cmd(cmd, "mk", command);
            if (res < 0) {
                return res;
            }
            command->type = FS_MK;
            return 0;

        case FS_MKDIR:
            res = _single_arg_cmd(cmd, "mkdir", command);
            if (res < 0) {
                return res;
            }
            command->type = FS_MKDIR;
            return 0;

        case FS_RM:
            res = _single_arg_cmd(cmd, "rm", command);
            if (res < 0) {
                return res;
            }
            command->type = FS_RM;
            return 0;

        case FS_RMDIR:
            res = _single_arg_cmd(cmd, "rmdir", command);
            if (res < 0) {
                return res;
            }
            command->type = FS_RMDIR;
            return 0;

        case FS_CD:
            res = _single_arg_cmd(cmd, "cd", command);
            if (res < 0) {
                return res;
            }
            command->type = FS_CD;
            return 0;

        case FS_LS:
            res = sscanf(cmd, "%256s %1024s %[^\n]", type_buffer, command->name, data_buffer);
            if (res < 1 || (res > 2 && strpbrk(data_buffer, alnum) != NULL)) {
                return -1;
            }
            if (res == 2) {
                if (strcmp(command->name, "-l") != 0) {
                    return -1;
                }
                command->name_len = 2;
            }
            command->type = FS_LS;
            return 0;

        case FS_CAT:
            res = _single_arg_cmd(cmd, "cat", command);
            if (res < 0) {
                return res;
            }
            command->type = FS_CAT;
            return 0;

        case FS_WRITE: {
            char* type_token = strtok(cmd, " \t");
            if (type_token == NULL || strcmp(type_token, "w") != 0) {
                return -1;
            }
            char* name_token = strtok(NULL, " \t");
            if (name_token == NULL) {
                return -1;
            }
            char* len_token = strtok(NULL, " \t");
            if (len_token == NULL || sscanf(len_token, "%d", &data_len) < 1 || data_len < 0) {
                return -2;
            }
            command->data_len = (u_int32_t)data_len;
            char* data_token = strtok(NULL, "");
            if (data_token == NULL) {
                return -2;
            }
            *data = data_token;
            res = _parse_escape(data_token, command->data_len);
            if (res < 0) {
                return res;
            }
            strcpy(command->name, name_token);
            command->name_len = strlen(command->name);
            command->type = FS_WRITE;
            if ((u_int32_t)res != command->data_len) {
                return res;
            }
            return 0;
        }

        case FS_INSERT: {
            char* type_token = strtok(cmd, " \t");
            if (type_token == NULL || strcmp(type_token, "i") != 0) {
                return -1;
            }
            char* name_token = strtok(NULL, " \t");
            if (name_token == NULL) {
                return -1;
            }
            char* pos_token = strtok(NULL, " \t");
            if (pos_token == NULL || sscanf(pos_token, "%d", &pos) < 1 || pos < 0) {
                return -2;
            }
            command->position = (u_int32_t)pos;
            char* len_token = strtok(NULL, " \t");
            if (len_token == NULL || sscanf(len_token, "%d", &data_len) < 1 || data_len < 0) {
                return -2;
            }
            command->data_len = (u_int32_t)data_len;
            char* data_token = strtok(NULL, "");
            if (data_token == NULL) {
                return -1;
            }
            *data = data_token;
            res = _parse_escape(data_token, command->data_len);
            if (res < 0) {
                return res;
            }
            strcpy(command->name, name_token);
            command->name_len = strlen(command->name);
            command->type = FS_INSERT;
            if ((u_int32_t)res != command->data_len) {
                return res;
            }
            return 0;
        }

        case FS_DELETE:
            res = sscanf(cmd, "%256s %1024s %d %d %[^\n]", type_buffer, command->name, &pos, &data_len, data_buffer);
            if (res < 4 || (res > 4 && strpbrk(data_buffer, alnum) != NULL)) {
                return -1;
            }
            if (strcmp(type_buffer, "d") != 0) {
                return -1;
            }
            if (pos < 0 || data_len < 0) {
                return -2;
            }
            command->position = (u_int32_t)pos;
            command->data_len = (u_int32_t)data_len;
            command->name_len = strlen(command->name);
            command->type = FS_DELETE;
            return 0;

        case FS_PWD:
            if (strpbrk(cmd + 3, alnum) != NULL) {
                return -1;
            }
            command->type = FS_PWD;
            return 0;

        case FS_STAT:
            res = _single_arg_cmd(cmd, "stat", command);
            if (res < 0) {
                return res;
            }
            command->type = FS_STAT;
            return 0;

        case FS_DU:
            if (strpbrk(cmd + 2, alnum) != NULL) {
                return -1;
            }
            command->type = FS_DU;
            return 0;

        case FS_USERADD:
            res = _single_arg_cmd(cmd, "useradd", command);
            if (res < 0) {
                return res;
            }
            if (strpbrk(command->name, "\\/.*+?^$[]") != NULL) {
                return -2;
            }
            command->type = FS_USERADD;
            return 0;

        case FS_USERDEL:
            res = _single_arg_cmd(cmd, "userdel", command);
            if (res < 0) {
                return res;
            }
            command->type = FS_USERDEL;
            return 0;

        case FS_PASSWD: {
            char* cmd_end = cmd + strlen(cmd) + 1;
            char* type_token = strtok(cmd, " \n\t");
            if (type_token == NULL || strcmp(type_token, "passwd") != 0) {
                return -1;
            }
            char* name_token = strtok(NULL, " \n\t");
            if (name_token == NULL) {
                command->name_len = 0;
            } else {
                command->name_len = strlen(name_token);
                strcpy(command->name, name_token);
            }
            char* data_token = strtok(NULL, "");
            if (data_token != NULL) {
                return -1;
            }
            res = get_passwd(cmd_end, input, true);
            if (res < 0) {
                return res;
            }
            *data = cmd_end;
            command->type = FS_PASSWD;
            command->data_len = 32;
            return 0;
        }

        case FS_SU: {
            char* cmd_end = cmd + strlen(cmd) + 1;
            char* type_token = strtok(cmd, " \n\t");
            if (type_token == NULL || strcmp(type_token, "su") != 0) {
                return -1;
            }
            char* name_token = strtok(NULL, " \n\t");
            if (name_token == NULL) {
                command->name_len = 0;
            } else {
                command->name_len = strlen(name_token);
                strcpy(command->name, name_token);
            }
            char* data_token = strtok(NULL, "");
            if (data_token != NULL) {
                return -1;
            }
            int res = get_passwd(cmd_end, input, false);
            if (res < 0) {
                return res;
            }
            *data = cmd_end;
            command->data_len = 32;
            command->type = FS_SU;
            return 0;
        }

        case FS_CHMOD: {
            char* type_token = strtok(cmd, " \t");
            if (type_token == NULL || strcmp(type_token, "chmod") != 0) {
                return -1;
            }
            char* name_token = strtok(NULL, " \t");
            if (name_token == NULL) {
                return -1;
            }
            char* prem_token = strtok(NULL, " \n\t");
            if (prem_token == NULL) {
                return -1;
            }
            if (strlen(prem_token) != 2) {
                printf("%ld\n", strlen(prem_token));
                printf("%d %d %d", prem_token[0], prem_token[1], prem_token[2]);
                return -2;
            }
            // Check if the permission is valid
            u_int16_t prem;
            if (prem_token[1] < '0' || prem_token[1] > '7') {
                return -2;
            }
            if (sscanf(prem_token, "%ho", &prem) < 1) {
                return -2;
            }
            char* data_token = strtok(NULL, "");
            if (data_token != NULL) {
                return -1;
            }
            command->name_len = strlen(name_token);
            command->position = prem;
            strcpy(command->name, name_token);
            command->type = FS_CHMOD;
            return 0;
        }

        case FS_CHOWN: {
            char* type_token = strtok(cmd, " \t");
            if (type_token == NULL || strcmp(type_token, "chown") != 0) {
                return -1;
            }
            char* name_token = strtok(NULL, " \t");
            if (name_token == NULL) {
                return -1;
            }
            char* user_token = strtok(NULL, " \n\t");
            if (user_token == NULL) {
                return -1;
            }
            char* data_token = strtok(NULL, "");
            if (data_token != NULL) {
                return -1;
            }
            command->name_len = strlen(name_token);
            strcpy(command->name, name_token);
            command->data_len = strlen(user_token);
            *data = user_token;
            command->type = FS_CHOWN;
            return 0;
        }

        case FS_EXIT:
            if (strpbrk(cmd + 1, alnum) != NULL) {
                return -1;
            }
            command->type = FS_EXIT;
            return 0;

        default:
            break;
    }
    return -1;
}

void print_help() {
    printf(
        "Commands:\n"
        "f                                          Format the disk\n"
        "mk <filename>                              Create a file\n"
        "mkdir <dirname>                            Create a directory\n"
        "rm <filename>                              Remove a file\n"
        "rmdir <dirname>                            Remove a directory\n"
        "cd <dirname>                               Change directory\n"
        "ls [-l]                                    List directory contents\n"
        "cat <filename>                             Print file contents\n"
        "w <filename> <length> <data>               Write to a file\n"
        "i <filename> <position> <length> <data>    Insert to a file\n"
        "d <filename> <position> <length>           Delete from a file\n"
        "pwd                                        Print working directory\n"
        "stat <filename>                            Print file status\n"
        "du                                         Print disk usage\n"
        "useradd <username>                         Add a user\n"
        "userdel <username>                         Delete a user\n"
        "passwd [username]                          Change password\n"
        "su [username]                              Change user\n"
        "chmod <filename> <permission>              Change file permission\n"
        "chown <filename> <username>                Change file owner\n"
        "e                                          Exit the file server\n");
}