/**
 * UserOp.h
 *
 * Protocol for user operations
 */

#ifndef USEROP_H
#define USEROP_H

#include "UserFunc.h"

/**
 * @brief Initialize a user
 * @param user User struct: the user to be initialized, contains username and vol
 * @return int 0 if success, -1 if failed
 */
int u_init_user(User* user);

/**
 * @brief Format the volume
 * @param vol Volume struct: the volume to be formatted
 * @return int 0 if success, -1 if failed
 */
int u_format_volume(Volume* vol);

/**
 * @brief Create a file
 * @param user User struct: the user to create the file
 * @param filename char*: the filename to be created
 * @return int 0 if success, -1 if failed
 */
int u_make_file(User* user, char* filename);

/**
 * @brief Create a directory
 * @param user User struct: the user to create the directory
 * @param dirname char*: the directory name to be created
 * @return int 0 if success, -1 if failed
 */
int u_make_dir(User* user, char* dirname);

/**
 * @brief Remove a file
 * @param user User struct: the user to remove the file
 * @param filename char*: the filename to be removed
 * @return int 0 if success, -1 if failed
 */
int u_remove_file(User* user, char* filename);

/**
 * @brief Remove a directory
 * @param user User struct: the user to remove the directory
 * @param dirname char*: the directory name to be removed
 * @return int 0 if success, <0 if failed
 */
int u_remove_dir(User* user, char* dirname);

/**
 * @brief Change directory
 * @param user User struct: the user to be changed
 * @param dir char*: the directory to be changed to
 * @return int 0 if success, -1 if failed
 */
int u_change_dir(User* user, char* dir);

/**
 * @brief List directory contents
 * @param user User struct: the user to list the directory
 * @param is_long bool: true if long format
 * @return int 0 if success, -1 if failed
 */
int u_list_dir(User* user, bool is_long, char** data, int* len);

/**
 * @brief Print file status
 * @param user User struct: the user to print the file status
 * @param filename char*: the filename to be printed
 * @param data char**: the data to be printed
 * @param len int*: the length of data
 * @return int 0 if success, -1 if failed
 */
int u_stat_file(User* user, char* filename, char** data, int* len);

/**
 * @brief Print file contents
 * @param user User struct: the user to print the file
 * @param filename char*: the filename to be printed
 * @param data char**: the data to be printed
 * @param len int*: the length of data
 * @return int 0 if success, -1 if failed
 */
int u_cat_file(User* user, char* filename, char** data, int* len);

/**
 * @brief Write to a file
 * @param user User struct: the user to write the file
 * @param filename char*: the filename to be written
 * @param length int: the length of data
 * @param data char*: the data to be written
 * @return int 0 if success, -1 if failed
 */
int u_write_file(User* user, char* filename, int length, char* data);

/**
 * @brief Insert to a file
 * @param user User struct: the user to insert the file
 * @param filename char*: the filename to be inserted
 * @param pos int: the position to be inserted
 * @param length int: the length of data
 * @param data char*: the data to be inserted
 * @return int 0 if success, -1 if failed
 */
int u_insert_file(User* user, char* filename, int pos, int length, char* data);

/**
 * @brief Delete from a file
 * @param user User struct: the user to delete the file
 * @param filename char*: the filename to be deleted
 * @param pos int: the position to be deleted
 * @param length int: the length of data
 * @return int 0 if success, -1 if failed
 */
int u_delete_file(User* user, char* filename, int pos, int length);

#endif