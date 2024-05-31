#include "ServerCore.h"

// Initialize the server and return the master socket
int init_server(struct sockaddr_in *address, int PORT) {
    // Create a master socket
    int master_socket;
    if ((master_socket = socket(AF_INET, SOCK_STREAM, 0)) == 0) {
        perror("socket failed");
        exit(EXIT_FAILURE);
    }

    // Set master socket to allow multiple connections
    int opt = 1;
    if (setsockopt(master_socket, SOL_SOCKET, SO_REUSEADDR, (char *)&opt, sizeof(opt)) < 0) {
        perror("setsockopt");
        exit(EXIT_FAILURE);
    }

    // Type of socket created
    address->sin_family = AF_INET;
    address->sin_addr.s_addr = INADDR_ANY;
    address->sin_port = htons(PORT);

    // Bind the socket to localhost port 8888
    if (bind(master_socket, (struct sockaddr *)address, sizeof(*address)) < 0) {
        perror("bind failed");
        exit(EXIT_FAILURE);
    }
    printf("Listener on port %d \n", PORT);

    // Try to specify maximum of 3 pending connections for the master socket
    if (listen(master_socket, 3) < 0) {
        perror("listen");
        exit(EXIT_FAILURE);
    }
    return master_socket;
}

// Accept new connection and return the new socket
int accept_new(int master_socket, struct sockaddr_in *address, int *addrlen) {
    // If something happened on the master socket ,
    // then its an incoming connection
    int new_socket;
    if ((new_socket = accept(master_socket, (struct sockaddr *)address, (socklen_t *)addrlen)) < 0) {
        perror("accept");
        exit(EXIT_FAILURE);
    }

    // inform user of socket number - used in send and receive commands
    printf("[New connection]\tsocket fd is %d , ip is : %s , port : %d\n", new_socket, inet_ntoa(address->sin_addr), ntohs(address->sin_port));

    // send new connection greeting message
    char *message = "Basic Disk-storage Server Connected\n";
    size_t len = strlen(message);
    if (send(new_socket, message, len, 0) != (ssize_t)len) {
        perror("send");
        return -1;
    }

    printf("Connect successfully\n");
    return new_socket;
}

// Disconnect the client socket, close the pipe and mark them as 0
void disconnect(int *client_socket, pid_t *pid_list, int status) {
    if (status == 0) {
        // Quit by exit command
        if (shutdown(*client_socket, SHUT_RDWR) < 0) {
            perror("shutdown");
        }
    }
    printf("[Host disconnected]\n");
    // Close the socket and mark as 0 in list for reuse
    *client_socket = 0;
    *pid_list = 0;
}

// Check if the server should exit
void check_exit(fd_set *controlfds, int *client_socket, pid_t *pid_list) {
    FD_ZERO(controlfds);
    FD_SET(STDIN_FILENO, controlfds);
    struct timeval timeout = {0, 125000};
    if (select(STDIN_FILENO + 1, controlfds, NULL, NULL, &timeout) > 0) {
        // Read from stdin
        char buffer[MAX_BUF_SIZE];
        if (read(STDIN_FILENO, buffer, MAX_BUF_SIZE) > 0 && strncmp(buffer, "exit", 4) != 0) {
            return;
        }
        fprintf(stdout, "Exiting...\n");
        for (int i = 0; i < MAX_CLIENTS; i++) {
            // Close all clients
            if (pid_list[i] > 0) {
                pid_t pid = pid_list[i];
                printf("Killing child process %d\n", pid);
                disconnect(client_socket + i, pid_list + i, 0);
                waitpid(pid, NULL, 0);
            }
        }
        exit(0);
    }
}