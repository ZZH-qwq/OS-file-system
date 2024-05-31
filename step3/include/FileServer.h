/**
 * FileServer.h
 *
 * Server-side operations for file system
 */

#ifndef FILESERVER_H
#define FILESERVER_H

#include "UserOp.h"
#include "CmdEncoder.h"
#include "ClientCore.h"

/**
 * @brief Process a command from the user
 * @param user User struct: the user to process the command
 * @return int 0 if success, <0 if failed
 */
int fs_process_cmd(User* user, AuthList* list, CmdHeader* command, char* data);

/**
 * @brief Get the command from the user
 */
int fs_get_cmd(User* user, char* cmd, char* data);

/**
 * @brief Login to the file server
 */
int fs_login(AuthUser* auth, int sockfd);

int fs_auth_user(User* user, AuthList* list);

/**
 * @brief Check the length of the data with the expected length
 */
int fs_data_len_check(char* cmd, char* data_ptr, int data_len, int res);

/**
 * @brief Print the current working directory
 * @param user User struct: the user to print the working directory
 * @return int 0 if success, -1 if failed
 */
int fs_print_work_dir(User* user);

/**
 * @brief Send a response to the user and wait for confirmation
 * @param user User struct: the user to send the response
 * @param message char*: the message to be sent
 * @param len int: the length of the message
 * @return int 0 if success, -1 if failed
 */
int fs_respond(User* user, char* message, int len);

/**
 * @brief Send a warning to the user
 */
int fs_warning(User* user, char* message, int len);

/**
 * @brief Send an error to the user
 */
int fs_error(User* user, char* message, int len);

/**
 * @brief Disconnect the user
 */
int fs_disconnect(User* user);

/**
 * @brief Exit the file server
 * @param user_list User*: the list of users
 * @return int 0 if success, -1 if failed
 */
int fs_exit(User* user_list);

#endif