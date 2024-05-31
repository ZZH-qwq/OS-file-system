#include "BasicDisk.h"
#include "FileServer.h"
#include "ServerCore.h"

#define SECTOR_SIZE 256

int main(int argc, char* argv[]) {
    if (argc != 4) {
        fprintf(stderr, "Usage: %s <DiskServerAddress> <#BDS_port> <#FS_port>\n", argv[0]);
        exit(1);
    }
    char cmd_buffer[MAX_BUF_SIZE];
    char data_buffer[MAX_BUF_SIZE];

    int fs_port = atoi(argv[3]);
    int master_socket, addrlen, max_fd;
    struct sockaddr_in address;

    // Users
    Inode dir_inode[MAX_CLIENTS];
    User user[MAX_CLIENTS];
    int connections = 0;

    // Set of socket descriptors
    fd_set readfds, controlfds;
    struct timeval timeout = {0, 500000};

    // Connect to Basic Disk-storage Server
    int bds_sockfd = connect_to(argv[1], atoi(argv[2]));
    int activity, nbytes;

    printf("Waiting for connections ... \n");
    Volume vol;
    MetaBlocks meta;
    vol.blockptr = &meta;
    vol.sockfd = bds_sockfd;
    int res = init_volume(&vol);
    bool is_formatted = res == 0;

    // 0 refers to unconnected socket and child pid
    for (int i = 0; i < MAX_CLIENTS; i++) {
        user[i].sockfd = 0;
        user[i].path[0] = '\0';
        user[i].vol = &vol;
        user[i].cur_dir.inodeptr = &dir_inode[i];
        user[i].cur_dir.dir_entries = NULL;
        user[i].cur_dir.orig_data = NULL;
    }

    master_socket = init_server(&address, fs_port);

    // Accept the incoming connection
    addrlen = sizeof(address);
    printf("Waiting for connections ... \n");

    // Listen to the socket
    listen(master_socket, 5);

    while (1) {
        // Clear the socket set
        FD_ZERO(&readfds);
        FD_ZERO(&controlfds);

        // Add sockets to set
        FD_SET(master_socket, &readfds);
        max_fd = master_socket;
        for (int i = 0; i < MAX_CLIENTS; i++) {
            if (user[i].sockfd > 0) {
                FD_SET(user[i].sockfd, &readfds);
                if (user[i].sockfd > max_fd) {
                    max_fd = user[i].sockfd;
                }
            }
        }
        FD_SET(STDIN_FILENO, &controlfds);
        FD_SET(bds_sockfd, &controlfds);

        // readfds - process new connection and client activity
        activity = select(max_fd + 1, &readfds, NULL, NULL, &timeout);
        if ((activity < 0) && (errno != EINTR)) {
            perror("select");
        }

        for (int i = 0; i < MAX_CLIENTS; i++) {
            if (user[i].sockfd == 0 || !FD_ISSET(user[i].sockfd, &readfds)) {
                continue;
            }
            // Read command from client
            res = fs_get_cmd(&user[i], cmd_buffer, data_buffer);
            if (res < 0) {
                if (res == FS_EXIT) {
                    close(user[i].sockfd);
                    user[i].sockfd = 0;
                    free_dir(&user[i].cur_dir);
                    connections--;
                    printf("Client %d disconnected\n", i);
                } else {
                    printf("Failed to read command\n");
                }
                continue;
            }
            CmdHeader* cmd = (CmdHeader*)cmd_buffer;
            if (is_formatted || cmd->type == FS_FORMAT) {
                res = fs_process_cmd(&user[i], cmd, data_buffer);
                if (res < 0) {
                    printf("Failed to process command\n");
                } else if (cmd->type == FS_FORMAT) {
                    is_formatted = true;
                }
            } else {
                fs_respond(&user[i], "\nPlease format the disk", 23);
            }
            res = fs_print_work_dir(&user[i]);
        }

        // Process new connection
        if (FD_ISSET(master_socket, &readfds) && connections < MAX_CLIENTS) {
            int new_socket = accept_new(master_socket, &address, &addrlen);
            // Add new socket to array of sockets
            int i = 0;
            while (i < MAX_CLIENTS && user[i].sockfd != 0) {
                i++;
            }
            user[i].sockfd = new_socket;
            connections++;
            if (is_formatted) {
                res = u_init_user(&user[i]);
                if (res != 0) {
                    printf("Failed to initialize user\n");
                }
            } else {
                fs_respond(&user[i], "Please format the disk", 22);
            }
            res = fs_print_work_dir(&user[i]);
            printf("Adding to list of sockets as %d\n", i);
        }

        activity = select(bds_sockfd + 1, &controlfds, NULL, NULL, &timeout);
        if ((activity < 0) && (errno != EINTR)) {
            perror("select");
        }

        if (activity > 0 && FD_ISSET(STDIN_FILENO, &controlfds)) {
            char* ch = fgets(data_buffer, 200, stdin);
            if (ch == NULL) {
                break;
            } else if (strncmp(data_buffer, "exit", 4) == 0) {
                return fs_exit(user);
            }
        }

        if (activity > 0 && FD_ISSET(bds_sockfd, &controlfds)) {
            nbytes = receive_response(bds_sockfd, data_buffer);
            if (nbytes == 0) {
                printf("Server disconnected\n");
                break;
            } else if (nbytes > 0) {
                printf("%s\n", data_buffer);
            }
        }
    }

    return 0;
}