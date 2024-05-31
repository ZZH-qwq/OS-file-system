#include "ClientCore.h"

#define SECTOR_SIZE 256

int main(int argc, char* argv[]) {
    if (argc != 3) {
        fprintf(stderr, "Usage: %s <DiskServerAddress> <#port>\n", argv[0]);
        exit(1);
    }

    char command[MAX_BUF_SIZE];
    char response[MAX_BUF_SIZE];

    // Connect to the server
    int sockfd = connect_to(argv[1], atoi(argv[2]));
    int activity, nbytes;

    fd_set controlfds;
    struct timeval timeout = {0, 500000};

    while (1) {
        FD_ZERO(&controlfds);
        FD_SET(STDIN_FILENO, &controlfds);
        FD_SET(sockfd, &controlfds);

        activity = select(sockfd + 1, &controlfds, NULL, NULL, &timeout);

        if (activity > 0 && FD_ISSET(STDIN_FILENO, &controlfds)) {
            char* ch = fgets(command, MAX_BUF_SIZE, stdin);
            if (ch == NULL) {
                break;
            } else if (command[0] == '\n') {
                continue;
            }
            send_command(sockfd, command);
        }

        if (activity > 0 && FD_ISSET(sockfd, &controlfds)) {
            nbytes = receive_response(sockfd, response);
            if (nbytes == 0) {
                printf("Server disconnected\n");
                break;
            } else if (nbytes > 0) {
                printf("%s\n", response);
            }
        }
    }

    return 0;
}