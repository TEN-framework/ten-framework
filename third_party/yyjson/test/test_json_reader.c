// This file is used to test the functionality of JSON reader.

#include "yyjson.h"
#include "yy_test_utils.h"

#if !YYJSON_DISABLE_READER

typedef enum {
    EXPECT_NONE,
    EXPECT_PASS,
    EXPECT_FAIL,
} expect_type;

typedef enum {
    FLAG_NONE       = 0 << 0,
    FLAG_COMMA      = 1 << 0,
    FLAG_COMMENT    = 1 << 1,
    FLAG_INF_NAN    = 1 << 2,
    FLAG_EXTRA      = 1 << 3,
    FLAG_NUM_RAW    = 1 << 4,
    FLAG_MAX        = 1 << 5,
} flag_type;

static void test_read_file(const char *path, flag_type type, expect_type expect) {
    
#if YYJSON_DISABLE_UTF8_VALIDATION
    {
        u8 *dat;
        usize dat_len;
        if (yy_file_read(path, &dat, &dat_len)) {
            bool is_utf8 = yy_str_is_utf8((const char *)dat, dat_len);
            free(dat);
            if (!is_utf8) return;
        }
    }
#endif
    
    yyjson_read_flag flag = YYJSON_READ_NOFLAG;
    if (type & FLAG_COMMA) flag |= YYJSON_READ_ALLOW_TRAILING_COMMAS;
    if (type & FLAG_COMMENT) flag |= YYJSON_READ_ALLOW_COMMENTS;
    if (type & FLAG_INF_NAN) flag |= YYJSON_READ_ALLOW_INF_AND_NAN;
    if (type & FLAG_EXTRA) flag |= YYJSON_READ_STOP_WHEN_DONE;
    if (type & FLAG_NUM_RAW) flag |= YYJSON_READ_NUMBER_AS_RAW;
    
    // test read from file
    yyjson_read_err err;
    yyjson_doc *doc = yyjson_read_file(path, flag, NULL, &err);
    if (expect == EXPECT_PASS) {
        yy_assertf(doc != NULL, "file should pass with flag 0x%u, but fail:\n%s\n", flag, path);
        yy_assert(yyjson_doc_get_read_size(doc) > 0);
        yy_assert(yyjson_doc_get_val_count(doc) > 0);
        yy_assert(err.code == YYJSON_READ_SUCCESS);
        yy_assert(err.msg == NULL);
    }
    if (expect == EXPECT_FAIL) {
        yy_assertf(doc == NULL, "file should fail with flag 0x%u, but pass:\n%s\n", flag, path);
        yy_assert(yyjson_doc_get_read_size(doc) == 0);
        yy_assert(yyjson_doc_get_val_count(doc) == 0);
        yy_assert(err.code != YYJSON_READ_SUCCESS);
        yy_assert(err.msg != NULL);
    }
    if (doc) { // test write again
#if !YYJSON_DISABLE_WRITER
        usize len;
        char *ret;
        ret = yyjson_write(doc, YYJSON_WRITE_ALLOW_INF_AND_NAN, &len);
        yy_assert(ret && len);
        free(ret);
        ret = yyjson_write(doc, YYJSON_WRITE_PRETTY | YYJSON_WRITE_ALLOW_INF_AND_NAN, &len);
        yy_assert(ret && len);
        free(ret);
#endif
    }
    yyjson_doc_free(doc);
    
    
    // test alloc fail
    yyjson_alc alc_small;
    char alc_buf[64];
    yy_assert(yyjson_alc_pool_init(&alc_small, alc_buf, sizeof(void *) * 8));
    yy_assert(!yyjson_read_file(path, flag, &alc_small, NULL));
    
    
    // test read insitu
    flag |= YYJSON_READ_INSITU;
    
    u8 *dat;
    usize len;
    bool read_suc = yy_file_read_with_padding(path, &dat, &len, YYJSON_PADDING_SIZE);
    yy_assert(read_suc);
    
    usize max_mem_len = yyjson_read_max_memory_usage(len, flag);
    void *buf = malloc(max_mem_len);
    yyjson_alc alc;
    yyjson_alc_pool_init(&alc, buf, max_mem_len);
    
    doc = yyjson_read_opts((char *)dat, len, flag, &alc, &err);
    if (expect == EXPECT_PASS) {
        yy_assertf(doc != NULL, "file should pass but fail:\n%s\n", path);
        yy_assert(yyjson_doc_get_read_size(doc) > 0);
        yy_assert(yyjson_doc_get_val_count(doc) > 0);
        yy_assert(err.code == YYJSON_READ_SUCCESS);
        yy_assert(err.msg == NULL);
    }
    if (expect == EXPECT_FAIL) {
        yy_assertf(doc == NULL, "file should fail but pass:\n%s\n", path);
        yy_assert(yyjson_doc_get_read_size(doc) == 0);
        yy_assert(yyjson_doc_get_val_count(doc) == 0);
        yy_assert(err.code != YYJSON_READ_SUCCESS);
        yy_assert(err.msg != NULL);
    }
    yyjson_doc_free(doc);
    free(buf);
    free(dat);
}

// yyjson test data
static void test_json_yyjson(void) {
    char dir[YY_MAX_PATH];
    yy_path_combine(dir, YYJSON_TEST_DATA_PATH, "data", "json", "test_yyjson", NULL);
    int count;
    char **names = yy_dir_read(dir, &count);
    yy_assertf(names != NULL && count != 0, "read dir fail:%s\n", dir);

    for (int i = 0; i < count; i++) {
        char *name = names[i];
        char path[YY_MAX_PATH];
        yy_path_combine(path, dir, name, NULL);
     
        for (flag_type type = 0; type < FLAG_MAX; type++) {
            if (yy_str_has_prefix(name, "pass_")) {
                bool should_fail = false;
                if (yy_str_contains(name, "(comma)")) {
#if !YYJSON_DISABLE_NON_STANDARD
                    should_fail |= (type & FLAG_COMMA) == 0;
#else
                    should_fail = true;
#endif
                }
                if (yy_str_contains(name, "(comment)")) {
#if !YYJSON_DISABLE_NON_STANDARD
                    should_fail |= (type & FLAG_COMMENT) == 0;
#else
                    should_fail = true;
#endif
                }
                if (yy_str_contains(name, "(inf)") || yy_str_contains(name, "(nan)")) {
#if !YYJSON_DISABLE_NON_STANDARD
                    should_fail |= (type & FLAG_INF_NAN) == 0;
#else
                    should_fail = true;
#endif
                }
                if (yy_str_contains(name, "(big)")) {
#if !YYJSON_DISABLE_NON_STANDARD
                    should_fail |= (type & (FLAG_INF_NAN | FLAG_NUM_RAW)) == 0;
#else
                    should_fail |= (type & (FLAG_NUM_RAW)) == 0;
#endif
                }
                if (yy_str_contains(name, "(extra)")) {
                    should_fail |= (type & FLAG_EXTRA) == 0;
                }
                test_read_file(path, type, should_fail ? EXPECT_FAIL : EXPECT_PASS);
            } else if (yy_str_has_prefix(name, "fail_")) {
                test_read_file(path, type, EXPECT_FAIL);
            } else {
                test_read_file(path, type, EXPECT_NONE);
            }
        }
    }
    
    // test fail
    yy_assert(!yyjson_read_opts(NULL, 0, 0, NULL, NULL));
    yy_assert(!yyjson_read_opts("1", 0, 0, NULL, NULL));
    yy_assert(!yyjson_read_opts("1", SIZE_MAX, 0, NULL, NULL));
    
    yyjson_alc alc_small;
    char alc_buf[64];
    yy_assert(yyjson_alc_pool_init(&alc_small, alc_buf, sizeof(void *) * 8));
    yy_assert(!yyjson_read_opts("", 64, 0, &alc_small, NULL));
    
    yy_assert(!yyjson_read_file(NULL, 0, NULL, NULL));
    yy_assert(!yyjson_read_file("...not a valid file...", 0, NULL, NULL));
    
    yy_dir_free(names);
}



// http://www.json.org/JSON_checker/
static void test_json_checker(void) {
    char dir[YY_MAX_PATH];
    yy_path_combine(dir, YYJSON_TEST_DATA_PATH, "data", "json", "test_checker", NULL);
    int count;
    char **names = yy_dir_read(dir, &count);
    yy_assertf(names != NULL && count != 0, "read dir fail:%s\n", dir);
    
    for (int i = 0; i < count; i++) {
        char *name = names[i];
        char path[YY_MAX_PATH];
        yy_path_combine(path, dir, name, NULL);
        if (yy_str_has_prefix(name, "pass_")) {
            test_read_file(path, FLAG_NONE, EXPECT_PASS);
        } else if (yy_str_has_prefix(name, "fail_") &&
                   !yy_str_contains(name, "EXCLUDE")) {
            test_read_file(path, FLAG_NONE, EXPECT_FAIL);
        } else {
            test_read_file(path, FLAG_NONE, EXPECT_NONE);
        }
    }
    
    yy_dir_free(names);
}

// https://github.com/nst/JSONTestSuite
static void test_json_parsing(void) {
    char dir[YY_MAX_PATH];
    yy_path_combine(dir, YYJSON_TEST_DATA_PATH, "data", "json", "test_parsing", NULL);
    int count;
    char **names = yy_dir_read(dir, &count);
    yy_assertf(names != NULL && count != 0, "read dir fail:%s\n", dir);
    
    for (int i = 0; i < count; i++) {
        char *name = names[i];
        char path[YY_MAX_PATH];
        yy_path_combine(path, dir, name, NULL);
        if (yy_str_has_prefix(name, "y_")) {
            test_read_file(path, FLAG_NONE, EXPECT_PASS);
        } else if (yy_str_has_prefix(name, "n_")) {
            test_read_file(path, FLAG_NONE, EXPECT_FAIL);
        } else {
            test_read_file(path, FLAG_NONE, EXPECT_NONE);
        }
    }
    yy_dir_free(names);
}

// https://github.com/nst/JSONTestSuite
static void test_json_transform(void) {
    char dir[YY_MAX_PATH];
    yy_path_combine(dir, YYJSON_TEST_DATA_PATH, "data", "json", "test_transform", NULL);
    int count;
    char **names = yy_dir_read(dir, &count);
    yy_assertf(names != NULL && count != 0, "read dir fail:%s\n", dir);
    
    const char *pass_names[] = {
        "number_1.0.json",
        "number_1.000000000000000005.json",
        "number_1e-999.json",
        "number_1e6.json",
        "number_1000000000000000.json",
        "number_10000000000000000999.json",
        "number_-9223372036854775808.json",
        "number_-9223372036854775809.json",
        "number_9223372036854775807.json",
        "number_9223372036854775808.json",
        "object_key_nfc_nfd.json",
        "object_key_nfd_nfc.json",
        "object_same_key_different_values.json",
        "object_same_key_same_value.json",
        "object_same_key_unclear_values.json",
        "string_with_escaped_NULL.json"
    };
    
    const char *fail_names[] = {
        "string_1_escaped_invalid_codepoint.json",
        "string_1_invalid_codepoint.json",
        "string_2_escaped_invalid_codepoints.json",
        "string_2_invalid_codepoints.json",
        "string_3_escaped_invalid_codepoints.json",
        "string_3_invalid_codepoints.json"
    };
    
    for (int i = 0; i < count; i++) {
        char *name = names[i];
        char path[YY_MAX_PATH];
        
        expect_type expect = EXPECT_NONE;
        for (usize p = 0; p < sizeof(pass_names) / sizeof(pass_names[0]); p++) {
            if (strcmp(name, pass_names[p]) == 0) expect = EXPECT_PASS;
        }
        for (usize p = 0; p < sizeof(fail_names) / sizeof(fail_names[0]); p++) {
            if (strcmp(name, fail_names[p]) == 0) expect = EXPECT_FAIL;
        }
        
        yy_path_combine(path, dir, name, NULL);
        test_read_file(path, FLAG_NONE, expect);
    }
    
    yy_dir_free(names);
}

// https://github.com/miloyip/nativejson-benchmark
static void test_json_encoding(void) {
    char dir[YY_MAX_PATH];
    yy_path_combine(dir, YYJSON_TEST_DATA_PATH, "data", "json", "test_encoding", NULL);
    int count;
    char **names = yy_dir_read(dir, &count);
    yy_assertf(names != NULL && count != 0, "read dir fail:%s\n", dir);
    
    for (int i = 0; i < count; i++) {
        char *name = names[i];
        char path[YY_MAX_PATH];
        yy_path_combine(path, dir, name, NULL);
        if (strcmp(name, "utf8.json") == 0) {
            test_read_file(path, FLAG_NONE, EXPECT_PASS);
        } else {
            test_read_file(path, FLAG_NONE, EXPECT_FAIL);
        }
    }
    yy_dir_free(names);
}

yy_test_case(test_json_reader) {
    test_json_yyjson();
    test_json_checker();
    test_json_parsing();
    test_json_transform();
    test_json_encoding();
}

#else
yy_test_case(test_json_reader) {}
#endif
