/*
 * Basic Disk-storage Server
 *
 * ./BDS <DiskFileName> <#cylinders> <#sector per cylinder> <track-to-track delay> <#port>
 */

#include "BasicDisk.h"
#include "ServerCore.h"

void disk_loop(BasicDisk* disk, int id) {
    char command[MAX_BUF_SIZE];
    char response[MAX_BUF_SIZE];
    int args[4];

    while (1) {
        int nbytes = read(disk->socket_fd, command, MAX_BUF_SIZE - 1);
        if (nbytes < 0) {
            perror("read");
            exit(-1);
        } else if (nbytes == 0) {
            printf("Client %d disconnected\n", id);
            close(disk->socket_fd);
            break;
        }
        command[nbytes] = '\0';
        printf("\nReceived command: %s", command);

        int cmd_type = parse_command(command, args, response);
        switch (cmd_type) {
            case CMD_I:
                info(disk);
                break;

            case CMD_R:
                read_sector(disk, args[0], args[1]);
                break;

            case CMD_W:
                write_sector(disk, args[0], args[1], args[2], response);
                break;

            case CMD_E:
                close(disk->socket_fd);
                exit(0);
                break;

            case INVALID_COMMAND:
                write_to_socket(disk, "Invalid command\n");
                fprintf(stderr, "Invalid command: %s\n", command);
                break;
        }
    }
}

int main(int argc, char* argv[]) {
    if (argc != 6) {
        fprintf(stderr, "Usage: %s <DiskFileName> <#cylinders> <#sector per cylinder> <track-to-track delay> <#port>\n", argv[0]);
        exit(1);
    }

    BasicDisk disk;
    disk.n_cylinders = atoi(argv[2]);
    disk.n_sectors = atoi(argv[3]);
    disk.track_to_track_delay = atoi(argv[4]);
    disk.current_track = 0;

    init_disk(argv[1], &disk);

    int port = atoi(argv[5]);
    int master_socket, addrlen, client_socket[MAX_CLIENTS], activity, status;
    struct sockaddr_in address;

    // char buffer[MAX_COMMAND];
    pid_t child_pid[MAX_CLIENTS];

    // Set of socket descriptors
    fd_set readfds, controlfds;
    struct timeval timeout = {0, 250000};

    // 0 refers to unconnected socket and child pid
    for (int i = 0; i < MAX_CLIENTS; i++) {
        client_socket[i] = 0;
        child_pid[i] = 0;
    }

    master_socket = init_server(&address, port);

    // Accept the incoming connection
    addrlen = sizeof(address);
    printf("Waiting for connections ... \n");

    // Listen to the socket
    listen(master_socket, 3);

    while (1) {
        // Clear the socket set
        FD_ZERO(&readfds);

        // Add master socket to set
        FD_SET(master_socket, &readfds);

        // Wait for an activity on the sockets
        activity = select(master_socket + 1, &readfds, NULL, NULL, &timeout);
        if ((activity < 0) && (errno != EINTR)) {
            perror("select");
        }

        // Process new connection
        if (FD_ISSET(master_socket, &readfds) && client_socket[0] == 0) {
            int new_socket = accept_new(master_socket, &address, &addrlen);

            disk.socket_fd = new_socket;

            // Add new socket to array of sockets
            int i = 0;
            client_socket[i] = new_socket;
            printf("Adding to list of sockets as %d\n", i);

            // Start processing the new connection
            pid_t pid = fork();
            if (pid == 0) {
                // Child process
                disk_loop(&disk, i);
                exit(0);
            } else if (pid < 0) {
                perror("fork");
                exit(-1);
            }
            child_pid[i] = pid;
        }

        if (child_pid[0] != 0 && waitpid(child_pid[0], &status, WNOHANG) != 0) {
            printf("Child process terminated, status = %d\n", status);
            disconnect(client_socket, child_pid, status);
        }

        // Check if the server should exit
        check_exit(&controlfds, client_socket, child_pid);
    }

    dystroy_disk(&disk);

    return 0;
}