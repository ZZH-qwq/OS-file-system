#include "ClientCore.h"

#define SECTOR_SIZE 256

void generate_commands(int n_cylinders, int n_sectors, char* command) {
    int type = rand() % 2;
    int c, s;
    char data[SECTOR_SIZE + 1];

    switch (type) {
        case 0:  // Read
            c = rand() % n_cylinders;
            s = rand() % n_sectors;
            sprintf(command, "R %d %d\n", c, s);
            printf("R %d %d\n", c, s);
            break;

        case 1:  // Write
            c = rand() % n_cylinders;
            s = rand() % n_sectors;
            for (int i = 0; i < SECTOR_SIZE; i++) {
                data[i] = ' ' + rand() % 95;
            }
            if (data[0] == ' ') {  // Avoid the first byte to be a space
                data[0] = 'A';
            }
            data[SECTOR_SIZE] = '\0';
            sprintf(command, "W %d %d %d %s\n", c, s, SECTOR_SIZE, data);
            printf("W %d %d\n", c, s);
            break;

        default:
            break;
    }
}

int main(int argc, char* argv[]) {
    if (argc != 4) {
        fprintf(stderr, "Usage: %s <DiskServerAddress> <#port> <#requests>\n", argv[0]);
        exit(1);
    }
    srand(time(NULL));

    int n_requests = atoi(argv[3]);
    char command[MAX_BUF_SIZE];
    char response[MAX_BUF_SIZE];

    // Connect to the server
    int sockfd = connect_to(argv[1], atoi(argv[2]));

    // Send the information request
    send_command(sockfd, "I");
    receive_response(sockfd, response);
#ifdef _DEBUG
    int len = strlen(response);
    for (int i = 0; i < len; i++) {
        printf("%d ", response[i]);
    }
    printf("\n");
#endif
    printf("I\n%s\n", response);

    int n_cylinders, n_sectors;
    sscanf(response, "%d %d", &n_cylinders, &n_sectors);

    struct timeval startTime, endTime;
    long run_time_in_microseconds;

    gettimeofday(&startTime, NULL);

    // Generate random commands
    for (int i = 0; i < n_requests; i++) {
        generate_commands(n_cylinders, n_sectors, command);
        send_command(sockfd, command);
        receive_response(sockfd, response);
        printf("%.3s\n\n", response);
    }

    gettimeofday(&endTime, NULL);

    // Send the exit command
    send_command(sockfd, "E");
    receive_response(sockfd, response);
    printf("E\nExiting...\n");
    close(sockfd);

    run_time_in_microseconds = (endTime.tv_sec - startTime.tv_sec) * 1000000 + (endTime.tv_usec - startTime.tv_usec);
    printf("Time used: %ld microseconds.\n", run_time_in_microseconds);

    return 0;
}