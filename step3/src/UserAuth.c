#include "UserAuth.h"

int init_auth_list(AuthList* list) {
    if (list == NULL) {
        return -1;
    }
    list->users = (AuthUser*)malloc(sizeof(AuthUser));
    list->list_size = 1;
    list->main_key = 1;
    AuthUser* auth_user = &list->users[0];
    auth_user->id = 0;
    auth_user->name_len = 4;
    strcpy(auth_user->username, "root");
    auth_user->home_len = 4;
    strcpy(auth_user->user_home, "home");
    memset(auth_user->hashed_password, 0, 32);
    list->tot_size = AUTH_USER_OFFSET + 6 + 32 + auth_user->name_len + auth_user->home_len;
    return 0;
}

int load_auth_list(Volume* vol, AuthList* list) {
    int res = read_inode(vol, list->inodeptr);
    if (res < 0) {
        printf("Failed to read auth inode\n");
        return DISK_FAILURE;
    }
    FileType auth_file;
    auth_file.inodeptr = list->inodeptr;
    auth_file.start_block = 0;
    res = read_file(vol, &auth_file);
    if (res < 0) {
        printf("Failed to read auth file\n");
        return DISK_FAILURE;
    }
    res = parse_auth(&auth_file, list);
    if (res < 0) {
        printf("Failed to parse auth file\n");
        return -1;
    }
#ifdef _DEBUG
    print_auth(list, NULL, NULL);
#endif
    return 0;
}

int save_auth_list(Volume* vol, AuthList* list) {
    FileType file;
    file.inodeptr = list->inodeptr;
    file.start_block = 0;
    int res = encode_auth(list, &file);
    if (res < 0) {
        print_err("save_auth_list:\tFailed to encode auth");
        return res;
    }
    res = write_file(vol, &file);
    free(file.data);
    if (res < 0) {
        print_err("save_auth_list:\tFailed to write auth");
        return DISK_FAILURE;
    }
    return 0;
}

int parse_auth(FileType* file, AuthList* users) {
    /**
     * Structure of a AuthList :
     * [ListSize:2]
     * [MainKey:2]
     * [AuthUser1]
     *  | [id:2]
     *  | [name_len:2]
     *  | [home_len:2]
     *  | [hashed_password:32]
     *  | [username:name_len]
     *  | [user_home:home_len]
     * [AuthUser2]...
     */

    if (file == NULL || users == NULL) {
        return -1;
    }
    if (file->inodeptr->i_mode != INODE_FILE) {
        return -1;
    }
    u_int16_t list_size = *((u_int16_t*)file->data);
    u_int16_t main_key = *((u_int16_t*)(file->data + 2));
    users->list_size = list_size;
    users->main_key = main_key;
    users->users = (AuthUser*)malloc(sizeof(AuthUser) * list_size);
    users->tot_size = AUTH_USER_OFFSET;
    char* ptr = file->data + AUTH_USER_OFFSET;

    for (int i = 0; i < list_size; i++) {
        AuthUser* user = users->users + i;
        user->id = *((u_int16_t*)ptr);
        ptr += 2;
        user->name_len = *((u_int16_t*)ptr);
        ptr += 2;
        user->home_len = *((u_int16_t*)ptr);
        ptr += 2;
        memcpy(user->hashed_password, ptr, 32);
        ptr += 32;
        memcpy(user->username, ptr, user->name_len);
        user->username[user->name_len] = '\0';
        ptr += user->name_len;
        memcpy(user->user_home, ptr, user->home_len);
        user->user_home[user->home_len] = '\0';
        ptr += user->home_len;
        users->tot_size += 6 + 32 + user->name_len + user->home_len;
    }
    return 0;
}

int encode_auth(AuthList* users, FileType* file) {
    if (users == NULL || file == NULL) {
        return -1;
    }
    if (file->inodeptr->i_mode != INODE_FILE) {
        return -1;
    }
    file->inodeptr = users->inodeptr;
    file->data = (char*)malloc(users->tot_size);
    file->size = users->tot_size;
    if (file->data == NULL) {
        return -1;
    }
    u_int16_t* ptr = (u_int16_t*)file->data;
    *ptr = users->list_size;
    ptr++;
    *ptr = users->main_key;
    char* data_ptr = file->data + AUTH_USER_OFFSET;
    for (int i = 0; i < users->list_size; i++) {
        AuthUser* user = users->users + i;
        *((u_int16_t*)data_ptr) = user->id;
        data_ptr += 2;
        *((u_int16_t*)data_ptr) = user->name_len;
        data_ptr += 2;
        *((u_int16_t*)data_ptr) = user->home_len;
        data_ptr += 2;
        memcpy(data_ptr, user->hashed_password, 32);
        data_ptr += 32;
        memcpy(data_ptr, user->username, user->name_len);
        data_ptr += user->name_len;
        memcpy(data_ptr, user->user_home, user->home_len);
        data_ptr += user->home_len;
    }
    return 0;
}

AuthUser* search_user(AuthList* list, char* username) {
    if (list == NULL || username == NULL) {
        return NULL;
    }
    for (int i = 0; i < list->list_size; i++) {
        AuthUser* u = list->users + i;
        if (u->name_len == strlen(username) && strncmp(u->username, username, u->name_len) == 0) {
            return u;
        }
    }
    return NULL;
}

AuthUser* search_uid(AuthList* list, u_int16_t uid) {
    if (list == NULL) {
        return NULL;
    }
    for (int i = 0; i < list->list_size; i++) {
        AuthUser* u = list->users + i;
        if (u->id == uid) {
            return u;
        }
    }
    return NULL;
}

int create_auth(AuthList* list, char* username) {
    if (list == NULL || username == NULL) {
        return -1;
    }
    list->users = (AuthUser*)realloc(list->users, sizeof(AuthUser) * (list->list_size + 1));
    if (list->users == NULL) {
        return -1;
    }
    AuthUser* new_user = list->users + list->list_size;
    new_user->id = list->main_key;
    strcpy(new_user->username, username);
    new_user->name_len = strlen(username);
    sprintf(new_user->user_home, "/home/%s", username);
    new_user->home_len = strlen(new_user->user_home);
    memset(new_user->hashed_password, 0, 32);
    list->list_size++;
    list->main_key++;
    list->tot_size += 6 + 32 + new_user->name_len + new_user->home_len;
    return new_user->id;
}

int remove_auth(AuthList* list, u_int16_t uid, bool recycle) {
    if (list == NULL) {
        return -1;
    }
    AuthUser* user = search_uid(list, uid);
    if (user == NULL) {
        return -1;
    }
    if (recycle && user->id == list->main_key - 1) {
        list->main_key--;
    }
    list->tot_size -= 6 + 32 + user->name_len + user->home_len;
    int idx = user - list->users;
    memmove(user, user + 1, (list->list_size - idx - 1) * sizeof(AuthUser));
    list->list_size--;
    return 0;
}

int set_password(AuthUser* user, u_int32_t* hashed_pwd) {
    if (user == NULL || hashed_pwd == NULL) {
        return -1;
    }
    memcpy(user->hashed_password, hashed_pwd, 32);
    return 0;
}

int print_auth(AuthList* users, char** data, int* len) {
    char* data_buf = (char*)malloc(users->tot_size + users->list_size * 20 + 20);
    if (data_buf == NULL) {
        print_err("print_auth:\tFailed to allocate memory\n");
        return -1;
    }
    char* ptr = data_buf;
    ptr += sprintf(ptr, "AuthList: %d users\n", users->list_size);
    for (int i = 0; i < users->list_size; i++) {
        AuthUser* u = users->users + i;
        ptr += sprintf(ptr, "User %d: %.*s\thome: %.*s\n", u->id, u->name_len, u->username, u->home_len, u->user_home);
    }
    if (data == NULL || len == NULL) {
        printf("%s", data_buf);
    } else {
        *data = data_buf;
        *len = ptr - data_buf;
    }
    return 0;
}

#define RTSHIFT(x, n) ((x) >> (n))

#define RTROT(xsize, x, n) (((x) >> (n)) | (((x) & ((1 << (n + 1)) - 1)) << (((xsize) << 3) - (n))))

void cal_hash(AuthUser* user, unsigned char* input, u_int64_t len) {
    int i;

    int nk;
    u_int8_t chunk_data[512 / 8] = {0};  // 512bits chunk
    u_int32_t n_chunks;                  // Total number of 512bits_chunks
    u_int32_t mod;                       // Result of mod calculation: msgbitlen%512

    u_int32_t words[64] = {0};  // unit words
    u_int64_t bitlen;           // length in bits

    static u_int32_t hv_primes[8] = {0x6a09e667, 0xbb67ae85, 0x3c6ef372, 0xa54ff53a, 0x510e527f, 0x9b05688c, 0x1f83d9ab, 0x5be0cd19};

    // Variable: hash values　
    u_int32_t hv[8];

    const u_int32_t kv_primes[64] = {
        0x428a2f98, 0x71374491, 0xb5c0fbcf, 0xe9b5dba5, 0x3956c25b, 0x59f111f1, 0x923f82a4, 0xab1c5ed5,
        0xd807aa98, 0x12835b01, 0x243185be, 0x550c7dc3, 0x72be5d74, 0x80deb1fe, 0x9bdc06a7, 0xc19bf174,
        0xe49b69c1, 0xefbe4786, 0x0fc19dc6, 0x240ca1cc, 0x2de92c6f, 0x4a7484aa, 0x5cb0a9dc, 0x76f988da,
        0x983e5152, 0xa831c66d, 0xb00327c8, 0xbf597fc7, 0xc6e00bf3, 0xd5a79147, 0x06ca6351, 0x14292967,
        0x27b70a85, 0x2e1b2138, 0x4d2c6dfc, 0x53380d13, 0x650a7354, 0x766a0abb, 0x81c2c92e, 0x92722c85,
        0xa2bfe8a1, 0xa81a664b, 0xc24b8b70, 0xc76c51a3, 0xd192e819, 0xd6990624, 0xf40e3585, 0x106aa070,
        0x19a4c116, 0x1e376c08, 0x2748774c, 0x34b0bcb5, 0x391c0cb3, 0x4ed8aa4a, 0x5b9cca4f, 0x682e6ff3,
        0x748f82ee, 0x78a5636f, 0x84c87814, 0x8cc70208, 0x90befffa, 0xa4506ceb, 0xbef9a3f7, 0xc67178f2};

    u_int32_t kv[64];

    // SHA compression vars
    u_int32_t a, b, c, d, e, f, g, h;
    u_int32_t s0, s1, ch, temp1, temp2, maj;

    // Step 1: Pre-processing
    bitlen = len * 8;

    mod = bitlen % 512;
    if (mod > 448 - 1) {
        // mod > 448-1: fill the last chunk and need a new chunck
        n_chunks = bitlen / 512 + 2;
    } else {
        // mod <= 448-1: fill the last chunk.
        n_chunks = bitlen / 512 + 1;
    }

#ifdef _DEBUG
    printf("Input message has %lubits, with mod=%d %s 448-1!\n", bitlen, mod, mod > 448 - 1 ? ">" : "<=");
#endif

    // Step 2: Initialize hash values
    memcpy(hv, hv_primes, sizeof(hv_primes));
    memcpy(kv, kv_primes, sizeof(kv_primes));

    for (nk = 0; (u_int32_t)nk < n_chunks; nk++) {
        if (mod <= 448 - 1) {
            // For complete chunck blocks
            if ((u_int32_t)nk < n_chunks - 1)
                memcpy((void*)chunk_data, input + (nk * (512 >> 3)), (512 >> 3));
            // The last block, Need to fill 1+0s for the last chunk
            else {  // nk == n_chunks -1
                bzero(chunk_data, sizeof(chunk_data));
                memcpy((void*)chunk_data, input + (nk * (512 >> 3)), mod >> 3);
                // Append a single 1, big_endian
                chunk_data[mod >> 3] = 0x80;
                // Append 64bits to the end of chunk, as BIT_LENGTH of original message, in big_endian!
                for (i = 0; (u_int64_t)i < sizeof(bitlen); i++) {
                    *(chunk_data + (512 >> 3) - 1 - i) = (bitlen >> (i * 8)) & 0xff;  // Big_endian
                }
            }
        } else if (mod > 448 - 1) {
            // For complete chunck blocks
            if ((u_int32_t)nk < n_chunks - 2)
                memcpy((void*)chunk_data, input + (nk * (512 >> 3)), (512 >> 3));
            // The last block of original msg, appended with 1+0s to complete a 512bits chunck.
            else if ((u_int32_t)nk == n_chunks - 2) {
                bzero(chunk_data, sizeof(chunk_data));
                memcpy((void*)chunk_data, input + (nk * (512 >> 3)), mod >> 3);
                // Append a single 1, big_endian
                chunk_data[mod >> 3] = 0x80;
            }
            // Additional new chunk, as the last chunck.
            else {  // nk == n_chunks -1
                // All 0s
                bzero(chunk_data, sizeof(chunk_data));
                // Append 64bits to the end of chunk, as BIT_LENGTH of original message, in big_endian!
                for (i = 0; (u_int64_t)i < sizeof(bitlen); i++) {
                    *(chunk_data + (512 >> 3) - 1 - i) = (bitlen >> (i * 8)) & 0xff;  // Big_endian
                }
            }
        }

        // Step 5: Generate words array from chunk_data
        // 5.1 words[0]~[15]:  u8 chunck_data[64] ---> u32 words[64]
        for (i = 0; i < 16; i++) {
            words[i] = (chunk_data[4 * i] << 24) + (chunk_data[4 * i + 1] << 16) + (chunk_data[4 * i + 2] << 8) + chunk_data[4 * i + 3];
        }
        // 5.2 words[15]~[63]: 48 more words
        for (i = 16; i < 64; i++) {
            // s0 = (w[i-15] rightrotate 7) xor (w[i-15] rightrotate 18) xor (w[i-15] rightshift 3)
            s0 = RTROT(4, words[i - 15], 7) ^ RTROT(4, words[i - 15], 18) ^ RTSHIFT(words[i - 15], 3);
            // s1 = (w[i- 2] rightrotate 17) xor (w[i- 2] rightrotate 19) xor (w[i- 2] rightshift 10)
            s1 = RTROT(4, words[i - 2], 17) ^ RTROT(4, words[i - 2], 19) ^ RTSHIFT(words[i - 2], 10);
            // w[i] = w[i-16] + s0 + w[i-7] + s1
            words[i] = words[i - 16] + s0 + words[i - 7] + s1;
        }

        // Step 6:　SHA Compression, 64 rounds.
        a = hv[0];
        b = hv[1];
        c = hv[2];
        d = hv[3];
        e = hv[4];
        f = hv[5];
        g = hv[6];
        h = hv[7];
        // Compress for 64 rounds
        for (i = 0; i < 64; i++) {
            // S1 = (e rightrotate 6) xor (e rightrotate 11) xor (e rightrotate 25)
            s1 = RTROT(4, e, 6) ^ RTROT(4, e, 11) ^ RTROT(4, e, 25);

            // ch = (e and f) xor ((not e) and g)
            ch = (e & f) ^ ((~e) & g);

            // temp1 = h + S1 + ch + kv[i] + w[i]
            temp1 = h + s1 + ch + kv[i] + words[i];

            // S0 = (a rightrotate 2) xor (a rightrotate 13) xor (a rightrotate 22)
            s0 = RTROT(4, a, 2) ^ RTROT(4, a, 13) ^ RTROT(4, a, 22);

            // maj = (a and b) xor (a and c) xor (b and c)
            maj = (a & b) ^ (a & c) ^ (b & c);

            // temp2 = S0 + maj
            temp2 = s0 + maj;

            h = g;
            g = f;
            f = e;
            e = d + temp1;
            d = c;
            c = b;
            b = a;
            a = temp1 + temp2;
        }
#ifdef _DEBUG
        printf("a: %08x \n", a);
        printf("b: %08x \n", b);
        printf("c: %08x \n", c);
        printf("d: %08x \n", d);
        printf("e: %08x \n", e);
        printf("f: %08x \n", f);
        printf("g: %08x \n", g);
        printf("h: %08x \n", h);
#endif

        // Step 7:　Modify final values
        hv[0] += a;
        hv[1] += b;
        hv[2] += c;
        hv[3] += d;
        hv[4] += e;
        hv[5] += f;
        hv[6] += g;
    }
    // Step 8:　Generate final hash digest
    memcpy(user->hashed_password, hv, 32);
#ifdef _DEBUG
    u_int8_t digest[8 * 8 + 1] = {0};
    for (i = 0; i < 8; i++)
        sprintf((char*)digest + 8 * i, "%08x", hv[i]);
    printf("\nDigest: %s\n", digest);
#endif
}