#include "FileServer.h"
#include "ServerCore.h"

int fs_process_cmd(User* user, AuthList* list, CmdHeader* command, char* data) {
    int data_len, res;
    char* data_ptr = NULL;
#ifdef _DEBUG
    printf("Cmd Type: %d\tName_len: %d\tData_len: %d\tPosition: %d\tName: %.*s\n", command->type, command->name_len, command->data_len, command->position, command->name_len, command->name);
    if (command->data_len > 0) {
        printf("\tData: %.*s", command->data_len, data);
    }
#endif

    if (command->type != FS_FORMAT) {
        res = _check_cache(user);
        if (res < 0) {
            return -1;
        } else if (res == FLUSH_DIR) {
            fs_respond(user, "\0F\0\0", 4);
            return 0;
        }
    }

    switch (command->type) {
        case FS_FORMAT:
            // Format the disk
            res = u_format_volume(user, list);
            break;

        case FS_MK:
            // Create a file
            res = u_make_file(user, command->name);
            break;

        case FS_MKDIR:
            // Create a directory
            res = u_make_dir(user, command->name);
            break;

        case FS_RM:
            // Remove a file
            res = u_remove_file(user, command->name);
            break;

        case FS_RMDIR:
            // Remove a directory
            res = u_remove_dir(user, command->name);
            break;

        case FS_CD:
            // Change directory
            res = u_change_dir(user, command->name);
            break;

        case FS_LS:
            // List directory
            res = u_list_dir(user, list, command->name_len == 2, &data_ptr, &data_len);
            if (res >= 0) {
                fs_respond(user, data_ptr, data_len);
                free(data_ptr);
            }
            break;

        case FS_PWD:
            // Print the current directory
            res = _get_path(user);
            if (res >= 0) {
                fs_respond(user, user->path, strlen(user->path));
                fs_respond(user, "\n", 1);
            }
            break;

        case FS_STAT:
            // Print file status
            res = u_stat_file(user, command->name, &data_ptr, &data_len);
            if (res >= 0) {
                fs_respond(user, data_ptr, data_len);
                free(data_ptr);
            }
            break;

        case FS_DU:
            // Print disk usage
            res = print_vol_info(user->vol, &data_ptr, &data_len);
            if (res >= 0) {
                fs_respond(user, data_ptr, data_len);
                free(data_ptr);
            }
            break;

        case FS_WRITE:
            // Write a file
            res = u_write_file(user, command->name, command->data_len, data);
            break;

        case FS_CAT:
            // Catch a file
            res = u_cat_file(user, command->name, &data_ptr, &data_len);
            if (res >= 0) {
                fs_respond(user, data_ptr, data_len);
                fs_respond(user, "\n", 1);
                free(data_ptr);
            }
            break;

        case FS_INSERT:
            // Insert a file
            res = u_insert_file(user, command->name, command->position, command->data_len, data);
            break;

        case FS_DELETE:
            // Delete a file
            res = u_delete_file(user, command->name, command->position, command->data_len);
            break;

        case FS_USERADD:
            // Add a user
            res = u_add_user(user, list, command->name);
            break;

        case FS_USERDEL:
            // Delete a user
            res = u_remove_user(user, list, command->name);
            break;

        case FS_PASSWD:
            // Change password
            if (command->name_len == 0) {
                strcpy(command->name, user->username);
            }
            res = u_change_password(user, list, command->name, (u_int32_t*)data);
            break;

        case FS_SU:
            // Change user
            if (command->name_len == 0) {
                strcpy(command->name, "root");
            }
            res = u_switch_user(user, list, command->name, (u_int32_t*)data);
            break;

        case FS_CHMOD:
            // Change mode
            res = u_change_mode(user, command->name, command->position);
            break;

        case FS_CHOWN:
            // Change owner
            res = u_change_owner(user, list, command->name, data);
            break;

        default:
            fs_error(user, "Invalid command", 15);
            return -1;
    }

    switch (res) {
        case FLUSH_DIR:
            fs_respond(user, "\0F\0\0", 4);
            break;

        case GENERIC_ERROR:
            fs_error(user, "Operation terminated due to error\n", 34);
            break;

        case TARGET_NOT_FOUND:
            fs_error(user, "No such file or directory\n", 26);
            break;

        case TARGET_EXISTS:
            fs_error(user, "Target already exists\n", 22);
            break;

        case DIR_NOT_EMPTY:
            fs_error(user, "Directory not empty\n", 20);
            break;

        case INVALID_PATH:
            fs_error(user, "Invalid path\n", 13);
            break;

        case INVALID_POS:
            fs_error(user, "Invalid position\n", 17);
            break;

        case DISK_FAILURE:
            fs_error(user, "Disk failure\n", 13);
            break;

        case PERMISSION_DENIED:
            fs_error(user, "Permission denied\n", 18);
            break;

        case INVALID_USER:
            fs_error(user, "Invalid user\n", 13);
            break;

        case PASSWORD_MISMATCH:
            fs_error(user, "Password mismatch\n", 18);
            break;

        default:
            break;
    }
    return res;
}

int fs_get_cmd(User* user, char* cmd, char* data) {
    int nbytes = receive_response(user->sockfd, cmd);
    if (nbytes == 0) {
        // Client disconnected
        return FS_EXIT;
    }
    if (write(user->sockfd, "", 1) != 1) {
        perror("send");
        return -1;
    }
    CmdHeader* command = (CmdHeader*)cmd;
    if (command->type == (u_int16_t)FS_EXIT) {
        // @todo exit
        return FS_EXIT;
    }
    if (command->data_len > 0 && command->type != FS_DELETE) {
        for (int i = 0; (u_int32_t)i < command->data_len; i += MAX_BUF_SIZE / 2) {
            nbytes = receive_response(user->sockfd, data + i);
            if (nbytes == 0) {
                // Client disconnected
                return FS_EXIT;
            }
            if (write(user->sockfd, "", 1) != 1) {
                perror("send");
                return -1;
            }
        }
    }
    return 0;
}

int fs_login(AuthUser* auth, int sockfd) {
    int pack_len = 4 + 32 + auth->name_len;
    char* pack = (char*)auth;
    int res = write(sockfd, pack, pack_len);
    if (res != pack_len) {
        perror("send");
        return -1;
    }
    char response[200];
    int nbytes = receive_response(sockfd, response);
    if (nbytes == 0) {
        // Client disconnected
        return FS_EXIT;
    }
    auth->id = *(u_int16_t*)response;
#ifdef _DEBUG
    printf("User ID: %d\n", auth->id);
#endif
    return 0;
}

int fs_auth_user(User* user, AuthList* list) {
    AuthUser auth;
    char* pack = (char*)&auth;
    int res = read(user->sockfd, pack, 4 + 32 + 64);
    if (res < 0) {
        perror("recv");
        return -1;
    }
    user->id = 0;
    auth.username[auth.name_len] = 0;
    strcpy(user->username, auth.username);
    AuthUser* entry = search_user(list, user->username);
    if (entry == NULL) {
        return -1;
    }
    // Check password
    for (int i = 0; i < 8; i++) {
        if (entry->hashed_password[i] != auth.hashed_password[i]) {
            return -1;
        }
    }
    // Copy user info
    res = u_init_user(user);
    if (res < 0) {
        print_err("fs_auth_user:\tFailed to initialize user");
        return -1;
    }
    res = u_change_dir(user, entry->user_home);
    if (res < 0) {
        print_err("fs_auth_user:\tFailed to change directory");
        return -1;
    }
    strcpy(user->username, entry->username);
    user->id = entry->id;
    res = write(user->sockfd, (char*)&user->id, 2);
    if (res != 2) {
        perror("send");
        return -1;
    }
    return 0;
}

int fs_warning_local(char* message) {
    printf("\033[1;33mWarning:\033[0m %s\n", message);
    return 0;
}

int fs_data_len_check(char* cmd, char* data_ptr, int data_len, int res) {
    if (data_len < res) {
        fs_warning_local("Input data truncated by data length");
    } else {
        fs_warning_local("Input data shorter than data length, padding with zeros");
        int pad_size = data_len - res;
        if (data_ptr - cmd + data_len > MAX_BUF_SIZE) {
            pad_size = MAX_BUF_SIZE - (data_ptr - cmd) - res;
            fs_warning_local("Input data length truncated by buffer size");
        }
        memset(data_ptr + res, 0, pad_size);
    }
    return 0;
}

int fs_print_work_dir(User* user) {
    char path[1200];
    sprintf(path, "\n\033[1;32m%s\033[0m:\033[1;34m%.1024s\033[0m$ ", user->username, user->path);
    fs_respond(user, path, strlen(path));
    return 0;
}

int fs_respond(User* user, char* message, int len) {
    char buffer[200];
    for (int i = 0; i < len; i += MAX_BUF_SIZE / 2) {
        size_t write_len = len - i > MAX_BUF_SIZE / 2 ? MAX_BUF_SIZE / 2 : len - i;
        if (write(user->sockfd, message + i, write_len) != (ssize_t)write_len) {
            perror("send");
            return -1;
        }
        // Wait for the client
        do {
            int nbytes = receive_response(user->sockfd, buffer);
            if (nbytes == 0) {
                // Client disconnected
                return FS_EXIT;
            }
        } while (buffer[0] != 0);
    }
    return 0;
}

int fs_warning(User* user, char* message, int len) {
    char* msg = (char*)malloc(len + 50);
    int w_len = sprintf(msg, "\033[1;33mWarning:\033[0m %.*s", len, message);
    fs_respond(user, msg, w_len);
    free(msg);
    return 0;
}

int fs_error(User* user, char* message, int len) {
    char* msg = (char*)malloc(len + 50);
    int e_len = sprintf(msg, "\033[1;31mError:\033[0m %.*s", len, message);
    fs_respond(user, msg, e_len);
    free(msg);
    return 0;
}

int fs_disconnect(User* user) {
    close(user->sockfd);
    user->sockfd = 0;
    free_dir(&user->cur_dir);
    return 0;
}

int fs_exit(User* user_list) {
    fprintf(stdout, "Exiting...\n");
    for (int i = 0; i < MAX_CLIENTS; i++) {
        if (user_list[i].sockfd > 0) {
            if (shutdown(user_list[i].sockfd, SHUT_RDWR) < 0) {
                perror("shutdown");
            }
            free_dir(&user_list[i].cur_dir);
        }
    }
    return 0;
}