// This file is used to test string processing and Unicode encoding validation.

#include "yyjson.h"
#include "yy_test_utils.h"

// Validate string encoding and decoding.
// This file must be compiled with UTF-8 encoding.

typedef struct {
    const char *str;
    usize len;
} string_val;

typedef struct {
    string_val str;         // raw string
    string_val esc_non;     // json string
    string_val esc_sla;     // json string escape slashes
    string_val esc_uni;     // json string escape unicode
    string_val esc_all;     // json string escape unicode and slashes
    bool invalid_unicode;   // flag `ALLOW_INVALID_UNICODE`
} string_set;

/// `src` should be decoded as `dst` with flag.
static void validate_str_read(string_val *src,
                              string_val *dst,
                              yyjson_read_flag flg) {
#if !YYJSON_DISABLE_READER
    
#if YYJSON_DISABLE_UTF8_VALIDATION
    if (src->str && !yy_str_is_utf8(src->str, src->len)) return;
#endif
    
    string_val empty = { NULL, 0 };
    if (!src || !src->str) return;
    if (!dst) dst = &empty;
    
    usize buf_len = src->len + 2;
    char *buf = malloc(buf_len);
    buf[0] = '"';
    memcpy(buf + 1, src->str, src->len);
    buf[buf_len - 1] = '"';
    
    yyjson_doc *doc = yyjson_read(buf, buf_len, flg);
    if (dst->str) {
        yy_assertf(doc,
                   "read fail,\ninput: \"%s\" len:%u\nexpect:\"%s\" len:%u\noutput:NULL doc\n",
                   src->str, (u32)src->len, dst->str, (u32)dst->len);
        yyjson_val *val = yyjson_doc_get_root(doc);
        yy_assertf(yyjson_is_str(val),
                   "read fail,\ninput: \"%s\" len:%u\nexpect:\"%s\" len:%u\noutput:root not string\n",
                   src->str, (u32)src->len, dst->str, (u32)dst->len);
        yy_assertf(yyjson_equals_strn(val, dst->str, dst->len),
                   "read fail,\ninput: \"%s\" len:%u\nexpect:\"%s\" len:%u\noutput:\"%s\" len:%u\n",
                   src->str, (u32)src->len, dst->str, (u32)dst->len,
                   yyjson_get_str(val), (u32)yyjson_get_len(val));
    } else {
        yy_assertf(!doc,
                   "input string should be rejected by reader, but accepted: \"%s\"\n",
                   src->str);
    }
    free(buf);
    yyjson_doc_free(doc);
    
#endif
}

static void validate_roundtrip(char *str, usize len, yyjson_write_flag flg) {
#if !YYJSON_DISABLE_READER && !YYJSON_DISABLE_WRITER
    
#if YYJSON_DISABLE_UTF8_VALIDATION
    if (str && !yy_str_is_utf8(str, len)) return;
#endif
    
    yyjson_doc *doc = yyjson_read(str, len, YYJSON_READ_ALLOW_INVALID_UNICODE);
    usize ret_len = 0;
    char *ret = yyjson_write(doc, flg, &ret_len);
    yy_assert(ret);
    yy_assert(ret_len == len);
    yy_assert(memcmp(ret, str, len) == 0);
    free(ret);
    yyjson_doc_free(doc);
    
    doc = yyjson_read(str, len, 0);
    ret = yyjson_write(doc, flg, NULL);
    if (ret) free(ret);
    yyjson_doc_free(doc);
    
    doc = yyjson_read(str, len, YYJSON_READ_ALLOW_INVALID_UNICODE);
    ret = yyjson_write(doc, 0, NULL);
    if (ret) free(ret);
    yyjson_doc_free(doc);
    
    doc = yyjson_read(str, len, YYJSON_READ_ALLOW_INVALID_UNICODE);
    ret = yyjson_write(doc, YYJSON_WRITE_PRETTY, NULL);
    if (ret) free(ret);
    yyjson_doc_free(doc);
#endif
}


/// `src` should be encoded as `dst` with flag.
static void validate_str_write(string_val *src,
                               string_val *dst,
                               yyjson_write_flag flg) {
#if !YYJSON_DISABLE_WRITER
    
#if YYJSON_DISABLE_UTF8_VALIDATION
    if (src->str && !yy_str_is_utf8(src->str, src->len)) return;
#endif
    
    string_val empty = { NULL, 0 };
    if (!src || !src->str) return;
    if (!dst) dst = &empty;
    
    char *buf = src->len ? malloc(src->len) : (char *)1;
    memcpy(buf, src->str, src->len);
    
    yyjson_mut_doc *doc = yyjson_mut_doc_new(NULL);
    yyjson_mut_val *val = yyjson_mut_strn(doc, buf, src->len);
    yyjson_mut_doc_set_root(doc, val);
    
    // single value
    usize ret_len = 0;
    char *ret = yyjson_mut_write_opts(doc, flg, NULL, &ret_len, NULL);
    if (dst->str) {
        yy_assertf(ret,
                   "write fail,\ninput: \"%s\" len:%u\nexpect:\"%s\" len:%u\noutput:NULL\n",
                   src->str, (u32)src->len, dst->str, (u32)dst->len);
        yy_assertf((ret_len == dst->len + 2) && (memcmp(ret + 1, dst->str, dst->len) == 0),
                   "write fail,\ninput: \"%s\" len:%u\nexpect:\"%s\" len:%u\noutput:%s len:%u\n",
                   src->str, (u32)src->len, dst->str, (u32)dst->len, ret, (u32)ret_len - 2);
        validate_roundtrip(ret, ret_len, flg);
        free((void *)ret);
    } else {
        yy_assertf(!ret,
                   "input string should be rejected by writer, but accepted: \"%s\"\n",
                   src->str);
    }
    
    // string in array (minify)
    yyjson_mut_val *arr = yyjson_mut_arr(doc);
    yyjson_mut_arr_append(arr, val);
    yyjson_mut_doc_set_root(doc, arr);
    ret = yyjson_mut_write_opts(doc, flg, NULL, &ret_len, NULL);
    if (dst->str) {
        yy_assertf(ret,
                   "write fail,\ninput: \"%s\" len:%u\nexpect:\"%s\" len:%u\noutput:NULL\n",
                   src->str, (u32)src->len, dst->str, (u32)dst->len);
        yy_assertf((ret_len == dst->len + 4) && (memcmp(ret + 2, dst->str, dst->len) == 0),
                   "write fail,\ninput: \"%s\" len:%u\nexpect:\"%s\" len:%u\noutput:%s len:%u\n",
                   src->str, (u32)src->len, dst->str, (u32)dst->len, ret, (u32)ret_len - 4);
        validate_roundtrip(ret, ret_len, flg);
        free((void *)ret);
    } else {
        yy_assertf(!ret,
                   "input string should be rejected by writer, but accepted: \"%s\"\n",
                   src->str);
    }
    
    // string in array (pretty)
    ret = yyjson_mut_write_opts(doc, flg | YYJSON_WRITE_PRETTY, NULL, &ret_len, NULL);
    if (dst->str) {
        yy_assertf(ret,
                   "write fail,\ninput: \"%s\" len:%u\nexpect:\"%s\" len:%u\noutput:NULL\n",
                   src->str, (u32)src->len, dst->str, (u32)dst->len);
        yy_assertf((ret_len == dst->len + 10) && (memcmp(ret + 7, dst->str, dst->len) == 0),
                   "write fail,\ninput: \"%s\" len:%u\nexpect:\"%s\" len:%u\noutput:%s len:%u\n",
                   src->str, (u32)src->len, dst->str, (u32)dst->len, ret, (u32)ret_len - 10);
        validate_roundtrip(ret, ret_len, flg | YYJSON_WRITE_PRETTY);
        free((void *)ret);
    } else {
        yy_assertf(!ret,
                   "input string should be rejected by writer, but accepted: \"%s\"\n",
                   src->str);
    }
    
    
    yyjson_mut_doc_free(doc);
    if (src->len) free(buf);
#endif
}

/// validate string decode:
///     esc_non -> str
///     esc_sla -> str
///     esc_uni -> str
///     esc_all -> str
static void validate_read(string_set set) {
    yyjson_read_flag flg = YYJSON_READ_ALLOW_INVALID_UNICODE;
    
    if (set.invalid_unicode) {
        validate_str_read(&set.esc_non, NULL, 0);
        validate_str_read(&set.esc_sla, NULL, 0);
#if YYJSON_DISABLE_NON_STANDARD
        validate_str_read(&set.esc_non, NULL, flg);
        validate_str_read(&set.esc_sla, NULL, flg);
#else
        validate_str_read(&set.esc_non, &set.str, flg);
        validate_str_read(&set.esc_sla, &set.str, flg);
#endif
        validate_str_read(&set.esc_uni, &set.str, flg);
        validate_str_read(&set.esc_all, &set.str, flg);
    } else {
        validate_str_read(&set.esc_non, &set.str, 0);
        validate_str_read(&set.esc_sla, &set.str, 0);
        validate_str_read(&set.esc_uni, &set.str, 0);
        validate_str_read(&set.esc_all, &set.str, 0);
        
        validate_str_read(&set.esc_non, &set.str, flg);
        validate_str_read(&set.esc_sla, &set.str, flg);
        validate_str_read(&set.esc_uni, &set.str, flg);
        validate_str_read(&set.esc_all, &set.str, flg);
    }
}

/// validate string encode:
///     str -> esc_non
///     str -> esc_sla
///     str -> esc_uni
///     str -> esc_all
static void validate_write(string_set set) {
    yyjson_write_flag flg_non = YYJSON_WRITE_NOFLAG;
    yyjson_write_flag flg_sla = YYJSON_WRITE_ESCAPE_SLASHES;
    yyjson_write_flag flg_uni = YYJSON_WRITE_ESCAPE_UNICODE;
    yyjson_write_flag flg_all = YYJSON_WRITE_ESCAPE_UNICODE | YYJSON_WRITE_ESCAPE_SLASHES;
    yyjson_write_flag flg_inv = YYJSON_WRITE_ALLOW_INVALID_UNICODE;
    
    if (set.invalid_unicode) {
        validate_str_write(&set.str, NULL, flg_non);
        validate_str_write(&set.str, NULL, flg_sla);
        validate_str_write(&set.str, NULL, flg_uni);
        validate_str_write(&set.str, NULL, flg_all);
#if YYJSON_DISABLE_NON_STANDARD
        validate_str_write(&set.str, NULL, flg_non | flg_inv);
        validate_str_write(&set.str, NULL, flg_sla | flg_inv);
        validate_str_write(&set.str, NULL, flg_uni | flg_inv);
        validate_str_write(&set.str, NULL, flg_all | flg_inv);
#else
        validate_str_write(&set.str, &set.esc_non, flg_non | flg_inv);
        validate_str_write(&set.str, &set.esc_sla, flg_sla | flg_inv);
        validate_str_write(&set.str, &set.esc_uni, flg_uni | flg_inv);
        validate_str_write(&set.str, &set.esc_all, flg_all | flg_inv);
#endif
    } else {
        validate_str_write(&set.str, &set.esc_non, flg_non);
        validate_str_write(&set.str, &set.esc_sla, flg_sla);
        validate_str_write(&set.str, &set.esc_uni, flg_uni);
        validate_str_write(&set.str, &set.esc_all, flg_all);
        
        validate_str_write(&set.str, &set.esc_non, flg_non | flg_inv);
        validate_str_write(&set.str, &set.esc_sla, flg_sla | flg_inv);
        validate_str_write(&set.str, &set.esc_uni, flg_uni | flg_inv);
        validate_str_write(&set.str, &set.esc_all, flg_all | flg_inv);
    }
}

static void validate_read_write(string_set set) {
    validate_read(set);
    validate_write(set);
}

yy_test_case(test_string) {
    
    validate_read_write((string_set) {
        { "", 0 },
        { "", 0 },
        { "", 0 },
        { "", 0 },
        { "", 0 },
    });
    
    validate_read_write((string_set) {
        { "a", 1 },
        { "a", 1 },
        { "a", 1 },
        { "a", 1 },
        { "a", 1 },
    });
    
    validate_read_write((string_set) {
        { "abc", 3 },
        { "abc", 3 },
        { "abc", 3 },
        { "abc", 3 },
        { "abc", 3 },
    });
    
    validate_read_write((string_set) {
        { "\0", 1 },
        { "\\u0000", 6, },
        { "\\u0000", 6, },
        { "\\u0000", 6, },
        { "\\u0000", 6, },
    });
    
    validate_read_write((string_set) {
        { "abc\0", 4 },
        { "abc\\u0000", 9 },
        { "abc\\u0000", 9 },
        { "abc\\u0000", 9 },
        { "abc\\u0000", 9 },
    });
    
    validate_read_write((string_set) {
        { "\0abc", 4 },
        { "\\u0000abc", 9 },
        { "\\u0000abc", 9 },
        { "\\u0000abc", 9 },
        { "\\u0000abc", 9 },
    });
    
    validate_read_write((string_set) {
        { "abc\0def", 7 },
        { "abc\\u0000def", 12 },
        { "abc\\u0000def", 12 },
        { "abc\\u0000def", 12 },
        { "abc\\u0000def", 12 },
    });
    
    validate_read_write((string_set) {
        { "a\\b", 3 },
        { "a\\\\b", 4 },
        { "a\\\\b", 4 },
        { "a\\\\b", 4 },
        { "a\\\\b", 4 },
    });
    
    validate_read_write((string_set) {
        { "a/b", 3 },
        { "a/b", 3 },
        { "a\\/b", 4 },
        { "a/b", 3 },
        { "a\\/b", 4 },
    });
    
    validate_read((string_set) {
        { "abc\x20\x7F", 5 },
        { "abc\x20\x7F", 5 },
        { "abc\x20\x7F", 5 },
        { "abc\x20\x7F", 5 },
        { "abc\x20\x7F", 5 },
    });
    
    validate_read_write((string_set) {
        { "\"\\/\b\f\n\r\t", 8 },
        { "\\\"\\\\/\\b\\f\\n\\r\\t", 15 },
        { "\\\"\\\\\\/\\b\\f\\n\\r\\t", 16 },
        { "\\\"\\\\/\\b\\f\\n\\r\\t", 15 },
        { "\\\"\\\\\\/\\b\\f\\n\\r\\t", 16 },
    });
    
    validate_read_write((string_set) {
        { "Alizée", 7 },
        { "Alizée", 7 },
        { "Alizée", 7 },
        { "Aliz\\u00E9e", 11 },
        { "Aliz\\u00E9e", 11 },
    });
    
    validate_read_write((string_set) {
        { "Hello世界", 11 },
        { "Hello世界", 11 },
        { "Hello世界", 11 },
        { "Hello\\u4E16\\u754C", 17 },
        { "Hello\\u4E16\\u754C", 17 },
    });
    
    validate_read((string_set) {
        { "Hello世界", 11 },
        { "Hello世界", 11 },
        { "Hello世界", 11 },
        { "Hello\\u4e16\\u754c", 17 },
        { "Hello\\u4e16\\u754c", 17 },
    });
    
    validate_read_write((string_set) {
        { "Emoji😊", 9 },
        { "Emoji😊", 9 },
        { "Emoji😊", 9 },
        { "Emoji\\uD83D\\uDE0A", 17 },
        { "Emoji\\uD83D\\uDE0A", 17 },
    });
    
    validate_read_write((string_set) {
        { "🐱\t🐶", 9 },
        { "🐱\\t🐶", 10 },
        { "🐱\\t🐶", 10 },
        { "\\uD83D\\uDC31\\t\\uD83D\\uDC36", 26 },
        { "\\uD83D\\uDC31\\t\\uD83D\\uDC36", 26 },
    });
    
    validate_read_write((string_set) {
        { "Check✅©\t2020®яблоко////แอปเปิ้ล\\\\リンゴ|تفاحة|蘋果|사과|", 97 },
        { "Check✅©\\t2020®яблоко////แอปเปิ้ล\\\\\\\\リンゴ|تفاحة|蘋果|사과|", 100 },
        { "Check✅©\\t2020®яблоко\\/\\/\\/\\/แอปเปิ้ล\\\\\\\\リンゴ|تفاحة|蘋果|사과|", 104 },
        { "Check\\u2705\\u00A9\\t2020\\u00AE\\u044F\\u0431\\u043B\\u043E\\u043A\\u043E////\\u0E41\\u0E2D\\u0E1B\\u0E40\\u0E1B\\u0E34\\u0E49\\u0E25\\\\\\\\\\u30EA\\u30F3\\u30B4|\\u062A\\u0641\\u0627\\u062D\\u0629|\\u860B\\u679C|\\uC0AC\\uACFC|\\uF8FF", 203 },
        { "Check\\u2705\\u00A9\\t2020\\u00AE\\u044F\\u0431\\u043B\\u043E\\u043A\\u043E\\/\\/\\/\\/\\u0E41\\u0E2D\\u0E1B\\u0E40\\u0E1B\\u0E34\\u0E49\\u0E25\\\\\\\\\\u30EA\\u30F3\\u30B4|\\u062A\\u0641\\u0627\\u062D\\u0629|\\u860B\\u679C|\\uC0AC\\uACFC|\\uF8FF", 207 },
    });
    
    
    // string with different length
    char rand_str[65] = { 0 };
    for (int i = 0; i < 64; i++) {
        rand_str[i] = 'a' + (i % 26);
    }
    for (int len = 1; len <= 64; len++) {
        validate_read_write((string_set) {
            { rand_str, len },
            { rand_str, len },
            { rand_str, len },
            { rand_str, len },
            { rand_str, len },
        });
    }
    for (int len = 0; len <= 64; len++) {
        // escape first char
        char buf1[1 + 64];
        char buf2[2 + 64];
        buf1[0] = '\t';
        memcpy(buf1 + 1, rand_str, len);
        buf2[0] = '\\';
        buf2[1] = 't';
        memcpy(buf2 + 2, rand_str, len);
        validate_read_write((string_set) {
            { buf1, len + 1 },
            { buf2, len + 2 },
            { buf2, len + 2 },
            { buf2, len + 2 },
            { buf2, len + 2 },
        });
    }
    
    
    // 1 byte invalid UTF-8
    for (int len = 0; len <= 6; len++) {
        validate_write((string_set) {
            { "\x80qwerty", 1 + len },
            { "\x80qwerty", 1 + len },
            { "\x80qwerty", 1 + len },
            { "\\uFFFDqwerty", 6 + len },
            { "\\uFFFDqwerty", 6 + len },
            true
        });
        validate_read((string_set) {
            { "\x80qwerty", 1 + len },
            { "\x80qwerty", 1 + len },
            { "\x80qwerty", 1 + len },
            { NULL, 0 },
            { NULL, 0 },
            true
        });
        validate_write((string_set) {
            { "\x80\x8Fqwerty", 2 + len },
            { "\x80\x8Fqwerty", 2 + len },
            { "\x80\x8Fqwerty", 2 + len },
            { "\\uFFFD\\uFFFDqwerty", 12 + len },
            { "\\uFFFD\\uFFFDqwerty", 12 + len },
            true
        });
        validate_read((string_set) {
            { "\x80\x8Fqwerty", 2 + len },
            { "\x80\x8Fqwerty", 2 + len },
            { "\x80\x8Fqwerty", 2 + len },
            { NULL, 0 },
            { NULL, 0 },
            true
        });
    }
    
    
    // 2 byte invalid UTF-8
    for (int len = 0; len <= 6; len++) {
        validate_write((string_set) {
            { "\xC0qwerty", 1 + len },
            { "\xC0qwerty", 1 + len },
            { "\xC0qwerty", 1 + len },
            { "\\uFFFDqwerty", 6 + len },
            { "\\uFFFDqwerty", 6 + len },
            true
        });
        validate_read((string_set) {
            { "\xC0qwerty", 1 + len },
            { "\xC0qwerty", 1 + len },
            { "\xC0qwerty", 1 + len },
            { NULL, 0 },
            { NULL, 0 },
            true
        });
        validate_write((string_set) {
            { "\xC0\xC0qwerty", 2 + len },
            { "\xC0\xC0qwerty", 2 + len },
            { "\xC0\xC0qwerty", 2 + len },
            { "\\uFFFD\\uFFFDqwerty", 12 + len },
            { "\\uFFFD\\uFFFDqwerty", 12 + len },
            true
        });
        validate_read((string_set) {
            { "\xC0\xC0qwerty", 2 + len },
            { "\xC0\xC0qwerty", 2 + len },
            { "\xC0\xC0qwerty", 2 + len },
            { NULL, 0 },
            { NULL, 0 },
            true
        });
        validate_write((string_set) {
            { "\xC0\tqwerty", 2 + len },
            { "\xC0\\tqwerty", 3 + len },
            { "\xC0\\tqwerty", 3 + len },
            { "\\uFFFD\\tqwerty", 8 + len },
            { "\\uFFFD\\tqwerty", 8 + len },
            true
        });
        validate_read((string_set) {
            { "\xC0\tqwerty", 2 + len },
            { "\xC0\tqwerty", 2 + len },
            { "\xC0\\tqwerty", 3 + len },
            { NULL, 0 },
            { NULL, 0 },
            true
        });
    }
    
    
    // 3 byte invalid UTF-8
    for (int len = 0; len <= 6; len++) {
        validate_write((string_set) {
            { "\xE0qwerty", 1 + len },
            { "\xE0qwerty", 1 + len },
            { "\xE0qwerty", 1 + len },
            { "\\uFFFDqwerty", 6 + len },
            { "\\uFFFDqwerty", 6 + len },
            true
        });
        validate_read((string_set) {
            { "\xE0qwerty", 1 + len },
            { "\xE0qwerty", 1 + len },
            { "\xE0qwerty", 1 + len },
            { NULL, 0 },
            { NULL, 0 },
            true
        });
        validate_write((string_set) {
            { "\xE0\x81\x81qwerty", 3 + len },
            { "\xE0\x81\x81qwerty", 3 + len },
            { "\xE0\x81\x81qwerty", 3 + len },
            { "\\uFFFD\\uFFFD\\uFFFDqwerty", 18 + len },
            { "\\uFFFD\\uFFFD\\uFFFDqwerty", 18 + len },
            true
        });
        validate_read((string_set) {
            { "\xE0\x81\x81qwerty", 3 + len },
            { "\xE0\x81\x81qwerty", 3 + len },
            { "\xE0\x81\x81qwerty", 3 + len },
            { NULL, 0 },
            { NULL, 0 },
            true
        });
    }
    
    
    // 4 byte invalid UTF-8
    for (int len = 0; len <= 6; len++) {
        validate_write((string_set) {
            { "\xF0qwerty", 1 + len },
            { "\xF0qwerty", 1 + len },
            { "\xF0qwerty", 1 + len },
            { "\\uFFFDqwerty", 6 + len },
            { "\\uFFFDqwerty", 6 + len },
            true
        });
        validate_read((string_set) {
            { "\xF0qwerty", 1 + len },
            { "\xF0qwerty", 1 + len },
            { "\xF0qwerty", 1 + len },
            { NULL, 0 },
            { NULL, 0 },
            true
        });
        validate_write((string_set) {
            { "\xF0\x81\x81\x81qwerty", 4 + len },
            { "\xF0\x81\x81\x81qwerty", 4 + len },
            { "\xF0\x81\x81\x81qwerty", 4 + len },
            { "\\uFFFD\\uFFFD\\uFFFD\\uFFFDqwerty", 24 + len },
            { "\\uFFFD\\uFFFD\\uFFFD\\uFFFDqwerty", 24 + len },
            true
        });
        validate_read((string_set) {
            { "\xF0\x81\x81\x81qwerty", 4 + len },
            { "\xF0\x81\x81\x81qwerty", 4 + len },
            { "\xF0\x81\x81\x81qwerty", 4 + len },
            { NULL, 0 },
            { NULL, 0 },
            true
        });
    }
    
    
    // special case
    validate_read((string_set) {
        { "qwerty\0", 7 },
        { "qwerty\0", 7 },
        { "qwerty\0", 7 },
        { NULL, 0 },
        { NULL, 0 },
        true
    });
    validate_read((string_set) {
        { "qwerty\0abc", 10 },
        { "qwerty\0abc", 10 },
        { "qwerty\0abc", 10 },
        { NULL, 0 },
        { NULL, 0 },
        true
    });
    validate_read((string_set) {
        { "\tqwerty\0", 8 },
        { "\\tqwerty\0", 9 },
        { "\\tqwerty\0", 9 },
        { NULL, 0 },
        { NULL, 0 },
        true
    });
    validate_read((string_set) {
        { "\tqwerty\0abc", 11 },
        { "\\tqwerty\0abc", 12 },
        { "\\tqwerty\0abc", 12 },
        { NULL, 0 },
        { NULL, 0 },
        true
    });
    validate_read((string_set) {
        { "\tqwerty\x80", 8 },
        { "\\tqwerty\x80", 9 },
        { "\\tqwerty\x80", 9 },
        { NULL, 0 },
        { NULL, 0 },
        true
    });
    validate_read((string_set) {
        { "\tqwerty\x80xyz", 11 },
        { "\\tqwerty\x80xyz", 12 },
        { "\\tqwerty\x80xyz", 12 },
        { NULL, 0 },
        { NULL, 0 },
        true
    });
    
    
    // invalid escape
    validate_read((string_set) {
        { NULL, 0 },
        { "\\T", 2 },
    });
    validate_read((string_set) {
        { NULL, 0 },
        { "\\U00E9", 2 },
    });
    validate_read((string_set) {
        { NULL, 0 },
        { "\\a", 2 },
    });
    validate_read((string_set) {
        { NULL, 0 },
        { "\\e", 2 },
    });
    validate_read((string_set) {
        { NULL, 0 },
        { "\\v", 2 },
    });
    validate_read((string_set) {
        { NULL, 0 },
        { "\\'", 2 },
    });
    validate_read((string_set) {
        { NULL, 0 },
        { "\\?", 2 },
    });
    validate_read((string_set) {
        { NULL, 0 },
        { "\\000", 4 },
    });
    validate_read((string_set) {
        { NULL, 0 },
        { "\\101", 4 },
    });
    validate_read((string_set) {
        { NULL, 0 },
        { "\\x00", 4 },
    });
    validate_read((string_set) {
        { NULL, 0 },
        { "\\x41", 4 },
    });
    validate_read((string_set) {
        { NULL, 0 },
        { "\\U1234", 6 },
    });
    validate_read((string_set) {
        { NULL, 0 },
        { "\\u123Z", 6 },
    });
    validate_read((string_set) {
        { NULL, 0 },
        { "\\x1234", 6 },
    });
    
    
    // invalid high surrogate
    validate_read((string_set) {
        { NULL, 0 },
        { "\\uDE0A", 6 },
    });
    validate_read((string_set) {
        { NULL, 0 },
        { "\\uDE0A\\u0000", 12 },
    });
    
    
    // no matched low surrogate
    validate_read((string_set) {
        { NULL, 0 },
        { "\\uD83D", 6 },
    });
    validate_read((string_set) {
        { NULL, 0 },
        { "\\uD83D\\", 7 },
    });
    validate_read((string_set) {
        { NULL, 0 },
        { "\\uD83D\\u", 8 },
    });
    validate_read((string_set) {
        { NULL, 0 },
        { "\\uD83DAAAA", 10 },
    });
    
    
    // invalid low surrogate
    validate_read((string_set) {
        { NULL, 0 },
        { "\\uD83D\\u0000", 12 },
    });
    validate_read((string_set) {
        { NULL, 0 },
        { "\\uD83D\\uD83D", 12 },
    });
    
    
    // truncated escape sequence
    for (int len = 1; len < 12; len++) {
        if (len == 6) continue;
        const char *str = "\\u0024\\u0024";
        validate_read((string_set) {
            { NULL, 0 },
            { str, len },
            { str, len },
            { str, len },
            { str, len },
        });
    }
    
}
