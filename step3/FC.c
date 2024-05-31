#include "FileServer.h"
#include <fcntl.h>

int main(int argc, char* argv[]) {
    if (argc < 4 && argc > 6) {
        fprintf(stderr, "Usage: %s <File-systemServerAddress> <#port> <username> [passwd] [cmdFile]\n", argv[0]);
        exit(1);
    }

    char* password = NULL;
    if (argc > 4 && strcmp(argv[4], "-") != 0) {
        password = argv[4];
    }
    bool is_file = argc == 6;
    FILE* file = is_file ? fopen(argv[5], "r") : stdin;
    if (file == NULL) {
        perror("fopen");
        return -1;
    }

    char* data_ptr;
    CmdHeader command;
    char cmd[MAX_BUF_SIZE];
    char response[MAX_BUF_SIZE];
    bool operatable = false;

    // Connect to the server
    int sockfd = connect_to(argv[1], atoi(argv[2]));
    int activity, nbytes;

    User user;
    user.sockfd = sockfd;
    strcpy(user.username, argv[3]);
    sprintf(user.path, "\n\033[1;32mFileServer\033[0m:\033[1;34mhome/%s\033[0m$ ", argv[3]);

    AuthUser auth_user;
    auth_user.name_len = strlen(argv[3]);
    strcpy(auth_user.username, argv[3]);
    memset(auth_user.hashed_password, 0, 32);
    if (password != NULL) {
        cal_hash(&auth_user, (unsigned char*)argv[4], strlen(argv[4]));
    }

    int res = fs_login(&auth_user, sockfd);
    if (res < 0) {
        close(sockfd);
        fprintf(stderr, "Login failed\n");
        return -1;
    }

    fd_set controlfds;
    struct timeval timeout = {0, 500000};

    while (1) {
        FD_ZERO(&controlfds);
        FD_SET(sockfd, &controlfds);
        if (!is_file) {
            FD_SET(STDIN_FILENO, &controlfds);
        }

        activity = select(sockfd + 1, &controlfds, NULL, NULL, &timeout);

        if (operatable && (is_file || (activity > 0 && FD_ISSET(STDIN_FILENO, &controlfds)))) {
            char* ch = fgets(cmd, MAX_BUF_SIZE, file);
            if (ch == NULL) {
                break;
            } else if (cmd[0] == '\n') {
                printf("%s", user.path);
                fflush(stdout);
                continue;
            }
            if (is_file) {
                printf("%.48s", cmd);
                int cmd_len = strlen(cmd);
                if (cmd_len > 50) {
                    printf("...(%d bytes more)\n", cmd_len - 50);
                }
                usleep(250000);
            }
            operatable = false;
            int res = parse_command(cmd, &command, &data_ptr, file);
#ifdef _DEBUG
            printf("Cmd Type: %d\tName_len: %d\tData_len: %d\tPosition: %d\tName: %.*s\n", command.type, command.name_len, command.data_len, command.position, command.name_len, command.name);
#endif
            if (res == -1) {
                fprintf(stderr, "\033[1;31mError:\033[0m Invalid command: %s\n", cmd);
            } else if (res == -2) {
                fprintf(stderr, "\033[1;31mError:\033[0m Invalid argument\n");
            }
            if (res < 0) {
                if (!is_file) {
                    print_help();
                }
                printf("%s", user.path);
                fflush(stdout);
                operatable = true;
            } else if (command.type == (u_int16_t)FS_EXIT) {
                close(sockfd);
                break;
            } else {
                if (res > 0) {
                    // Check data
                    fs_data_len_check(cmd, data_ptr, command.data_len, res);
                }
                int header_size = 12 + command.name_len;
                res = fs_respond(&user, (char*)&command, header_size);
                if (res < 0) {
                    close(sockfd);
                    return -1;
                }
                if (command.data_len > 0 && command.type != FS_DELETE) {
                    for (int i = 0; (u_int32_t)i < command.data_len; i += MAX_BUF_SIZE / 2) {
                        size_t read_len = command.data_len - i > MAX_BUF_SIZE / 2 ? MAX_BUF_SIZE / 2 : command.data_len - i;
                        res = fs_respond(&user, data_ptr + i, read_len);
                        if (res < 0) {
                            close(sockfd);
                            return -1;
                        }
#ifdef _DEBUG
                        printf("Send data: %d bytes\n", i);
#endif
                    }
                }
            }
        }

        if (activity > 0 && FD_ISSET(sockfd, &controlfds)) {
            nbytes = receive_response(sockfd, response);
            if (nbytes == 0) {
                printf("Server disconnected\n");
                break;
            } else if (nbytes > 0) {
                if (strncmp(response, "\n\033[1;32m", 8) == 0) {
                    operatable = true;
                    strcpy(user.path, response);
                } else if (strncmp(response, "\0F\0\0", 4) == 0) {
                    // Flushed
                    printf("\033[1;33mWarning:\033[0m Current path no longer exists\n");
                }
                printf("%s", response);
                fflush(stdout);
                // Confirm recived
                if (write(sockfd, "", 1) != 1) {
                    perror("send");
                    return -1;
                }
            }
        }
    }

    return 0;
}