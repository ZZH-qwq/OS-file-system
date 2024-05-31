#include "UserFunc.h"

int _get_root_dir(User* user) {
    user->cur_dir.inodeptr->i_idx = 0;
    int res = read_inode(user->vol, user->cur_dir.inodeptr);
    if (res < 0) {
        print_err("_get_root_dir:\tFailed to read inode");
        user->cur_dir.dir_entries = NULL;
        user->cur_dir.orig_data = NULL;
        return DISK_FAILURE;
    }
    FileType root;
    root.inodeptr = user->cur_dir.inodeptr;
    root.start_block = 0;
    res = read_file(user->vol, &root);
    if (res < 0) {
        print_err("_get_root_dir:\tFailed to read file");
        user->cur_dir.dir_entries = NULL;
        user->cur_dir.orig_data = NULL;
        return DISK_FAILURE;
    }
    DirType* dir = &user->cur_dir;
    res = parse_dir(&root, dir);
    if (res < 0) {
        print_err("_get_root_dir:\tFailed to parse dir");
        user->cur_dir.dir_entries = NULL;
        user->cur_dir.orig_data = NULL;
        return DISK_FAILURE;
    }
    user->cache_time = time(NULL);
    return 0;
}

int _change_dir_relative(User* user, char* targetpath) {
    DirType* dir = &user->cur_dir;
    char* ptr = targetpath;
    while (ptr != NULL && *ptr != '\0') {
        if (*ptr == '/') {
            return INVALID_PATH;
        }
        // Get target name
        char* next = strchr(ptr, '/');
        int len = next == NULL ? (int)strlen(ptr) : next - ptr;
#ifdef _DEBUG
        printf("_change_dir_relative:\tTarget: %.*s\n", len, ptr);
#endif
        // Search for target
        DirEntry* entry = _search_dir(dir, ptr, len);
        if (entry == NULL) {
            return TARGET_NOT_FOUND;
        }
#ifdef _DEBUG
        printf("_change_dir_relative:\tEntry: %u, %u\n", entry->inode_mode, entry->inode_idx);
#endif
        // Check if target is a directory
        if (entry->inode_mode != INODE_DIR) {
            return INVALID_PATH;
        }
        // Read target directory
        Inode* inode = dir->inodeptr;
        inode->i_idx = entry->inode_idx;
        int res = read_inode(user->vol, inode);
        if (res < 0) {
            print_err("_change_dir_relative:\tFailed to read inode");
            return DISK_FAILURE;
        }
#ifdef _DEBUG
        print_inode(user->vol, inode, NULL, NULL);
#endif
        FileType file;
        file.inodeptr = inode;
        file.start_block = 0;
        res = read_file(user->vol, &file);
        if (res < 0) {
            print_err("_change_dir_relative:\tFailed to read file");
            return DISK_FAILURE;
        }
        free_dir(dir);
        res = parse_dir(&file, dir);
        if (res < 0) {
            print_err("_change_dir_relative:\tFailed to parse dir");
            return DISK_FAILURE;
        }
        ptr = (next == NULL ? next : next + 1);
    }
    return 0;
}

int _copy_user(User* dest, User* src) {
    dest->sockfd = src->sockfd;
    dest->vol = src->vol;
    Inode* vinode = dest->cur_dir.inodeptr;
    dest->cur_dir = src->cur_dir;
    dest->cur_dir.inodeptr = vinode;
    DirType* dir = &dest->cur_dir;
    *dir->inodeptr = *src->cur_dir.inodeptr;
    dir->orig_data = (char*)malloc(dir->tot_size);
    if (dir->orig_data == NULL) {
        print_err("_copy_user:\tFailed to malloc dir data");
        dir->dir_entries = NULL;
        return -1;
    }
    memcpy(dir->orig_data, src->cur_dir.orig_data, dir->tot_size);
    dir->dir_entries = (DirEntry*)malloc(sizeof(DirEntry) * dir->dir_size);
    if (dir->dir_entries == NULL) {
        print_err("_copy_user:\tFailed to malloc dir entries");
        free_dir(dir);
        return -1;
    }
    if (src->cur_dir.orig_data == NULL) {
        memcpy(dir->dir_entries, src->cur_dir.dir_entries, sizeof(DirEntry) * dir->dir_size);
        return 0;
    }
    for (int i = 0; i < dir->dir_size; i++) {
        DirEntry* src_entry = &src->cur_dir.dir_entries[i];
        DirEntry* dest_entry = &dir->dir_entries[i];
        dest_entry->inode_mode = src_entry->inode_mode;
        dest_entry->inode_idx = src_entry->inode_idx;
        dest_entry->filename_len = src_entry->filename_len;
        dest_entry->filename = dir->orig_data + (src_entry->filename - src->cur_dir.orig_data);
    }
    return 0;
}

int _user_pathfinder(User* src, User* vuser, char* path) {
    if (*path == '/') {
        // Absolute path
        path++;
        vuser->sockfd = src->sockfd;
        vuser->vol = src->vol;
        _get_root_dir(vuser);
    } else {
        // Relative path
        _copy_user(vuser, src);
    }
    int res = _change_dir_relative(vuser, path);
    if (res < 0) {
        print_err("_user_pathfinder:\tFailed to change dir");
        return res;
    }
    return 0;
}

int _user_target_locator(User* src, char* targetpath, bool is_dir, User* vuser, char** target) {
    // If is a valid path
    size_t len = strlen(targetpath);
    if (len == 0) {
        return INVALID_PATH;
    }
    if (targetpath[len - 1] == '/') {
        if (!is_dir) {
            return INVALID_PATH;
        } else {
            targetpath[len - 1] = '\0';
            len--;
        }
    }
    // Seperate path and target
    char* ptr = targetpath + len - 1;
    while (ptr != targetpath && *ptr != '/') {
        ptr--;
    }
    if (ptr == targetpath && *ptr == '/') {
        // Root directory
        *target = targetpath + 1;
        vuser->sockfd = src->sockfd;
        vuser->vol = src->vol;
        _get_root_dir(vuser);
        return 0;
    } else if (ptr == targetpath) {
        // Current directory
        *target = targetpath;
        _copy_user(vuser, src);
        return 0;
    }
    *ptr = '\0';
    *target = ptr + 1;
    // Change directory
    int res = _user_pathfinder(src, vuser, targetpath);
    if (res < 0) {
        print_err("_user_target_locator:\tFailed to change dir");
        return res;
    }
    return 0;
}

int _check_cache(User* user) {
    int res = read_inode(user->vol, user->cur_dir.inodeptr);
    u_int32_t mod_time = user->cur_dir.inodeptr->i_mtime;
#ifdef _DEBUG
    printf("Mod time: %u, Cache time: %u\n", mod_time, user->cache_time);
#endif
    if (mod_time < user->cache_time) {
        return 0;
    }
    // Update cache
    res = read_inode(user->vol, user->cur_dir.inodeptr);
    if (res < 0) {
        print_err("_check_cache:\tFailed to read inode");
        return DISK_FAILURE;
    }
    if (user->cur_dir.inodeptr->i_mode != INODE_DIR) {
#ifdef _DEBUG
        printf("_check_cache:\tBack to root\n");
#endif
        free_dir(&user->cur_dir);
        _get_root_dir(user);
        strcpy(user->path, "/");
        return FLUSH_DIR;
    }
    FileType file;
    file.inodeptr = user->cur_dir.inodeptr;
    file.start_block = 0;
    res = read_file(user->vol, &file);
    if (res < 0) {
        print_err("_check_cache:\tFailed to read file");
        return DISK_FAILURE;
    }
    free_dir(&user->cur_dir);
    res = parse_dir(&file, &user->cur_dir);
    if (res < 0) {
        print_err("_check_cache:\tFailed to parse dir");
        return DISK_FAILURE;
    }
    user->cache_time = time(NULL);
#ifdef _DEBUG
    printf("_check_cache:\tCache updated\n");
#endif
    return 0;
}

int _get_path(User* user) {
    char* path = user->path;
    u_int16_t cur_idx = user->cur_dir.inodeptr->i_idx;
    Inode cur_inode;
    cur_inode.i_idx = user->cur_dir.inodeptr->i_parent;
    DirType dir;
    FileType file;
    file.inodeptr = &cur_inode;
    file.start_block = 0;
    int pathlen = 1;
    path[0] = '\0';
    if (cur_idx == 0) {
        path[0] = '/';
        path[1] = '\0';
        return 0;
    }
    while (cur_idx != 0) {
        int res = read_inode(user->vol, &cur_inode);
        if (res < 0) {
            print_err("_get_path:\tFailed to read inode");
            return -1;
        }
#ifdef _DEBUG
        printf("Cur inode: %d\n", cur_inode.i_idx);
#endif
        res = read_file(user->vol, &file);
        if (res < 0) {
            print_err("_get_path:\tFailed to read file");
            free(file.data);
            return -1;
        }
        res = parse_dir(&file, &dir);
        if (res < 0) {
            print_err("_get_path:\tFailed to parse dir");
            free_dir(&dir);
            return -1;
        }

        // Find parent
        DirEntry* entry = _search_parent(&dir, cur_idx);
        if (entry == NULL) {
            print_err("_get_path:\tFailed to find parent");
            free_dir(&dir);
            return -1;
        }
        int len = entry->filename_len;
        pathlen += len + 1;
        if (pathlen > 1024) {
            print_err("_get_path:\tPath too long");
            free_dir(&dir);
            return -1;
        }
        memmove(path + len + 1, path, pathlen);
        memcpy(path + 1, entry->filename, len);
        path[0] = '/';

        // Update
        cur_idx = cur_inode.i_idx;
        cur_inode.i_idx = dir.inodeptr->i_parent;
        free_dir(&dir);
        if (res < 0) {
            print_err("_get_path:\tFailed to read inode");
            return -1;
        }
#ifdef _DEBUG
        printf("Path: %s\n", path);
#endif
    }
    return 0;
}

DirEntry* _search_dir(DirType* dir, char* targetname, int len) {
    // @todo if targetname is path
    for (int i = 0; i < dir->dir_size; i++) {
        if (dir->dir_entries[i].filename_len == len && strncmp(dir->dir_entries[i].filename, targetname, len) == 0) {
            return &dir->dir_entries[i];
        }
    }
    return NULL;
}

DirEntry* _search_parent(DirType* parent, u_int16_t inode_idx) {
    int init_item = parent->inodeptr->i_idx == 0 ? 1 : 2;
    for (int i = init_item; i < parent->dir_size; i++) {
        if (parent->dir_entries[i].inode_idx == inode_idx) {
            return &parent->dir_entries[i];
        }
    }
    return NULL;
}

int _make_target_entry(User* user, char* targetname, u_int16_t mode) {
    DirType* dir = &user->cur_dir;
    size_t len = strlen(targetname);
    if (len == 2 && memcmp(targetname, "..", 2) == 0) {
        print_err("_make_target_entry:\tInvalid target name");
        return INVALID_PATH;
    }
    DirEntry* target_entry = _search_dir(dir, targetname, len);
    if (target_entry != NULL) {
        print_err("_make_target_entry:\tFile already exists");
        return TARGET_EXISTS;
    }
    int inode_idx = allocate_inode(user->vol);
    if (inode_idx < 0) {
        print_err("_make_target_entry:\tFailed to allocate inode");
        return DISK_FAILURE;
    }
    u_int16_t new_tot_size = dir->tot_size + sizeof(u_int16_t) * 3 + len;
    char* new_data = (char*)realloc(dir->orig_data, new_tot_size);
    if (new_data == NULL) {
        print_err("_make_target_entry:\tFailed to malloc new data");
        return DISK_FAILURE;
    }
    *((u_int16_t*)new_data) = *((u_int16_t*)new_data) + 1;
    char* ptr = new_data + dir->tot_size;
    // Add entry
    ((u_int16_t*)ptr)[0] = mode;
    ((u_int16_t*)ptr)[1] = inode_idx;
    ((u_int16_t*)ptr)[2] = len;
    ptr += sizeof(u_int16_t) * 3;
    memcpy(ptr, targetname, len);
    // Update dir
    dir->dir_size++;
    dir->tot_size = new_tot_size;
    dir->orig_data = new_data;
    free(dir->dir_entries);
    FileType file;
    file.inodeptr = dir->inodeptr;
    file.data = new_data;
    file.size = new_tot_size;
    file.start_block = 0;
    int res = parse_dir(&file, dir);
    if (res < 0) {
        print_err("_make_target_entry:\tFailed to parse dir");
        return DISK_FAILURE;
    }
    res = write_file(user->vol, &file);
    if (res < 0) {
        print_err("_make_target_entry:\tFailed to write file");
        return DISK_FAILURE;
    }
    return inode_idx;
}

int _remove_target_entry(User* user, int entry_idx, size_t len) {
    // Remove entry
    DirType* dir = &user->cur_dir;
    DirEntry* entry = &dir->dir_entries[entry_idx];
    memmove(entry, entry + 1, sizeof(DirEntry) * (dir->dir_size - entry_idx - 1));
    dir->dir_size--;
    dir->tot_size -= sizeof(u_int16_t) * 3 + len;
    dir->dir_entries = (DirEntry*)realloc(dir->dir_entries, sizeof(DirEntry) * dir->dir_size);
    // Update file
    FileType file;
    file.inodeptr = dir->inodeptr;
    file.start_block = 0;
    int res = encode_dir(dir, &file);
    if (res < 0) {
        print_err("_remove_target_entry:\tFailed to encode dir");
        return DISK_FAILURE;
    }
    if (dir->orig_data != NULL) {
        free(dir->orig_data);
    }
    dir->orig_data = file.data;
    res = write_file(user->vol, &file);
    if (res < 0) {
        print_err("_remove_target_entry:\tFailed to write file");
        return DISK_FAILURE;
    }
    return 0;
}

int _remove_inode_link(Volume* vol, int inode_idx) {
    Inode inode;
    inode.i_idx = inode_idx;
    int res = read_inode(vol, &inode);
    if (res < 0) {
        print_err("_remove_inode_link:\tFailed to read inode");
        return DISK_FAILURE;
    }
    if (inode.i_mode == INODE_UNUSED) {
        print_err("_remove_inode_link:\tInode already removed");
        return TARGET_NOT_FOUND;
    }
    inode.i_nlink--;
    if (inode.i_nlink == 0) {
        // Delete file
        FileType file;
        file.inodeptr = &inode;
        file.start_block = 0;
        file.size = 0;
        file.data = NULL;
        res = write_file(vol, &file);
        if (res < 0) {
            print_err("_remove_inode_link:\tFailed to write file");
            return DISK_FAILURE;
        }
        // Update inode
        inode.i_mode = INODE_UNUSED;
        inode.i_size = 0;
        inode.i_blocks = 0;
        inode.i_parent = 0;
        res = write_inode(vol, &inode);
        if (res < 0) {
            print_err("_remove_inode_link:\tFailed to write inode");
            return DISK_FAILURE;
        }
        // Free inode
        res = free_inode(vol, inode.i_idx);
        if (res < 0) {
            print_err("_remove_inode_link:\tFailed to free inode");
            return DISK_FAILURE;
        }
    } else {
        // Update inode
        res = write_inode(vol, &inode);
        if (res < 0) {
            print_err("_remove_inode_link:\tFailed to write inode");
            return DISK_FAILURE;
        }
    }
    return 0;
}