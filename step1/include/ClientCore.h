/*
 * ClientCore.h
 *
 * Client core functions
 */

#ifndef CLIENTCORE_H
#define CLIENTCORE_H

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <time.h>
#include <sys/time.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h>
#include <stdbool.h>

#define MAX_BUF_SIZE 1024

int connect_to(char *hostname, int port);

void send_command(int sockfd, char *command);

int receive_response(int sockfd, char *buffer);

#endif