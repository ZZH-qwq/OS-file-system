/**
 * UserFunc.h
 *
 * Helper functions for user operations
 * Should be private, but C doesn't have private functions
 */

#ifndef USERFUNC_H
#define USERFUNC_H

#include "Files.h"

#define FLUSH_DIR 1

#define GENERIC_ERROR -1
#define TARGET_NOT_FOUND -2
#define TARGET_EXISTS -3
#define DIR_NOT_EMPTY -4
#define INVALID_PATH -5
#define INVALID_POS -6
#define DISK_FAILURE -7

typedef struct User {
    int sockfd;
    Volume* vol;
    DirType cur_dir;
    u_int32_t cache_time;
    char path[1024];
    char username[64];
} User;

int _get_root_dir(User* user);

int _change_dir_relative(User* user, char* targetpath);

int _copy_user(User* dest, User* src);

int _user_pathfinder(User* src, User* vuser, char* path);

int _user_target_locator(User* src, char* targetpath, bool is_dir, User* vuser, char** target);

int _check_cache(User* user);

int _get_path(User* user);

DirEntry* _search_dir(DirType* dir, char* targetname, int len);

DirEntry* _search_parent(DirType* parent, u_int16_t inode_idx);

int _make_target_entry(User* user, char* targetname, u_int16_t mode);

int _remove_target_entry(User* user, int entry_idx, size_t len);

int _remove_inode_link(Volume* vol, int inode_idx);

#endif