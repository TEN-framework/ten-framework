/*==============================================================================
 * Utilities for single thread test (C99, Win/Mac/Linux).
 *
 * Copyright (C) 2018 YaoYuan <ibireme@gmail.com>.
 * Released under the MIT license (MIT).
 *============================================================================*/

#ifndef yy_test_utils_h
#define yy_test_utils_h

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <float.h>
#include <math.h>
#include <stdint.h>
#include <stdbool.h>
#include <stdarg.h>

#ifdef _WIN32
#   include <windows.h>
#   include <io.h>
#else
#   include <dirent.h>
#   include <sys/stat.h>
#endif

/* compiler builtin check (clang) */
#ifndef yy_has_builtin
#   ifdef __has_builtin
#       define yy_has_builtin(x) __has_builtin(x)
#   else
#       define yy_has_builtin(x) 0
#   endif
#endif

/* compiler attribute check (gcc/clang) */
#ifndef yy_has_attribute
#   ifdef __has_attribute
#       define yy_has_attribute(x) __has_attribute(x)
#   else
#       define yy_has_attribute(x) 0
#   endif
#endif

/* include check (gcc/clang) */
#ifndef yy_has_include
#   ifdef __has_include
#       define yy_has_include(x) __has_include(x)
#   else
#       define yy_has_include(x) 0
#   endif
#endif

/* inline */
#ifndef yy_inline
#   if _MSC_VER >= 1200
#       define yy_inline __forceinline
#   elif defined(_MSC_VER)
#       define yy_inline __inline
#   elif yy_has_attribute(always_inline) || __GNUC__ >= 4
#       define yy_inline __inline__ __attribute__((always_inline))
#   elif defined(__clang__) || defined(__GNUC__)
#       define yy_inline __inline__
#   elif defined(__cplusplus) || (__STDC__ >= 1 && __STDC_VERSION__ >= 199901L)
#       define yy_inline inline
#   else
#       define yy_inline
#   endif
#endif

/* noinline */
#ifndef yy_noinline
#   if _MSC_VER >= 1200
#       define yy_noinline __declspec(noinline)
#   elif yy_has_attribute(noinline) || __GNUC__ >= 4
#       define yy_noinline __attribute__((noinline))
#   else
#       define yy_noinline
#   endif
#endif

/* likely */
#ifndef yy_likely
#   if yy_has_builtin(__builtin_expect) || __GNUC__ >= 4
#       define yy_likely(expr) __builtin_expect(!!(expr), 1)
#   else
#       define yy_likely(expr) (expr)
#   endif
#endif

/* unlikely */
#ifndef yy_unlikely
#   if yy_has_builtin(__builtin_expect) || __GNUC__ >= 4
#       define yy_unlikely(expr) __builtin_expect(!!(expr), 0)
#   else
#       define yy_unlikely(expr) (expr)
#   endif
#endif

/* assert */
#define yy_assert(expr) do { \
    if (!(expr)) { \
        fprintf(stderr, "Assertion failed: %s (%s: %d)\n", #expr, __FILE__, __LINE__); \
        abort(); \
    }; \
} while(false)

#define yy_assertf(expr, ...) do { \
    if (!(expr)) { \
        fprintf(stderr, "Assertion failed: %s (%s: %d)\n", #expr, __FILE__, __LINE__); \
        fprintf(stderr, __VA_ARGS__); \
        fprintf(stderr, "\n"); \
        abort(); \
    }; \
} while(false)

/* test */
#if yy_has_include("yy_xctest.h")
#   include "yy_xctest.h"
#   define yy_test_case(name) \
        void name(void)
#else
#   define yy_test_case(name) \
    void name(void); \
    int main(int argc, const char * argv[]) { \
        name(); \
        return 0; \
    } \
    void name(void)
#endif

/* snprintf and vsnprintf before Visual Studio 2015 (14.0) */
#ifdef _MSC_VER
#   if _MSC_VER < 1900
#       ifndef vsnprintf
#           define vsnprintf yy_msvc_vsnprintf
#       endif
#       ifndef snprintf
#           define snprintf yy_msvc_snprintf
#       endif
static yy_inline int yy_msvc_vsnprintf(char *buf, size_t size,
                                       const char *format, va_list vlist) {
    int count = -1;
    if (size != 0) count = _vsnprintf_s(buf, size, _TRUNCATE, format, vlist);
    if (count == -1) count = _vscprintf(format, vlist);
    return count;
}
static yy_inline int yy_msvc_snprintf(char *buf, size_t size,
                                      const char *format, ...) {
    int count;
    va_list vlist;
    va_start(vlist, format);
    count = yy_msvc_vsnprintf(buf, size, format, vlist);
    va_end(vlist);
    return count;
}
#   endif
#endif

/* number of elements in c array */
#define yy_nelems(x)  (sizeof(x) / sizeof((x)[0]))

/* minimum */
#define yy_min(a, b) ((a) < (b) ? (a) : (b))

/* maximum */
#define yy_max(a, b) ((a) > (b) ? (a) : (b))



#ifdef __cplusplus
extern "C" {
#endif



/*==============================================================================
 * Type Defines
 *============================================================================*/

typedef float       f32;
typedef double      f64;
typedef int8_t      i8;
typedef uint8_t     u8;
typedef int16_t     i16;
typedef uint16_t    u16;
typedef int32_t     i32;
typedef uint32_t    u32;
typedef int64_t     i64;
typedef uint64_t    u64;
typedef size_t      usize;



// =============================================================================
// Pseudo Random Number Generator
// =============================================================================

/// Reset the random number generator with a seed (default 0).
void yy_rand_reset(u64 seed);

/// Generate a uniformly distributed 32-bit random integer.
u32 yy_rand_u32(void);
/// Generate a uniformly distributed 32-bit integer, where 0 <= r < bound.
u32 yy_rand_u32_uniform(u32 bound);
/// Generate a uniformly distributed 32-bit integer, where min <= r <= max.
u32 yy_rand_u32_range(u32 min, u32 max);

/// Generate a uniformly distributed 64-bit integer number.
u64 yy_rand_u64(void);
/// Generate a uniformly distributed 64-bit integer, where 0 <= r < bound.
u64 yy_rand_u64_uniform(u64 bound);
/// Generate a uniformly distributed 64-bit integer, where min <= r <= max.
u64 yy_rand_u64_range(u64 min, u64 max);

/// Generate a uniformly distributed random float, where 0.0 <= r < 1.0.
f32 yy_rand_f32(void);
/// Generate a uniformly distributed float number, where min <= r < max.
f32 yy_rand_f32_range(f32 min, f32 max);

/// Generate a uniformly distributed random double, where 0.0 <= r < 1.0.
f64 yy_rand_f64(void);
/// Generate a uniformly distributed number, where min <= r < max.
f64 yy_rand_f64_range(f64 min, f64 max);



/*==============================================================================
 * File Utils
 *============================================================================*/

#ifdef _WIN32
#define YY_DIR_SEPARATOR '\\'
#else
#define YY_DIR_SEPARATOR '/'
#endif

#define YY_MAX_PATH 4096



/** Combine multiple component into a path, store the result to the buffer.
    The input parameter list should end with NULL.
    Return false if input is NULL. */
#if yy_has_attribute(sentinel)
__attribute__((sentinel))
#endif
bool yy_path_combine(char *buf, const char *path, ...);

/** Remove the last component of path, copy the result to the buffer.
    Return false if input is NULL or no last component. */
bool yy_path_remove_last(char *buf, const char *path);

/** Get the last component of path, copy it to the buffer.
    Return false if input is NULL or no last component. */
bool yy_path_get_last(char *buf, const char *path);

/** Append a file extension to the path, copy the result to the buffer.
    Return false if input is NULL. */
bool yy_path_append_ext(char *buf, const char *path, const char *ext);

/** Remove the file extension, copy the result to the buffer.
    Return false if input is NULL or no extension. */
bool yy_path_remove_ext(char *buf, const char *path);

/** Get the file extension, copy it to the buffer.
    Return false if input is NULL or no extension. */
bool yy_path_get_ext(char *buf, const char *path);



/** Returns whether a path exist. */
bool yy_path_exist(const char *path);

/** Returns whether a path is directory. */
bool yy_path_is_dir(const char *path);



/** Returns content file names of a dir. Returns NULL on error.
    The result should be released by yy_dir_free() */
char **yy_dir_read(const char *path, int *count);

/** Returns content file full paths of a dir. Returns NULL on error.
    The result should be released by yy_dir_free() */
char **yy_dir_read_full(const char *path, int *count);

/** Free the return value of yy_dir_read(). */
void yy_dir_free(char **names);


/** Open a file pointer. */
FILE *yy_file_open(const char *path, const char *mode);

/** Read a file to memory, dat should be release with free(). */
bool yy_file_read(const char *path, u8 **dat, usize *len);

/** Read a file to memory with zero padding, dat should be release with free(). */
bool yy_file_read_with_padding(const char *path, u8 **dat, usize *len, usize padding);

/** Write data to file, overwrite if exist. */
bool yy_file_write(const char *path, u8 *dat, usize len);

/** Delete a file, returns true if success. */
bool yy_file_delete(const char *path);



/*==============================================================================
 * String Utils
 *============================================================================*/

/** Copy a string, same as strdup(). */
char *yy_str_copy(const char *str);

/** Compares the C string str1 to the C string str2, similar to strcmp(). */
int yy_str_cmp(const char *str1, const char *str2, bool ignore_case);

/** Returns whether the string contains a given string. */
bool yy_str_contains(const char *str, const char *search);

/** Returns whether the string begins with a prefix. */
bool yy_str_has_prefix(const char *str, const char *prefix);

/** Returns whether the string ends with a suffix. */
bool yy_str_has_suffix(const char *str, const char *suffix);

/** Returns whether the string is valid UTF-8. */
bool yy_str_is_utf8(const char *str, size_t len);



/*==============================================================================
 * Memory Buffer
 *============================================================================*/

/** A memory buffer s*/
typedef struct yy_buf {
    u8 *cur; /* cursor between hdr and end */
    u8 *hdr; /* head of the buffer */
    u8 *end; /* tail of the buffer */
    bool need_free;
} yy_buf;

/** Initialize a memory buffer with length. */
bool yy_buf_init(yy_buf *buf, usize len);

/** Release the memory in buffer. */
void yy_buf_release(yy_buf *buf);

/** Returns the used length of buffer (cur - hdr). */
usize yy_buf_len(yy_buf *buf);

/** Increase memory buffer and let (end - cur >= len). */
bool yy_buf_grow(yy_buf *buf, usize len);

/** Append data to buffer and move cursor. */
bool yy_buf_append(yy_buf *buf, u8 *dat, usize len);



/*==============================================================================
 * Data Reader
 *============================================================================*/

/** A data reader */
typedef struct yy_buf yy_dat;

/** Initialize a data reader with file. */
bool yy_dat_init_with_file(yy_dat *dat, const char *path);

/** Initialize a data reader with memory (no copy). */
bool yy_dat_init_with_mem(yy_dat *dat, u8 *mem, usize len);

/** Release the data reader. */
void yy_dat_release(yy_dat *dat);

/** Reset the cursor of data reader. */
void yy_dat_reset(yy_dat *dat);

/** Read a line from data reader (NULL on end or error).
    The cursor will moved to next line.
    The string is not null-terminated. */
char *yy_dat_read_line(yy_dat *dat, usize *len);

/** Read and copy a line from data reader (NULL on end or error).
    The cursor will moved to next line.
    The return value should be release with free(). */
char *yy_dat_copy_line(yy_dat *dat, usize *len);



#ifdef __cplusplus
}
#endif

#endif /* yy_test_utils_h */
