/**
 * UserAuth.h
 *
 * User authentication and management
 */

#ifndef USERAUTH_h
#define USERAUTH_h

#include "UserFunc.h"

typedef struct AuthUser {
    u_int16_t name_len;
    u_int16_t home_len;
    u_int32_t hashed_password[8];
    char username[64];
    u_int16_t id;
    char user_home[128];
} AuthUser;

typedef struct AuthList {
    Inode* inodeptr;
    AuthUser* users;
    u_int16_t list_size;
    u_int16_t main_key;
    u_int16_t tot_size;
} AuthList;

#define AUTH_INODE 2
#define AUTH_USER_OFFSET 4

int init_auth_list(AuthList* list);

int load_auth_list(Volume* vol, AuthList* list);

int save_auth_list(Volume* vol, AuthList* list);

int parse_auth(FileType* file, AuthList* user);

int encode_auth(AuthList* user, FileType* file);

AuthUser* search_user(AuthList* list, char* username);

AuthUser* search_uid(AuthList* list, u_int16_t uid);

int create_auth(AuthList* list, char* username);

int remove_auth(AuthList* list, u_int16_t uid, bool recycle);

int set_password(AuthUser* user, u_int32_t* hashed_pwd);

int print_auth(AuthList* users, char** data, int* len);

void cal_hash(AuthUser* user, unsigned char* input, u_int64_t len);

#endif