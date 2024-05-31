/*
 * ServerCore.h
 *
 * Listening and accepting connection
 * Managing multiple clients
 * Ref: https://www.geeksforgeeks.org/socket-programming-in-cc-handling-multiple-clients-on-server-without-multi-threading/
 */

#ifndef SERVERCORE_h
#define SERVERCORE_h

#include <unistd.h>
#include <errno.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <netinet/in.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>

#define MAX_CLIENTS 30
#define MAX_BUF_SIZE 8192

int init_server(struct sockaddr_in *address, int PORT);

int accept_new(int master_socket, struct sockaddr_in *address, int *addrlen);

void disconnect(int *client_socket, pid_t *pid_list, int status);

void check_exit(fd_set *controlfds, int *client_socket, pid_t *pid_list);

#endif