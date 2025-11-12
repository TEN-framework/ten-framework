// This file is used to test the functions related to `yyjson_val`.

#include "yyjson.h"
#include "yy_test_utils.h"

#if !YYJSON_DISABLE_READER

/// Validate value type
static bool validate_val_type(yyjson_val *val,
                              yyjson_type type,
                              yyjson_subtype subtype) {
    
    if (yyjson_is_null(val) != (type == YYJSON_TYPE_NULL &&
                                subtype == YYJSON_SUBTYPE_NONE)) return false;
    if (yyjson_is_true(val) != (type == YYJSON_TYPE_BOOL &&
                                subtype == YYJSON_SUBTYPE_TRUE)) return false;
    if (yyjson_is_false(val) != (type == YYJSON_TYPE_BOOL &&
                                 subtype == YYJSON_SUBTYPE_FALSE)) return false;
    if (yyjson_is_bool(val) != (type == YYJSON_TYPE_BOOL &&
                                (subtype == YYJSON_SUBTYPE_TRUE ||
                                 subtype == YYJSON_SUBTYPE_FALSE))) return false;
    if (yyjson_is_uint(val) != (type == YYJSON_TYPE_NUM &&
                                subtype == YYJSON_SUBTYPE_UINT)) return false;
    if (yyjson_is_sint(val) != (type == YYJSON_TYPE_NUM &&
                                subtype == YYJSON_SUBTYPE_SINT)) return false;
    if (yyjson_is_int(val) != (type == YYJSON_TYPE_NUM &&
                               (subtype == YYJSON_SUBTYPE_UINT ||
                                subtype == YYJSON_SUBTYPE_SINT))) return false;
    if (yyjson_is_real(val) != (type == YYJSON_TYPE_NUM &&
                                subtype == YYJSON_SUBTYPE_REAL)) return false;
    if (yyjson_is_num(val) != (type == YYJSON_TYPE_NUM &&
                               (subtype == YYJSON_SUBTYPE_UINT ||
                                subtype == YYJSON_SUBTYPE_SINT ||
                                subtype == YYJSON_SUBTYPE_REAL))) return false;
    if (yyjson_is_str(val) != (type == YYJSON_TYPE_STR)) return false;
    if (yyjson_is_arr(val) != (type == YYJSON_TYPE_ARR &&
                               subtype == YYJSON_SUBTYPE_NONE)) return false;
    if (yyjson_is_obj(val) != (type == YYJSON_TYPE_OBJ &&
                               subtype == YYJSON_SUBTYPE_NONE)) return false;
    if (yyjson_is_ctn(val) != ((type == YYJSON_TYPE_ARR ||
                                type == YYJSON_TYPE_OBJ) &&
                                subtype == YYJSON_SUBTYPE_NONE)) return false;
    
    if (yyjson_get_type(val) != type) return false;
    if (yyjson_get_subtype(val) != subtype) return false;
    if (yyjson_get_tag(val) != (type | subtype)) return false;
    
    return true;
}

/// Test simple json value api
static void test_json_val_api(void) {
    yyjson_doc *doc;
    yyjson_val *val;
    const char *json;
    
    val = NULL;
    yy_assert(strcmp(yyjson_get_type_desc(val), "unknown") == 0);
    yy_assert(yyjson_get_len(val) == 0);
    yy_assert(yyjson_equals_str(val, "") == false);
    yy_assert(yyjson_equals_strn(val, "", 0) == false);
    yy_assert(yyjson_arr_size(val) == 0);
    yy_assert(yyjson_obj_size(val) == 0);
    yy_assert(yyjson_get_uint(val) == (u64)0);
    yy_assert(yyjson_get_sint(val) == (i64)0);
    yy_assert(yyjson_get_int(val) == (i64)0);
    yy_assert(yyjson_get_real(val) == (f64)0);
    yy_assert(yyjson_get_num(val) == (f64)0);
    yy_assert(yyjson_get_bool(val) == false);
    yy_assert(yyjson_get_str(val) == NULL);
    
    json = "null";
    doc = yyjson_read(json, strlen(json), 0);
    val = yyjson_doc_get_root(doc);
    yy_assert(validate_val_type(val, YYJSON_TYPE_NULL, YYJSON_SUBTYPE_NONE));
    yy_assert(strcmp(yyjson_get_type_desc(val), "null") == 0);
    yyjson_doc_free(doc);
    
    json = "true";
    doc = yyjson_read(json, strlen(json), 0);
    val = yyjson_doc_get_root(doc);
    yy_assert(validate_val_type(val, YYJSON_TYPE_BOOL, YYJSON_SUBTYPE_TRUE));
    yy_assert(strcmp(yyjson_get_type_desc(val), "true") == 0);
    yy_assert(yyjson_get_bool(val) == true);
    yyjson_doc_free(doc);
    
    json = "false";
    doc = yyjson_read(json, strlen(json), 0);
    val = yyjson_doc_get_root(doc);
    yy_assert(validate_val_type(val, YYJSON_TYPE_BOOL, YYJSON_SUBTYPE_FALSE));
    yy_assert(strcmp(yyjson_get_type_desc(val), "false") == 0);
    yy_assert(yyjson_get_bool(val) == false);
    yyjson_doc_free(doc);
    
    json = "123";
    doc = yyjson_read(json, strlen(json), 0);
    val = yyjson_doc_get_root(doc);
    yy_assert(validate_val_type(val, YYJSON_TYPE_NUM, YYJSON_SUBTYPE_UINT));
    yy_assert(strcmp(yyjson_get_type_desc(val), "uint") == 0);
    yy_assert(yyjson_get_uint(val) == (u64)123);
    yy_assert(yyjson_get_sint(val) == (i64)123);
    yy_assert(yyjson_get_int(val) == (i64)123);
    yy_assert(yyjson_get_real(val) == (f64)0);
    yy_assert(yyjson_get_num(val) == (f64)123);
    yy_assert(yyjson_get_bool(val) == false);
    yyjson_doc_free(doc);
    
    json = "-123";
    doc = yyjson_read(json, strlen(json), 0);
    val = yyjson_doc_get_root(doc);
    yy_assert(validate_val_type(val, YYJSON_TYPE_NUM, YYJSON_SUBTYPE_SINT));
    yy_assert(strcmp(yyjson_get_type_desc(val), "sint") == 0);
    yy_assert(yyjson_get_uint(val) == (u64)-123);
    yy_assert(yyjson_get_sint(val) == (i64)-123);
    yy_assert(yyjson_get_int(val) == (i64)-123);
    yy_assert(yyjson_get_real(val) == (f64)0);
    yy_assert(yyjson_get_num(val) == (f64)-123);
    yyjson_doc_free(doc);
    
    json = "123.0";
    doc = yyjson_read(json, strlen(json), 0);
    val = yyjson_doc_get_root(doc);
    yy_assert(validate_val_type(val, YYJSON_TYPE_NUM, YYJSON_SUBTYPE_REAL));
    yy_assert(strcmp(yyjson_get_type_desc(val), "real") == 0);
    yy_assert(yyjson_get_uint(val) == (u64)0);
    yy_assert(yyjson_get_sint(val) == (i64)0);
    yy_assert(yyjson_get_int(val) == (i64)0);
    yy_assert(yyjson_get_real(val) == (f64)123.0);
    yy_assert(yyjson_get_num(val) == (f64)123.0);
    yyjson_doc_free(doc);
    
    json = "\"abc\"";
    doc = yyjson_read(json, strlen(json), 0);
    val = yyjson_doc_get_root(doc);
    yy_assert(validate_val_type(val, YYJSON_TYPE_STR, YYJSON_SUBTYPE_NOESC));
    yy_assert(strcmp(yyjson_get_type_desc(val), "string") == 0);
    yy_assert(strcmp(yyjson_get_str(val), "abc") == 0);
    yy_assert(yyjson_get_uint(val) == (u64)0);
    yy_assert(yyjson_get_sint(val) == (i64)0);
    yy_assert(yyjson_get_int(val) == (i64)0);
    yy_assert(yyjson_get_real(val) == (f64)0.0);
    yy_assert(yyjson_get_num(val) == (f64)0.0);
    yy_assert(yyjson_get_len(val) == 3);
    yy_assert(yyjson_equals_str(val, "abc"));
    yyjson_doc_free(doc);
    
    json = "\"abc\\u0000def\"";
    doc = yyjson_read(json, strlen(json), 0);
    val = yyjson_doc_get_root(doc);
    yy_assert(validate_val_type(val, YYJSON_TYPE_STR, YYJSON_SUBTYPE_NONE));
    yy_assert(strcmp(yyjson_get_type_desc(val), "string") == 0);
    yy_assert(strcmp(yyjson_get_str(val), "abc") == 0);
    yy_assert(memcmp(yyjson_get_str(val), "abc\0def", 7) == 0);
    yy_assert(yyjson_get_len(val) == 7);
    yy_assert(yyjson_equals_str(val, "abc") == false);
    yy_assert(yyjson_equals_strn(val, "abc", 3) == false);
    yy_assert(yyjson_equals_str(val, "abc\0def") == false);
    yy_assert(yyjson_equals_strn(val, "abc\0def", 7) == true);
    yyjson_doc_free(doc);
    
    json = "[]";
    doc = yyjson_read(json, strlen(json), 0);
    val = yyjson_doc_get_root(doc);
    yy_assert(validate_val_type(val, YYJSON_TYPE_ARR, YYJSON_SUBTYPE_NONE));
    yy_assert(strcmp(yyjson_get_type_desc(val), "array") == 0);
    yyjson_doc_free(doc);
    
    json = "{}";
    doc = yyjson_read(json, strlen(json), 0);
    val = yyjson_doc_get_root(doc);
    yy_assert(validate_val_type(val, YYJSON_TYPE_OBJ, YYJSON_SUBTYPE_NONE));
    yy_assert(strcmp(yyjson_get_type_desc(val), "object") == 0);
    yyjson_doc_free(doc);
    
    val = NULL;
    yy_assert(validate_val_type(val, YYJSON_TYPE_NONE, YYJSON_SUBTYPE_NONE));
}

/// Test json array api
static void test_json_arr_api(void) {
    yyjson_doc *doc;
    yyjson_val *arr, *val;
    const char *json;
    yyjson_arr_iter iter;
    size_t idx, max, tmp[16];
    
    //---------------------------------------------
    // array (size 0)
    
    json = "[]";
    doc = yyjson_read(json, strlen(json), 0);
    arr = yyjson_doc_get_root(doc);
    yy_assert(yyjson_is_arr(arr));
    yy_assert(yyjson_arr_size(arr) == 0);
    
    val = yyjson_arr_get(arr, 0);
    yy_assert(val == NULL);
    
    val = yyjson_arr_get_first(arr);
    yy_assert(val == NULL);
    val = yyjson_arr_get_last(arr);
    yy_assert(val == NULL);
    
    // iter
    iter = yyjson_arr_iter_with(arr);
    yy_assert(yyjson_arr_iter_has_next(&iter) == false);
    while ((val = yyjson_arr_iter_next(&iter))) {
        yy_assert(false);
    }
    
    // foreach
    yyjson_arr_foreach(arr, idx, max, val) {
        yy_assert(false);
    }
    
    yyjson_doc_free(doc);
    
    
    //---------------------------------------------
    // array (size 1)
    
    json = "[1]";
    doc = yyjson_read(json, strlen(json), 0);
    arr = yyjson_doc_get_root(doc);
    yy_assert(yyjson_arr_size(arr) == 1);
    
    val = yyjson_arr_get(arr, 0);
    yy_assert(yyjson_get_int(val) == 1);
    val = yyjson_arr_get(arr, 1);
    yy_assert(val == NULL);
    
    val = yyjson_arr_get_first(arr);
    yy_assert(yyjson_get_int(val) == 1);
    val = yyjson_arr_get_last(arr);
    yy_assert(yyjson_get_int(val) == 1);
    
    // iter
    idx = 0;
    yyjson_arr_iter_init(arr, &iter);
    yy_assert(yyjson_arr_iter_has_next(&iter) == true);
    while ((val = yyjson_arr_iter_next(&iter))) {
        idx++;
        yy_assert(yyjson_get_int(val) == (i64)idx);
        yy_assert(yyjson_arr_iter_has_next(&iter) == idx < 1);
    }
    yy_assert(yyjson_arr_iter_has_next(&iter) == false);
    yy_assert(idx == 1);
    
    // foreach
    memset(tmp, 0, sizeof(tmp));
    yyjson_arr_foreach(arr, idx, max, val) {
        yy_assert(yyjson_get_int(val) == (i64)idx + 1);
        yy_assert(max == 1);
        yy_assert(tmp[idx] == 0);
        tmp[idx] = idx + 1;
    }
    yy_assert(tmp[0] == 1);
    yy_assert(tmp[1] == 0);
    
    yyjson_doc_free(doc);
    
    
    //---------------------------------------------
    // array (size 2)
    
    json = "[1,2]";
    doc = yyjson_read(json, strlen(json), 0);
    arr = yyjson_doc_get_root(doc);
    yy_assert(yyjson_arr_size(arr) == 2);
    
    val = yyjson_arr_get(arr, 0);
    yy_assert(yyjson_get_int(val) == 1);
    val = yyjson_arr_get(arr, 1);
    yy_assert(yyjson_get_int(val) == 2);
    val = yyjson_arr_get(arr, 2);
    yy_assert(val == NULL);
    
    val = yyjson_arr_get_first(arr);
    yy_assert(yyjson_get_int(val) == 1);
    val = yyjson_arr_get_last(arr);
    yy_assert(yyjson_get_int(val) == 2);
    
    // iter
    idx = 0;
    yyjson_arr_iter_init(arr, &iter);
    yy_assert(yyjson_arr_iter_has_next(&iter) == true);
    while ((val = yyjson_arr_iter_next(&iter))) {
        idx++;
        yy_assert(yyjson_get_int(val) == (i64)idx);
        yy_assert(yyjson_arr_iter_has_next(&iter) == idx < 2);
    }
    yy_assert(yyjson_arr_iter_has_next(&iter) == false);
    yy_assert(idx == 2);
    
    // foreach
    memset(tmp, 0, sizeof(tmp));
    yyjson_arr_foreach(arr, idx, max, val) {
        yy_assert(yyjson_get_int(val) == (i64)idx + 1);
        yy_assert(max == 2);
        yy_assert(tmp[idx] == 0);
        tmp[idx] = idx + 1;
    }
    yy_assert(tmp[0] == 1);
    yy_assert(tmp[1] == 2);
    yy_assert(tmp[2] == 0);
    
    yyjson_doc_free(doc);
    
    
    //---------------------------------------------
    // array (size 3)
    
    json = "[1,2,3]";
    doc = yyjson_read(json, strlen(json), 0);
    arr = yyjson_doc_get_root(doc);
    yy_assert(yyjson_arr_size(arr) == 3);
    
    val = yyjson_arr_get(arr, 0);
    yy_assert(yyjson_get_int(val) == 1);
    val = yyjson_arr_get(arr, 1);
    yy_assert(yyjson_get_int(val) == 2);
    val = yyjson_arr_get(arr, 2);
    yy_assert(yyjson_get_int(val) == 3);
    val = yyjson_arr_get(arr, 3);
    yy_assert(val == NULL);
    
    val = yyjson_arr_get_first(arr);
    yy_assert(yyjson_get_int(val) == 1);
    val = yyjson_arr_get_last(arr);
    yy_assert(yyjson_get_int(val) == 3);
    
    // iter
    idx = 0;
    yyjson_arr_iter_init(arr, &iter);
    yy_assert(yyjson_arr_iter_has_next(&iter) == true);
    while ((val = yyjson_arr_iter_next(&iter))) {
        idx++;
        yy_assert(yyjson_get_int(val) == (i64)idx);
        yy_assert(yyjson_arr_iter_has_next(&iter) == idx < 3);
    }
    yy_assert(yyjson_arr_iter_has_next(&iter) == false);
    yy_assert(idx == 3);
    
    // foreach
    memset(tmp, 0, sizeof(tmp));
    yyjson_arr_foreach(arr, idx, max, val) {
        yy_assert(yyjson_get_int(val) == (i64)idx + 1);
        yy_assert(max == 3);
        yy_assert(tmp[idx] == 0);
        tmp[idx] = idx + 1;
    }
    yy_assert(tmp[0] == 1);
    yy_assert(tmp[1] == 2);
    yy_assert(tmp[2] == 3);
    yy_assert(tmp[3] == 0);
    
    yyjson_doc_free(doc);
    
    
    //---------------------------------------------
    // array (size 3, non-flat)
    
    json = "[1,[null],3]";
    doc = yyjson_read(json, strlen(json), 0);
    arr = yyjson_doc_get_root(doc);
    yy_assert(yyjson_arr_size(arr) == 3);
    
    val = yyjson_arr_get(arr, 0);
    yy_assert(yyjson_get_int(val) == 1);
    val = yyjson_arr_get(arr, 2);
    yy_assert(yyjson_get_int(val) == 3);
    val = yyjson_arr_get(arr, 3);
    yy_assert(val == NULL);
    
    val = yyjson_arr_get_first(arr);
    yy_assert(yyjson_get_int(val) == 1);
    val = yyjson_arr_get_last(arr);
    yy_assert(yyjson_get_int(val) == 3);
    
    //---------------------------------------------
    // iter
    yy_assert(yyjson_arr_iter_init(arr, NULL) == false);
    yy_assert(yyjson_arr_iter_init(NULL, &iter) == false);
    yy_assert(yyjson_arr_iter_init(NULL, NULL) == false);
    
    
    yyjson_doc_free(doc);
}

/// Test json object api
static void test_json_obj_api(void) {
    yyjson_doc *doc;
    yyjson_val *obj, *key, *val;
    const char *json;
    yyjson_obj_iter iter;
    size_t idx, max, tmp[16];
    
    
    //---------------------------------------------
    // object (size 0)
    
    json = "{}";
    doc = yyjson_read(json, strlen(json), 0);
    obj = yyjson_doc_get_root(doc);
    yy_assert(yyjson_is_obj(obj));
    yy_assert(yyjson_obj_size(obj) == 0);
    
    val = yyjson_obj_get(obj, "x");
    yy_assert(val == NULL);
    val = yyjson_obj_get(obj, "");
    yy_assert(val == NULL);
    val = yyjson_obj_get(obj, NULL);
    yy_assert(val == NULL);
    
    // iter
    iter = yyjson_obj_iter_with(obj);
    yy_assert(yyjson_obj_iter_has_next(&iter) == false);
    while ((key = yyjson_obj_iter_next(&iter))) {
        yy_assert(false);
    }
    
    // iter get
    yy_assert(yyjson_obj_iter_init(obj, &iter));
    yy_assert(!yyjson_obj_iter_get(&iter, "x"));
    
    // foreach
    yyjson_obj_foreach(obj, idx, max, key, val) {
        yy_assert(false);
    }

    yyjson_doc_free(doc);
    
    
    //---------------------------------------------
    // object (size 1)
    
    json = "{\"a\":1}";
    doc = yyjson_read(json, strlen(json), 0);
    obj = yyjson_doc_get_root(doc);
    yy_assert(yyjson_is_obj(obj));
    yy_assert(yyjson_obj_size(obj) == 1);
    
    val = yyjson_obj_get(obj, "a");
    yy_assert(yyjson_get_int(val) == 1);
    val = yyjson_obj_get(obj, "x");
    yy_assert(val == NULL);
    val = yyjson_obj_get(obj, "");
    yy_assert(val == NULL);
    val = yyjson_obj_get(obj, NULL);
    yy_assert(val == NULL);
    
    // iter
    memset(tmp, 0, sizeof(tmp));
    yyjson_obj_iter_init(obj, &iter);
    yy_assert(yyjson_obj_iter_has_next(&iter) == true);
    while ((key = yyjson_obj_iter_next(&iter))) {
        val = key + 1;
        if (yyjson_equals_str(key, "a")) {
            yy_assert(yyjson_get_int(val) == 1);
            yy_assert(tmp[0] == 0);
            tmp[0] = 1;
            yy_assert(yyjson_obj_iter_has_next(&iter) == false);
        } else {
            yy_assert(false);
        }
    }
    yy_assert(yyjson_obj_iter_has_next(&iter) == false);
    yy_assert(tmp[0]);
    
    // iter get
    yy_assert(yyjson_obj_iter_init(obj, &iter));
    yy_assert(yyjson_get_int(yyjson_obj_iter_get(&iter, "a")) == 1);
    yy_assert(!yyjson_obj_iter_get(&iter, "x"));
    yy_assert(yyjson_get_int(yyjson_obj_iter_get(&iter, "a")) == 1);
    yy_assert(!yyjson_obj_iter_get(&iter, "x"));
    
    yy_assert(yyjson_obj_iter_init(obj, &iter));
    yy_assert(!yyjson_obj_iter_get(&iter, "x"));
    yy_assert(yyjson_get_int(yyjson_obj_iter_get(&iter, "a")) == 1);
    yy_assert(!yyjson_obj_iter_get(&iter, "x"));
    yy_assert(yyjson_get_int(yyjson_obj_iter_get(&iter, "a")) == 1);
    
    // foreach
    memset(tmp, 0, sizeof(tmp));
    yyjson_obj_foreach(obj, idx, max, key, val) {
        if (yyjson_equals_str(key, "a")) {
            yy_assert(yyjson_get_int(val) == 1);
            yy_assert(tmp[0] == 0);
            tmp[0] = 1;
        } else {
            yy_assert(false);
        }
    }
    yy_assert(tmp[0]);
    
    yyjson_doc_free(doc);
    
    
    //---------------------------------------------
    // object (size 2)
    
    json = "{\"a\":1,\"b\":2}";
    doc = yyjson_read(json, strlen(json), 0);
    obj = yyjson_doc_get_root(doc);
    yy_assert(yyjson_is_obj(obj));
    yy_assert(yyjson_obj_size(obj) == 2);
    
    val = yyjson_obj_get(obj, "a");
    yy_assert(yyjson_get_int(val) == 1);
    val = yyjson_obj_get(obj, "b");
    yy_assert(yyjson_get_int(val) == 2);
    val = yyjson_obj_get(obj, "x");
    yy_assert(val == NULL);
    val = yyjson_obj_get(obj, "");
    yy_assert(val == NULL);
    val = yyjson_obj_get(obj, NULL);
    yy_assert(val == NULL);
    
    // iter
    memset(tmp, 0, sizeof(tmp));
    yyjson_obj_iter_init(obj, &iter);
    yy_assert(yyjson_obj_iter_has_next(&iter) == true);
    while ((key = yyjson_obj_iter_next(&iter))) {
        val = yyjson_obj_iter_get_val(key);
        if (yyjson_equals_str(key, "a")) {
            yy_assert(yyjson_get_int(val) == 1);
            yy_assert(tmp[0] == 0);
            tmp[0] = 1;
            yy_assert(yyjson_obj_iter_has_next(&iter) == true);
        } else if (yyjson_equals_str(key, "b")) {
            yy_assert(yyjson_get_int(val) == 2);
            yy_assert(tmp[1] == 0);
            tmp[1] = 2;
            yy_assert(yyjson_obj_iter_has_next(&iter) == false);
        } else {
            yy_assert(false);
        }
    }
    yy_assert(yyjson_obj_iter_has_next(&iter) == false);
    yy_assert(tmp[0]);
    yy_assert(tmp[1]);
    
    // iter get
    yy_assert(yyjson_obj_iter_init(obj, &iter));
    yy_assert(yyjson_get_int(yyjson_obj_iter_get(&iter, "a")) == 1);
    yy_assert(!yyjson_obj_iter_get(&iter, "x"));
    yy_assert(yyjson_get_int(yyjson_obj_iter_get(&iter, "a")) == 1);
    yy_assert(!yyjson_obj_iter_get(&iter, "x"));
    
    yy_assert(yyjson_obj_iter_init(obj, &iter));
    yy_assert(!yyjson_obj_iter_get(&iter, "x"));
    yy_assert(yyjson_get_int(yyjson_obj_iter_get(&iter, "a")) == 1);
    yy_assert(!yyjson_obj_iter_get(&iter, "x"));
    yy_assert(yyjson_get_int(yyjson_obj_iter_get(&iter, "a")) == 1);
    
    yy_assert(yyjson_obj_iter_init(obj, &iter));
    yy_assert(yyjson_get_int(yyjson_obj_iter_get(&iter, "a")) == 1);
    yy_assert(yyjson_get_int(yyjson_obj_iter_get(&iter, "b")) == 2);
    yy_assert(!yyjson_obj_iter_get(&iter, "x"));
    yy_assert(yyjson_get_int(yyjson_obj_iter_get(&iter, "b")) == 2);
    
    yy_assert(yyjson_obj_iter_init(obj, &iter));
    yy_assert(yyjson_get_int(yyjson_obj_iter_get(&iter, "b")) == 2);
    yy_assert(yyjson_get_int(yyjson_obj_iter_get(&iter, "a")) == 1);
    yy_assert(!yyjson_obj_iter_get(&iter, "x"));
    yy_assert(yyjson_get_int(yyjson_obj_iter_get(&iter, "b")) == 2);
    
    // foreach
    memset(tmp, 0, sizeof(tmp));
    yyjson_obj_foreach(obj, idx, max, key, val) {
        if (yyjson_equals_str(key, "a")) {
            yy_assert(yyjson_get_int(val) == 1);
            yy_assert(tmp[0] == 0);
            tmp[0] = 1;
        } else if (yyjson_equals_str(key, "b")) {
            yy_assert(yyjson_get_int(val) == 2);
            yy_assert(tmp[1] == 0);
            tmp[1] = 2;
        } else {
            yy_assert(false);
        }
    }
    yy_assert(tmp[0]);
    yy_assert(tmp[1]);
    
    yyjson_doc_free(doc);
    
    
    //---------------------------------------------
    // object (size 3)
    
    json = "{\"a\":1,\"b\":2,\"c\":3}";
    doc = yyjson_read(json, strlen(json), 0);
    obj = yyjson_doc_get_root(doc);
    yy_assert(yyjson_is_obj(obj));
    yy_assert(yyjson_obj_size(obj) == 3);
    
    val = yyjson_obj_get(obj, "a");
    yy_assert(yyjson_get_int(val) == 1);
    val = yyjson_obj_get(obj, "b");
    yy_assert(yyjson_get_int(val) == 2);
    val = yyjson_obj_get(obj, "c");
    yy_assert(yyjson_get_int(val) == 3);
    val = yyjson_obj_get(obj, "x");
    yy_assert(val == NULL);
    val = yyjson_obj_get(obj, "");
    yy_assert(val == NULL);
    val = yyjson_obj_get(obj, NULL);
    yy_assert(val == NULL);
    
    // iter
    memset(tmp, 0, sizeof(tmp));
    yyjson_obj_iter_init(obj, &iter);
    yy_assert(yyjson_obj_iter_has_next(&iter) == true);
    while ((key = yyjson_obj_iter_next(&iter))) {
        val = key + 1;
        if (yyjson_equals_str(key, "a")) {
            yy_assert(yyjson_get_int(val) == 1);
            yy_assert(tmp[0] == 0);
            tmp[0] = 1;
            yy_assert(yyjson_obj_iter_has_next(&iter) == true);
        } else if (yyjson_equals_str(key, "b")) {
            yy_assert(yyjson_get_int(val) == 2);
            yy_assert(tmp[1] == 0);
            tmp[1] = 2;
            yy_assert(yyjson_obj_iter_has_next(&iter) == true);
        } else if (yyjson_equals_str(key, "c")) {
            yy_assert(yyjson_get_int(val) == 3);
            yy_assert(tmp[2] == 0);
            tmp[2] = 3;
            yy_assert(yyjson_obj_iter_has_next(&iter) == false);
        } else {
            yy_assert(false);
        }
    }
    yy_assert(yyjson_obj_iter_has_next(&iter) == false);
    yy_assert(tmp[0]);
    yy_assert(tmp[1]);
    yy_assert(tmp[2]);
    
    // iter get
    yy_assert(yyjson_obj_iter_init(obj, &iter));
    yy_assert(yyjson_get_int(yyjson_obj_iter_get(&iter, "a")) == 1);
    yy_assert(!yyjson_obj_iter_get(&iter, "x"));
    yy_assert(yyjson_get_int(yyjson_obj_iter_get(&iter, "a")) == 1);
    yy_assert(!yyjson_obj_iter_get(&iter, "x"));
    
    yy_assert(yyjson_obj_iter_init(obj, &iter));
    yy_assert(!yyjson_obj_iter_get(&iter, "x"));
    yy_assert(yyjson_get_int(yyjson_obj_iter_get(&iter, "a")) == 1);
    yy_assert(!yyjson_obj_iter_get(&iter, "x"));
    yy_assert(yyjson_get_int(yyjson_obj_iter_get(&iter, "a")) == 1);
    
    yy_assert(yyjson_obj_iter_init(obj, &iter));
    yy_assert(yyjson_get_int(yyjson_obj_iter_get(&iter, "a")) == 1);
    yy_assert(yyjson_get_int(yyjson_obj_iter_get(&iter, "b")) == 2);
    yy_assert(!yyjson_obj_iter_get(&iter, "x"));
    yy_assert(yyjson_get_int(yyjson_obj_iter_get(&iter, "b")) == 2);
    
    yy_assert(yyjson_obj_iter_init(obj, &iter));
    yy_assert(yyjson_get_int(yyjson_obj_iter_get(&iter, "b")) == 2);
    yy_assert(yyjson_get_int(yyjson_obj_iter_get(&iter, "a")) == 1);
    yy_assert(!yyjson_obj_iter_get(&iter, "x"));
    yy_assert(yyjson_get_int(yyjson_obj_iter_get(&iter, "b")) == 2);
    
    yy_assert(yyjson_obj_iter_init(obj, &iter));
    yy_assert(yyjson_get_int(yyjson_obj_iter_get(&iter, "c")) == 3);
    yy_assert(yyjson_get_int(yyjson_obj_iter_get(&iter, "b")) == 2);
    yy_assert(!yyjson_obj_iter_get(&iter, "x"));
    yy_assert(yyjson_get_int(yyjson_obj_iter_get(&iter, "b")) == 2);
    
    yy_assert(yyjson_obj_iter_init(obj, &iter));
    yy_assert(yyjson_get_int(yyjson_obj_iter_get(&iter, "a")) == 1);
    yy_assert(yyjson_get_int(yyjson_obj_iter_get(&iter, "b")) == 2);
    yy_assert(yyjson_get_int(yyjson_obj_iter_get(&iter, "c")) == 3);
    yy_assert(yyjson_get_int(yyjson_obj_iter_get(&iter, "a")) == 1);
    yy_assert(yyjson_get_int(yyjson_obj_iter_get(&iter, "b")) == 2);
    yy_assert(yyjson_get_int(yyjson_obj_iter_get(&iter, "c")) == 3);
    
    yy_assert(yyjson_obj_iter_init(obj, &iter));
    yy_assert(yyjson_get_int(yyjson_obj_iter_get(&iter, "c")) == 3);
    yy_assert(yyjson_get_int(yyjson_obj_iter_get(&iter, "b")) == 2);
    yy_assert(yyjson_get_int(yyjson_obj_iter_get(&iter, "a")) == 1);
    yy_assert(yyjson_get_int(yyjson_obj_iter_get(&iter, "c")) == 3);
    yy_assert(yyjson_get_int(yyjson_obj_iter_get(&iter, "b")) == 2);
    yy_assert(yyjson_get_int(yyjson_obj_iter_get(&iter, "a")) == 1);
    
    // foreach
    memset(tmp, 0, sizeof(tmp));
    yyjson_obj_foreach(obj, idx, max, key, val) {
        if (yyjson_equals_str(key, "a")) {
            yy_assert(yyjson_get_int(val) == 1);
            yy_assert(tmp[0] == 0);
            tmp[0] = 1;
        } else if (yyjson_equals_str(key, "b")) {
            yy_assert(yyjson_get_int(val) == 2);
            yy_assert(tmp[1] == 0);
            tmp[1] = 2;
        } else if (yyjson_equals_str(key, "c")) {
            yy_assert(yyjson_get_int(val) == 3);
            yy_assert(tmp[2] == 0);
            tmp[2] = 3;
        } else {
            yy_assert(false);
        }
    }
    yy_assert(tmp[0]);
    yy_assert(tmp[1]);
    yy_assert(tmp[2]);
    
    //---------------------------------------------
    // iter
    yy_assert(yyjson_obj_iter_init(obj, NULL) == false);
    yy_assert(yyjson_obj_iter_init(NULL, &iter) == false);
    yy_assert(yyjson_obj_iter_init(NULL, NULL) == false);
    
    yyjson_doc_free(doc);
}

static void validate_equals(const char *lhs_json, const char *rhs_json, bool equals) {
    yyjson_doc *lhs_doc = yyjson_read(lhs_json, strlen(lhs_json), 0);
    yyjson_doc *rhs_doc = yyjson_read(rhs_json, strlen(rhs_json), 0);
    
    yyjson_val *lhs_val = yyjson_doc_get_root(lhs_doc);
    yyjson_val *rhs_val = yyjson_doc_get_root(rhs_doc);
    
    yy_assert(yyjson_equals(lhs_val, rhs_val) == equals);
    yy_assert(yyjson_equals(rhs_val, lhs_val) == equals);

    yyjson_doc_free(rhs_doc);
    yyjson_doc_free(lhs_doc);
    
    // RAW type
    lhs_doc = yyjson_read(lhs_json, strlen(lhs_json), YYJSON_READ_NUMBER_AS_RAW);
    rhs_doc = yyjson_read(rhs_json, strlen(rhs_json), YYJSON_READ_NUMBER_AS_RAW);
    
    lhs_val = yyjson_doc_get_root(lhs_doc);
    rhs_val = yyjson_doc_get_root(rhs_doc);
    
    yy_assert(yyjson_equals(lhs_val, rhs_val) == equals);
    yy_assert(yyjson_equals(rhs_val, lhs_val) == equals);
    
    yyjson_doc_free(rhs_doc);
    yyjson_doc_free(lhs_doc);
}

static void test_json_equals_api(void) {
    yy_assert(!yyjson_equals(NULL, NULL));
    validate_equals("", "", false);
    validate_equals("", "true", false);
    validate_equals("true", "", false);
    validate_equals("true", "false", false);
    validate_equals("null", "null", true);
    validate_equals("true", "true", true);
    validate_equals("false", "false", true);
    validate_equals("1", "1", true);
    validate_equals("1", "2", false);
    validate_equals("-1", "-1", true);
    validate_equals("-1", "1", false);
    validate_equals("1", "\"hello\"", false);
    validate_equals("\"hello\"", "\"hello\"", true);
    validate_equals("\"hello\"", "\"world\"", false);
    validate_equals("\"\"", "\"\"", true);
    validate_equals("123.456", "123.456", true);
    validate_equals("-123.456", "-123.456", true);
    validate_equals("-123.456", "123.456", false);
    validate_equals("{}", "{}", true);
    validate_equals("[]", "[]", true);
    validate_equals("[]", "{}", false);
    validate_equals("{}", "[]", false);
    validate_equals("[]", "[1]", false);
    validate_equals("[1]", "[1]", true);
    validate_equals("[1]", "[2]", false);
    validate_equals("[1]", "[1, 2]", false);
    validate_equals("{}", "{\"a\":0}", false);
    validate_equals("{\"a\":0}", "{\"a\":0}", true);
    validate_equals("{\"a\":0}", "{\"a\":1}", false);
    validate_equals("{\"a\":0}", "{\"b\":0}", false);
    validate_equals("{\"a\":0}", "{\"a\":0,\"b\":0}", false);
    validate_equals("{\"a\":{\"b\":[1.0, 2.0]}}",
                    "{\"a\":{\"b\":[1.0, 2.0]}}",
                    true);
    validate_equals("{\"a\":{\"b\":[1.0, 2.0]}}",
                    "{\"a\":{\"b\":[1.0, 2]}}",
                    false);
    validate_equals("[1,2,3,4,5,\"test\",123.456,true,false,null]",
                    "[1,2,3,4,5,\"test\",123.456,true,false,null]",
                    true);
    validate_equals("[null,1,2,3,4,5,\"test\",123.456,true,false]",
                    "[1,2,3,4,5,\"test\",123.456,true,false,null]",
                    false);
    validate_equals("{}",
                    "{\"a\":1,\"b\":2,\"c\":3}",
                    false);
    validate_equals("{\"a\":1,\"b\":2,\"c\":3}",
                    "{\"b\":2,\"a\":1,\"c\":3}",
                    true);
    validate_equals("{\"a\":1,\"b\":2,\"c\":3}",
                    "{\"a\":1,\"b\":2,\"c\":3,\"d\":4}",
                    false);
    validate_equals("\
[{\
  \"array\": [1,2,3,4,5,\"test\",123.456,true,false,null,{\"a\":1,\"b\":2,\"c\":3}],\
  \"object\": {\
    \"key1\": 1,\
    \"key2\": 2,\
    \"key3\": true,\
    \"key4\": false,\
    \"key5\": null,\
    \"key6\": [1,2,3,4,5,\"test\",123.456,true,false,null],\
    \"key7\": {\"a\":1,\"b\":2,\"c\":3}\
  }\
}]",
"\
[{\
  \"object\": {\
    \"key5\": null,\
    \"key6\": [1,2,3,4,5,\"test\",123.456,true,false,null],\
    \"key1\": 1,\
    \"key7\": {\"c\":3,\"a\":1,\"b\":2},\
    \"key2\": 2,\
    \"key3\": true,\
    \"key4\": false\
  },\
  \"array\": [1,2,3,4,5,\"test\",123.456,true,false,null,{\"a\":1,\"b\":2,\"c\":3}]\
}]", true);
}

yy_test_case(test_json_val) {
    test_json_val_api();
    test_json_arr_api();
    test_json_obj_api();
    test_json_equals_api();
}

#else
yy_test_case(test_json_val) {}
#endif
