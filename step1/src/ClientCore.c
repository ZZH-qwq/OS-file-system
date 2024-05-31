#include "ClientCore.h"

int connect_to(char *hostname, int port) {
    struct hostent *server;
    struct sockaddr_in serv_addr;
    int sockfd;

    server = gethostbyname(hostname);
    if (server == NULL) {
        fprintf(stderr, "ERROR, no such host\n");
        exit(0);
    }

    sockfd = socket(AF_INET, SOCK_STREAM, 0);
    if (sockfd < 0) {
        fprintf(stderr, "ERROR opening socket\n");
        exit(0);
    }

    bzero((char *)&serv_addr, sizeof(serv_addr));
    serv_addr.sin_family = AF_INET;
    bcopy((char *)server->h_addr_list[0], (char *)&serv_addr.sin_addr.s_addr, server->h_length);
    serv_addr.sin_port = htons(port);

    printf("Connecting to %s:%d\n", hostname, port);

    if (connect(sockfd, (struct sockaddr *)&serv_addr, sizeof(serv_addr)) < 0) {
        fprintf(stderr, "ERROR connecting\n");
        exit(0);
    }

    char buffer[MAX_BUF_SIZE];
    receive_response(sockfd, buffer);
    printf("%s\n", buffer);

    return sockfd;
}

void send_command(int sockfd, char *command) {
    int n = write(sockfd, command, strlen(command));
    if (n < 0) {
        fprintf(stderr, "ERROR writing to socket\n");
        exit(0);
    }
}

int receive_response(int sockfd, char *buffer) {
    int n = read(sockfd, buffer, MAX_BUF_SIZE - 1);
    if (n < 0) {
        fprintf(stderr, "ERROR reading from socket\n");
        exit(0);
    }
#ifdef _DEBUG
    printf("Response:\t get %d bytes\n", n);
#endif
    buffer[n] = '\0';
    return n;
}