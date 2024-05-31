#include "Parser.h"

// Parse the command and extract the arguments
// Check for invalid commands including missing arguments and additional characters
int parse_command(char* command, int* args, char* data_buffer) {
    int res;
    static const char* alnum = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789";
    while (*command == ' ' || *command == '\t') {
        command++;
    }
    switch (command[0]) {
        case 'I':
            if (strpbrk(command + 1, alnum) != NULL) {
                return INVALID_COMMAND;
            }
#ifdef _DEBUG
            printf("Parser:\tI\n");
#endif
            return CMD_I;
            break;

        case 'R':
            res = sscanf(command, "R %d %d %[^\n]", &args[0], &args[1], data_buffer);
            if (res < 2 || (res > 2 && strpbrk(data_buffer, alnum) != NULL)) {
                return INVALID_COMMAND;
            }
#ifdef _DEBUG
            printf("Parser:\tR\t%d\t%d\n", args[0], args[1]);
#endif
            return CMD_R;
            break;

        case 'W':
            res = sscanf(command, "W %d %d %d %[^\n]", &args[0], &args[1], &args[2], data_buffer);
            if (res < 3) {
                return INVALID_COMMAND;
            }
#ifdef _DEBUG
            printf("Parser:\tW\t%d\t%d\t%d\t%s\n", args[0], args[1], args[2], data_buffer);
#endif
            return CMD_W;
            break;

        case 'E':
            if (strpbrk(command + 1, alnum) != NULL) {
                return INVALID_COMMAND;
            }
#ifdef _DEBUG
            printf("Parser:\tE\n");
#endif
            return CMD_E;
            break;

        default:
            return INVALID_COMMAND;
            break;
    }
}