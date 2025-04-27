// This file is used to test the functionality of number reading and writing.
// It contains various test data to detect how numbers are handled in different
// boundary cases. The results are compared with google/double-conversion to
// ensure accuracy.

#include "yyjson.h"
#include "yy_test_utils.h"
#include "goo_double_conv.h"
#include <locale.h>

#if defined(__clang__)
#   pragma clang diagnostic push
#   pragma clang diagnostic ignored "-Wformat"
#elif defined(__GNUC__)
#   pragma GCC diagnostic push
#   pragma GCC diagnostic ignored "-Wformat"
#endif



#if !YYJSON_DISABLE_READER && !YYJSON_DISABLE_WRITER

/*==============================================================================
 * Number converter
 *============================================================================*/

typedef enum {
    NUM_TYPE_FAIL,
    NUM_TYPE_SINT,
    NUM_TYPE_UINT,
    NUM_TYPE_REAL,
    NUM_TYPE_INF_NAN_LITERAL,
} num_type;

/// Convert double to raw.
static yy_inline u64 f64_to_u64_raw(f64 f) {
    u64 u;
    memcpy((void *)&u, (void *)&f, sizeof(u64));
    return u;
}

/// Convert raw to double.
static yy_inline f64 f64_from_u64_raw(u64 u) {
    f64 f;
    memcpy((void *)&f, (void *)&u, sizeof(u64));
    return f;
}

/// Get a random finite double number.
static yy_inline f64 rand_f64(void) {
    while (true) {
        u64 u = yy_rand_u64();
        f64 f = f64_from_u64_raw(u);
        if (isfinite(f)) return f;
    };
}

/// Whether this character is digit.
static yy_inline bool char_is_digit(char c) {
    return '0' <= c && c <= '9';
}

/// Get the number of significant digit from a floating-point number string.
static yy_inline int str_get_sig_len(const char *str) {
    const char *cur = str, *dot = NULL, *hdr = NULL, *end = NULL;
    for (; *cur && *cur != 'e' && *cur != 'E' ; cur++) {
        if (*cur == '.') dot = cur;
        else if (char_is_digit(*cur)) {
            if (!hdr) hdr = cur;
            end = cur;
        }
    }
    if (!hdr) return 0;
    return (int)((end - hdr + 1) - (hdr < dot && dot < end));
}

/// Check JSON number format and return type (FAIL/SINT/UINT/REAL).
static yy_inline num_type check_json_num(const char *str) {
    bool sign = (*str == '-');
    str += sign;
    if (!char_is_digit(*str)) {
        if (!yy_str_cmp(str, "nan", true) ||
            !yy_str_cmp(str, "inf", true) ||
            !yy_str_cmp(str, "infinity", true)) return NUM_TYPE_INF_NAN_LITERAL;
        return NUM_TYPE_FAIL;
    }
    if (*str == '0' && char_is_digit(str[1])) return NUM_TYPE_FAIL;
    while (char_is_digit(*str)) str++;
    if (*str == '\0') return sign ? NUM_TYPE_SINT : NUM_TYPE_UINT;
    if (*str == '.') {
        str++;
        if (!char_is_digit(*str)) return NUM_TYPE_FAIL;
        while (char_is_digit(*str)) str++;
    }
    if (*str == 'e' || *str == 'E') {
        str++;
        if (*str == '-' || *str == '+') str++;
        if (!char_is_digit(*str)) return NUM_TYPE_FAIL;
        while (char_is_digit(*str)) str++;
    }
    if (*str == '\0') return NUM_TYPE_REAL;
    return NUM_TYPE_FAIL;
}



#if !YYJSON_DISABLE_FAST_FP_CONV && GOO_HAS_IEEE_754

/// read double from string
static usize f64_read(const char *str, f64 *val) {
    int str_len = (int)strlen(str);
    *val = goo_strtod(str, &str_len);
    return (usize)str_len;
}

/// write double to string
static usize f64_write(char *buf, usize len, f64 val) {
    return (usize)goo_dtoa(val, buf, (int)len);
}

#else

/// Get locale decimal point.
static char locale_decimal_point(void) {
    struct lconv *conv = localeconv();
    char c = conv->decimal_point[0];
    yy_assertf(c && conv->decimal_point[1] == '\0',
               "locale decimal point is invalid: %s\n", conv->decimal_point);
    return c;
}

/// read double from string
static usize f64_read(const char *str, f64 *val) {
    if (locale_decimal_point() != '.') {
        char *dup = yy_str_copy(str);
        for (char *cur = dup; *cur; cur++) {
            if (*cur == '.') *cur = locale_decimal_point();
        }
        char *end = NULL;
        *val = strtod(dup, &end);
        usize len = end ? (end - dup) : 0;
        free((void *)dup);
        return len;
    } else {
        char *end = NULL;
        *val = strtod(str, &end);
        usize len = end ? (end - str) : 0;
        return len;
    }
}

/// write double to string
static usize f64_write(char *buf, usize len, f64 val) {
    int out_len = snprintf(buf, len, "%.17g", val);
    if (out_len < 1 || out_len >= (int)len) return 0;
    if (locale_decimal_point() != '.') {
        char c = locale_decimal_point();
        for (int i = 0; i < out_len; i++) {
            if (buf[i] == c) buf[i] = '.';
        }
    }
    if (out_len >= 3 && !strncmp(buf, "inf", 3)) return snprintf(buf, len, "Infinity");
    if (out_len >= 4 && !strncmp(buf, "+inf", 4)) return snprintf(buf, len, "Infinity");
    if (out_len >= 4 && !strncmp(buf, "-inf", 4)) return snprintf(buf, len, "-Infinity");
    if (out_len >= 3 && !strncmp(buf, "nan", 3)) return snprintf(buf, len, "NaN");
    if (out_len >= 4 && !strncmp(buf, "-nan", 4)) return snprintf(buf, len, "NaN");
    if (out_len >= 4 && !strncmp(buf, "+nan", 4)) return snprintf(buf, len, "NaN");
    if (!strchr(buf, '.') && !strchr(buf, 'e') && !strchr(buf, 'e')) {
        if ((usize)out_len + 3 >= len) return 0;
        memcpy(buf + out_len, ".0", 3);
        out_len += 2;
    }
    return out_len;
}

#endif



/// check if there's overflow when reading as integer number
static bool check_int_overflow(const char *str, num_type type) {
    if (type != NUM_TYPE_SINT && type != NUM_TYPE_UINT) return false;
    
    bool sign = (*str == '-');
    str += sign;
    const char *max = sign ? "9223372036854775808" : "18446744073709551615";
    usize max_len = strlen(max);
    usize str_len = strlen(str);
    if (str_len > max_len) return true;
    if (str_len == max_len && strcmp(str, max) > 0) return true;
    return false;
}

/// check if there's overflow when reading as real number
static bool check_real_overflow(const char *str, num_type type) {
    if (type != NUM_TYPE_SINT && type != NUM_TYPE_UINT && type != NUM_TYPE_REAL) return false;
    f64 val = 0;
    f64_read(str, &val);
    return isinf(val);
}



/*==============================================================================
 * Test single number (uint)
 *============================================================================*/

static void test_uint_read(const char *line, usize len, u64 num) {
#if !YYJSON_DISABLE_READER
    yyjson_doc *doc = yyjson_read(line, len, 0);
    yyjson_val *val = yyjson_doc_get_root(doc);
    yy_assertf(yyjson_is_uint(val),
               "num should be read as uint: %s\n", line);
    u64 get = yyjson_get_uint(val);
    yy_assertf(num == get,
               "uint num read not match:\nstr: %s\nreturn: %llu\nexpect: %llu\n",
               line, get, num);
    yyjson_doc_free(doc);
    
    // read number as raw
    doc = yyjson_read(line, len, YYJSON_READ_NUMBER_AS_RAW);
    val = yyjson_doc_get_root(doc);
    yy_assertf(yyjson_is_raw(val),
               "num should be read as raw: %s\n", line);
    yy_assertf(strcmp(line, yyjson_get_raw(val)) == 0,
               "uint num read as raw not match:\nstr: %s\nreturn: %s\n",
               line, yyjson_get_raw(val));
    yyjson_doc_free(doc);
    
    // read big number as raw
    doc = yyjson_read(line, len, YYJSON_READ_BIGNUM_AS_RAW);
    val = yyjson_doc_get_root(doc);
    yy_assertf(yyjson_is_uint(val),
               "num should be read as uint: %s\n", line);
    get = yyjson_get_uint(val);
    yy_assertf(num == get,
               "uint num read not match:\nstr: %s\nreturn: %llu\nexpect: %llu\n",
               line, get, num);
    yyjson_doc_free(doc);
#endif
}

static void test_uint_write(const char *line, u64 num) {
#if !YYJSON_DISABLE_WRITER
    yyjson_mut_doc *doc = yyjson_mut_doc_new(NULL);
    yyjson_mut_val *val = yyjson_mut_uint(doc, num);
    yyjson_mut_doc_set_root(doc, val);
    
    usize out_len;
    char *out_str;
    out_str = yyjson_mut_write(doc, 0, &out_len);
    yy_assertf(strcmp(out_str, line) == 0,
               "uint num write not match:\nstr: %s\nreturn: %s\nexpect: %s\n",
               line, out_str, line);
    
    free(out_str);
    yyjson_mut_doc_free(doc);
#endif
}

static void test_uint(const char *line, usize len) {
    yy_assertf(check_json_num(line) == NUM_TYPE_UINT,
               "input is not uint: %s\n", line);
    u64 num = strtoull(line, NULL, 10);
    test_uint_read(line, len, num);
    
    yyjson_val val;
    const char *ptr = yyjson_read_number(line, &val, 0, NULL, NULL);
    yy_assertf(ptr == &line[len], "uint num fail: %s\n", line);
    yy_assertf(yyjson_is_uint(&val),
               "num should be read as uint: %s\n", line);
    u64 get = yyjson_get_uint(&val);
    yy_assertf(num == get,
               "uint num read not match:\nstr: %s\nreturn: %llu\nexpect: %llu\n",
               line, get, num);
    
    ptr = yyjson_read_number(line, &val, YYJSON_READ_NUMBER_AS_RAW, NULL, NULL);
    yy_assertf(ptr == &line[len], "uint num fail: %s\n", line);
    yy_assertf(yyjson_is_raw(&val),
               "num should be read as raw: %s\n", line);
    yy_assertf(strcmp(line, yyjson_get_raw(&val)) == 0,
               "uint num read as raw not match:\nstr: %s\nreturn: %s\n",
               line, yyjson_get_raw(&val));
    
    ptr = yyjson_read_number(line, &val, YYJSON_READ_BIGNUM_AS_RAW, NULL, NULL);
    yy_assertf(ptr == &line[len], "uint num fail: %s\n", line);
    yy_assertf(yyjson_is_uint(&val),
               "num should be read as uint: %s\n", line);
    get = yyjson_get_uint(&val);
    yy_assertf(num == get,
               "uint num read not match:\nstr: %s\nreturn: %llu\nexpect: %llu\n",
               line, get, num);
    
    char buf[32] = { 0 };
    snprintf(buf, 32, "%llu%c", num, '\0');
    test_uint_write(buf, num);
}



/*==============================================================================
 * Test single number (sint)
 *============================================================================*/

static void test_sint_read(const char *line, usize len, i64 num) {
#if !YYJSON_DISABLE_READER
    yyjson_doc *doc = yyjson_read(line, len, 0);
    yyjson_val *val = yyjson_doc_get_root(doc);
    yy_assertf(yyjson_is_sint(val),
               "num should be read as sint: %s\n", line);
    i64 get = yyjson_get_sint(val);
    yy_assertf(num == get,
               "uint num read not match:\nstr: %s\nreturn: %lld\nexpect: %lld\n",
               line, get, num);
    yyjson_doc_free(doc);
    
    // read number as raw
    doc = yyjson_read(line, len, YYJSON_READ_NUMBER_AS_RAW);
    val = yyjson_doc_get_root(doc);
    yy_assertf(yyjson_is_raw(val),
               "num should be read as raw: %s\n", line);
    yy_assertf(strcmp(line, yyjson_get_raw(val)) == 0,
               "sint num read as raw not match:\nstr: %s\nreturn: %s\n",
               line, yyjson_get_raw(val));
    yyjson_doc_free(doc);
    
    // read big number as raw
    doc = yyjson_read(line, len, YYJSON_READ_BIGNUM_AS_RAW);
    val = yyjson_doc_get_root(doc);
    yy_assertf(yyjson_is_sint(val),
               "num should be read as sint: %s\n", line);
    get = yyjson_get_sint(val);
    yy_assertf(num == get,
               "uint num read not match:\nstr: %s\nreturn: %lld\nexpect: %lld\n",
               line, get, num);
    yyjson_doc_free(doc);
#endif
}

static void test_sint_write(const char *line, i64 num) {
#if !YYJSON_DISABLE_WRITER
    yyjson_mut_doc *doc = yyjson_mut_doc_new(NULL);
    yyjson_mut_val *val = yyjson_mut_sint(doc, num);
    yyjson_mut_doc_set_root(doc, val);
    
    char *str = yyjson_mut_write(doc, 0, NULL);
    yy_assertf(strcmp(str, line) == 0,
               "uint num write not match:\nreturn: %s\nexpect: %s\n",
               str, line);
    free(str);
    yyjson_mut_doc_free(doc);
#endif
}

static void test_sint(const char *line, usize len) {
    yy_assertf(check_json_num(line) == NUM_TYPE_SINT,
               "input is not sint: %s\n", line);
    
    i64 num = strtoll(line, NULL, 10);
    test_sint_read(line, len, num);
    
    yyjson_val val;
    const char *ptr = yyjson_read_number(line, &val, 0, NULL, NULL);
    yy_assertf(ptr == &line[len], "sint num fail: %s\n", line);
    yy_assertf(yyjson_is_sint(&val),
               "num should be read as sint: %s\n", line);
    i64 get = yyjson_get_sint(&val);
    yy_assertf(num == get,
               "sint num read not match:\nstr: %s\nreturn: %lld\nexpect: %lld\n",
               line, get, num);
    
    ptr = yyjson_read_number(line, &val, YYJSON_READ_NUMBER_AS_RAW, NULL, NULL);
    yy_assertf(ptr == &line[len], "uint num fail: %s\n", line);
    yy_assertf(yyjson_is_raw(&val),
               "num should be read as raw: %s\n", line);
    yy_assertf(strcmp(line, yyjson_get_raw(&val)) == 0,
               "sint num read as raw not match:\nstr: %s\nreturn: %s\n",
               line, yyjson_get_raw(&val));
    
    ptr = yyjson_read_number(line, &val, YYJSON_READ_BIGNUM_AS_RAW, NULL, NULL);
    yy_assertf(ptr == &line[len], "sint num fail: %s\n", line);
    yy_assertf(yyjson_is_sint(&val),
               "num should be read as sint: %s\n", line);
    get = yyjson_get_sint(&val);
    yy_assertf(num == get,
               "sint num read not match:\nstr: %s\nreturn: %lld\nexpect: %lld\n",
               line, get, num);
    
    char buf[32] = { 0 };
    snprintf(buf, 32, "%lld%c", num, '\0');
    test_sint_write(buf, num);
    num = (i64)((u64)~num + 1); // num = -num, avoid ubsan
    snprintf(buf, 32, "%lld%c", num, '\0');
    test_sint_write(buf, num);
}



/*==============================================================================
 * Test single number (real)
 *============================================================================*/

static void test_real_read(const char *line, usize len, f64 num) {
#if !YYJSON_DISABLE_READER
    f64 ret;
    yyjson_doc *doc;
    yyjson_val *val;
    if (isinf(num)) {
        // read number as JSON value
        doc = yyjson_read(line, len, 0);
        val = yyjson_doc_get_root(doc);
        yy_assertf(!doc, "num %s should fail, but returns %.17g\n",
                   line, yyjson_get_real(val));
        
        // read number as raw string
        doc = yyjson_read(line, len, YYJSON_READ_NUMBER_AS_RAW);
        val = yyjson_doc_get_root(doc);
        yy_assertf(yyjson_is_raw(val),
                   "num should be read as raw: %s\n", line);
        yy_assertf(strcmp(line, yyjson_get_raw(val)) == 0,
                   "num read as raw not match:\nstr: %s\nreturn: %s\n",
                   line, yyjson_get_raw(val));
        yyjson_doc_free(doc);
        
        // read big number as raw string
        doc = yyjson_read(line, len, YYJSON_READ_BIGNUM_AS_RAW);
        val = yyjson_doc_get_root(doc);
        yy_assertf(yyjson_is_raw(val),
                   "num should be read as raw: %s\n", line);
        yy_assertf(strcmp(line, yyjson_get_raw(val)) == 0,
                   "num read as raw not match:\nstr: %s\nreturn: %s\n",
                   line, yyjson_get_raw(val));
        yyjson_doc_free(doc);
        
#if !YYJSON_DISABLE_NON_STANDARD
        // read number as JSON value
        doc = yyjson_read(line, len, YYJSON_READ_ALLOW_INF_AND_NAN);
        val = yyjson_doc_get_root(doc);
        ret = yyjson_get_real(val);
        yy_assertf(yyjson_is_real(val) && (ret == num),
                   "num %s should be read as inf, but returns %.17g\n",
                   line, ret);
        yyjson_doc_free(doc);
#endif
        
    } else {
        u64 num_raw = f64_to_u64_raw(num);
        u64 ret_raw;
        i64 ulp;
        
        // 0 ulp error
        doc = yyjson_read(line, len, 0);
        val = yyjson_doc_get_root(doc);
        ret = yyjson_get_real(val);
        ret_raw = f64_to_u64_raw(ret);
        ulp = (i64)num_raw - (i64)ret_raw;
        if (ulp < 0) ulp = -ulp;
        yy_assertf(yyjson_is_real(val) && ulp == 0,
                   "string %s should be read as %.17g, but returns %.17g\n",
                   line, num, ret);
        yyjson_doc_free(doc);
        
        // read big number as raw
        doc = yyjson_read(line, len, YYJSON_READ_BIGNUM_AS_RAW);
        val = yyjson_doc_get_root(doc);
        if (check_json_num(line) == NUM_TYPE_REAL) {
            yy_assert(yyjson_is_real(val));
        } else {
            yy_assert(yyjson_is_raw(val));
        }
        yyjson_doc_free(doc);
    }
#endif
}

static void test_real_write(const char *line, usize len, f64 num) {
#if !YYJSON_DISABLE_WRITER
    yyjson_mut_doc *doc = yyjson_mut_doc_new(NULL);
    yyjson_mut_val *val = yyjson_mut_real(doc, num);
    yyjson_mut_doc_set_root(doc, val);
    
    char *str = yyjson_mut_write(doc, 0, NULL);
    char *str_nan_inf = yyjson_mut_write(doc, YYJSON_WRITE_ALLOW_INF_AND_NAN, NULL);
    
    if (isnan(num) || isinf(num)) {
        yy_assertf(str == NULL,
                   "num write should return NULL, but returns: %s\n",
                   str);
#if !YYJSON_DISABLE_NON_STANDARD
        yy_assertf(strcmp(str_nan_inf, line) == 0,
                   "num write not match:\nexpect: %s\nreturn: %s\n",
                   line, str_nan_inf);
#else
        yy_assertf(str_nan_inf == NULL,
                   "num write should return NULL, but returns: %s\n",
                   str);
#endif
    } else {
        yy_assert(strcmp(str, str_nan_inf) == 0);
        f64 dst_num;
        yy_assert(f64_read(str, &dst_num) == strlen(str));
        u64 dst_raw = f64_to_u64_raw(dst_num);
        u64 num_raw = f64_to_u64_raw(num);
        yy_assertf(dst_raw == num_raw,
                   "real number write value not match:\nexpect: %s\nreturn: %s\n",
                   line, str);
        yy_assertf(str_get_sig_len(str) == str_get_sig_len(line),
                   "real number write value not shortest:\nexpect: %s\nreturn: %s\n",
                   line, str);
        yy_assertf(check_json_num(str) == NUM_TYPE_REAL,
                   "real number write value not valid JSON format: %s\nreturn: %s\n",
                   line, str);
    }
    
    yyjson_mut_doc_free(doc);
    if (str) free(str);
    if (str_nan_inf) free(str_nan_inf);
    
#endif
}

static void test_real(const char *line, usize len) {
    yy_assertf(check_json_num(line) != NUM_TYPE_FAIL,
               "input is not number: %s\n", line);
    
    f64 num;
    usize read_len = f64_read(line, &num);
    yy_assertf(len == read_len,
               "f64_read() failed: %s\ninput length: %d\nread length: %d\n",
               line, (int)len, (int)read_len);
    yy_assertf(!isnan(num),
               "f64_read() failed: %s\nread as NaN", line);
    test_real_read(line, len, num);
    
    yyjson_val val;
    const char *ptr = yyjson_read_number(line, &val, 0, NULL, NULL);
    
    if (isinf(num)) {
        yy_assertf(ptr == NULL, "num %s should fail, but returns %.17g\n",
                   line, yyjson_get_real(&val));
        
#if !YYJSON_DISABLE_NON_STANDARD
        ptr = yyjson_read_number(line, &val, YYJSON_READ_ALLOW_INF_AND_NAN, NULL, NULL);
        yy_assertf(ptr == &line[len], "real num fail: %s\n", line);
        
        yy_assertf(yyjson_is_real(&val),
                   "num should be read as real: %s\n", line);
        
        f64 get = yyjson_get_real(&val);
        yy_assertf(isinf(get), "num should be read as inf: %s\n", line);
#endif
    } else {
        yy_assertf(ptr == &line[len], "real num fail: %s\n", line);
        
        yy_assertf(yyjson_is_real(&val),
                   "num should be read as real: %s\n", line);
        
        f64 get = yyjson_get_real(&val);
        yy_assertf(f64_to_u64_raw(num) == f64_to_u64_raw(get),
                   "real num read not match:\nstr: %s\n"
                   "return: %.17g\n"
                   "expect: %.17g\n",
                   line, get, num);
    }
    
    ptr = yyjson_read_number(line, &val, YYJSON_READ_NUMBER_AS_RAW, NULL, NULL);
    yy_assertf(ptr == &line[len], "uint num fail: %s\n", line);
    
    yy_assertf(yyjson_is_raw(&val),
               "num should be read as raw: %s\n", line);
    
    yy_assertf(strcmp(line, yyjson_get_raw(&val)) == 0,
               "sint num read as raw not match:\nstr: %s\nreturn: %s\n",
               line, yyjson_get_raw(&val));
    
    char buf[32] = { 0 };
    usize write_len = f64_write(buf, sizeof(buf), num);
    yy_assertf(write_len > 0,
               "f64_write() fail: %s\n", line);
    test_real_write(buf, write_len, num);
}



/*==============================================================================
 * Test single number (nan/inf)
 *============================================================================*/

static void test_nan_inf_read(const char *line, usize len, f64 num) {
#if !YYJSON_DISABLE_READER
    f64 ret;
    yyjson_doc *doc;
    yyjson_val *val;
    
    // read fail
    doc = yyjson_read(line, len, 0);
    yy_assertf(doc == NULL, "number %s should fail in default mode\n", line);
    doc = yyjson_read(line, len, YYJSON_READ_NUMBER_AS_RAW);
    yy_assertf(doc == NULL, "number %s should fail in raw mode\n", line);
    
    // read allow
    doc = yyjson_read(line, len, YYJSON_READ_ALLOW_INF_AND_NAN);
    val = yyjson_doc_get_root(doc);
    ret = yyjson_get_real(val);
    yy_assertf(yyjson_is_real(val), "nan or inf read fail: %s \n", line);
    if (isnan(num)) {
        yy_assertf(isnan(ret), "num %s should read as nan\n", line);
    } else {
        yy_assertf(ret == num, "num %s should read as inf\n", line);
    }
    yyjson_doc_free(doc);
    
    // read raw
    doc = yyjson_read(line, len, YYJSON_READ_ALLOW_INF_AND_NAN | YYJSON_READ_NUMBER_AS_RAW);
    val = yyjson_doc_get_root(doc);
    yy_assertf(yyjson_is_raw(val),
               "num should be read as raw: %s\n", line);
    yy_assertf(strcmp(line, yyjson_get_raw(val)) == 0,
               "num read as raw not match:\nstr: %s\nreturn: %s\n",
               line, yyjson_get_raw(val));
    yyjson_doc_free(doc);
    
#endif
}

static void test_nan_inf_write(const char *line, usize len, f64 num) {
#if !YYJSON_DISABLE_WRITER
    yyjson_mut_doc *doc = yyjson_mut_doc_new(NULL);
    yyjson_mut_val *val = yyjson_mut_real(doc, num);
    yyjson_mut_doc_set_root(doc, val);
    
    char *str = yyjson_mut_write(doc, 0, NULL);
    char *str_nan_inf = yyjson_mut_write(doc, YYJSON_WRITE_ALLOW_INF_AND_NAN, NULL);
    
    yy_assertf(str == NULL,
               "num write should return NULL, but returns: %s\n",
               str);
    yy_assertf(strcmp(str_nan_inf, line) == 0,
               "num write not match:\nexpect: %s\nreturn: %s\n",
               line, str_nan_inf);
    
    yyjson_mut_doc_free(doc);
    if (str) free(str);
    if (str_nan_inf) free(str_nan_inf);
#endif
}

static void test_nan_inf(const char *line, usize len) {
    num_type type = check_json_num(line);
    yy_assertf(type == NUM_TYPE_FAIL || type == NUM_TYPE_INF_NAN_LITERAL,
               "input should not be a valid JSON number: %s\n", line);
    
    f64 num;
    usize read_len = f64_read(line, &num);
    yy_assertf(len == read_len,
               "f64_read() failed: %s\ninput length: %d\nread length: %d\n",
               line, (int)len, (int)read_len);
    yy_assertf(isnan(num) || isinf(num),
               "f64_read() failed: %s\nexpect: NaN or Inf, out: %.17g", line, num);
    test_nan_inf_read(line, len, num);
    
    yyjson_val val;
    const char *ptr = yyjson_read_number(line, &val, 0, NULL, NULL);
    yy_assertf(ptr != &line[len], "num should fail: %s\n", line);
    
    char buf[32] = { 0 };
    usize write_len = f64_write(buf, sizeof(buf), num);
    yy_assertf(write_len > 0,
               "f64_write() fail: %s\n", line);
    test_nan_inf_write(buf, write_len, num);
}



/*==============================================================================
 * Test single number (fail)
 *============================================================================*/

static void test_fail(const char *line, usize len) {
#if !YYJSON_DISABLE_READER
    yyjson_doc *doc;
    doc = yyjson_read(line, len, 0);
    yy_assertf(doc == NULL, "num should fail: %s\n", line);
    doc = yyjson_read(line, len, YYJSON_READ_ALLOW_INF_AND_NAN);
    yy_assertf(doc == NULL, "num should fail: %s\n", line);
    doc = yyjson_read(line, len, YYJSON_READ_NUMBER_AS_RAW);
    yy_assertf(doc == NULL, "num should fail: %s\n", line);
    doc = yyjson_read(line, len, YYJSON_READ_NUMBER_AS_RAW | 
                                 YYJSON_READ_ALLOW_INF_AND_NAN);
    yy_assertf(doc == NULL, "num should fail: %s\n", line);
    
    yyjson_val val;
    const char *ptr;
    ptr = yyjson_read_number(line, &val, 0, NULL, NULL);
    yy_assertf(ptr != &line[len], "num should fail: %s\n", line);
    ptr = yyjson_read_number(line, &val, YYJSON_READ_ALLOW_INF_AND_NAN, NULL, NULL);
    yy_assertf(ptr != &line[len], "num should fail: %s\n", line);
    ptr = yyjson_read_number(line, &val, YYJSON_READ_NUMBER_AS_RAW, NULL, NULL);
    yy_assertf(ptr != &line[len], "num should fail: %s\n", line);
    ptr = yyjson_read_number(line, &val, YYJSON_READ_NUMBER_AS_RAW | 
                                         YYJSON_READ_ALLOW_INF_AND_NAN, NULL, NULL);
    yy_assertf(ptr != &line[len], "num should fail: %s\n", line);
#endif
}



/*==============================================================================
 * Test with input file
 *============================================================================*/

static void test_with_file(const char *name, num_type type) {
    char path[YY_MAX_PATH];
    yy_path_combine(path, YYJSON_TEST_DATA_PATH, "data", "num", name, NULL);
    yy_dat dat;
    bool file_suc = yy_dat_init_with_file(&dat, path);
    yy_assertf(file_suc == true, "file read fail: %s\n", path);
    
    usize len;
    const char *line;
    while ((line = yy_dat_copy_line(&dat, &len))) {
        if (len && line[0] != '#') {
            if (type == NUM_TYPE_FAIL) {
                test_fail(line, len);
            } else if (type == NUM_TYPE_UINT) {
                test_uint(line, len);
            } else if (type == NUM_TYPE_SINT) {
                test_sint(line, len);
            } else if (type == NUM_TYPE_REAL) {
                test_real(line, len);
            } else if (type == NUM_TYPE_INF_NAN_LITERAL) {
#if !YYJSON_DISABLE_NON_STANDARD
                test_nan_inf(line, len);
#endif
            }
        }
        free((void *)line);
    }
    
    yy_dat_release(&dat);
    
    yyjson_mut_val val;
    const char *ptr;
    ptr = yyjson_mut_read_number(NULL, &val, 0, NULL, NULL);
    yy_assertf(ptr == NULL, "read line NULL should fail\n");
    ptr = yyjson_mut_read_number("123", NULL, 0, NULL, NULL);
    yy_assertf(ptr == NULL, "read val NULL should fail\n");
    ptr = yyjson_mut_read_number(NULL, NULL, 0, NULL, NULL);
    yy_assertf(ptr == NULL, "read line and val NULL should fail\n");
}



/*==============================================================================
 * Test with random value
 *============================================================================*/

static void test_random_int(void) {
    char buf[32] = { 0 };
    char *end;
    int count = 10000;
    
    yy_rand_reset(0);
    for (int i = 0; i < count; i++) {
        u64 rnd = yy_rand_u64();
        end = buf + snprintf(buf, 32, "%llu%c", rnd, '\0') - 1;
        test_uint(buf, end - buf);
    }
    
    yy_rand_reset(0);
    for (int i = 0; i < count; i++) {
        i64 rnd = (i64)(yy_rand_u64() | ((u64)1 << 63));
        end = buf + snprintf(buf, 32, "%lld%c", rnd, '\0') - 1;
        test_sint(buf, end - buf);
    }
    
    yy_rand_reset(0);
    for (int i = 0; i < count; i++) {
        u32 rnd = yy_rand_u32();
        end = buf + snprintf(buf, 32, "%u%c", rnd, '\0') - 1;
        test_uint(buf, end - buf);
    }
    
    yy_rand_reset(0);
    for (int i = 0; i < count; i++) {
        i32 rnd = (i32)(yy_rand_u32() | ((u32)1 << 31));
        end = buf + snprintf(buf, 32, "%i%c", rnd, '\0') - 1;
        test_sint(buf, end - buf);
    }
}

static void test_random_real(void) {
    char buf[32] = { 0 };
    char *end;
    int count = 10000;
    
    yy_rand_reset(0);
    for (int i = 0; i < count; i++) {
        f64 rnd = rand_f64();
        usize out_len = f64_write(buf, sizeof(buf), rnd);
        end = buf + out_len;
        yy_assertf(out_len > 0, "f64_write() fail: %.17g\n", rnd);
        if (!yy_str_contains(buf, "e") && !yy_str_contains(buf, ".")) {
            *end++ = '.';
            *end++ = '0';
            *end = '\0';
        }
        test_real(buf, end - buf);
    }
}

static void test_bignum(void) {
    const char *num_arr[] = {
        "0", // uint
        "-0", // sint
        "0.0", // real
        "-0.0", // real
        
        "123", // uint
        "-123", // sint
        "123.0", // real
        "-123.0", // real
        
        "9223372036854775808", // uint
        "9223372036854775808.0", // real
        
        "18446744073709551615", // uint
        "18446744073709551615.0", // real
        "18446744073709551616", // uint overflow
        "184467440737095516160", // uint overflow
        
        "-9223372036854775808", // sint
        "-9223372036854775808.0", // real
        "-9223372036854775809", // sint overflow
        "-92233720368547758090", // sint overflow
        
        "12345678901234567890123456789012345678901234567890"
        "12345678901234567890123456789012345678901234567890"
        "12345678901234567890123456789012345678901234567890"
        "12345678901234567890123456789012345678901234567890"
        "12345678901234567890123456789012345678901234567890"
        "12345678901234567890123456789012345678901234567890"
        "12345678901234567890123456789012345678901234567890"
        "12345678901234567890123456789012345678901234567890", // uint->real overflow
        "-12345678901234567890123456789012345678901234567890"
        "12345678901234567890123456789012345678901234567890"
        "12345678901234567890123456789012345678901234567890"
        "12345678901234567890123456789012345678901234567890"
        "12345678901234567890123456789012345678901234567890"
        "12345678901234567890123456789012345678901234567890"
        "12345678901234567890123456789012345678901234567890"
        "12345678901234567890123456789012345678901234567890", // sint->real overflow
        
        "123e999", // real overflow
        "-123e999", // real overflow
        
        "NaN", // nan
        "Inf", // inf
        "001", // fail
    };
    
    yyjson_read_flag flag_arr[] = {
        YYJSON_READ_NUMBER_AS_RAW,
        YYJSON_READ_BIGNUM_AS_RAW,
#if !YYJSON_DISABLE_NON_STANDARD
        YYJSON_READ_ALLOW_INF_AND_NAN,
#endif
    };
    
    for (usize i = 0; i < yy_nelems(num_arr); i++) {
        const char *num_str = num_arr[i];
        usize num_len = strlen(num_str);
        num_type type = check_json_num(num_str);
        
        // test flag combination
        u32 flag_count = (u32)yy_nelems(flag_arr);
        u32 comb_count = 1 << flag_count;
        for (u32 c = 0; c < comb_count; c++) {
            yyjson_read_flag comb_flag = 0;
            for (u32 f = 0; f < flag_count; f++) {
                if (c & (1 << f)) comb_flag |= flag_arr[f];
            }
            
            yyjson_doc *doc = yyjson_read(num_str, num_len, comb_flag);
            yyjson_val *val = yyjson_doc_get_root(doc);
            if (type == NUM_TYPE_FAIL) {
                // invalid number format
                yy_assert(!doc);
            } else if (comb_flag & YYJSON_READ_NUMBER_AS_RAW) {
                // all number should be raw
                if (type == NUM_TYPE_INF_NAN_LITERAL &&
                    !(comb_flag & YYJSON_READ_ALLOW_INF_AND_NAN)) {
                    yy_assert(!doc);
                } else {
                    yy_assert(yyjson_is_raw(val));
                    yy_assert(!strcmp(num_str, yyjson_get_raw(val)));
                }
            } else switch (type) {
                case NUM_TYPE_SINT:
                case NUM_TYPE_UINT: {
                    // integer number format
                    if (!check_int_overflow(num_str, type)) {
                        // integer number not overflow
                        yy_assert(yyjson_is_int(val));
                    } else if (!check_real_overflow(num_str, type)) {
                        // integer number overflow, but real number not overflow
                        if (comb_flag & YYJSON_READ_BIGNUM_AS_RAW) {
                            yy_assert(yyjson_is_raw(val));
                            yy_assert(!strcmp(num_str, yyjson_get_raw(val)));
                        } else {
                            yy_assert(yyjson_is_real(val));
                        }
                    } else {
                        // real number overflow
                        if (comb_flag & YYJSON_READ_BIGNUM_AS_RAW) {
                            yy_assert(yyjson_is_raw(val));
                            yy_assert(!strcmp(num_str, yyjson_get_raw(val)));
                        } else if (comb_flag & YYJSON_READ_ALLOW_INF_AND_NAN) {
                            yy_assert(yyjson_is_real(val));
                        } else {
                            yy_assert(!doc);
                        }
                    }
                    break;
                }
                case NUM_TYPE_REAL: {
                    // real number
                    if (!check_real_overflow(num_str, type)) {
                        // real number not overflow
                        yy_assert(yyjson_is_real(val));
                    } else {
                        // real number overflow
                        if (comb_flag & YYJSON_READ_BIGNUM_AS_RAW) {
                            yy_assert(yyjson_is_raw(val));
                            yy_assert(!strcmp(num_str, yyjson_get_raw(val)));
                        } else if (comb_flag & YYJSON_READ_ALLOW_INF_AND_NAN) {
                            yy_assert(yyjson_is_real(val));
                        } else {
                            yy_assert(!doc);
                        }
                    }
                    break;
                }
                case NUM_TYPE_INF_NAN_LITERAL: {
                    if ((comb_flag & YYJSON_READ_ALLOW_INF_AND_NAN)) {
                        if (comb_flag & YYJSON_READ_BIGNUM_AS_RAW) {
                            yy_assert(yyjson_is_raw(val));
                            yy_assert(!strcmp(num_str, yyjson_get_raw(val)));
                        } else {
                            yy_assert(yyjson_is_real(val));
                        }
                    } else {
                        yy_assert(!doc);
                    }
                    break;
                }
                default: {
                    break;
                }
            }
            
            {   // check minity format
                usize buf_len = num_len + 2;
                char *buf = calloc(1, buf_len + 1);
                buf[0] = '[';
                memcpy(buf + 1, num_str, num_len);
                buf[num_len + 1] = ']';
                yyjson_doc *doc2 = yyjson_read(buf, buf_len, comb_flag);
                yyjson_val *val2 = yyjson_arr_get_first(yyjson_doc_get_root(doc2));
                yy_assert(val == val2 || yyjson_equals(val, val2));
                yyjson_doc_free(doc2);
                free(buf);
            }
            {   // check pretty format
                usize buf_len = num_len + 6;
                char *buf = calloc(1, buf_len + 1);
                memcpy(buf, "[\n  ", 4);
                memcpy(buf + 4, num_str, num_len);
                memcpy(buf + 4 + num_len, "\n]", 2);
                yyjson_doc *doc2 = yyjson_read(buf, buf_len, comb_flag);
                yyjson_val *val2 = yyjson_arr_get_first(yyjson_doc_get_root(doc2));
                yy_assert(val == val2 || yyjson_equals(val, val2));
                yyjson_doc_free(doc2);
                free(buf);
            }
            
            yyjson_doc_free(doc);
        }
    }
}


/*==============================================================================
 * Test all
 *============================================================================*/

static void test_number_locale(void) {
    test_with_file("num_fail.txt", NUM_TYPE_FAIL);
    test_with_file("uint_pass.txt", NUM_TYPE_UINT);
    test_with_file("sint_pass.txt", NUM_TYPE_SINT);
    test_with_file("uint_bignum.txt", NUM_TYPE_REAL);
    test_with_file("sint_bignum.txt", NUM_TYPE_REAL);
    test_with_file("real_pass_1.txt", NUM_TYPE_REAL);
    test_with_file("real_pass_2.txt", NUM_TYPE_REAL);
    test_with_file("real_pass_3.txt", NUM_TYPE_REAL);
    test_with_file("real_pass_4.txt", NUM_TYPE_REAL);
    test_with_file("real_pass_5.txt", NUM_TYPE_REAL);
    test_with_file("real_pass_6.txt", NUM_TYPE_REAL);
    test_with_file("real_pass_7.txt", NUM_TYPE_REAL);
    test_with_file("nan_inf_literal_pass.txt", NUM_TYPE_INF_NAN_LITERAL);
    test_with_file("nan_inf_literal_fail.txt", NUM_TYPE_FAIL);
    test_random_int();
    test_random_real();
    test_bignum();
}

yy_test_case(test_number) {
    setlocale(LC_ALL, "C");     // decimal point is '.'
    test_number_locale();
    setlocale(LC_ALL, "fr_FR"); // decimal point is ','
    test_number_locale();
}


#else
yy_test_case(test_number) {}
#endif


#if defined(__clang__)
#   pragma clang diagnostic pop
#elif defined(__GNUC__)
#   pragma GCC diagnostic pop
#endif
