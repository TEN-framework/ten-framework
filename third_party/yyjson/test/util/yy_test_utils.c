/*==============================================================================
 * Utilities for single thread test (C99, Win/Mac/Linux).
 *
 * Copyright (C) 2018 YaoYuan <ibireme@gmail.com>.
 * Released under the MIT license (MIT).
 *============================================================================*/

#include "yy_test_utils.h"
#if yy_has_include(<glob.h>) && defined(__linux__)
#   define YY_HAS_GLOB 1
#   include <glob.h>
#endif


// =============================================================================
// Pseudo Random Number Generator (SplitMix)
// A constant seed should be used to ensure repeatability of benchmark.
// =============================================================================

static u64 yy_rand_seed = 0;

void yy_rand_reset(u64 seed) {
    yy_rand_seed = seed;
}

u32 yy_rand_u32(void) {
    u64 z = (yy_rand_seed += 0x9e3779b97f4a7c15ull);
    z = (z ^ (z >> 33)) * 0x62a9d9ed799705f5ull;
    z = (z ^ (z >> 28)) * 0xcb24d0a5c88c35b3ull;
    return (u32)(z >> 32);
}

u32 yy_rand_u32_uniform(u32 bound) {
    if (yy_unlikely(bound < 2)) return 0;
    while (true) {
        u32 r = yy_rand_u32();
        u32 x = r % bound;
        if (r - x <= (u32)(-(i32)bound)) return x;
    }
}

u32 yy_rand_u32_range(u32 min, u32 max) {
    return yy_rand_u32_uniform(max - min + 1) + min;
}

u64 yy_rand_u64(void) {
    u64 z = (yy_rand_seed += 0x9e3779b97f4a7c15ull);
    z = (z ^ (z >> 30)) * 0xbf58476d1ce4e5b9ull;
    z = (z ^ (z >> 27)) * 0x94d049bb133111ebull;
    return (z ^ (z >> 31));
}

u64 yy_rand_u64_uniform(u64 bound) {
    if (yy_unlikely(bound < 2)) return 0;
    while (true) {
        u64 r = yy_rand_u64();
        u64 x = r % bound;
        if (r - x <= (u64)(-(i64)bound)) return x;
    }
}

u64 yy_rand_u64_range(u64 min, u64 max) {
    return yy_rand_u64_uniform(max - min + 1) + min;
}

f32 yy_rand_f32(void) {
    while (true) {
        u32 r = yy_rand_u32();
        u32 x = ~(u32)0;
        if (r != x) return (f32)r / (f32)x;
    }
}

f32 yy_rand_f32_range(f32 min, f32 max) {
    return min + (max - min) * yy_rand_f32();
}

f64 yy_rand_f64(void) {
    while (true) {
        u64 r = yy_rand_u64();
        u64 x = ~(u64)0;
        if (r != x) return (f64)r / (f64)x;
    }
}

f64 yy_rand_f64_range(f64 min, f64 max) {
    return min + (max - min) * yy_rand_f64();
}



/*==============================================================================
 * File Utils
 *============================================================================*/

bool yy_path_combine(char *buf, const char *path, ...) {
    if (!buf) return false;
    *buf = '\0';
    if (!path) return false;
    
    usize len = strlen(path);
    memmove(buf, path, len);
    const char *hdr = buf;
    buf += len;
    
    va_list args;
    va_start(args, path);
    while (true) {
        const char *item = va_arg(args, const char *);
        if (!item) break;
        if (buf > hdr && *(buf - 1) != YY_DIR_SEPARATOR) {
            *buf++ = YY_DIR_SEPARATOR;
        }
        len = strlen(item);
        if (len && *item == YY_DIR_SEPARATOR) {
            len--;
            item++;
        }
        memmove(buf, item, len);
        buf += len;
    }
    va_end(args);
    
    *buf = '\0';
    return true;
}

bool yy_path_remove_last(char *buf, const char *path) {
    usize len = path ? strlen(path) : 0;
    if (!buf) return false;
    *buf = '\0';
    if (len == 0) return false;
    
    const char *cur = path + len - 1;
    if (*cur == YY_DIR_SEPARATOR) cur--;
    for (; cur >= path; cur--) {
        if (*cur == YY_DIR_SEPARATOR) break;
    }
    len = cur + 1 - path;
    memmove(buf, path, len);
    buf[len] = '\0';
    return len > 0;
}

bool yy_path_get_last(char *buf, const char *path) {
    usize len = path ? strlen(path) : 0;
    const char *end, *cur;
    if (!buf) return false;
    *buf = '\0';
    if (len == 0) return false;
    
    end = path + len - 1;
    if (*end == YY_DIR_SEPARATOR) end--;
    for (cur = end; cur >= path; cur--) {
        if (*cur == YY_DIR_SEPARATOR) break;
    }
    len = end - cur;
    memmove(buf, cur + 1, len);
    buf[len] = '\0';
    return len > 0;
}

bool yy_path_append_ext(char *buf, const char *path, const char *ext) {
    usize len = path ? strlen(path) : 0;
    char tmp[YY_MAX_PATH];
    char *cur = tmp;
    if (!buf) return false;
    
    memcpy(cur, path, len);
    cur += len;
    *cur++ = '.';
    
    len = ext ? strlen(ext) : 0;
    memcpy(cur, ext, len);
    cur += len;
    *cur++ = '\0';
    
    memcpy(buf, tmp, cur - tmp);
    return true;
}

bool yy_path_remove_ext(char *buf, const char *path) {
    usize len = path ? strlen(path) : 0;
    if (!buf) return false;
    memmove(buf, path, len + 1);
    for (char *cur = buf + len; cur >= buf; cur--) {
        if (*cur == YY_DIR_SEPARATOR) break;
        if (*cur == '.') {
            *cur = '\0';
            return true;
        }
    }
    return false;
}

bool yy_path_get_ext(char *buf, const char *path) {
    usize len = path ? strlen(path) : 0;
    if (!buf) return false;
    for (const char *cur = path + len; cur >= path; cur--) {
        if (*cur == YY_DIR_SEPARATOR) break;
        if (*cur == '.') {
            memmove(buf, cur + 1, len - (cur - path));
            return true;
        }
    }
    *buf = '\0';
    return false;
}



bool yy_path_exist(const char *path) {
    if (!path || !strlen(path)) return false;
#ifdef _WIN32
    DWORD attrs = GetFileAttributesA(path);
    return attrs != INVALID_FILE_ATTRIBUTES;
#else
    struct stat attr;
    if (stat(path, &attr) != 0) return false;
    return true;
#endif
}

bool yy_path_is_dir(const char *path) {
    if (!path || !strlen(path)) return false;
#ifdef _WIN32
    DWORD attrs = GetFileAttributesA(path);
    return (attrs & FILE_ATTRIBUTE_DIRECTORY) != 0;
#else
    struct stat attr;
    if (stat(path, &attr) != 0) return false;
    return S_ISDIR(attr.st_mode);
#endif
}


// sort function for dir read
int yy_dir_strcmp_func(void const *a, void const *b) {
    char const *astr = *(char const **)a;
    char const *bstr = *(char const **)b;
    return strcmp(astr, bstr);
}

char **yy_dir_read_opts(const char *path, int *count, bool full) {
#ifdef _WIN32
    struct _finddata_t entry;
    intptr_t handle;
    int idx = 0, alc = 0;
    char **names = NULL, **names_tmp, *search;
    usize path_len = path ? strlen(path) : 0;
    
    if (count) *count = 0;
    if (path_len == 0) return NULL;
    search = malloc(path_len + 3);
    if (!search) return NULL;
    memcpy(search, path, path_len);
    if (search[path_len - 1] == '\\') path_len--;
    memcpy(search + path_len, "\\*\0", 3);
    
    handle = _findfirst(search, &entry);
    if (handle == -1) goto fail;
    
    alc = 4;
    names = malloc(alc * sizeof(char*));
    if (!names) goto fail;
    
    do {
        char *name = (char *)entry.name;
        if (!name || !strlen(name)) continue;
        if (strcmp(name, ".") == 0 || strcmp(name, "..") == 0) continue;
        name = yy_str_copy(name);
        if (!name) goto fail;
        if (idx + 1 >= alc) {
            alc *= 2;
            names_tmp = realloc(names, alc * sizeof(char*));
            if (!names_tmp) goto fail;
            names = names_tmp;
        }
        if (full) {
            char *fullpath = malloc(strlen(path) + strlen(name) + 4);
            if (!fullpath) goto fail;
            yy_path_combine(fullpath, path, name, NULL);
            free(name);
            if (fullpath) name = fullpath;
            else break;
        }
        names[idx] = name;
        idx++;
    } while (_findnext(handle, &entry) == 0);
    _findclose(handle);
    
    if (idx > 1) qsort(names, idx, sizeof(char *), yy_dir_strcmp_func);
    names[idx] = NULL;
    if (count) *count = idx;
    return names;
    
fail:
    if (handle != -1)_findclose(handle);
    if (search) free(search);
    if (names) free(names);
    return NULL;
    
#elif defined(YY_HAS_GLOB)
    // readdir() may fail for 32-bit user-static qemu on 64-bit host
    // use glob() instead: https://gitlab.com/qemu-project/qemu/-/issues/263
    
    if (count) *count = 0;
    if (!path) return NULL;
    size_t path_len = strlen(path);
    if (!path_len) return NULL;
    
    char *patt = calloc(1, path_len * 2 + 4);
    if (!patt) return NULL;
    for (size_t i = 0, p = 0; i < path_len; i++, p++) {
        char c = path[i];
        if (c == '*') patt[p++] = '\\';
        patt[p] = c;
        if (i + 1 == path_len) {
            if (patt[p] != '/') patt[++p] = '/';
            patt[++p] = '*';
        }
    }
    
    glob_t buf = { 0 };
    int flag = 0;
#ifdef GLOB_NOESCAPE
    flag |= GLOB_NOESCAPE;
#endif
#ifdef GLOB_PERIOD
    flag |= GLOB_PERIOD;
#endif
    if (glob(patt, flag, NULL, &buf)) {
        free((void *)patt);
        globfree(&buf);
        return NULL;
    }
    free((void *)patt);
    char **names = calloc(buf.gl_pathc + 1, sizeof(char *));
    if (!names) {
        globfree(&buf);
        return NULL;
    }
    
    int i = 0, icount = 0;
    for(; i < (int)buf.gl_pathc; i++) {
        const char *one_path = buf.gl_pathv[i];
        if (!one_path) continue;
        size_t one_len = strlen(one_path);
        if (!one_len) continue;
        const char *one_name = one_path + one_len;
        while (one_name > one_path && *(one_name - 1) != '/') one_name--;
        if (!strcmp(one_name, ".") || !strcmp(one_name, "..")) continue;
        const char *one = full ? one_path : one_name;
        one = yy_str_copy(one);
        if (!one) {
            for (i = 0; i < icount; i++) free((void *)names[i]);
            globfree(&buf);
            free((void *)names);
            return NULL;
        }
        names[icount++] = (char *)one;
    }
    globfree(&buf);
    
    if (count) *count = icount;
    return names;
    
#else
    DIR *dir = NULL;
    struct dirent *entry;
    int idx = 0, alc = 0;
    char **names = NULL, **names_tmp;
    
    if (count) *count = 0;
    if (!path || !strlen(path) || !(dir = opendir(path))) {
        goto fail;
    }
    
    alc = 4;
    names = calloc(1, alc * sizeof(char *));
    if (!names) goto fail;
    
    while ((entry = readdir(dir))) {
        char *name = (char *)entry->d_name;
        if (!name || !strlen(name)) continue;
        if (strcmp(name, ".") == 0 || strcmp(name, "..") == 0) continue;
        if (idx + 1 >= alc) {
            alc *= 2;
            names_tmp = realloc(names, alc * sizeof(char *));
            if (!names_tmp)
                goto fail;
            names = names_tmp;
        }
        name = yy_str_copy(name);
        if (!name) goto fail;
        if (full) {
            char *fullpath = malloc(strlen(path) + strlen(name) + 4);
            if (!fullpath) goto fail;
            yy_path_combine(fullpath, path, name, NULL);
            free(name);
            if (fullpath) name = fullpath;
            else break;
        }
        names[idx] = name;
        idx++;
    }
    closedir(dir);
    if (idx > 1) qsort(names, idx, sizeof(char *), yy_dir_strcmp_func);
    names[idx] = NULL;
    if (count) *count = idx;
    return names;
    
fail:
    if (dir) closedir(dir);
    yy_dir_free(names);
    return NULL;
#endif
}

char **yy_dir_read(const char *path, int *count) {
    return yy_dir_read_opts(path, count, false);
}

char **yy_dir_read_full(const char *path, int *count) {
    return yy_dir_read_opts(path, count, true);
}

void yy_dir_free(char **names) {
    if (names) {
        for (int i = 0; ; i++) {
            if (names[i]) free(names[i]);
            else break;
        }
        free(names);
    }
}



FILE *yy_file_open(const char *path, const char *mode) {
    if (!path || !mode) return false;
    FILE *file = NULL;
#if _MSC_VER >= 1400
    if (fopen_s(&file, path, mode) != 0) return NULL;
#else
    file = fopen(path, mode);
#endif
    return file;
}

bool yy_file_read(const char *path, u8 **dat, usize *len) {
    return yy_file_read_with_padding(path, dat, len, 1); // for string
}

bool yy_file_read_with_padding(const char *path, u8 **dat, usize *len, usize padding) {
    if (!path || !strlen(path)) return false;
    if (!dat || !len) return false;
    
    FILE *file = yy_file_open(path, "rb");
    if (file == NULL) return false;
    if (fseek(file, 0, SEEK_END) != 0) {
        fclose(file);
        return false;
    }
    long file_size = ftell(file);
    if (file_size < 0) {
        fclose(file);
        return false;
    }
    if (fseek(file, 0, SEEK_SET) != 0) {
        fclose(file);
        return false;
    }
    void *buf = malloc((usize)file_size + padding);
    if (buf == NULL) {
        fclose(file);
        return false;
    }
    
    if (file_size > 0) {
#if _MSC_VER >= 1400
        if (fread_s(buf, file_size, file_size, 1, file) != 1) {
            free(buf);
            fclose(file);
            return false;
        }
#else
        if (fread(buf, file_size, 1, file) != 1) {
            free(buf);
            fclose(file);
            return false;
        }
#endif
    }
    fclose(file);
    
    memset((char *)buf + file_size, 0, padding);
    *dat = (u8 *)buf;
    *len = (usize)file_size;
    return true;
}

bool yy_file_write(const char *path, u8 *dat, usize len) {
    if (!path || !strlen(path)) return false;
    if (len && !dat) return false;
    
    FILE *file = NULL;
#if _MSC_VER >= 1400
    if (fopen_s(&file, path, "wb") != 0) return false;
#else
    file = fopen(path, "wb");
#endif
    if (file == NULL) return false;
    if (fwrite(dat, len, 1, file) != 1) {
        fclose(file);
        return false;
    }
    if (fclose(file) != 0) {
        file = NULL;
        return false;
    }
    return true;
}

bool yy_file_delete(const char *path) {
    if (!path || !*path) return false;
    return remove(path) == 0;
}



/*==============================================================================
 * String Utils
 *============================================================================*/

static char to_lower(char c) {
    return ('A' <= c && c <= 'Z') ? c + ('a' - 'A') : c;
}

char *yy_str_copy(const char *str) {
    if (!str) return NULL;
    usize len = strlen(str) + 1;
    char *dup = malloc(len);
    if (dup) memcpy(dup, str, len);
    return dup;
}

int yy_str_cmp(const char *str1, const char *str2, bool ignore_case) {
    if (str1 == str2) return 0;
    if (!str1) return -1;
    if (!str2) return +1;
    if (!ignore_case) {
        return strcmp(str1, str2);
    }
    const unsigned char *s1 = (const unsigned char *)str1;
    const unsigned char *s2 = (const unsigned char *)str2;
    int result;
    while ((result = to_lower(*s1) - to_lower(*s2++)) == 0) {
        if (*s1++ == '\0') break;
    }
    return result;
}

bool yy_str_contains(const char *str, const char *search) {
    if (!str || !search) return false;
    return strstr(str, search) != NULL;
}

bool yy_str_has_prefix(const char *str, const char *prefix) {
    if (!str || !prefix) return false;
    usize len1 = strlen(str);
    usize len2 = strlen(prefix);
    if (len2 > len1) return false;
    return memcmp(str, prefix, len2) == 0;
}

bool yy_str_has_suffix(const char *str, const char *suffix) {
    if (!str || !suffix) return false;
    usize len1 = strlen(str);
    usize len2 = strlen(suffix);
    if (len2 > len1) return false;
    return memcmp(str + (len1 - len2), suffix, len2) == 0;
}

bool yy_str_is_utf8(const char *str, size_t len) {
    // https://en.wikipedia.org/wiki/UTF-8
    const uint8_t *cur = (const uint8_t *)str;
    const uint8_t *end = cur + len;
    uint32_t u;
    if (!str) return false;
    while (cur < end) {
        // Range: [U+0000, U+007F] Bytes: 0xxxxxxx
        if ((cur[0] & 0x80) == 0) {
            cur++;
            continue;
        }
        // Range: [U+0080, U+07FF] Bytes: 110xxxxx 10xxxxxx
        if ((cur[0] & 0xE0) == 0xC0) {
            if (end - cur < 2) return false;
            if ((cur[1] & 0xC0) != 0x80) return false;
            u = ((uint32_t)(cur[1] & 0x3F) << 0) |
                ((uint32_t)(cur[0] & 0x1F) << 6);
            if (u < 0x80 || u > 0x7FF) return false;
            cur += 2;
            continue;
        }
        // Range: [U+0800, U+FFFF] Bytes: 1110xxxx 10xxxxxx 10xxxxxx
        if ((cur[0] & 0xF0) == 0xE0) {
            if (end - cur < 3) return false;
            if ((cur[1] & 0xC0) != 0x80) return false;
            if ((cur[2] & 0xC0) != 0x80) return false;
            u = ((uint32_t)(cur[2] & 0x3F) << 0) |
                ((uint32_t)(cur[1] & 0x3F) << 6) |
                ((uint32_t)(cur[0] & 0x0F) << 12);
            if (u < 0x800 || u > 0xFFFF) return false;
            // Surrogate halves: U+D800 through U+DFFF
            if (0xD800 <= u && u <= 0xDFFF) return false;
            cur += 3;
            continue;
        }
        // Range: [U+10000, U+10FFFF] Bytes: 11110xxx 10xxxxxx 10xxxxxx 10xxxxxx
        if ((cur[0] & 0xF8) == 0xF0) {
            if (end - cur < 4) return false;
            if ((cur[1] & 0xC0) != 0x80) return false;
            if ((cur[2] & 0xC0) != 0x80) return false;
            if ((cur[3] & 0xC0) != 0x80) return false;
            u = ((uint32_t)(cur[3] & 0x3F) << 0) |
                ((uint32_t)(cur[2] & 0x3F) << 6) |
                ((uint32_t)(cur[1] & 0x3F) << 12) |
                ((uint32_t)(cur[0] & 0x07) << 18);
            if (u < 0x10000 || u > 0x10FFFF) return false;
            cur += 4;
            continue;
        }
        return false;
    }
    return true;
}



/*==============================================================================
 * Memory Buffer
 *============================================================================*/

bool yy_buf_init(yy_buf *buf, usize len) {
    if (!buf) return false;
    if (len < 16) len = 16;
    memset(buf, 0, sizeof(yy_buf));
    buf->hdr = malloc(len);
    if (!buf->hdr) return false;
    buf->cur = buf->hdr;
    buf->end = buf->hdr + len;
    buf->need_free = true;
    return true;
}

void yy_buf_release(yy_buf *buf) {
    if (!buf || !buf->hdr) return;
    if (buf->need_free) free(buf->hdr);
    memset(buf, 0, sizeof(yy_buf));
}

usize yy_buf_len(yy_buf *buf) {
    if (!buf) return 0;
    return buf->cur - buf->hdr;
}

bool yy_buf_grow(yy_buf *buf, usize len) {
    if (!buf) return false;
    if ((usize)(buf->end - buf->cur) >= len) return true;
    if (!buf->hdr) return yy_buf_init(buf, len);
    
    usize use = buf->cur - buf->hdr;
    usize alc = buf->end - buf->hdr;
    do {
        if (alc * 2 < alc) return false; /* overflow */
        alc *= 2;
    } while (alc - use < len);
    u8 *tmp = (u8 *)realloc(buf->hdr, alc);
    if (!tmp) return false;
    
    buf->cur = tmp + (buf->cur - buf->hdr);
    buf->hdr = tmp;
    buf->end = tmp + alc;
    return true;
}

bool yy_buf_append(yy_buf *buf, u8 *dat, usize len) {
    if (!buf) return false;
    if (len == 0) return true;
    if (!dat) return false;
    if (!yy_buf_grow(buf, len)) return false;
    memcpy(buf->cur, dat, len);
    buf->cur += len;
    return true;
}



/*==============================================================================
 * Data Reader
 *============================================================================*/

bool yy_dat_init_with_file(yy_dat *dat, const char *path) {
    u8 *mem;
    usize len;
    if (!dat) return false;
    memset(dat, 0, sizeof(yy_dat));
    if (!yy_file_read(path, &mem, &len)) return false;
    dat->hdr = mem;
    dat->cur = mem;
    dat->end = mem + len;
    dat->need_free = true;
    return true;
}

bool yy_dat_init_with_mem(yy_dat *dat, u8 *mem, usize len) {
    if (!dat) return false;
    if (len && !mem) return false;
    dat->hdr = mem;
    dat->cur = mem;
    dat->end = mem + len;
    dat->need_free = false;
    return true;
}

void yy_dat_release(yy_dat *dat) {
    yy_buf_release(dat);
}

void yy_dat_reset(yy_dat *dat) {
    if (dat) dat->cur = dat->hdr;
}

char *yy_dat_read_line(yy_dat *dat, usize *len) {
    if (len) *len = 0;
    if (!dat || dat->cur >= dat->end) return NULL;
    
    u8 *str = dat->cur;
    u8 *cur = dat->cur;
    u8 *end = dat->end;
    while (cur < end && *cur != '\r' && *cur != '\n' && *cur != '\0') cur++;
    if (len) *len = cur - str;
    if (cur < end) {
        if (cur + 1 < end && *cur == '\r' && cur[1] == '\n') cur += 2;
        else if (*cur == '\r' || *cur == '\n' || *cur == '\0') cur++;
    }
    dat->cur = cur;
    return (char *)str;
}

char *yy_dat_copy_line(yy_dat *dat, usize *len) {
    if (len) *len = 0;
    usize _len;
    char *_str = yy_dat_read_line(dat, &_len);
    if (!_str) return NULL;
    char *str = malloc(_len + 1);
    if (!str) return NULL;
    memcpy(str, _str, _len);
    str[_len] = '\0';
    if (len) *len = _len;
    return str;
}
