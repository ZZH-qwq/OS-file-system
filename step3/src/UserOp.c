#include "UserOp.h"

int u_init_user(User* user) {
    if (user == NULL || user->vol == NULL) {
        return -1;
    }
    user->cur_dir.inodeptr->i_idx = 0;
    int res = read_inode(user->vol, user->cur_dir.inodeptr);
    if (res < 0) {
        print_err("init_user:\tFailed to read inode");
        return DISK_FAILURE;
    }
#ifdef _DEBUG
    printf("Inode: %d\n", user->cur_dir.inodeptr->i_idx);
    print_inode(user->vol, user->cur_dir.inodeptr, NULL, NULL);
#endif
    if (user->cur_dir.inodeptr->i_size == 0) {
        print_err("init_user:\tInvalid inode size");
        return INVALID_PATH;
    }
    FileType root;
    root.inodeptr = user->cur_dir.inodeptr;
    root.start_block = 0;
    res = read_file(user->vol, &root);
    if (res < 0) {
        print_err("init_user:\tFailed to read file");
        return -1;
    }
    res = parse_dir(&root, &user->cur_dir);
    if (res < 0) {
        print_err("init_user:\tFailed to parse dir");
        return -1;
    }
#ifdef _DEBUG
    print_dir(&user->cur_dir);
#endif
    _get_path(user);
    user->cache_time = time(NULL);
    return 0;
}

int u_format_volume(User* user, AuthList* list) {
    int res = format_disk(user->vol);
    if (res < 0) {
        print_err("format_volume:\tFailed to format disk");
        return -1;
    }
    // Create root directory
    Inode root_inode;
    res = allocate_inode(user->vol);
    if (res != 0) {
        print_err("format_volume:\tFailed to allocate inode");
        return -1;
    }
    root_inode.i_idx = 0;
    root_inode.i_mode = INODE_DIR;
    root_inode.i_nlink = 1;
    root_inode.i_uid = 0;
    root_inode.i_prem = 075;
    FileType root_file;
    root_file.inodeptr = &root_inode;
    root_file.start_block = 0;
    res = init_dir(&root_file, true);
    if (res < 0) {
        print_err("format_volume:\tFailed to encode root dir");
        return -1;
    }
    res = write_file(user->vol, &root_file);
    free(root_file.data);
    if (res < 0) {
        print_err("format_volume:\tFailed to write root file");
        return -1;
    }
    // Set user as root
    user->id = 0;
    strcpy(user->username, "root");
    free_dir(&user->cur_dir);
    res = u_init_user(user);
    // Create home dir
    res = u_make_dir(user, "home");
    if (res < 0) {
        print_err("format_volume:\tFailed to make home dir");
        return -1;
    }
    // Create auth list
    res = u_make_file(user, "auth");
    if (res < 0) {
        print_err("format_volume:\tFailed to make auth file");
        return -1;
    }
    res = read_inode(user->vol, list->inodeptr);
    if (res < 0) {
        print_err("format_volume:\tFailed to read auth inode");
        return -1;
    }
    list->inodeptr->i_prem = 070;
    free(list->users);
    res = init_auth_list(list);
    if (res < 0) {
        print_err("format_volume:\tFailed to init auth list");
        return -1;
    }
    res = save_auth_list(user->vol, list);
    if (res < 0) {
        print_err("format_volume:\tFailed to save auth list");
        return -1;
    }
    res = u_change_dir(user, "home");
    if (res < 0) {
        print_err("format_volume:\tFailed to change directory");
        return res;
    }
    res = u_make_dir(user, "public");
    if (res < 0) {
        print_err("format_volume:\tFailed to make public dir");
        return res;
    }
    res = u_change_mode(user, "public", 077);
    if (res < 0) {
        print_err("format_volume:\tFailed to change mode");
        return res;
    }
    return 0;
}

int u_make_file(User* user, char* filename) {
    Inode vinode = *user->cur_dir.inodeptr;
    User vuser;
    vuser.cur_dir.inodeptr = &vinode;
    char* target;
    int res = _user_target_locator(user, filename, false, &vuser, &target);
    if (res < 0) {
        print_err("make_file:\tFailed to locate target");
        free_dir(&vuser.cur_dir);
        return res;
    }
    res = _make_target_entry(&vuser, target, INODE_FILE);
    if (res < 0) {
        print_err("make_file:\tFailed to make target entry");
        free_dir(&vuser.cur_dir);
        return res;
    }
    Inode inode;
    inode.i_idx = res;
    inode.i_mode = INODE_FILE;
    inode.i_size = 0;
    inode.i_blocks = 0;
    inode.i_parent = vuser.cur_dir.inodeptr->i_idx;
    inode.i_nlink = 1;
    inode.i_uid = user->id;
    inode.i_prem = 064;
    inode.i_ctime = time(NULL);
    inode.i_mtime = inode.i_ctime;
    inode.i_atime = inode.i_ctime;
    res = write_inode(user->vol, &inode);
    free_dir(&vuser.cur_dir);
    if (res < 0) {
        print_err("make_file:\tFailed to write inode");
        return DISK_FAILURE;
    }
    res = _check_cache(user);
    bool flush_dir = res == FLUSH_DIR;
    if (res < 0) {
        print_err("make_file:\tFailed to check cache");
        return DISK_FAILURE;
    }
    res = save_meta_blocks(user->vol);
    if (res < 0) {
        print_err("make_file:\tFailed to save meta blocks");
        return DISK_FAILURE;
    }
    return flush_dir ? FLUSH_DIR : 0;
}

int u_make_dir(User* user, char* dirname) {
    Inode vinode = *user->cur_dir.inodeptr;
    User vuser;
    vuser.cur_dir.inodeptr = &vinode;
    char* target;
    int res = _user_target_locator(user, dirname, true, &vuser, &target);
    if (res < 0) {
        print_err("make_dir:\tFailed to locate target");
        free_dir(&vuser.cur_dir);
        return res;
    }
    res = _make_target_entry(&vuser, target, INODE_DIR);
    if (res < 0) {
        print_err("make_dir:\tFailed to make target entry");
        free_dir(&vuser.cur_dir);
        return res;
    }
    Inode inode;
    inode.i_idx = res;
    inode.i_mode = INODE_DIR;
    inode.i_size = 0;
    inode.i_blocks = 0;
    inode.i_parent = vuser.cur_dir.inodeptr->i_idx;
    inode.i_nlink = 1;
    inode.i_uid = user->id;
    inode.i_prem = 075;
    inode.i_ctime = time(NULL);
    inode.i_mtime = inode.i_ctime;
    inode.i_atime = inode.i_ctime;
    FileType file;
    file.inodeptr = &inode;
    file.start_block = 0;
    res = init_dir(&file, false);
    res = write_file(user->vol, &file);
    free(file.data);
    free_dir(&vuser.cur_dir);
    if (res < 0) {
        print_err("make_dir:\tFailed to write file");
        return DISK_FAILURE;
    }
    res = _check_cache(user);
    bool flush_dir = res == FLUSH_DIR;
    if (res < 0) {
        print_err("make_dir:\tFailed to check cache");
        return DISK_FAILURE;
    }
    res = save_meta_blocks(user->vol);
    if (res < 0) {
        print_err("make_dir:\tFailed to save meta blocks");
        return DISK_FAILURE;
    }
    return flush_dir ? FLUSH_DIR : 0;
}

int u_remove_file(User* user, char* filename) {
    Inode vinode = *user->cur_dir.inodeptr;
    User vuser;
    vuser.cur_dir.inodeptr = &vinode;
    char* target;
    int res = _user_target_locator(user, filename, true, &vuser, &target);
    if (res < 0) {
        print_err("u_remove_file:\tFailed to locate target");
        free_dir(&vuser.cur_dir);
        return res;
    }
    size_t len = strlen(target);
    DirEntry* entry = _search_dir(&vuser.cur_dir, target, len);
    if (entry == NULL) {
        print_err("u_remove_file:\tFile not found");
        free_dir(&vuser.cur_dir);
        return TARGET_NOT_FOUND;
    }
    int inode_idx = entry->inode_idx;
    Inode inode;
    inode.i_idx = inode_idx;
    res = read_inode(user->vol, &inode);
    if (res < 0) {
        print_err("u_remove_file:\tFailed to read inode");
        free_dir(&vuser.cur_dir);
        return DISK_FAILURE;
    }
    // Check premission
    bool user_prem = inode.i_uid == user->id && (inode.i_prem & USER_W) != 0;
    bool other_prem = inode.i_uid != user->id && (inode.i_prem & OTHER_W) != 0;
    if (user->id != 0 && !user_prem && !other_prem) {
        print_err("u_remove_file:\tPermission denied");
        free_dir(&vuser.cur_dir);
        return PERMISSION_DENIED;
    }
    if (inode.i_mode != INODE_FILE) {
        print_err("u_remove_file:\tNot a file");
        free_dir(&vuser.cur_dir);
        return INVALID_PATH;
    }
    res = _remove_target_entry(&vuser, entry - vuser.cur_dir.dir_entries, len);
    free_dir(&vuser.cur_dir);
    if (res < 0) {
        return res;
    }
    res = _remove_inode_link(user->vol, inode_idx);
    if (res < 0) {
        return res;
    }
    res = _check_cache(user);
    bool flush_dir = res == FLUSH_DIR;
    if (res < 0) {
        print_err("u_remove_file:\tFailed to check cache");
        return DISK_FAILURE;
    }
    res = save_meta_blocks(user->vol);
    if (res < 0) {
        print_err("u_remove_file:\tFailed to save meta blocks");
        return DISK_FAILURE;
    }
    return flush_dir ? FLUSH_DIR : 0;
}

int u_remove_dir(User* user, char* dirname) {
    Inode vinode = *user->cur_dir.inodeptr;
    User vuser;
    vuser.cur_dir.inodeptr = &vinode;
    char* target;
    int res = _user_target_locator(user, dirname, true, &vuser, &target);
    if (res < 0) {
        print_err("u_remove_dir:\tFailed to locate target");
        free_dir(&vuser.cur_dir);
        return res;
    }
    size_t len = strlen(target);
    DirEntry* entry = _search_dir(&vuser.cur_dir, target, len);
    if (entry == NULL) {
        print_err("u_remove_dir:\tDirectory not found");
        free_dir(&vuser.cur_dir);
        return TARGET_NOT_FOUND;
    }
    int inode_idx = entry->inode_idx;
    Inode inode;
    inode.i_idx = inode_idx;
    res = read_inode(user->vol, &inode);
    if (res < 0) {
        print_err("u_remove_dir:\tFailed to read inode");
        free_dir(&vuser.cur_dir);
        return DISK_FAILURE;
    }
    // Check premission
    bool user_prem = inode.i_uid == user->id && (inode.i_prem & USER_W) != 0;
    bool other_prem = inode.i_uid != user->id && (inode.i_prem & OTHER_W) != 0;
    if (user->id != 0 && !user_prem && !other_prem) {
        print_err("u_remove_file:\tPermission denied");
        free_dir(&vuser.cur_dir);
        return PERMISSION_DENIED;
    }
    if (inode.i_mode != INODE_DIR) {
        print_err("u_remove_dir:\tNot a directory");
        free_dir(&vuser.cur_dir);
        return INVALID_PATH;
    }
    if (inode.i_size > DIR_INIT_SIZE) {
        print_err("u_remove_dir:\tDirectory not empty");
        free_dir(&vuser.cur_dir);
        return DIR_NOT_EMPTY;
    }
    res = _remove_target_entry(&vuser, entry - vuser.cur_dir.dir_entries, len);
    free_dir(&vuser.cur_dir);
    if (res < 0) {
        return res;
    }
    res = _remove_inode_link(user->vol, inode_idx);
    if (res < 0) {
        return res;
    }
    res = _check_cache(user);
    bool flush_dir = res == FLUSH_DIR;
    if (res < 0) {
        print_err("u_remove_dir:\tFailed to check cache");
        return DISK_FAILURE;
    }
    res = save_meta_blocks(user->vol);
    if (res < 0) {
        print_err("u_remove_dir:\tFailed to save meta blocks");
        return DISK_FAILURE;
    }
    return flush_dir ? FLUSH_DIR : 0;
}

int u_change_dir(User* user, char* dir) {
    if (dir == NULL) {
        return INVALID_PATH;
    }
    Inode inode = *user->cur_dir.inodeptr;
    User tmp;
    tmp.cur_dir.inodeptr = &inode;
    int res = _user_pathfinder(user, &tmp, dir);
    if (res < 0) {
        print_err("u_change_dir:\tFailed to change dir");
        free_dir(&tmp.cur_dir);
        return res;
    }
    free_dir(&user->cur_dir);
    Inode* cinode = user->cur_dir.inodeptr;
    *cinode = *tmp.cur_dir.inodeptr;
    user->cur_dir = tmp.cur_dir;
    user->cur_dir.inodeptr = cinode;
    res = _get_path(user);
    if (res < 0) {
        return res;
    }
    user->cache_time = time(NULL);
    return 0;
}

int _cmpfunc(const void* a, const void* b) {
    return strcmp((*(DirEntry**)a)->filename, (*(DirEntry**)b)->filename);
}

int u_list_dir(User* user, AuthList* list, bool is_long, char** data, int* len) {
    static const char* prem_str[] = {"---", "--x", "-w-", "-wx", "r--", "r-x", "rw-", "rwx"};
    bool user_prem = user->cur_dir.inodeptr->i_uid == user->id && (user->cur_dir.inodeptr->i_prem & USER_R) != 0;
    bool other_prem = user->cur_dir.inodeptr->i_uid != user->id && (user->cur_dir.inodeptr->i_prem & OTHER_R) != 0;
    if (user->id != 0 && !user_prem && !other_prem) {
        return PERMISSION_DENIED;
    }
    u_int16_t init_items = (user->cur_dir.inodeptr->i_idx == 0) ? 1 : 2;
    // Sort in lexical order
    u_int16_t dir_size = user->cur_dir.dir_size;
    DirEntry** order = (DirEntry**)malloc(sizeof(DirEntry*) * (dir_size - init_items));
    if (order == NULL) {
        print_err("u_list_dir:\tFailed to malloc order");
        return -1;
    }
    for (int i = 0; i < dir_size - init_items; i++) {
        order[i] = &user->cur_dir.dir_entries[i + init_items];
    }
    qsort(order, dir_size - init_items, sizeof(DirEntry*), _cmpfunc);
    // Generate output
    char* output;
    // @todo long format
    if (is_long) {
        output = (char*)malloc(user->cur_dir.tot_size + user->cur_dir.dir_size * 60);
    } else {
        output = (char*)malloc(user->cur_dir.tot_size + user->cur_dir.dir_size * 10);
    }
    if (output == NULL) {
        print_err("u_list_dir:\tFailed to malloc output");
        free(order);
        return -1;
    }
    char* ptr = output;
    if (is_long) {
        Inode inode;
        for (int i = 0; i < dir_size - init_items; i++) {
            DirEntry* entry = order[i];
            inode.i_idx = entry->inode_idx;
            int res = read_inode(user->vol, &inode);
            if (res < 0) {
                print_err("u_list_dir:\tFailed to read inode");
                free(output);
                free(order);
                return DISK_FAILURE;
            }
            const char* mode_w = (inode.i_mode == INODE_DIR) ? "d" : "-";
            const char* user_p = prem_str[(inode.i_prem >> 3) & 0x7];
            const char* other_p = prem_str[inode.i_prem & 0x7];
            AuthUser* auth = search_uid(list, inode.i_uid);
            const char* owner = (auth == NULL) ? "unknown" : auth->username;
            time_t i_mtime = inode.i_mtime;
            char* time_w = ctime(&i_mtime);
            int time_len = strlen(time_w);
            time_w[time_len - 1] = '\0';
            ptr += sprintf(ptr, "%s%s%s%16.16s %6d %28s\t", mode_w, user_p, other_p, owner, inode.i_size, time_w);
            if (inode.i_mode == INODE_DIR) {
                ptr += sprintf(ptr, "\033[1;34m%.*s\033[0m\n", entry->filename_len, entry->filename);
            } else {
                ptr += sprintf(ptr, "%.*s\n", entry->filename_len, entry->filename);
            }
        }
    } else {
        for (int i = 0; i < dir_size - init_items; i++) {
            DirEntry* entry = order[i];
            if (entry->inode_mode == INODE_DIR) {
                ptr += sprintf(ptr, "\033[1;34m%.*s\033[0m  ", entry->filename_len, entry->filename);
            } else {
                ptr += sprintf(ptr, "%.*s  ", entry->filename_len, entry->filename);
            }
        }
        if (dir_size - init_items > 0) {
            ptr += sprintf(ptr, "\n");
        }
    }
    *ptr = '\0';
    *data = output;
    *len = ptr - output;
    free(order);
    return 0;
}

int u_stat_file(User* user, char* filename, char** data, int* len) {
    if (filename == NULL) {
        return INVALID_PATH;
    }
    Inode vinode = *user->cur_dir.inodeptr;
    User vuser;
    vuser.cur_dir.inodeptr = &vinode;
    char* target;
    int res = _user_target_locator(user, filename, true, &vuser, &target);
    if (res < 0) {
        print_err("u_stat_file:\tFailed to locate target");
        free_dir(&vuser.cur_dir);
        return res;
    }
    DirEntry* entry = _search_dir(&vuser.cur_dir, target, strlen(target));
    if (entry == NULL) {
        print_err("u_stat_file:\tFile not found");
        free_dir(&vuser.cur_dir);
        return TARGET_NOT_FOUND;
    }
    Inode inode;
    inode.i_idx = entry->inode_idx;
    res = read_inode(user->vol, &inode);
    if (res < 0) {
        print_err("u_stat_file:\tFailed to read inode");
        free_dir(&vuser.cur_dir);
        return DISK_FAILURE;
    }
    // Check premission
    bool user_prem = inode.i_uid == user->id && (inode.i_prem & USER_R) != 0;
    bool other_prem = inode.i_uid != user->id && (inode.i_prem & OTHER_R) != 0;
    if (user->id != 0 && !user_prem && !other_prem) {
        free_dir(&vuser.cur_dir);
        return PERMISSION_DENIED;
    }
    char* data_ptr;
    int data_len;
    print_inode(user->vol, &inode, &data_ptr, &data_len);
    data_ptr = realloc(data_ptr, data_len + entry->filename_len + 20);
    if (data_ptr == NULL) {
        print_err("u_stat_file:\tFailed to realloc data");
        free_dir(&vuser.cur_dir);
        return -1;
    }
    memmove(data_ptr + entry->filename_len + 9, data_ptr, data_len);
    sprintf(data_ptr, "  File: %.*s", entry->filename_len, entry->filename);
    data_ptr[entry->filename_len + 8] = '\n';
    *data = data_ptr;
    *len = data_len + entry->filename_len + 9;
    free_dir(&vuser.cur_dir);
    return 0;
}

int u_cat_file(User* user, char* filename, char** data, int* len) {
    if (filename == NULL) {
        return INVALID_PATH;
    }
    Inode vinode = *user->cur_dir.inodeptr;
    User vuser;
    vuser.cur_dir.inodeptr = &vinode;
    char* target;
    int res = _user_target_locator(user, filename, true, &vuser, &target);
    if (res < 0) {
        print_err("u_cat_file:\tFailed to locate target");
        free_dir(&vuser.cur_dir);
        return res;
    }
    DirEntry* entry = _search_dir(&vuser.cur_dir, target, strlen(target));
    if (entry == NULL) {
        print_err("u_cat_file:\tFile not found");
        free_dir(&vuser.cur_dir);
        return TARGET_NOT_FOUND;
    }
    Inode inode;
    inode.i_idx = entry->inode_idx;
    res = read_inode(user->vol, &inode);
#ifdef _DEBUG
    print_inode(user->vol, &inode, NULL, NULL);
#endif
    free_dir(&vuser.cur_dir);
    if (res < 0) {
        print_err("u_cat_file:\tFailed to read inode");
        return DISK_FAILURE;
    }
    bool user_prem = inode.i_uid == user->id && (inode.i_prem & USER_R) != 0;
    bool other_prem = inode.i_uid != user->id && (inode.i_prem & OTHER_R) != 0;
    if (user->id != 0 && !user_prem && !other_prem) {
        return PERMISSION_DENIED;
    }
    if (inode.i_mode != INODE_FILE) {
        print_err("u_cat_file:\tNot a file");
        return INVALID_PATH;
    }
    FileType file;
    file.inodeptr = &inode;
    file.start_block = 0;
    res = read_file(user->vol, &file);
    if (res < 0) {
        print_err("u_cat_file:\tFailed to read file");
        return DISK_FAILURE;
    }
    *data = file.data;
    *len = file.size;
    return 0;
}

int u_write_file(User* user, char* filename, int length, char* data) {
    if (filename == NULL) {
        return INVALID_PATH;
    }
    Inode vinode = *user->cur_dir.inodeptr;
    User vuser;
    vuser.cur_dir.inodeptr = &vinode;
    char* target;
    int res = _user_target_locator(user, filename, true, &vuser, &target);
    if (res < 0) {
        print_err("u_write_file:\tFailed to locate target");
        free_dir(&vuser.cur_dir);
        return res;
    }
    DirEntry* entry = _search_dir(&vuser.cur_dir, target, strlen(target));
    if (entry == NULL) {
        print_err("u_write_file:\tFile not found");
        free_dir(&vuser.cur_dir);
        return TARGET_NOT_FOUND;
    }
    Inode inode;
    inode.i_idx = entry->inode_idx;
    res = read_inode(user->vol, &inode);
    free_dir(&vuser.cur_dir);
    if (res < 0) {
        print_err("u_write_file:\tFailed to read inode");
        return DISK_FAILURE;
    }
    // Check premission
    bool user_prem = inode.i_uid == user->id && (inode.i_prem & USER_W) != 0;
    bool other_prem = inode.i_uid != user->id && (inode.i_prem & OTHER_W) != 0;
    if (user->id != 0 && !user_prem && !other_prem) {
        return PERMISSION_DENIED;
    }
    // Check if it's a file
    if (inode.i_mode != INODE_FILE) {
        print_err("u_write_file:\tNot a file");
        return INVALID_PATH;
    }
    // Construct file
    FileType file;
    file.inodeptr = &inode;
    file.size = length;
    file.start_block = 0;
    file.data = data;
    res = write_file(user->vol, &file);
    if (res < 0) {
        print_err("u_write_file:\tFailed to write file");
        return DISK_FAILURE;
    }
    return 0;
}

int u_insert_file(User* user, char* filename, int pos, int length, char* data) {
    if (filename == NULL) {
        return INVALID_PATH;
    }
    if (pos < 0 || length < 0) {
        return INVALID_POS;
    }
    Inode vinode = *user->cur_dir.inodeptr;
    User vuser;
    vuser.cur_dir.inodeptr = &vinode;
    char* target;
    int res = _user_target_locator(user, filename, true, &vuser, &target);
    if (res < 0) {
        print_err("u_insert_file:\tFailed to locate target");
        free_dir(&vuser.cur_dir);
        return res;
    }
    DirEntry* entry = _search_dir(&vuser.cur_dir, target, strlen(target));
    if (entry == NULL) {
        print_err("u_insert_file:\tFile not found");
        free_dir(&vuser.cur_dir);
        return TARGET_NOT_FOUND;
    }
    Inode inode;
    inode.i_idx = entry->inode_idx;
    res = read_inode(user->vol, &inode);
    free_dir(&vuser.cur_dir);
    if (res < 0) {
        print_err("u_insert_file:\tFailed to read inode");
        return DISK_FAILURE;
    }
    // Check premission
    bool user_prem = inode.i_uid == user->id && (inode.i_prem & USER_W) != 0;
    bool other_prem = inode.i_uid != user->id && (inode.i_prem & OTHER_W) != 0;
    if (user->id != 0 && !user_prem && !other_prem) {
        return PERMISSION_DENIED;
    }
    // Check if it's a file
    if (inode.i_mode != INODE_FILE) {
        print_err("u_insert_file:\tNot a file");
        return INVALID_PATH;
    }
    // Calculate start block
    if ((u_int32_t)pos > inode.i_size) {
        pos = inode.i_size;
    }
    u_int32_t start_block = pos / SIZE_BLOCK;
    FileType file;
    file.inodeptr = &inode;
    file.start_block = start_block;
    res = read_file(user->vol, &file);
    if (res < 0) {
        print_err("u_insert_file:\tFailed to read file");
        return DISK_FAILURE;
    }
    // Calculate new size
    u_int32_t new_size = file.size + length;
    u_int32_t new_data_len = new_size - start_block * SIZE_BLOCK;
    // Reallocate data
    file.data = (char*)realloc(file.data, new_data_len);
    if (file.data == NULL) {
        print_err("u_insert_file:\tFailed to realloc data");
        return -1;
    }
    // Shift data
    u_int32_t pos_start = pos % SIZE_BLOCK;
    u_int32_t pos_end = pos_start + length;
    memmove(file.data + pos_end, file.data + pos_start, new_data_len - pos_end);
    memcpy(file.data + pos_start, data, length);
    // Update file
    file.size = new_size;
    res = write_file(user->vol, &file);
    free(file.data);
    if (res < 0) {
        print_err("u_insert_file:\tFailed to write file");
        return DISK_FAILURE;
    }
    return 0;
}

int u_delete_file(User* user, char* filename, int pos, int length) {
    if (filename == NULL) {
        return INVALID_PATH;
    }
    if (pos < 0 || length < 0) {
        return INVALID_POS;
    }
    Inode vinode = *user->cur_dir.inodeptr;
    User vuser;
    vuser.cur_dir.inodeptr = &vinode;
    char* target;
    int res = _user_target_locator(user, filename, true, &vuser, &target);
    if (res < 0) {
        print_err("u_delete_file:\tFailed to locate target");
        free_dir(&vuser.cur_dir);
        return res;
    }
    DirEntry* entry = _search_dir(&vuser.cur_dir, target, strlen(target));
    if (entry == NULL) {
        print_err("u_delete_file:\tFile not found");
        free_dir(&vuser.cur_dir);
        return TARGET_NOT_FOUND;
    }
    Inode inode;
    inode.i_idx = entry->inode_idx;
    res = read_inode(user->vol, &inode);
    free_dir(&vuser.cur_dir);
    if (res < 0) {
        print_err("u_delete_file:\tFailed to read inode");
        return DISK_FAILURE;
    }
    // Check premission
    bool user_prem = inode.i_uid == user->id && (inode.i_prem & USER_W) != 0;
    bool other_prem = inode.i_uid != user->id && (inode.i_prem & OTHER_W) != 0;
    if (user->id != 0 && !user_prem && !other_prem) {
        return PERMISSION_DENIED;
    }
    // Check if it's a file
    if (inode.i_mode != INODE_FILE) {
        print_err("u_delete_file:\tNot a file");
        return INVALID_PATH;
    }
    // Calculate start block
    if ((u_int32_t)pos >= inode.i_size) {
        print_err("u_delete_file:\tInvalid position");
        return INVALID_POS;
    }
    if ((u_int32_t)pos + length > inode.i_size) {
        length = inode.i_size - pos;
    }
    u_int32_t start_block = pos / SIZE_BLOCK;
    // Calculate new size
    u_int32_t new_size = inode.i_size - length;
    u_int32_t new_data_len = new_size - start_block * SIZE_BLOCK;
    u_int32_t pos_offset = pos % SIZE_BLOCK;
    // Construct file
    FileType file;
    file.inodeptr = &inode;
    file.start_block = start_block;
    if (length < SIZE_BLOCK) {
        // Read continous block
        res = read_file(user->vol, &file);
        if (res < 0) {
            print_err("u_delete_file:\tFailed to read file");
            return DISK_FAILURE;
        }
        // Shift data
        u_int32_t end_offset = pos_offset + length;
        u_int32_t end_len = inode.i_size - pos - length;
        memmove(file.data + pos_offset, file.data + end_offset, end_len);
    } else {
        // Some hacky way to only read one block containing the pos
        u_int32_t origin_size = inode.i_size;
        u_int32_t hack_size = (start_block + 1) * SIZE_BLOCK;
        u_int32_t origin_block = inode.i_blocks;
        u_int32_t hack_block = start_block + 1;
        inode.i_size = hack_size;
        inode.i_blocks = hack_block;
        res = read_file(user->vol, &file);
        // Restore size
        inode.i_size = origin_size;
        inode.i_blocks = origin_block;
        if (res < 0) {
            print_err("u_delete_file:\tFailed to read file");
            return DISK_FAILURE;
        }
        // Reallocate data
        file.data = (char*)realloc(file.data, new_data_len);
        if ((u_int32_t)length != origin_size - pos) {
            // Read the content after deletion end
            u_int32_t end_pos = pos + length;
            u_int32_t end_block = end_pos / SIZE_BLOCK;
            FileType end;
            end.inodeptr = &inode;
            end.start_block = end_block;
            res = read_file(user->vol, &end);
            if (res < 0) {
                print_err("u_delete_file:\tFailed to read file");
                return DISK_FAILURE;
            }
            // Shift data
            u_int32_t end_offset = end_pos % SIZE_BLOCK;
            u_int32_t end_len = origin_size - end_pos;
            memcpy(file.data + pos_offset, end.data + end_offset, end_len);
            free(end.data);
        }
    }
    // Update file
    file.size = new_size;
    res = write_file(user->vol, &file);
    free(file.data);
    if (res < 0) {
        print_err("u_delete_file:\tFailed to write file");
        return DISK_FAILURE;
    }
    return 0;
}

int u_add_user(User* user, AuthList* list, char* username) {
    if (username == NULL) {
        return INVALID_PATH;
    }
    if (user->id != 0) {
        return PERMISSION_DENIED;
    }
    AuthUser* auth = search_user(list, username);
    if (auth != NULL || strcmp(username, "root") == 0) {
        return INVALID_USER;
    }
    int id = create_auth(list, username);
#ifdef _DEBUG
    print_auth(list, NULL, NULL);
#endif
    if (id < 0) {
        print_err("u_add_user:\tFailed to create user");
        return -1;
    }
    char user_home[128];
    sprintf(user_home, "/home/%s", username);
    int res = u_make_dir(user, user_home);
    if (res < 0) {
        print_err("u_add_user:\tFailed to create home dir");
        remove_auth(list, id, true);
        return res;
    }
    sprintf(user_home, "/home/%s", username);
    res = u_change_mode(user, user_home, 070);
    if (res < 0) {
        print_err("u_add_user:\tFailed to change mode");
        remove_auth(list, id, true);
        return res;
    }
    sprintf(user_home, "/home/%s", username);
    res = u_change_owner(user, list, user_home, username);
    if (res < 0) {
        print_err("u_add_user:\tFailed to change owner");
        remove_auth(list, id, true);
        return res;
    }
    // Save auth list
    res = save_auth_list(user->vol, list);
    if (res < 0) {
        print_err("u_add_user:\tFailed to save auth list");
        remove_auth(list, id, true);
        return res;
    }
    return 0;
}

int u_remove_user(User* user, AuthList* list, char* username) {
    if (username == NULL) {
        return INVALID_PATH;
    }
    if (user->id != 0 || strcmp(username, "root") == 0) {
        return PERMISSION_DENIED;
    }
    AuthUser* auth = search_user(list, username);
    if (auth == NULL) {
        return INVALID_USER;
    }
    int id = auth->id;
    int res = remove_auth(list, id, false);
    if (res < 0) {
        print_err("u_remove_user:\tFailed to remove user");
        return -1;
    }
    // Save auth list
    res = save_auth_list(user->vol, list);
    if (res < 0) {
        print_err("u_remove_user:\tFailed to save auth list");
        return res;
    }
    return id;
}

int u_change_password(User* user, AuthList* list, char* username, u_int32_t* hashed_pwd) {
    if (username == NULL) {
        return INVALID_PATH;
    }
    AuthUser* auth = search_user(list, username);
    if (auth == NULL) {
        return INVALID_USER;
    }
    if (user->id != 0 && user->id != auth->id) {
        return PERMISSION_DENIED;
    }
    memcpy(auth->hashed_password, hashed_pwd, 32);
    // Save auth list
    int res = save_auth_list(user->vol, list);
    if (res < 0) {
        print_err("u_change_password:\tFailed to save auth list");
        return res;
    }
    return 0;
}

int u_switch_user(User* user, AuthList* list, char* username, u_int32_t* hashed_pwd) {
    AuthUser* auth = search_user(list, username);
    if (auth == NULL) {
        return INVALID_USER;
    }
    if (memcmp(auth->hashed_password, hashed_pwd, 32) != 0) {
        return PASSWORD_MISMATCH;
    }
    user->id = auth->id;
    strcpy(user->username, auth->username);
    free_dir(&user->cur_dir);
    int res = u_init_user(user);
    if (res < 0) {
        return res;
    }
    res = u_change_dir(user, auth->user_home);
    if (res < 0) {
        return res;
    }
    return 0;
}

int u_change_mode(User* user, char* filename, u_int16_t mode) {
    if (filename == NULL) {
        return INVALID_PATH;
    }
    Inode vinode = *user->cur_dir.inodeptr;
    User vuser;
    vuser.cur_dir.inodeptr = &vinode;
    char* target;
    int res = _user_target_locator(user, filename, true, &vuser, &target);
    if (res < 0) {
        print_err("u_change_mode:\tFailed to locate target");
        free_dir(&vuser.cur_dir);
        return res;
    }
    DirEntry* entry = _search_dir(&vuser.cur_dir, target, strlen(target));
    if (entry == NULL) {
        print_err("u_change_mode:\tFile not found");
        free_dir(&vuser.cur_dir);
        return TARGET_NOT_FOUND;
    }
    Inode inode;
    inode.i_idx = entry->inode_idx;
    res = read_inode(user->vol, &inode);
    free_dir(&vuser.cur_dir);
    if (res < 0) {
        print_err("u_change_mode:\tFailed to read inode");
        return DISK_FAILURE;
    }
    res = _change_inode_prem(user->vol, &inode, mode, user->id);
    if (res < 0) {
        print_err("u_change_mode:\tFailed to change mode");
        return res;
    }
    return 0;
}

int u_change_owner(User* user, AuthList* list, char* filename, char* new_owner) {
    if (filename == NULL || new_owner == NULL) {
        return INVALID_PATH;
    }
    AuthUser* auth = search_user(list, new_owner);
    if (auth == NULL) {
        return INVALID_USER;
    }
    Inode vinode = *user->cur_dir.inodeptr;
    User vuser;
    vuser.cur_dir.inodeptr = &vinode;
    char* target;
    int res = _user_target_locator(user, filename, true, &vuser, &target);
    if (res < 0) {
        print_err("u_change_owner:\tFailed to locate target");
        free_dir(&vuser.cur_dir);
        return res;
    }
    DirEntry* entry = _search_dir(&vuser.cur_dir, target, strlen(target));
    if (entry == NULL) {
        print_err("u_change_owner:\tFile not found");
        free_dir(&vuser.cur_dir);
        return TARGET_NOT_FOUND;
    }
    Inode inode;
    inode.i_idx = entry->inode_idx;
    res = read_inode(user->vol, &inode);
    free_dir(&vuser.cur_dir);
    if (res < 0) {
        print_err("u_change_owner:\tFailed to read inode");
        return DISK_FAILURE;
    }
    res = _change_inode_owner(user->vol, &inode, auth->id, user->id);
    if (res < 0) {
        print_err("u_change_owner:\tFailed to change owner");
        return res;
    }
    return 0;
}