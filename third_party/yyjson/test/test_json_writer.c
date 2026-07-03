// This file is used to test the functionality of JSON writer.

#include "yyjson.h"
#include "yy_test_utils.h"


#if !YYJSON_DISABLE_WRITER

static bool mut_val_has_inf_nan(yyjson_mut_val *val) {
    usize idx, max;
    yyjson_mut_val *k, *v;
    
    if (yyjson_mut_is_real(val)) {
        f64 num = yyjson_mut_get_real(val);
        if (isnan(num) || isinf(num)) return true;
        return false;
    }
    if (yyjson_mut_is_arr(val)) {
        yyjson_mut_arr_foreach(val, idx, max, v) {
            if (mut_val_has_inf_nan(v)) return true;
        }
    }else if (yyjson_mut_is_obj(val)) {
        yyjson_mut_obj_foreach(val, idx, max, k, v) {
            if (mut_val_has_inf_nan(v)) return true;
        }
    }
    return false;
}

static usize mut_val_get_num(yyjson_mut_val *val) {
    usize idx, max, num;
    yyjson_mut_val *k, *v;
    
    if (!val) return 0;
    if (!yyjson_mut_is_ctn(val)) return 1;
    
    num = 1;
    if (yyjson_mut_is_arr(val)) {
        yyjson_mut_arr_foreach(val, idx, max, v) {
            num += mut_val_get_num(v);
        }
    }else if (yyjson_mut_is_obj(val)) {
        yyjson_mut_obj_foreach(val, idx, max, k, v) {
            num += 1;
            num += mut_val_get_num(v);
        }
    }
    return num;
}


static void validate_json_write_with_flag(yyjson_write_flag flg,
                                          yyjson_mut_doc *doc,
                                          yyjson_alc *alc,
                                          const char *expect) {
#if !YYJSON_DISABLE_READER
    // write mutable doc to string
    usize len;
    char *ret = yyjson_mut_write_opts(doc, flg, alc, &len, NULL);
    if (!expect) {
        yy_assertf(!ret && len == 0, "write with flag 0x%x\nexpect fail, but return:\n%s\n", flg, ret);
        return;
    }
    yy_assertf(ret && len > 0, "write with flag 0x%x\nexpect:\n%s\noutput:\n%s\n", flg, expect, ret);
    yy_assertf(strlen(ret) == len, "write with flag 0x%x\nexpect:\n%s\noutput:\n%s\n", flg, expect, ret);
    yy_assertf(strlen(expect) == len, "write with flag 0x%x\nexpect:\n%s\noutput:\n%s\n", flg, expect, ret);
    yy_assertf(memcmp(ret, expect, len) == 0, "write with flag 0x%x\nexpect:\n%s\noutput:\n%s\n", flg, expect, ret);
    
    
    // temp file path
    const char *tmp_file_path = "__yyjson_test_tmp__.json";
    FILE *tmp_fp;
    u8 *dat, num = '0';
    usize dat_len;
    
    
    // write mutable doc to file
    yy_file_delete(tmp_file_path);
    yy_assert(yyjson_mut_write_file(tmp_file_path, doc, flg, alc, NULL));
    yy_assert(yy_file_read(tmp_file_path, &dat, &dat_len));
    yy_assert(dat_len == len);
    yy_assert(memcmp(dat, ret, len) == 0);
    free(dat);
    yy_file_delete(tmp_file_path);
    
    
    // write mutable doc to file pointer
    tmp_fp = yy_file_open(tmp_file_path, "wb");
    yy_assert(yyjson_mut_write_fp(tmp_fp, doc, flg, alc, NULL));
    fclose(tmp_fp);
    yy_assert(yy_file_read(tmp_file_path, &dat, &dat_len));
    yy_assert(dat_len == len);
    yy_assert(memcmp(dat, ret, len) == 0);
    free(dat);
    yy_file_delete(tmp_file_path);
    
    
    // write to read-only fp
    yy_file_write(tmp_file_path, (void *)&num, 1);
    tmp_fp = yy_file_open(tmp_file_path, "rb");
    yy_assert(!yyjson_mut_write_fp(tmp_fp, doc, flg, alc, NULL));
    fclose(tmp_fp);
    yy_file_delete(tmp_file_path);
    
    yy_assert(!yyjson_mut_write_fp(NULL, doc, flg, alc, NULL));
    
    
    // read
    yyjson_read_flag rflg = YYJSON_READ_NOFLAG;
    if (flg & YYJSON_WRITE_ALLOW_INF_AND_NAN) rflg |= YYJSON_READ_ALLOW_INF_AND_NAN;
    yyjson_doc *idoc = yyjson_read_opts(ret, len, rflg, NULL, NULL);
    yy_assert(idoc);
    if (mut_val_get_num(doc->root) != idoc->val_read) {
        idoc = yyjson_read_opts(ret, len, rflg, NULL, NULL);
    }
    yy_assert(mut_val_get_num(doc->root) == idoc->val_read);
    
    
    // write immutable doc to string
    usize len2;
    char *ret2 = yyjson_write_opts(idoc, flg, NULL, &len2, NULL);
    yy_assert(len == len2 && ret2);
    yy_assert(memcmp(ret, ret2, len) == 0);
    free(ret2);
    
    ret2 = yyjson_val_write_opts(idoc->root, flg, NULL, &len2, NULL);
    yy_assert(len == len2 && ret2);
    yy_assert(memcmp(ret, ret2, len) == 0);
    free(ret2);
    
    
    // write immutable doc to file
    yy_assert(yyjson_write_file(tmp_file_path, idoc, flg, alc, NULL));
    u8 *dat2;
    usize dat2_len;
    yy_assert(yy_file_read(tmp_file_path, &dat2, &dat2_len));
    yy_assert(dat2_len == len);
    yy_assert(memcmp(dat2, ret, len) == 0);
    free(dat2);
    yy_file_delete(tmp_file_path);
    
    tmp_fp = yy_file_open(tmp_file_path, "wb");
    yy_assert(yyjson_write_fp(tmp_fp, idoc, flg, alc, NULL));
    fclose(tmp_fp);
    yy_assert(yy_file_read(tmp_file_path, &dat2, &dat2_len));
    yy_assert(dat2_len == len);
    yy_assert(memcmp(dat2, ret, len) == 0);
    free(dat2);
    yy_file_delete(tmp_file_path);
    
    yy_file_write(tmp_file_path, (void *)&num, 1);
    tmp_fp = yy_file_open(tmp_file_path, "rb");
    yy_assert(!yyjson_write_fp(tmp_fp, idoc, flg, alc, NULL));
    fclose(tmp_fp);
    yy_file_delete(tmp_file_path);
    
    yy_assert(!yyjson_write_fp(NULL, idoc, flg, alc, NULL));
    
    
    // write immutable val to file
    yy_assert(yyjson_val_write_file(tmp_file_path, idoc->root, flg, alc, NULL));
    yy_assert(yy_file_read(tmp_file_path, &dat2, &dat2_len));
    yy_assert(dat2_len == len);
    yy_assert(memcmp(dat2, ret, len) == 0);
    free(dat2);
    yy_file_delete(tmp_file_path);
    
    tmp_fp = yy_file_open(tmp_file_path, "wb");
    yy_assert(yyjson_val_write_fp(tmp_fp, idoc->root, flg, alc, NULL));
    fclose(tmp_fp);
    yy_assert(yy_file_read(tmp_file_path, &dat2, &dat2_len));
    yy_assert(dat2_len == len);
    yy_assert(memcmp(dat2, ret, len) == 0);
    free(dat2);
    yy_file_delete(tmp_file_path);
    
    yy_file_write(tmp_file_path, (void *)&num, 1);
    tmp_fp = yy_file_open(tmp_file_path, "rb");
    yy_assert(!yyjson_val_write_fp(tmp_fp, idoc->root, flg, alc, NULL));
    fclose(tmp_fp);
    yy_file_delete(tmp_file_path);
    
    yy_assert(!yyjson_val_write_fp(NULL, idoc->root, flg, alc, NULL));
    
    
    // copy mutable doc and write again
    yyjson_mut_doc *mdoc = yyjson_doc_mut_copy(idoc, NULL);
    yy_assert(mdoc);
    usize len3;
    char *ret3 = yyjson_mut_write_opts(doc, flg, NULL, &len3, NULL);
    yy_assert(len == len3 && ret3);
    yy_assert(memcmp(ret, ret3, len) == 0);
    free(ret3);
    
    ret3 = yyjson_mut_val_write_opts(doc->root, flg, NULL, &len3, NULL);
    yy_assert(len == len3 && ret3);
    yy_assert(memcmp(ret, ret3, len) == 0);
    free(ret3);
    
    
    yyjson_doc_free(idoc);
    yyjson_mut_doc_free(mdoc);
    
    
    if (alc) alc->free(alc->ctx, (void *)ret);
    else free((void *)ret);
#endif
}

// @param min Expected minify string
// @param pre Expected pretty
// @param min_null Expected minify string with flat NAN_INF_AS_NULL
// @param pre_null Expected pretty string with flat NAN_INF_AS_NULL
static void validate_json_write_ex(yyjson_mut_doc *doc,
                                   yyjson_alc *alc,
                                   const char *min,
                                   const char *pre,
                                   const char *min_null,
                                   const char *pre_null) {
    yyjson_write_flag flg;
    bool has_nan_inf = mut_val_has_inf_nan(yyjson_mut_doc_get_root(doc));
    
    // nan inf should fail without 'INF_AND_NAN' flag
    if (has_nan_inf) {
        flg = YYJSON_WRITE_NOFLAG;
        validate_json_write_with_flag(flg, doc, alc, NULL);
        flg = YYJSON_WRITE_PRETTY;
        validate_json_write_with_flag(flg, doc, alc, NULL);
    }
    
    // minify
    flg = YYJSON_WRITE_NOFLAG;
    if (has_nan_inf) flg |= YYJSON_WRITE_ALLOW_INF_AND_NAN;
    validate_json_write_with_flag(flg, doc, alc, min);
    
    flg = YYJSON_WRITE_NOFLAG;
    if (has_nan_inf) flg |= YYJSON_WRITE_INF_AND_NAN_AS_NULL;
    validate_json_write_with_flag(flg, doc, alc, min_null);
    
    flg = YYJSON_WRITE_NOFLAG;
    if (has_nan_inf) flg |= YYJSON_WRITE_ALLOW_INF_AND_NAN |
                            YYJSON_WRITE_INF_AND_NAN_AS_NULL;
    validate_json_write_with_flag(flg, doc, alc, min_null);
    
    // pretty
    flg = YYJSON_WRITE_PRETTY;
    if (has_nan_inf) flg |= YYJSON_WRITE_ALLOW_INF_AND_NAN;
    validate_json_write_with_flag(flg, doc, alc, pre);
    
    flg = YYJSON_WRITE_PRETTY;
    if (has_nan_inf) flg |= YYJSON_WRITE_INF_AND_NAN_AS_NULL;
    validate_json_write_with_flag(flg, doc, alc, pre_null);
    
    flg = YYJSON_WRITE_PRETTY;
    if (has_nan_inf) flg |= YYJSON_WRITE_ALLOW_INF_AND_NAN |
                            YYJSON_WRITE_INF_AND_NAN_AS_NULL;
    validate_json_write_with_flag(flg, doc, alc, pre_null);
    
    // use small allocator to test allocation failure
    if (min && pre && alc && strlen(min) > 8) {
        char buf[64];
        yyjson_alc small_alc;
        yyjson_alc_pool_init(&small_alc, buf, 8 * sizeof(void *));
        for (int i = 1; i < 64; i++) small_alc.malloc(small_alc.ctx, i);
        validate_json_write_ex(doc, &small_alc, NULL, NULL, NULL, NULL);
    }
}

static void validate_json_write(yyjson_mut_doc *doc,
                                yyjson_alc *alc,
                                const char *min,
                                const char *pre) {
    validate_json_write_ex(doc, alc, min, pre, min, pre);
}

static void test_json_write(yyjson_alc *alc) {
    
    yyjson_mut_doc *doc;
    yyjson_mut_val *root, *val, *val2;
    char *str1, *str2, *cur1, *cur2;
    usize len;
    yyjson_write_err err;
    
    doc = yyjson_mut_doc_new(NULL);
    
    
    // invalid params
    yy_assert(!yyjson_mut_write(NULL, 0, NULL));
    yy_assert(!yyjson_mut_write(doc, 0, NULL));
    
    len = 1;
    yy_assert(!yyjson_mut_write(NULL, 0, &len));
    yy_assert(len == 0);
    len = 1;
    yy_assert(!yyjson_mut_write(doc, 0, &len));
    yy_assert(len == 0);
    
    yy_assert(!yyjson_mut_write_opts(NULL, 0, NULL, NULL, NULL));
    yy_assert(!yyjson_mut_write_opts(doc, 0, NULL, NULL, NULL));
    
    len = 1;
    yy_assert(!yyjson_mut_write_opts(NULL, 0, NULL, &len, NULL));
    yy_assert(len == 0);
    len = 1;
    yy_assert(!yyjson_mut_write_opts(doc, 0, NULL, &len, NULL));
    yy_assert(len == 0);
    
    memset(&err, 0, sizeof(err));
    yy_assert(!yyjson_mut_write_opts(NULL, 0, NULL, NULL, &err));
    yy_assert(err.code && err.msg);
    memset(&err, 0, sizeof(err));
    yy_assert(!yyjson_mut_write_opts(doc, 0, NULL, NULL, &err));
    yy_assert(err.code && err.msg);
    
    len = 1;
    memset(&err, 0, sizeof(err));
    yy_assert(!yyjson_mut_write_opts(NULL, 0, NULL, &len, &err));
    yy_assert(len == 0);
    yy_assert(err.code && err.msg);
    len = 1;
    memset(&err, 0, sizeof(err));
    yy_assert(!yyjson_mut_write_opts(doc, 0, NULL, &len, &err));
    yy_assert(len == 0);
    yy_assert(err.code && err.msg);
    
    
    // invalid
    root = yyjson_mut_null(doc);
    root->tag = 0;
    yyjson_mut_doc_set_root(doc, root);
    validate_json_write(doc, alc, NULL, NULL);
    
    root = yyjson_mut_arr(doc);
    yyjson_mut_doc_set_root(doc, root);
    val = yyjson_mut_null(doc);
    val->tag = 0;
    yyjson_mut_arr_add_val(root, val);
    validate_json_write(doc, alc, NULL, NULL);
    
    
    // single
#if !YYJSON_DISABLE_NON_STANDARD
    root = yyjson_mut_real(doc, NAN);
    yyjson_mut_doc_set_root(doc, root);
    validate_json_write_ex(doc, alc,
                           "NaN", "NaN",
                           "null", "null");
    
    root = yyjson_mut_real(doc, -INFINITY);
    yyjson_mut_doc_set_root(doc, root);
    validate_json_write_ex(doc, alc,
                           "-Infinity", "-Infinity",
                           "null", "null");
#else
    root = yyjson_mut_real(doc, NAN);
    yyjson_mut_doc_set_root(doc, root);
    validate_json_write_ex(doc, alc,
                           NULL, NULL,
                           "null", "null");
    
    root = yyjson_mut_real(doc, -INFINITY);
    yyjson_mut_doc_set_root(doc, root);
    validate_json_write_ex(doc, alc,
                           NULL, NULL,
                           "null", "null");
#endif
    
    root = yyjson_mut_null(doc);
    yyjson_mut_doc_set_root(doc, root);
    validate_json_write(doc, alc, "null", "null");
    
    root = yyjson_mut_true(doc);
    yyjson_mut_doc_set_root(doc, root);
    validate_json_write(doc, alc, "true", "true");
    
    root = yyjson_mut_false(doc);
    yyjson_mut_doc_set_root(doc, root);
    validate_json_write(doc, alc, "false", "false");
    
    root = yyjson_mut_uint(doc, 123);
    yyjson_mut_doc_set_root(doc, root);
    validate_json_write(doc, alc, "123", "123");
    
    root = yyjson_mut_sint(doc, -123);
    yyjson_mut_doc_set_root(doc, root);
    validate_json_write(doc, alc, "-123", "-123");
    
    root = yyjson_mut_real(doc, -1.5);
    yyjson_mut_doc_set_root(doc, root);
    validate_json_write(doc, alc, "-1.5", "-1.5");
    
    root = yyjson_mut_str(doc, "abc");
    yyjson_mut_doc_set_root(doc, root);
    validate_json_write(doc, alc, "\"abc\"", "\"abc\"");
    
    root = yyjson_mut_arr(doc);
    yyjson_mut_doc_set_root(doc, root);
    validate_json_write(doc, alc, "[]", "[]");
    
    root = yyjson_mut_obj(doc);
    yyjson_mut_doc_set_root(doc, root);
    validate_json_write(doc, alc, "{}", "{}");
    
    
    // string without null-terminator
    for (len = 0; len <= 128; len++) {
        char *str = len ? malloc(len) : (char *)1;
        for (usize i = 0; i < len; i++) {
            str[i] = 'a' + (yy_rand_u32() % 26);
        }
        
        char *json = malloc(len + 3);
        json[0] = '"';
        memcpy((void *)(json + 1), (void *)str, len);
        json[len + 1] = '"';
        json[len + 2] = '\0';
        
        root = yyjson_mut_strn(doc, str, len);
        yyjson_mut_doc_set_root(doc, root);
        validate_json_write(doc, alc, json, json);
        
        if (len) free((void *)str);
        free((void *)json);
    }
    
    
    // array
    root = yyjson_mut_arr(doc);
    yyjson_mut_doc_set_root(doc, root);
    yyjson_mut_arr_add_int(doc, root, 1);
    validate_json_write(doc, alc,
                        "[1]",
                        "[\n"
                        "    1\n"
                        "]");
    
    root = yyjson_mut_arr(doc);
    yyjson_mut_doc_set_root(doc, root);
    yyjson_mut_arr_add_int(doc, root, 1);
    yyjson_mut_arr_add_int(doc, root, 2);
    validate_json_write(doc, alc,
                        "[1,2]",
                        "[\n"
                        "    1,\n"
                        "    2\n"
                        "]");
    
#if !YYJSON_DISABLE_NON_STANDARD
    root = yyjson_mut_arr(doc);
    yyjson_mut_doc_set_root(doc, root);
    yyjson_mut_arr_add_real(doc, root, NAN);
    validate_json_write_ex(doc, alc,
                           "[NaN]",
                           "[\n"
                           "    NaN\n"
                           "]",
                           "[null]",
                           "[\n"
                           "    null\n"
                           "]");
#endif
    
    root = yyjson_mut_arr(doc);
    yyjson_mut_doc_set_root(doc, root);
    yyjson_mut_arr_add_str(doc, root, "abc");
    yyjson_mut_arr_add_bool(doc, root, true);
    yyjson_mut_arr_add_null(doc, root);
    validate_json_write(doc, alc,
                        "[\"abc\",true,null]",
                        "[\n"
                        "    \"abc\",\n"
                        "    true,\n"
                        "    null\n"
                        "]");
    
    root = yyjson_mut_arr(doc);
    yyjson_mut_doc_set_root(doc, root);
    val = yyjson_mut_arr(doc);
    yyjson_mut_arr_add_val(root, val);
    validate_json_write(doc, alc,
                        "[[]]",
                        "[\n"
                        "    []\n"
                        "]");
    
    root = yyjson_mut_arr(doc);
    yyjson_mut_doc_set_root(doc, root);
    val = yyjson_mut_arr_add_arr(doc, root);
    yyjson_mut_arr_add_arr(doc, val);
    validate_json_write(doc, alc,
                        "[[[]]]",
                        "[\n"
                        "    [\n"
                        "        []\n"
                        "    ]\n"
                        "]");
    
    root = yyjson_mut_arr(doc);
    yyjson_mut_doc_set_root(doc, root);
    val = yyjson_mut_obj(doc);
    yyjson_mut_arr_add_val(root, val);
    validate_json_write(doc, alc,
                        "[{}]",
                        "[\n"
                        "    {}\n"
                        "]");
    
    root = yyjson_mut_arr(doc);
    yyjson_mut_doc_set_root(doc, root);
    yyjson_mut_arr_add_arr(doc, root);
    yyjson_mut_arr_add_true(doc, root);
    validate_json_write(doc, alc,
                        "[[],true]",
                        "[\n"
                        "    [],\n"
                        "    true\n"
                        "]");
    
    root = yyjson_mut_arr(doc);
    yyjson_mut_doc_set_root(doc, root);
    yyjson_mut_arr_add_true(doc, root);
    yyjson_mut_arr_add_arr(doc, root);
    validate_json_write(doc, alc,
                        "[true,[]]",
                        "[\n"
                        "    true,\n"
                        "    []\n"
                        "]");
    
    root = yyjson_mut_arr(doc);
    yyjson_mut_doc_set_root(doc, root);
    val = yyjson_mut_arr_add_arr(doc, root);
    yyjson_mut_arr_add_true(doc, val);
    validate_json_write(doc, alc,
                        "[[true]]",
                        "[\n"
                        "    [\n"
                        "        true\n"
                        "    ]\n"
                        "]");
    
    root = yyjson_mut_arr(doc);
    yyjson_mut_doc_set_root(doc, root);
    val = yyjson_mut_arr_add_arr(doc, root);
    yyjson_mut_arr_add_true(doc, val);
    val = yyjson_mut_arr_add_arr(doc, root);
    validate_json_write(doc, alc,
                        "[[true],[]]",
                        "[\n"
                        "    [\n"
                        "        true\n"
                        "    ],\n"
                        "    []\n"
                        "]");
    
    cur1 = str1 = malloc(1024 * 2 + 4);
    cur2 = str2 = malloc(1024 * 7 + 4);
    root = yyjson_mut_arr(doc);
    yyjson_mut_doc_set_root(doc, root);
    *cur1++ = '[';
    *cur2++ = '[';
    *cur2++ = '\n';
    for (int i = 0; i < 1024; i++) {
        yyjson_mut_arr_add_int(doc, root, 1);
        memcpy(cur1, "1,", 2);
        cur1 += 2;
        memcpy(cur2, "    1,\n", 7);
        cur2 += 7;
    }
    cur1 -= 1;
    *cur1++ = ']';
    *cur1 = '\0';
    cur2 -= 2;
    *cur2++ = '\n';
    *cur2++ = ']';
    *cur2 = '\0';
    validate_json_write(doc, alc, str1, str2);
    free(str1);
    free(str2);
    
    
    // object
    root = yyjson_mut_obj(doc);
    yyjson_mut_doc_set_root(doc, root);
    yy_assert(doc->root);
    yyjson_mut_obj_add_int(doc, root, "abc", 123);
    yy_assert(doc->root);
    validate_json_write(doc, alc,
                        "{\"abc\":123}",
                        "{\n"
                        "    \"abc\": 123\n"
                        "}");
    
#if !YYJSON_DISABLE_NON_STANDARD
    root = yyjson_mut_obj(doc);
    yyjson_mut_doc_set_root(doc, root);
    yy_assert(doc->root);
    yyjson_mut_obj_add_real(doc, root, "abc", NAN);
    yy_assert(doc->root);
    validate_json_write_ex(doc, alc,
                           "{\"abc\":NaN}",
                           "{\n"
                           "    \"abc\": NaN\n"
                           "}",
                           "{\"abc\":null}",
                           "{\n"
                           "    \"abc\": null\n"
                           "}");
#endif
    
    root = yyjson_mut_obj(doc);
    yyjson_mut_doc_set_root(doc, root);
    yy_assert(doc->root);
    val = yyjson_mut_obj(doc);
    yyjson_mut_obj_add_val(doc, root, "abc", val);
    yy_assert(doc->root);
    validate_json_write(doc, alc,
                        "{\"abc\":{}}",
                        "{\n"
                        "    \"abc\": {}\n"
                        "}");
    
    root = yyjson_mut_obj(doc);
    yyjson_mut_doc_set_root(doc, root);
    yyjson_mut_obj_add_null(doc, root, "a");
    yyjson_mut_obj_add_true(doc, root, "b");
    yyjson_mut_obj_add_int(doc, root, "c", 123);
    yyjson_mut_obj_add_str(doc, root, "d", "zzz");
    validate_json_write(doc, alc,
                        "{\"a\":null,\"b\":true,\"c\":123,\"d\":\"zzz\"}",
                        "{\n"
                        "    \"a\": null,\n"
                        "    \"b\": true,\n"
                        "    \"c\": 123,\n"
                        "    \"d\": \"zzz\"\n"
                        "}");
    
    root = yyjson_mut_obj(doc);
    yyjson_mut_doc_set_root(doc, root);
    yyjson_mut_obj_add_null(doc, root, "a");
    yyjson_mut_obj_add_true(doc, root, "a");
    yyjson_mut_obj_add_false(doc, root, "a");
    yyjson_mut_obj_add_int(doc, root, "a", 123);
    yyjson_mut_obj_add_str(doc, root, "a", "zzz");
    validate_json_write(doc, alc,
                        "{\"a\":null,\"a\":true,\"a\":false,\"a\":123,\"a\":\"zzz\"}",
                        "{\n"
                        "    \"a\": null,\n"
                        "    \"a\": true,\n"
                        "    \"a\": false,\n"
                        "    \"a\": 123,\n"
                        "    \"a\": \"zzz\"\n"
                        "}");
    
    root = yyjson_mut_obj(doc);
    yyjson_mut_doc_set_root(doc, root);
    val = yyjson_mut_arr(doc);
    yyjson_mut_arr_add_int(doc, val, 123);
    yyjson_mut_obj_add_val(doc, root, "a", val);
    validate_json_write(doc, alc,
                        "{\"a\":[123]}",
                        "{\n"
                        "    \"a\": [\n"
                        "        123\n"
                        "    ]\n"
                        "}");
    
    root = yyjson_mut_obj(doc);
    yyjson_mut_doc_set_root(doc, root);
    val = yyjson_mut_obj(doc);
    yyjson_mut_obj_add_val(doc, root, "a", val);
    val2 = yyjson_mut_obj(doc);
    yyjson_mut_obj_add_val(doc, val, "b", val2);
    validate_json_write(doc, alc,
                        "{\"a\":{\"b\":{}}}",
                        "{\n"
                        "    \"a\": {\n"
                        "        \"b\": {}\n"
                        "    }\n"
                        "}");
    
    // large object with same key
    cur1 = str1 = malloc(1024 * 6 + 32);
    cur2 = str2 = malloc(1024 * 12 + 32);
    root = yyjson_mut_obj(doc);
    yyjson_mut_doc_set_root(doc, root);
    *cur1++ = '{';
    *cur2++ = '{';
    *cur2++ = '\n';
    for (int i = 0; i < 1024; i++) {
        yyjson_mut_obj_add_int(doc, root, "a", 1);
        memcpy(cur1, "\"a\":1,", 6);
        cur1 += 6;
        memcpy(cur2, "    \"a\": 1,\n", 12);
        cur2 += 12;
    }
    cur1 -= 1;
    *cur1++ = '}';
    *cur1 = '\0';
    cur2 -= 2;
    *cur2++ = '\n';
    *cur2++ = '}';
    *cur2 = '\0';
    validate_json_write(doc, alc, str1, str2);
    free(str1);
    free(str2);
    
    yyjson_mut_doc_free(doc);
}

yy_test_case(test_json_writer) {
    // test read and roundtrip
    {
        yyjson_alc alc;
        usize len = 1024 * 1024;
        void *buf = malloc(len);
        yyjson_alc_pool_init(&alc, buf, len);
        test_json_write(&alc);
        test_json_write(NULL);
        free(buf);
    }
    
    // test invalid parameters
    {
        yyjson_write_file(NULL, NULL, 0, NULL, NULL);
        yyjson_write_file("", NULL, 0, NULL, NULL);
        yyjson_write_file("tmp.json", NULL, 0, NULL, NULL);
        yyjson_mut_write_file(NULL, NULL, 0, NULL, NULL);
        yyjson_mut_write_file("", NULL, 0, NULL, NULL);
        yyjson_mut_write_file("tmp.json", NULL, 0, NULL, NULL);
    }
    
#if !YYJSON_DISABLE_READER
    // test invalid immutable doc
    {
        yyjson_doc *doc = yyjson_read("[1]", 3, 0);
        yy_assert(doc);
        yyjson_val *arr = yyjson_doc_get_root(doc);
        yy_assert(arr);
        yyjson_val *one = yyjson_arr_get(arr, 0);
        yy_assert(one);
        one->tag = YYJSON_TYPE_NONE;
        yy_assert(!yyjson_write(doc, 0, NULL));
        yy_assert(!yyjson_write(doc, YYJSON_WRITE_PRETTY, NULL));
        one->tag = YYJSON_TYPE_NUM | YYJSON_SUBTYPE_REAL;
        one->uni.f64 = NAN;
        yy_assert(!yyjson_write(doc, 0, NULL));
        yy_assert(!yyjson_write(doc, YYJSON_WRITE_PRETTY, NULL));
        yyjson_doc_free(doc);
    }
    
    // test fail
    {
        char path[4100];
        memset(path, 'a', sizeof(path));
        path[4099] = '\0';
        yyjson_doc *idoc = yyjson_read("1", 1, 0);
        yy_assert(!yyjson_write_file(path, idoc, 0, NULL, NULL));
        yyjson_doc_free(idoc);
        
        yyjson_mut_doc *mdoc = yyjson_mut_doc_new(NULL);
        yyjson_mut_doc_set_root(mdoc, yyjson_mut_null(mdoc));
        yy_assert(!yyjson_mut_write_file(path, mdoc, 0, NULL, NULL));
        yyjson_mut_doc_free(mdoc);
    }
    
    // test raw
    {
        const char *str = "[1.2345678901234567890e999]";
        yyjson_doc *idoc = yyjson_read(str, strlen(str), YYJSON_READ_NUMBER_AS_RAW);
        yyjson_val *root = yyjson_doc_get_root(idoc);
        yyjson_val *raw = yyjson_arr_get_first(root);
        yy_assert(yyjson_is_raw(raw));
        yy_assert(yyjson_get_len(raw) == strlen(str) - 2);
        yy_assert(memcmp(yyjson_get_raw(raw), str + 1, strlen(str) - 2) == 0);
        
        usize ret_len;
        char *ret = yyjson_write(idoc, 0, &ret_len);
        yy_assert(ret_len == strlen(str) && memcmp(ret, str, ret_len) == 0);
        free(ret);
        ret = yyjson_write(idoc, YYJSON_WRITE_PRETTY, &ret_len);
        yy_assert(ret);
        free(ret);
        
        yyjson_mut_doc *mdoc = yyjson_doc_mut_copy(idoc, NULL);
        ret = yyjson_mut_write(mdoc, 0, &ret_len);
        yy_assert(ret_len == strlen(str) && memcmp(ret, str, ret_len) == 0);
        free(ret);
        ret = yyjson_mut_write(mdoc, YYJSON_WRITE_PRETTY, &ret_len);
        yy_assert(ret);
        free(ret);
        yyjson_mut_doc_free(mdoc);
        
        yyjson_doc_free(idoc);
    }
    
    // test modify input
    {
        char *ret;
        const char *str = "[123]";
        yyjson_doc *doc = yyjson_read(str, strlen(str), 0);
        yyjson_val *root = yyjson_doc_get_root(doc);
        yyjson_val *val = yyjson_arr_get(root, 0);
        
        yy_assert(!yyjson_set_bool(root, true));
        
        yyjson_set_raw(val, "aaa", 3);
        ret = yyjson_write(doc, 0, NULL);
        yy_assert(strcmp(ret, "[aaa]") == 0);
        free(ret);
        
        yyjson_set_null(val);
        ret = yyjson_write(doc, 0, NULL);
        yy_assert(strcmp(ret, "[null]") == 0);
        free(ret);
        
        yyjson_set_bool(val, true);
        ret = yyjson_write(doc, 0, NULL);
        yy_assert(strcmp(ret, "[true]") == 0);
        free(ret);
        
        yyjson_set_uint(val, 111);
        ret = yyjson_write(doc, 0, NULL);
        yy_assert(strcmp(ret, "[111]") == 0);
        free(ret);
        
        yyjson_set_sint(val, -111);
        ret = yyjson_write(doc, 0, NULL);
        yy_assert(strcmp(ret, "[-111]") == 0);
        free(ret);
        
        yyjson_set_int(val, 100);
        ret = yyjson_write(doc, 0, NULL);
        yy_assert(strcmp(ret, "[100]") == 0);
        free(ret);
        
        yyjson_set_real(val, 1.5);
        ret = yyjson_write(doc, 0, NULL);
        yy_assert(strcmp(ret, "[1.5]") == 0);
        free(ret);
        
        yyjson_set_str(val, "abc");
        ret = yyjson_write(doc, 0, NULL);
        yy_assert(strcmp(ret, "[\"abc\"]") == 0);
        free(ret);
        
        yyjson_set_strn(val, "abcd", 3);
        ret = yyjson_write(doc, 0, NULL);
        yy_assert(strcmp(ret, "[\"abc\"]") == 0);
        free(ret);
        
        yyjson_doc_free(doc);
    }
    
    
    // test 2 space indent
    {
        const char *str =
            "[\n"
            "  123\n"
            "]";
        yyjson_doc *doc = yyjson_read(str, strlen(str), 0);
        yyjson_mut_doc *mdoc = yyjson_doc_mut_copy(doc, NULL);
        
        char *ret = yyjson_write(doc, YYJSON_WRITE_PRETTY_TWO_SPACES, NULL);
        yy_assert(strcmp(ret, str) == 0);
        free(ret);
        
        char *mret = yyjson_mut_write(mdoc, YYJSON_WRITE_PRETTY_TWO_SPACES, NULL);
        yy_assert(strcmp(mret, str) == 0);
        free(mret);
        
        yyjson_doc_free(doc);
        yyjson_mut_doc_free(mdoc);
    }
    
    
    // test newline at end
    {
        size_t len;
        const char *str;
        yyjson_doc *doc;
        yyjson_mut_doc *mdoc;
        char *ret;
        
        // single value
        str = "123";
        doc = yyjson_read(str, strlen(str), 0);
        ret = yyjson_write(doc, YYJSON_WRITE_NEWLINE_AT_END, &len);
        yy_assert(strlen(ret) == len && len == strlen(str) + 1);
        yy_assert(memcmp(str, ret, strlen(str)) == 0);
        yy_assert(ret[strlen(str)] == '\n');
        free(ret);
        
        mdoc = yyjson_doc_mut_copy(doc, NULL);
        ret = yyjson_mut_write(mdoc, YYJSON_WRITE_NEWLINE_AT_END, &len);
        yy_assert(strlen(ret) == len && len == strlen(str) + 1);
        yy_assert(memcmp(str, ret, strlen(str)) == 0);
        yy_assert(ret[strlen(str)] == '\n');
        free(ret);
        
        yyjson_doc_free(doc);
        yyjson_mut_doc_free(mdoc);
        
        // multiple values
        str = "[123]";
        doc = yyjson_read(str, strlen(str), 0);
        ret = yyjson_write(doc, YYJSON_WRITE_NEWLINE_AT_END, &len);
        yy_assert(strlen(ret) == len && len == strlen(str) + 1);
        yy_assert(memcmp(str, ret, strlen(str)) == 0);
        yy_assert(ret[strlen(str)] == '\n');
        free(ret);
        
        mdoc = yyjson_doc_mut_copy(doc, NULL);
        ret = yyjson_mut_write(mdoc, YYJSON_WRITE_NEWLINE_AT_END, &len);
        yy_assert(strlen(ret) == len && len == strlen(str) + 1);
        yy_assert(memcmp(str, ret, strlen(str)) == 0);
        yy_assert(ret[strlen(str)] == '\n');
        free(ret);
        
        yyjson_doc_free(doc);
        yyjson_mut_doc_free(mdoc);
        
        // multiple values, pretty
        str = "[\n    123\n]";
        doc = yyjson_read(str, strlen(str), 0);
        ret = yyjson_write(doc, YYJSON_WRITE_PRETTY | YYJSON_WRITE_NEWLINE_AT_END, &len);
        yy_assert(strlen(ret) == len && len == strlen(str) + 1);
        yy_assert(memcmp(str, ret, strlen(str)) == 0);
        yy_assert(ret[strlen(str)] == '\n');
        free(ret);
        
        mdoc = yyjson_doc_mut_copy(doc, NULL);
        ret = yyjson_mut_write(mdoc, YYJSON_WRITE_PRETTY | YYJSON_WRITE_NEWLINE_AT_END, &len);
        yy_assert(strlen(ret) == len && len == strlen(str) + 1);
        yy_assert(memcmp(str, ret, strlen(str)) == 0);
        yy_assert(ret[strlen(str)] == '\n');
        free(ret);
        
        yyjson_doc_free(doc);
        yyjson_mut_doc_free(mdoc);
    }
#endif
    
    // test build JSON on stack
    {
        const char *expect = "{\"code\":200,\"msg\":\"success\",\"arr\":[true,false,null,1,-1,0.5,inf]}";
        
        yyjson_mut_val root, code_key, code, msg_key, msg, arr_key, arr;
        yyjson_mut_val vals[7];
        yyjson_mut_set_obj(&root);
        yyjson_mut_set_str(&code_key, "code");
        yyjson_mut_set_int(&code, 200);
        yyjson_mut_set_str(&msg_key, "msg");
        yyjson_mut_set_str(&msg, "success");
        yyjson_mut_set_str(&arr_key, "arr");
        yyjson_mut_set_arr(&arr);
        yyjson_mut_set_bool(&vals[0], true);
        yyjson_mut_set_bool(&vals[1], false);
        yyjson_mut_set_null(&vals[2]);
        yyjson_mut_set_uint(&vals[3], 1);
        yyjson_mut_set_sint(&vals[4], -1);
        yyjson_mut_set_real(&vals[5], 0.5);
        yyjson_mut_set_raw(&vals[6], "inf", 3);
        
        yyjson_mut_obj_add(&root, &code_key, &code);
        yyjson_mut_obj_add(&root, &msg_key, &msg);
        yyjson_mut_obj_add(&root, &arr_key, &arr);
        for (size_t i = 0; i < yy_nelems(vals); i++) {
            yyjson_mut_arr_append(&arr, &vals[i]);
        }
        
        char buf[256];
        yyjson_alc alc;
        yyjson_alc_pool_init(&alc, buf, sizeof(buf));
        char *json = yyjson_mut_val_write_opts(&root, 0, &alc, NULL, NULL);
        yy_assert(strcmp(json, expect) == 0);
    }
    
    // test bool conversion
    // some environments don't have a native bool type
    // and the bool type may store values other than 0/1
    {
        yyjson_mut_doc *mdoc = yyjson_mut_doc_new(NULL);
        yyjson_mut_val *mobj = yyjson_mut_obj(mdoc);
        yyjson_mut_doc_set_root(mdoc, mobj);
        
        for (u8 i = 0; i < 10; i++) {
            char str[2];
            snprintf(str, sizeof(str), "%d", (int)i);
            yyjson_mut_val *key = yyjson_mut_strcpy(mdoc, str);
            yyjson_mut_val *val = yyjson_mut_bool(mdoc, (bool)i);
            yyjson_mut_obj_add(mobj, key, val);
        }
        
        char *json = yyjson_mut_write(mdoc, YYJSON_WRITE_PRETTY, NULL);
        yy_assert(json != NULL);
        
#if !YYJSON_DISABLE_READER
        yyjson_doc *doc = yyjson_read(json, strlen(json), 0);
        yyjson_val *obj = yyjson_doc_get_root(doc);
        yy_assert(yyjson_obj_size(obj) == 10);
        for (u8 i = 0; i < 10; i++) {
            char str[2];
            snprintf(str, sizeof(str), "%d", (int)i);
            yyjson_val *val = yyjson_obj_get(obj, str);
            yy_assert(yyjson_is_bool(val));
            yy_assert(yyjson_get_bool(val) == (i != 0));
        }
        yyjson_doc_free(doc);
#endif
        
        yyjson_mut_doc_free(mdoc);
        free(json);
    }
}

#else
yy_test_case(test_json_writer) {}
#endif
