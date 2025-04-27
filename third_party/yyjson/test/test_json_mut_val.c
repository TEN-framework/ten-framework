// This file is used to test the functions related to `yyjson_mut_val`.

#include "yyjson.h"
#include "yy_test_utils.h"


/// Validate value type
static bool validate_mut_val_type(yyjson_mut_val *val,
                                  yyjson_type type,
                                  yyjson_subtype subtype) {
    
    if (yyjson_mut_is_raw(val) != (type == YYJSON_TYPE_RAW &&
                                subtype == YYJSON_SUBTYPE_NONE)) return false;
    if (yyjson_mut_is_null(val) != (type == YYJSON_TYPE_NULL &&
                                subtype == YYJSON_SUBTYPE_NONE)) return false;
    if (yyjson_mut_is_true(val) != (type == YYJSON_TYPE_BOOL &&
                                subtype == YYJSON_SUBTYPE_TRUE)) return false;
    if (yyjson_mut_is_false(val) != (type == YYJSON_TYPE_BOOL &&
                                 subtype == YYJSON_SUBTYPE_FALSE)) return false;
    if (yyjson_mut_is_bool(val) != (type == YYJSON_TYPE_BOOL &&
                                (subtype == YYJSON_SUBTYPE_TRUE ||
                                 subtype == YYJSON_SUBTYPE_FALSE))) return false;
    if (yyjson_mut_is_uint(val) != (type == YYJSON_TYPE_NUM &&
                                subtype == YYJSON_SUBTYPE_UINT)) return false;
    if (yyjson_mut_is_sint(val) != (type == YYJSON_TYPE_NUM &&
                                subtype == YYJSON_SUBTYPE_SINT)) return false;
    if (yyjson_mut_is_int(val) != (type == YYJSON_TYPE_NUM &&
                               (subtype == YYJSON_SUBTYPE_UINT ||
                                subtype == YYJSON_SUBTYPE_SINT))) return false;
    if (yyjson_mut_is_real(val) != (type == YYJSON_TYPE_NUM &&
                                subtype == YYJSON_SUBTYPE_REAL)) return false;
    if (yyjson_mut_is_num(val) != (type == YYJSON_TYPE_NUM &&
                               (subtype == YYJSON_SUBTYPE_UINT ||
                                subtype == YYJSON_SUBTYPE_SINT ||
                                subtype == YYJSON_SUBTYPE_REAL))) return false;
    if (yyjson_mut_is_str(val) != (type == YYJSON_TYPE_STR &&
                               subtype == YYJSON_SUBTYPE_NONE)) return false;
    if (yyjson_mut_is_arr(val) != (type == YYJSON_TYPE_ARR &&
                               subtype == YYJSON_SUBTYPE_NONE)) return false;
    if (yyjson_mut_is_obj(val) != (type == YYJSON_TYPE_OBJ &&
                               subtype == YYJSON_SUBTYPE_NONE)) return false;
    if (yyjson_mut_is_ctn(val) != ((type == YYJSON_TYPE_ARR ||
                                type == YYJSON_TYPE_OBJ) &&
                                subtype == YYJSON_SUBTYPE_NONE)) return false;
    
    if (yyjson_mut_get_type(val) != type) return false;
    if (yyjson_mut_get_subtype(val) != subtype) return false;
    if (yyjson_mut_get_tag(val) != (type | subtype)) return false;
    
    return true;
}


/// Validate creation of mutable string value
static void validate_mut_str(yyjson_mut_doc *doc,
                             const char *str, usize len, bool suc) {
    yyjson_mut_val *val;
    
    val = yyjson_mut_str(doc, str);
    if (suc) {
        yy_assert(validate_mut_val_type(val, YYJSON_TYPE_STR, YYJSON_SUBTYPE_NONE));
        yy_assert(strcmp(yyjson_mut_get_type_desc(val), "string") == 0);
        yy_assert(strcmp(yyjson_mut_get_str(val), str) == 0);
        yy_assert(yyjson_mut_get_len(val) == strlen(str));
        yy_assert(yyjson_mut_equals_str(val, str));
        yy_assert(yyjson_mut_get_str(val) == str);
    } else {
        yy_assert(val == NULL);
    }
    
    val = yyjson_mut_strn(doc, str, len);
    if (suc) {
        yy_assert(validate_mut_val_type(val, YYJSON_TYPE_STR, YYJSON_SUBTYPE_NONE));
        yy_assert(strcmp(yyjson_mut_get_type_desc(val), "string") == 0);
        yy_assert(strcmp(yyjson_mut_get_str(val), str) == 0);
        yy_assert(yyjson_mut_get_len(val) == len);
        yy_assert(yyjson_mut_equals_str(val, str) == (strlen(str) == len));
        yy_assert(yyjson_mut_get_str(val) == str);
    } else {
        yy_assert(val == NULL);
    }
    
    val = yyjson_mut_strcpy(doc, str);
    if (suc) {
        yy_assert(validate_mut_val_type(val, YYJSON_TYPE_STR, YYJSON_SUBTYPE_NONE));
        yy_assert(strcmp(yyjson_mut_get_type_desc(val), "string") == 0);
        yy_assert(strcmp(yyjson_mut_get_str(val), str) == 0);
        yy_assert(yyjson_mut_get_len(val) == strlen(str));
        yy_assert(yyjson_mut_equals_str(val, str));
        yy_assert(yyjson_mut_get_str(val) != str);
    } else {
        yy_assert(val == NULL);
    }
    
    val = yyjson_mut_strncpy(doc, str, len);
    if (suc) {
        yy_assert(validate_mut_val_type(val, YYJSON_TYPE_STR, YYJSON_SUBTYPE_NONE));
        yy_assert(strcmp(yyjson_mut_get_type_desc(val), "string") == 0);
        yy_assert(strcmp(yyjson_mut_get_str(val), str) == 0);
        yy_assert(yyjson_mut_get_len(val) == len);
        yy_assert(yyjson_mut_equals_str(val, str) == (strlen(str) == len));
        yy_assert(yyjson_mut_get_str(val) != str);
    } else {
        yy_assert(val == NULL);
    }
}

static void test_json_mut_val_api(void) {
    yyjson_mut_doc *doc = yyjson_mut_doc_new(NULL);
    yyjson_mut_val *val;

    yy_assert(yyjson_mut_raw(NULL, NULL) == NULL);
    yy_assert(yyjson_mut_raw(NULL, "abc") == NULL);
    yy_assert(yyjson_mut_raw(doc, NULL) == NULL);
    val = yyjson_mut_raw(doc, "abc");
    yy_assert(validate_mut_val_type(val, YYJSON_TYPE_RAW, YYJSON_SUBTYPE_NONE));
    yy_assert(strcmp(yyjson_mut_get_type_desc(val), "raw") == 0);
    yy_assert(strcmp(yyjson_mut_get_raw(val), "abc") == 0);
    
    yy_assert(yyjson_mut_rawcpy(NULL, NULL) == NULL);
    yy_assert(yyjson_mut_rawcpy(NULL, "abc") == NULL);
    yy_assert(yyjson_mut_rawcpy(doc, NULL) == NULL);
    val = yyjson_mut_rawcpy(doc, "abc");
    yy_assert(validate_mut_val_type(val, YYJSON_TYPE_RAW, YYJSON_SUBTYPE_NONE));
    yy_assert(strcmp(yyjson_mut_get_type_desc(val), "raw") == 0);

    yy_assert(yyjson_mut_null(NULL) == NULL);
    val = yyjson_mut_null(doc);
    yy_assert(validate_mut_val_type(val, YYJSON_TYPE_NULL, YYJSON_SUBTYPE_NONE));
    yy_assert(strcmp(yyjson_mut_get_type_desc(val), "null") == 0);

    yy_assert(yyjson_mut_true(NULL) == NULL);
    val = yyjson_mut_true(doc);
    yy_assert(validate_mut_val_type(val, YYJSON_TYPE_BOOL, YYJSON_SUBTYPE_TRUE));
    yy_assert(strcmp(yyjson_mut_get_type_desc(val), "true") == 0);
    yy_assert(yyjson_mut_get_bool(val) == true);

    yy_assert(yyjson_mut_bool(NULL, true) == NULL);
    val = yyjson_mut_bool(doc, true);
    yy_assert(validate_mut_val_type(val, YYJSON_TYPE_BOOL, YYJSON_SUBTYPE_TRUE));
    yy_assert(strcmp(yyjson_mut_get_type_desc(val), "true") == 0);
    yy_assert(yyjson_mut_get_bool(val) == true);

    yy_assert(yyjson_mut_false(NULL) == NULL);
    val = yyjson_mut_false(doc);
    yy_assert(validate_mut_val_type(val, YYJSON_TYPE_BOOL, YYJSON_SUBTYPE_FALSE));
    yy_assert(strcmp(yyjson_mut_get_type_desc(val), "false") == 0);
    yy_assert(yyjson_mut_get_bool(val) == false);

    yy_assert(yyjson_mut_bool(NULL, false) == NULL);
    val = yyjson_mut_bool(doc, false);
    yy_assert(validate_mut_val_type(val, YYJSON_TYPE_BOOL, YYJSON_SUBTYPE_FALSE));
    yy_assert(strcmp(yyjson_mut_get_type_desc(val), "false") == 0);
    yy_assert(yyjson_mut_get_bool(val) == false);

    yy_assert(yyjson_mut_uint(NULL, 123) == NULL);
    val = yyjson_mut_uint(doc, 123);
    yy_assert(validate_mut_val_type(val, YYJSON_TYPE_NUM, YYJSON_SUBTYPE_UINT));
    yy_assert(strcmp(yyjson_mut_get_type_desc(val), "uint") == 0);
    yy_assert(yyjson_mut_get_uint(val) == (u64)123);
    yy_assert(yyjson_mut_get_sint(val) == (i64)123);
    yy_assert(yyjson_mut_get_int(val) == (i64)123);
    yy_assert(yyjson_mut_get_real(val) == (f64)0);
    yy_assert(yyjson_mut_get_num(val) == (f64)123);
    yy_assert(yyjson_mut_get_bool(val) == false);

    yy_assert(yyjson_mut_sint(NULL, -123) == NULL);
    val = yyjson_mut_sint(doc, -123);
    yy_assert(validate_mut_val_type(val, YYJSON_TYPE_NUM, YYJSON_SUBTYPE_SINT));
    yy_assert(strcmp(yyjson_mut_get_type_desc(val), "sint") == 0);
    yy_assert(yyjson_mut_get_uint(val) == (u64)-123);
    yy_assert(yyjson_mut_get_sint(val) == (i64)-123);
    yy_assert(yyjson_mut_get_int(val) == (i64)-123);
    yy_assert(yyjson_mut_get_real(val) == (f64)0);
    yy_assert(yyjson_mut_get_num(val) == (f64)-123);

    yy_assert(yyjson_mut_real(NULL, 123.0) == NULL);
    val = yyjson_mut_real(doc, 123.0);
    yy_assert(validate_mut_val_type(val, YYJSON_TYPE_NUM, YYJSON_SUBTYPE_REAL));
    yy_assert(strcmp(yyjson_mut_get_type_desc(val), "real") == 0);
    yy_assert(yyjson_mut_get_uint(val) == (u64)0);
    yy_assert(yyjson_mut_get_sint(val) == (i64)0);
    yy_assert(yyjson_mut_get_int(val) == (i64)0);
    yy_assert(yyjson_mut_get_real(val) == (f64)123.0);
    yy_assert(yyjson_mut_get_num(val) == (f64)123.0);

    yy_assert(yyjson_mut_arr(NULL) == NULL);
    val = yyjson_mut_arr(doc);
    yy_assert(validate_mut_val_type(val, YYJSON_TYPE_ARR, YYJSON_SUBTYPE_NONE));
    yy_assert(strcmp(yyjson_mut_get_type_desc(val), "array") == 0);

    yy_assert(yyjson_mut_obj(NULL) == NULL);
    val = yyjson_mut_obj(doc);
    yy_assert(validate_mut_val_type(val, YYJSON_TYPE_OBJ, YYJSON_SUBTYPE_NONE));
    yy_assert(strcmp(yyjson_mut_get_type_desc(val), "object") == 0);

    val = NULL;
    yy_assert(validate_mut_val_type(val, YYJSON_TYPE_NONE, YYJSON_SUBTYPE_NONE));

    yy_assert(yyjson_mut_str(NULL, "abc") == NULL);
    yy_assert(yyjson_mut_strn(NULL, "abc", 3) == NULL);
    yy_assert(yyjson_mut_strcpy(NULL, "abc") == NULL);
    yy_assert(yyjson_mut_strncpy(NULL, "abc", 3) == NULL);
    validate_mut_str(doc, NULL, 0, false);
    validate_mut_str(doc, NULL, 1, false);
    validate_mut_str(doc, "", 0, true);
    validate_mut_str(doc, "abc", 3, true);
    validate_mut_str(doc, "abc\0def", 7, true);
    validate_mut_str(doc, "\0abc", 4, true);
    validate_mut_str(doc, "abc\0", 4, true);
    validate_mut_str(doc, "\0", 1, true);
    validate_mut_str(doc, "\0\0\0", 3, true);
    
    yyjson_mut_doc_free(doc);
}

/// Validate array with int
static void validate_mut_arr(yyjson_mut_val *arr, i64 *cmp,  usize len) {
    yy_assert(yyjson_mut_is_arr(arr));
    yy_assert(yyjson_mut_arr_size(arr) == len);
    yy_assert(yyjson_mut_is_arr(NULL) == false);
    yy_assert(yyjson_mut_arr_size(NULL) == 0);
    
    yyjson_mut_arr_iter iter;
    yyjson_mut_val *val;
    usize idx, max, count;
    int tmp[8];
    
    if (len == 0) {
        val = yyjson_mut_arr_get(arr, 0);
        yy_assert(val == NULL);
        val = yyjson_mut_arr_get_first(arr);
        yy_assert(val == NULL);
        val = yyjson_mut_arr_get_last(arr);
        yy_assert(val == NULL);
        
        iter = yyjson_mut_arr_iter_with(arr);
        yy_assert(yyjson_mut_arr_iter_has_next(&iter) == false);
        while ((val = yyjson_mut_arr_iter_next(&iter))) {
            yy_assert(false);
        }
        yy_assert(yyjson_mut_arr_iter_has_next(&iter) == false);
        
        yyjson_mut_arr_foreach(arr, idx, max, val) {
            yy_assert(false);
        }
        
    } else {
        for (usize i = 0; i < len; i++) {
            val = yyjson_mut_arr_get(arr, i);
            yy_assert(yyjson_mut_get_int(val) == cmp[i]);
        }
        val = yyjson_mut_arr_get(arr, len);
        yy_assert(val == NULL);
        val = yyjson_mut_arr_get_first(arr);
        yy_assert(yyjson_mut_get_int(val) == cmp[0]);
        val = yyjson_mut_arr_get_last(arr);
        yy_assert(yyjson_mut_get_int(val) == cmp[len - 1]);
        
        count = 0;
        memset(tmp, 0, sizeof(tmp));
        yyjson_mut_arr_iter_init(arr, &iter);
        yy_assert(yyjson_mut_arr_iter_has_next(&iter) == true);
        while ((val = yyjson_mut_arr_iter_next(&iter))) {
            yy_assert(yyjson_mut_get_int(val) == cmp[count]);
            yy_assert(tmp[count] == 0);
            tmp[count] = 1;
            count++;
            yy_assert(yyjson_mut_arr_iter_has_next(&iter) == count < len);
        }
        yy_assert(yyjson_mut_arr_iter_has_next(&iter) == false);
        yy_assert(count == len);
        
        count = 0;
        memset(tmp, 0, sizeof(tmp));
        yyjson_mut_arr_foreach(arr, idx, max, val) {
            yy_assert(yyjson_mut_get_int(val) == cmp[count]);
            yy_assert(tmp[count] == 0);
            yy_assert(count == idx);
            yy_assert(max == len);
            tmp[count] = 1;
            count++;
        }
        yy_assert(count == len);
    }
}

static void test_json_mut_arr_api(void) {
    yyjson_mut_doc *doc = yyjson_mut_doc_new(NULL);
    yyjson_mut_val *arr, *val, *num1, *num2, *num3, *num4, *num5, *num6;
    yyjson_mut_arr_iter iter;
    i64 cmp[8];
    
    num1 = yyjson_mut_int(doc, 1);
    num2 = yyjson_mut_int(doc, 2);
    num3 = yyjson_mut_int(doc, 3);
    num4 = yyjson_mut_int(doc, 4);
    num5 = yyjson_mut_int(doc, 5);
    num6 = yyjson_mut_int(doc, 6);
    arr = yyjson_mut_arr(doc);
    
    
    //---------------------------------------------
    // append()
    
    yy_assert(yyjson_mut_arr_append(NULL, num1) == false);
    
    cmp[0] = 1;
    cmp[1] = 2;
    cmp[2] = 3;
    
    validate_mut_arr(arr, cmp, 0);
    
    yy_assert(yyjson_mut_arr_append(arr, num1));
    validate_mut_arr(arr, cmp, 1);
    
    yy_assert(yyjson_mut_arr_append(arr, num2));
    validate_mut_arr(arr, cmp, 2);
    
    yy_assert(yyjson_mut_arr_append(arr, num3));
    validate_mut_arr(arr, cmp, 3);
    
    yyjson_mut_arr_clear(arr);
    validate_mut_arr(arr, cmp, 0);
    
    
    //---------------------------------------------
    // prepend()
    
    yy_assert(yyjson_mut_arr_prepend(NULL, num1) == false);
    
    cmp[0] = 1;
    yy_assert(yyjson_mut_arr_prepend(arr, num1));
    validate_mut_arr(arr, cmp, 1);
    
    cmp[0] = 2;
    cmp[1] = 1;
    yy_assert(yyjson_mut_arr_prepend(arr, num2));
    validate_mut_arr(arr, cmp, 2);
    
    cmp[0] = 3;
    cmp[1] = 2;
    cmp[2] = 1;
    yy_assert(yyjson_mut_arr_prepend(arr, num3));
    validate_mut_arr(arr, cmp, 3);
    
    yyjson_mut_arr_clear(arr);
    
    
    //---------------------------------------------
    // rotate(idx)
    
    yy_assert(!yyjson_mut_arr_rotate(arr, 0));
    
    cmp[0] = 2;
    cmp[1] = 1;
    
    yy_assert(yyjson_mut_arr_append(arr, num1));
    yy_assert(yyjson_mut_arr_append(arr, num2));
    
    yy_assert(yyjson_mut_arr_rotate(arr, 1));
    validate_mut_arr(arr, cmp, 2);
    
    yy_assert(yyjson_mut_arr_rotate(arr, 0));
    yy_assert(!yyjson_mut_arr_rotate(arr, 2));
    
    validate_mut_arr(arr, cmp, 2);
    yyjson_mut_arr_clear(arr);
    
    cmp[0] = 3;
    cmp[1] = 4;
    cmp[2] = 1;
    cmp[3] = 2;
    
    yy_assert(yyjson_mut_arr_append(arr, num1));
    yy_assert(yyjson_mut_arr_append(arr, num2));
    yy_assert(yyjson_mut_arr_append(arr, num3));
    yy_assert(yyjson_mut_arr_append(arr, num4));
    
    yy_assert(yyjson_mut_arr_rotate(arr, 2));
    validate_mut_arr(arr, cmp, 4);
    
    yy_assert(yyjson_mut_arr_rotate(arr, 0));
    yy_assert(yyjson_mut_arr_rotate(arr, 1));
    yy_assert(yyjson_mut_arr_rotate(arr, 3));
    yy_assert(!yyjson_mut_arr_rotate(arr, 4));
    
    validate_mut_arr(arr, cmp, 4);
    yyjson_mut_arr_clear(arr);
    
    
    //---------------------------------------------
    // insert(idx)
    
    yy_assert(yyjson_mut_arr_insert(NULL, num1, 0) == false);
    
    cmp[0] = 1;
    yy_assert(yyjson_mut_arr_insert(arr, num1, 0));
    validate_mut_arr(arr, cmp, 1);
    
    cmp[0] = 1;
    cmp[1] = 2;
    yy_assert(yyjson_mut_arr_insert(arr, num2, 1));
    validate_mut_arr(arr, cmp, 2);
    
    cmp[0] = 1;
    cmp[1] = 3;
    cmp[2] = 2;
    yy_assert(yyjson_mut_arr_insert(arr, num3, 1));
    validate_mut_arr(arr, cmp, 3);
    
    yyjson_mut_arr_clear(arr);
    
    
    //---------------------------------------------
    // replace(first)
    
    yy_assert(yyjson_mut_arr_replace(NULL, 0, num1) == NULL);
    
    val = yyjson_mut_arr_replace(arr, 0, num1);
    yy_assert(val == NULL);
    validate_mut_arr(arr, cmp, 0);
    
    cmp[0] = 4;
    yyjson_mut_arr_append(arr, num1);
    val = yyjson_mut_arr_replace(arr, 0, num4);
    yy_assert(val == num1);
    validate_mut_arr(arr, cmp, 1);
    yyjson_mut_arr_clear(arr);
    
    cmp[0] = 4;
    cmp[1] = 2;
    yyjson_mut_arr_append(arr, num1);
    yyjson_mut_arr_append(arr, num2);
    val = yyjson_mut_arr_replace(arr, 0, num4);
    yy_assert(val == num1);
    validate_mut_arr(arr, cmp, 2);
    yyjson_mut_arr_clear(arr);
    
    cmp[0] = 4;
    cmp[1] = 2;
    cmp[2] = 3;
    yyjson_mut_arr_append(arr, num1);
    yyjson_mut_arr_append(arr, num2);
    yyjson_mut_arr_append(arr, num3);
    val = yyjson_mut_arr_replace(arr, 0, num4);
    yy_assert(val == num1);
    validate_mut_arr(arr, cmp, 3);
    val = yyjson_mut_arr_replace(arr, 0, NULL);
    yy_assert(val == NULL);
    validate_mut_arr(arr, cmp, 3);
    val = yyjson_mut_arr_replace(arr, 3, num5);
    yy_assert(val == NULL);
    validate_mut_arr(arr, cmp, 3);
    yyjson_mut_arr_clear(arr);
    
    
    //---------------------------------------------
    // replace(last)
    
    cmp[0] = 1;
    cmp[1] = 4;
    yyjson_mut_arr_append(arr, num1);
    yyjson_mut_arr_append(arr, num2);
    val = yyjson_mut_arr_replace(arr, 1, num4);
    yy_assert(val == num2);
    validate_mut_arr(arr, cmp, 2);
    yyjson_mut_arr_clear(arr);
    
    cmp[0] = 1;
    cmp[1] = 2;
    cmp[2] = 4;
    yyjson_mut_arr_append(arr, num1);
    yyjson_mut_arr_append(arr, num2);
    yyjson_mut_arr_append(arr, num3);
    val = yyjson_mut_arr_replace(arr, 2, num4);
    yy_assert(val == num3);
    validate_mut_arr(arr, cmp, 3);
    yyjson_mut_arr_clear(arr);
    
    
    //---------------------------------------------
    // replace(mid)
    
    cmp[0] = 1;
    cmp[1] = 4;
    cmp[2] = 3;
    yyjson_mut_arr_append(arr, num1);
    yyjson_mut_arr_append(arr, num2);
    yyjson_mut_arr_append(arr, num3);
    val = yyjson_mut_arr_replace(arr, 1, num4);
    yy_assert(val == num2);
    validate_mut_arr(arr, cmp, 3);
    yyjson_mut_arr_clear(arr);
    
    
    //---------------------------------------------
    // remove_last()
    
    cmp[0] = 1;
    cmp[1] = 2;
    cmp[2] = 3;
    yyjson_mut_arr_append(arr, num1);
    yyjson_mut_arr_append(arr, num2);
    yyjson_mut_arr_append(arr, num3);
    
    yy_assert(yyjson_mut_arr_remove_last(arr) == num3);
    validate_mut_arr(arr, cmp, 2);
    
    yy_assert(yyjson_mut_arr_remove_last(arr) == num2);
    validate_mut_arr(arr, cmp, 1);
    
    yy_assert(yyjson_mut_arr_remove_last(arr) == num1);
    validate_mut_arr(arr, cmp, 0);
    
    yy_assert(yyjson_mut_arr_remove_last(arr) == NULL);
    validate_mut_arr(arr, cmp, 0);
    
    yyjson_mut_arr_clear(arr);
    
    
    //---------------------------------------------
    // remove_first()
    
    yyjson_mut_arr_append(arr, num1);
    yyjson_mut_arr_append(arr, num2);
    yyjson_mut_arr_append(arr, num3);
    
    cmp[0] = 2;
    cmp[1] = 3;
    yy_assert(yyjson_mut_arr_remove_first(arr) == num1);
    validate_mut_arr(arr, cmp, 2);
    
    cmp[0] = 3;
    yy_assert(yyjson_mut_arr_remove_first(arr) == num2);
    validate_mut_arr(arr, cmp, 1);
    
    yy_assert(yyjson_mut_arr_remove_first(arr) == num3);
    validate_mut_arr(arr, cmp, 0);
    
    yy_assert(yyjson_mut_arr_remove_first(arr) == NULL);
    validate_mut_arr(arr, cmp, 0);
    
    yyjson_mut_arr_clear(arr);
    
    
    //---------------------------------------------
    // remove(first)
    
    cmp[0] = 2;
    cmp[1] = 3;
    yyjson_mut_arr_append(arr, num1);
    yyjson_mut_arr_append(arr, num2);
    yyjson_mut_arr_append(arr, num3);
    
    yy_assert(yyjson_mut_arr_remove(arr, 0) == num1);
    validate_mut_arr(arr, cmp, 2);
    yyjson_mut_arr_clear(arr);
    
    
    //---------------------------------------------
    // remove(mid)
    
    cmp[0] = 1;
    cmp[1] = 3;
    yyjson_mut_arr_append(arr, num1);
    yyjson_mut_arr_append(arr, num2);
    yyjson_mut_arr_append(arr, num3);
    
    yy_assert(yyjson_mut_arr_remove(arr, 1) == num2);
    validate_mut_arr(arr, cmp, 2);
    yyjson_mut_arr_clear(arr);
    
    
    //---------------------------------------------
    // remove(last)
    
    cmp[0] = 1;
    cmp[1] = 2;
    yyjson_mut_arr_append(arr, num1);
    yyjson_mut_arr_append(arr, num2);
    yyjson_mut_arr_append(arr, num3);
    
    yy_assert(yyjson_mut_arr_remove(arr, 2) == num3);
    validate_mut_arr(arr, cmp, 2);
    
    yy_assert(yyjson_mut_arr_remove(arr, 2) == NULL);
    validate_mut_arr(arr, cmp, 2);
    
    yy_assert(yyjson_mut_arr_remove(arr, 1) == num2);
    validate_mut_arr(arr, cmp, 1);
    
    yy_assert(yyjson_mut_arr_remove(arr, 0) == num1);
    validate_mut_arr(arr, cmp, 0);
    
    yy_assert(yyjson_mut_arr_remove(arr, 0) == NULL);
    validate_mut_arr(arr, cmp, 0);
    
    yyjson_mut_arr_clear(arr);
    
    
    //---------------------------------------------
    // remove_range
    
    yy_assert(!yyjson_mut_arr_remove_range(NULL, 0, 0));
    yy_assert(yyjson_mut_arr_remove_range(arr, 0, 0));
    yy_assert(!yyjson_mut_arr_remove_range(arr, 1, 0));
    yy_assert(!yyjson_mut_arr_remove_range(arr, 0, 1));
    validate_mut_arr(arr, cmp, 0);
    
    cmp[0] = 1;
    cmp[1] = 2;
    cmp[2] = 3;
    cmp[3] = 4;
    yyjson_mut_arr_append(arr, num1);
    yyjson_mut_arr_append(arr, num2);
    yyjson_mut_arr_append(arr, num3);
    yyjson_mut_arr_append(arr, num4);
    validate_mut_arr(arr, cmp, 4);
    
    cmp[0] = 1;
    cmp[1] = 4;
    yy_assert(yyjson_mut_arr_remove_range(arr, 1, 2));
    validate_mut_arr(arr, cmp, 2);
    
    yy_assert(!yyjson_mut_arr_remove_range(arr, 1, 2));
    validate_mut_arr(arr, cmp, 2);
    
    yy_assert(yyjson_mut_arr_remove_range(arr, 0, 2));
    validate_mut_arr(arr, cmp, 0);
    
    yyjson_mut_arr_clear(arr);
    
    
    //---------------------------------------------
    // iterator
    yy_assert(yyjson_mut_arr_iter_init(arr, NULL) == false);
    yy_assert(yyjson_mut_arr_iter_init(NULL, &iter) == false);
    yy_assert(yyjson_mut_arr_iter_init(NULL, NULL) == false);
    yy_assert(yyjson_mut_arr_iter_remove(NULL) == NULL);
    
    
    //---------------------------------------------
    // iterator with remove(last)
    
    yyjson_mut_arr_append(arr, num1);
    yyjson_mut_arr_append(arr, num2);
    yyjson_mut_arr_append(arr, num3);
    
    cmp[0] = 1;
    cmp[1] = 2;
    cmp[2] = 3;
    yyjson_mut_arr_iter_init(arr, &iter);
    while ((val = yyjson_mut_arr_iter_next(&iter))) {
        if (yyjson_mut_get_int(val) == 3) {
            yyjson_mut_arr_iter_remove(&iter);
        }
    }
    validate_mut_arr(arr, cmp, 2);
    
    yyjson_mut_arr_iter_init(arr, &iter);
    while ((val = yyjson_mut_arr_iter_next(&iter))) {
        if (yyjson_mut_get_int(val) == 2) {
            yyjson_mut_arr_iter_remove(&iter);
        }
    }
    validate_mut_arr(arr, cmp, 1);
    
    yyjson_mut_arr_iter_init(arr, &iter);
    while ((val = yyjson_mut_arr_iter_next(&iter))) {
        if (yyjson_mut_get_int(val) == 1) {
            yyjson_mut_arr_iter_remove(&iter);
        }
    }
    validate_mut_arr(arr, cmp, 0);
    
    yyjson_mut_arr_iter_init(arr, &iter);
    while ((val = yyjson_mut_arr_iter_next(&iter))) {
        yy_assert(false);
    }
    
    yyjson_mut_arr_clear(arr);
    
    
    //---------------------------------------------
    // iterator with remove(first)
    
    yyjson_mut_arr_append(arr, num1);
    yyjson_mut_arr_append(arr, num2);
    yyjson_mut_arr_append(arr, num3);
    
    cmp[0] = 2;
    cmp[1] = 3;
    yyjson_mut_arr_iter_init(arr, &iter);
    while ((val = yyjson_mut_arr_iter_next(&iter))) {
        if (yyjson_mut_get_int(val) == 1) {
            yyjson_mut_arr_iter_remove(&iter);
        }
    }
    validate_mut_arr(arr, cmp, 2);
    
    cmp[0] = 3;
    yyjson_mut_arr_iter_init(arr, &iter);
    while ((val = yyjson_mut_arr_iter_next(&iter))) {
        if (yyjson_mut_get_int(val) == 2) {
            yyjson_mut_arr_iter_remove(&iter);
        }
    }
    validate_mut_arr(arr, cmp, 1);
    
    yyjson_mut_arr_iter_init(arr, &iter);
    while ((val = yyjson_mut_arr_iter_next(&iter))) {
        if (yyjson_mut_get_int(val) == 3) {
            yyjson_mut_arr_iter_remove(&iter);
        }
    }
    validate_mut_arr(arr, cmp, 0);
    
    yyjson_mut_arr_iter_init(arr, &iter);
    while ((val = yyjson_mut_arr_iter_next(&iter))) {
        yy_assert(false);
    }
    
    yyjson_mut_arr_clear(arr);
    
    
    //---------------------------------------------
    // iterator with remove(mid)
    
    yyjson_mut_arr_append(arr, num1);
    yyjson_mut_arr_append(arr, num2);
    yyjson_mut_arr_append(arr, num3);
    
    cmp[0] = 1;
    cmp[1] = 3;
    yyjson_mut_arr_iter_init(arr, &iter);
    while ((val = yyjson_mut_arr_iter_next(&iter))) {
        if (yyjson_mut_get_int(val) == 2) {
            yyjson_mut_arr_iter_remove(&iter);
        }
    }
    validate_mut_arr(arr, cmp, 2);
    
    yyjson_mut_arr_clear(arr);
    
    
    //---------------------------------------------
    // iterator with remove(all)
    
    yyjson_mut_arr_append(arr, num1);
    yyjson_mut_arr_append(arr, num2);
    yyjson_mut_arr_append(arr, num3);
    
    yyjson_mut_arr_iter_init(arr, &iter);
    while ((val = yyjson_mut_arr_iter_next(&iter))) {
        yyjson_mut_arr_iter_remove(&iter);
    }
    validate_mut_arr(arr, cmp, 0);
    
    yyjson_mut_arr_clear(arr);
    
    
    //---------------------------------------------
    // array add()
    val = yyjson_mut_str(doc, "abc");
    yy_assert(!yyjson_mut_arr_add_val(NULL, NULL));
    yy_assert(!yyjson_mut_arr_add_val(arr, NULL));
    yy_assert(!yyjson_mut_arr_add_val(NULL, val));
    yy_assert(yyjson_mut_arr_add_val(arr, val));
    yy_assert(yyjson_mut_arr_get_last(arr) == val);
    
    yy_assert(!yyjson_mut_arr_add_null(NULL, arr));
    yy_assert(!yyjson_mut_arr_add_null(doc, NULL));
    yy_assert(yyjson_mut_arr_add_null(doc, arr));
    val = yyjson_mut_arr_get_last(arr);
    yy_assert(yyjson_mut_is_null(val));
    
    yy_assert(!yyjson_mut_arr_add_true(NULL, arr));
    yy_assert(!yyjson_mut_arr_add_true(doc, NULL));
    yy_assert(yyjson_mut_arr_add_true(doc, arr));
    val = yyjson_mut_arr_get_last(arr);
    yy_assert(yyjson_mut_is_true(val));
    
    yy_assert(!yyjson_mut_arr_add_false(NULL, arr));
    yy_assert(!yyjson_mut_arr_add_false(doc, NULL));
    yy_assert(yyjson_mut_arr_add_false(doc, arr));
    val = yyjson_mut_arr_get_last(arr);
    yy_assert(yyjson_mut_is_false(val));
    
    yy_assert(!yyjson_mut_arr_add_bool(NULL, arr, true));
    yy_assert(!yyjson_mut_arr_add_bool(doc, NULL, true));
    yy_assert(yyjson_mut_arr_add_bool(doc, arr, true));
    val = yyjson_mut_arr_get_last(arr);
    yy_assert(yyjson_mut_is_true(val));
    
    yy_assert(!yyjson_mut_arr_add_uint(NULL, arr, 12));
    yy_assert(!yyjson_mut_arr_add_uint(doc, NULL, 12));
    yy_assert(yyjson_mut_arr_add_uint(doc, arr, 12));
    val = yyjson_mut_arr_get_last(arr);
    yy_assert(yyjson_mut_get_uint(val) == 12);
    
    yy_assert(!yyjson_mut_arr_add_sint(NULL, arr, -12));
    yy_assert(!yyjson_mut_arr_add_sint(doc, NULL, -12));
    yy_assert(yyjson_mut_arr_add_sint(doc, arr, -12));
    val = yyjson_mut_arr_get_last(arr);
    yy_assert(yyjson_mut_get_sint(val) == -12);
    
    yy_assert(!yyjson_mut_arr_add_int(NULL, arr, -12));
    yy_assert(!yyjson_mut_arr_add_int(doc, NULL, -12));
    yy_assert(yyjson_mut_arr_add_int(doc, arr, -12));
    val = yyjson_mut_arr_get_last(arr);
    yy_assert(yyjson_mut_get_int(val) == -12);
    
    yy_assert(!yyjson_mut_arr_add_real(NULL, arr, -20.0));
    yy_assert(!yyjson_mut_arr_add_real(doc, NULL, -20.0));
    yy_assert(yyjson_mut_arr_add_real(doc, arr, -20.0));
    val = yyjson_mut_arr_get_last(arr);
    yy_assert(yyjson_mut_get_real(val) == -20.0);
    
    yy_assert(!yyjson_mut_arr_add_str(NULL, arr, "abc"));
    yy_assert(!yyjson_mut_arr_add_str(doc, NULL, "abc"));
    yy_assert(!yyjson_mut_arr_add_str(doc, arr, NULL));
    yy_assert(yyjson_mut_arr_add_str(doc, arr, "abc"));
    val = yyjson_mut_arr_get_last(arr);
    yy_assert(yyjson_mut_equals_str(val, "abc"));
    
    yy_assert(!yyjson_mut_arr_add_strn(NULL, arr, "abc\0def", 7));
    yy_assert(!yyjson_mut_arr_add_strn(doc, NULL, "abc\0def", 7));
    yy_assert(yyjson_mut_arr_add_strn(doc, arr, "abc\0def", 7));
    val = yyjson_mut_arr_get_last(arr);
    yy_assert(yyjson_mut_equals_strn(val, "abc\0def", 7));
    
    yy_assert(!yyjson_mut_arr_add_strcpy(NULL, arr, "abc"));
    yy_assert(!yyjson_mut_arr_add_strcpy(doc, NULL, "abc"));
    yy_assert(!yyjson_mut_arr_add_strcpy(doc, arr, NULL));
    yy_assert(yyjson_mut_arr_add_strcpy(doc, arr, "abc"));
    val = yyjson_mut_arr_get_last(arr);
    yy_assert(yyjson_mut_equals_str(val, "abc"));
    
    yy_assert(!yyjson_mut_arr_add_strncpy(NULL, arr, "abc\0def", 7));
    yy_assert(!yyjson_mut_arr_add_strncpy(doc, NULL, "abc\0def", 7));
    yy_assert(yyjson_mut_arr_add_strncpy(doc, arr, "abc\0def", 7));
    val = yyjson_mut_arr_get_last(arr);
    yy_assert(yyjson_mut_equals_strn(val, "abc\0def", 7));
    
    yyjson_mut_arr_clear(arr);
    yy_assert(!yyjson_mut_arr_add_arr(NULL, NULL));
    yy_assert(!yyjson_mut_arr_add_arr(NULL, arr));
    yy_assert(!yyjson_mut_arr_add_arr(doc, NULL));
    val = yyjson_mut_arr_add_arr(doc, arr);
    yy_assert(yyjson_mut_is_arr(val));
    yy_assert(yyjson_mut_arr_get_first(arr) == val);
    yy_assert(yyjson_mut_arr_get_last(arr) == val);
    
    yyjson_mut_arr_clear(arr);
    yy_assert(!yyjson_mut_arr_add_obj(NULL, NULL));
    yy_assert(!yyjson_mut_arr_add_obj(NULL, arr));
    yy_assert(!yyjson_mut_arr_add_obj(doc, NULL));
    val = yyjson_mut_arr_add_obj(doc, arr);
    yy_assert(yyjson_mut_is_obj(val));
    yy_assert(yyjson_mut_arr_get_first(arr) == val);
    yy_assert(yyjson_mut_arr_get_last(arr) == val);
    
    
    //---------------------------------------------
    // array with bool
    {
        usize len = 0;
        yy_assert(yyjson_mut_arr_with_bool(NULL, NULL, 0) == NULL);
        arr = yyjson_mut_arr_with_bool(doc, NULL, len);
        yy_assert(yyjson_mut_is_arr(arr));
        yy_assert(yyjson_mut_arr_size(arr) == len);
    }
    {
        bool vals[] = {true};
        usize len = 0;
        yy_assert(yyjson_mut_arr_with_bool(doc, vals, SIZE_MAX / 2) == NULL);
        arr = yyjson_mut_arr_with_bool(doc, vals, len);
        yy_assert(yyjson_mut_is_arr(arr));
        yy_assert(yyjson_mut_arr_size(arr) == len);
    }
    {
        bool vals[] = {true};
        usize len = sizeof(vals) / sizeof(vals[0]);
        arr = yyjson_mut_arr_with_bool(doc, vals, len);
        yy_assert(yyjson_mut_is_arr(arr));
        yy_assert(yyjson_mut_arr_size(arr) == len);
        val = yyjson_mut_arr_get(arr, 0);
        yy_assert(yyjson_mut_is_true(val));
    }
    {
        bool vals[] = {true, false};
        usize len = sizeof(vals) / sizeof(vals[0]);
        arr = yyjson_mut_arr_with_bool(doc, vals, len);
        yy_assert(yyjson_mut_is_arr(arr));
        yy_assert(yyjson_mut_arr_size(arr) == len);
        val = yyjson_mut_arr_get(arr, 0);
        yy_assert(yyjson_mut_is_true(val));
        val = yyjson_mut_arr_get(arr, 1);
        yy_assert(yyjson_mut_is_false(val));
    }
    {
        bool vals[] = {true, false, true};
        usize len = sizeof(vals) / sizeof(vals[0]);
        arr = yyjson_mut_arr_with_bool(doc, vals, len);
        yy_assert(yyjson_mut_is_arr(arr));
        yy_assert(yyjson_mut_arr_size(arr) == len);
        val = yyjson_mut_arr_get(arr, 0);
        yy_assert(yyjson_mut_is_true(val));
        val = yyjson_mut_arr_get(arr, 1);
        yy_assert(yyjson_mut_is_false(val));
        val = yyjson_mut_arr_get(arr, 2);
        yy_assert(yyjson_mut_is_true(val));
    }
    
    //---------------------------------------------
    // array with sint
    {
        i64 vals[] = {1, -2, 3};
        usize len = sizeof(vals) / sizeof(vals[0]);
        arr = yyjson_mut_arr_with_sint(doc, vals, len);
        yy_assert(yyjson_mut_is_arr(arr));
        yy_assert(yyjson_mut_arr_size(arr) == len);
        val = yyjson_mut_arr_get(arr, 0);
        yy_assert(yyjson_mut_get_sint(val) == 1);
        val = yyjson_mut_arr_get(arr, 1);
        yy_assert(yyjson_mut_get_sint(val) == -2);
        val = yyjson_mut_arr_get(arr, 2);
        yy_assert(yyjson_mut_get_sint(val) == 3);
    }
    {
        i8 vals[] = {1, -2, 3};
        usize len = sizeof(vals) / sizeof(vals[0]);
        arr = yyjson_mut_arr_with_sint8(doc, vals, len);
        yy_assert(yyjson_mut_is_arr(arr));
        yy_assert(yyjson_mut_arr_size(arr) == len);
        val = yyjson_mut_arr_get(arr, 0);
        yy_assert(yyjson_mut_get_sint(val) == 1);
        val = yyjson_mut_arr_get(arr, 1);
        yy_assert(yyjson_mut_get_sint(val) == -2);
        val = yyjson_mut_arr_get(arr, 2);
        yy_assert(yyjson_mut_get_sint(val) == 3);
    }
    {
        i16 vals[] = {1, -2, 3};
        usize len = sizeof(vals) / sizeof(vals[0]);
        arr = yyjson_mut_arr_with_sint16(doc, vals, len);
        yy_assert(yyjson_mut_is_arr(arr));
        yy_assert(yyjson_mut_arr_size(arr) == len);
        val = yyjson_mut_arr_get(arr, 0);
        yy_assert(yyjson_mut_get_sint(val) == 1);
        val = yyjson_mut_arr_get(arr, 1);
        yy_assert(yyjson_mut_get_sint(val) == -2);
        val = yyjson_mut_arr_get(arr, 2);
        yy_assert(yyjson_mut_get_sint(val) == 3);
    }
    {
        i32 vals[] = {1, -2, 3};
        usize len = sizeof(vals) / sizeof(vals[0]);
        arr = yyjson_mut_arr_with_sint32(doc, vals, len);
        yy_assert(yyjson_mut_is_arr(arr));
        yy_assert(yyjson_mut_arr_size(arr) == len);
        val = yyjson_mut_arr_get(arr, 0);
        yy_assert(yyjson_mut_get_sint(val) == 1);
        val = yyjson_mut_arr_get(arr, 1);
        yy_assert(yyjson_mut_get_sint(val) == -2);
        val = yyjson_mut_arr_get(arr, 2);
        yy_assert(yyjson_mut_get_sint(val) == 3);
    }
    {
        i64 vals[] = {1, -2, 3};
        usize len = sizeof(vals) / sizeof(vals[0]);
        arr = yyjson_mut_arr_with_sint64(doc, vals, len);
        yy_assert(yyjson_mut_is_arr(arr));
        yy_assert(yyjson_mut_arr_size(arr) == len);
        val = yyjson_mut_arr_get(arr, 0);
        yy_assert(yyjson_mut_get_sint(val) == 1);
        val = yyjson_mut_arr_get(arr, 1);
        yy_assert(yyjson_mut_get_sint(val) == -2);
        val = yyjson_mut_arr_get(arr, 2);
        yy_assert(yyjson_mut_get_sint(val) == 3);
    }
    
    
    //---------------------------------------------
    // array with uint
    {
        u64 vals[] = {1, 2, 3};
        usize len = sizeof(vals) / sizeof(vals[0]);
        arr = yyjson_mut_arr_with_uint(doc, vals, len);
        yy_assert(yyjson_mut_is_arr(arr));
        yy_assert(yyjson_mut_arr_size(arr) == len);
        val = yyjson_mut_arr_get(arr, 0);
        yy_assert(yyjson_mut_get_uint(val) == 1);
        val = yyjson_mut_arr_get(arr, 1);
        yy_assert(yyjson_mut_get_uint(val) == 2);
        val = yyjson_mut_arr_get(arr, 2);
        yy_assert(yyjson_mut_get_uint(val) == 3);
    }
    {
        u8 vals[] = {1, 2, 3};
        usize len = sizeof(vals) / sizeof(vals[0]);
        arr = yyjson_mut_arr_with_uint8(doc, vals, len);
        yy_assert(yyjson_mut_is_arr(arr));
        yy_assert(yyjson_mut_arr_size(arr) == len);
        val = yyjson_mut_arr_get(arr, 0);
        yy_assert(yyjson_mut_get_uint(val) == 1);
        val = yyjson_mut_arr_get(arr, 1);
        yy_assert(yyjson_mut_get_uint(val) == 2);
        val = yyjson_mut_arr_get(arr, 2);
        yy_assert(yyjson_mut_get_uint(val) == 3);
    }
    {
        u16 vals[] = {1, 2, 3};
        usize len = sizeof(vals) / sizeof(vals[0]);
        arr = yyjson_mut_arr_with_uint16(doc, vals, len);
        yy_assert(yyjson_mut_is_arr(arr));
        yy_assert(yyjson_mut_arr_size(arr) == len);
        val = yyjson_mut_arr_get(arr, 0);
        yy_assert(yyjson_mut_get_uint(val) == 1);
        val = yyjson_mut_arr_get(arr, 1);
        yy_assert(yyjson_mut_get_uint(val) == 2);
        val = yyjson_mut_arr_get(arr, 2);
        yy_assert(yyjson_mut_get_uint(val) == 3);
    }
    {
        u32 vals[] = {1, 2, 3};
        usize len = sizeof(vals) / sizeof(vals[0]);
        arr = yyjson_mut_arr_with_uint32(doc, vals, len);
        yy_assert(yyjson_mut_is_arr(arr));
        yy_assert(yyjson_mut_arr_size(arr) == len);
        val = yyjson_mut_arr_get(arr, 0);
        yy_assert(yyjson_mut_get_uint(val) == 1);
        val = yyjson_mut_arr_get(arr, 1);
        yy_assert(yyjson_mut_get_uint(val) == 2);
        val = yyjson_mut_arr_get(arr, 2);
        yy_assert(yyjson_mut_get_uint(val) == 3);
    }
    {
        u64 vals[] = {1, 2, 3};
        usize len = sizeof(vals) / sizeof(vals[0]);
        arr = yyjson_mut_arr_with_uint64(doc, vals, len);
        yy_assert(yyjson_mut_is_arr(arr));
        yy_assert(yyjson_mut_arr_size(arr) == len);
        val = yyjson_mut_arr_get(arr, 0);
        yy_assert(yyjson_mut_get_uint(val) == 1);
        val = yyjson_mut_arr_get(arr, 1);
        yy_assert(yyjson_mut_get_uint(val) == 2);
        val = yyjson_mut_arr_get(arr, 2);
        yy_assert(yyjson_mut_get_uint(val) == 3);
    }
    
    
    //---------------------------------------------
    // array with real
    {
        f64 vals[] = {1.0, 2.0, 3.0};
        usize len = sizeof(vals) / sizeof(vals[0]);
        arr = yyjson_mut_arr_with_real(doc, vals, len);
        yy_assert(yyjson_mut_is_arr(arr));
        yy_assert(yyjson_mut_arr_size(arr) == len);
        val = yyjson_mut_arr_get(arr, 0);
        yy_assert(yyjson_mut_get_real(val) == 1.0);
        val = yyjson_mut_arr_get(arr, 1);
        yy_assert(yyjson_mut_get_real(val) == 2.0);
        val = yyjson_mut_arr_get(arr, 2);
        yy_assert(yyjson_mut_get_real(val) == 3.0);
    }
    {
        f32 vals[] = {1.0f, 2.0f, 3.0f};
        usize len = sizeof(vals) / sizeof(vals[0]);
        arr = yyjson_mut_arr_with_float(doc, vals, len);
        yy_assert(yyjson_mut_is_arr(arr));
        yy_assert(yyjson_mut_arr_size(arr) == len);
        val = yyjson_mut_arr_get(arr, 0);
        yy_assert(yyjson_mut_get_real(val) == 1.0);
        val = yyjson_mut_arr_get(arr, 1);
        yy_assert(yyjson_mut_get_real(val) == 2.0);
        val = yyjson_mut_arr_get(arr, 2);
        yy_assert(yyjson_mut_get_real(val) == 3.0);
    }
    {
        f64 vals[] = {1.0, 2.0, 3.0};
        usize len = sizeof(vals) / sizeof(vals[0]);
        arr = yyjson_mut_arr_with_double(doc, vals, len);
        yy_assert(yyjson_mut_is_arr(arr));
        yy_assert(yyjson_mut_arr_size(arr) == len);
        val = yyjson_mut_arr_get(arr, 0);
        yy_assert(yyjson_mut_get_real(val) == 1.0);
        val = yyjson_mut_arr_get(arr, 1);
        yy_assert(yyjson_mut_get_real(val) == 2.0);
        val = yyjson_mut_arr_get(arr, 2);
        yy_assert(yyjson_mut_get_real(val) == 3.0);
    }
    
    
    //---------------------------------------------
    // array with str
    {
        const char *vals[] = {"", "a", "bc", "abc\0def"};
        usize lens[] = {0, 1, 2, 7};
        usize len = sizeof(vals) / sizeof(vals[0]);
        
        arr = yyjson_mut_arr_with_str(doc, vals, len);
        yy_assert(yyjson_mut_is_arr(arr));
        yy_assert(yyjson_mut_arr_size(arr) == len);
        val = yyjson_mut_arr_get(arr, 0);
        yy_assert(yyjson_mut_equals_str(val, ""));
        val = yyjson_mut_arr_get(arr, 1);
        yy_assert(yyjson_mut_equals_str(val, "a"));
        val = yyjson_mut_arr_get(arr, 2);
        yy_assert(yyjson_mut_equals_str(val, "bc"));
        val = yyjson_mut_arr_get(arr, 3);
        yy_assert(yyjson_mut_equals_str(val, "abc"));
        yy_assert(yyjson_mut_get_str(val) == vals[3]);
        
        arr = yyjson_mut_arr_with_strn(doc, vals, lens, len);
        yy_assert(yyjson_mut_is_arr(arr));
        yy_assert(yyjson_mut_arr_size(arr) == len);
        val = yyjson_mut_arr_get(arr, 0);
        yy_assert(yyjson_mut_equals_str(val, ""));
        val = yyjson_mut_arr_get(arr, 1);
        yy_assert(yyjson_mut_equals_str(val, "a"));
        val = yyjson_mut_arr_get(arr, 2);
        yy_assert(yyjson_mut_equals_str(val, "bc"));
        val = yyjson_mut_arr_get(arr, 3);
        yy_assert(yyjson_mut_equals_strn(val, "abc\0def", 7));
        yy_assert(yyjson_mut_get_str(val) == vals[3]);
        
        arr = yyjson_mut_arr_with_strcpy(doc, vals, len);
        yy_assert(yyjson_mut_is_arr(arr));
        yy_assert(yyjson_mut_arr_size(arr) == len);
        val = yyjson_mut_arr_get(arr, 0);
        yy_assert(yyjson_mut_equals_str(val, ""));
        val = yyjson_mut_arr_get(arr, 1);
        yy_assert(yyjson_mut_equals_str(val, "a"));
        val = yyjson_mut_arr_get(arr, 2);
        yy_assert(yyjson_mut_equals_str(val, "bc"));
        val = yyjson_mut_arr_get(arr, 3);
        yy_assert(yyjson_mut_equals_str(val, "abc"));
        yy_assert(yyjson_mut_get_str(val) != vals[3]);
        
        arr = yyjson_mut_arr_with_strncpy(doc, vals, lens, len);
        yy_assert(yyjson_mut_is_arr(arr));
        yy_assert(yyjson_mut_arr_size(arr) == len);
        val = yyjson_mut_arr_get(arr, 0);
        yy_assert(yyjson_mut_equals_str(val, ""));
        val = yyjson_mut_arr_get(arr, 1);
        yy_assert(yyjson_mut_equals_str(val, "a"));
        val = yyjson_mut_arr_get(arr, 2);
        yy_assert(yyjson_mut_equals_str(val, "bc"));
        val = yyjson_mut_arr_get(arr, 3);
        yy_assert(yyjson_mut_equals_strn(val, "abc\0def", 7));
        yy_assert(yyjson_mut_get_str(val) != vals[3]);
    }
    
    yy_assert(yyjson_mut_arr_clear(arr));
    yy_assert(!yyjson_mut_arr_clear(NULL));
    
    //---------------------------------------------
    yyjson_mut_doc_free(doc);
}


/// Validate object with int
static void validate_mut_obj(yyjson_mut_val *obj,
                             const char **keys, usize *key_lens,
                             i64 *vals, usize len) {
    yy_assert(yyjson_mut_is_obj(obj));
    yy_assert(yyjson_mut_obj_size(obj) == len);
    yy_assert(yyjson_mut_is_obj(NULL) == false);
    yy_assert(yyjson_mut_obj_size(NULL) == 0);
    
    yyjson_mut_obj_iter iter;
    yyjson_mut_val *key, *val, *first_key;
    usize idx, max, count;
    int tmp[8];
        
    if (len == 0) {
        val = yyjson_mut_obj_get(obj, NULL);
        yy_assert(val == NULL);
        val = yyjson_mut_obj_getn(obj, NULL, 0);
        yy_assert(val == NULL);
        val = yyjson_mut_obj_get(obj, "");
        yy_assert(val == NULL);
        val = yyjson_mut_obj_getn(obj, "", 0);
        yy_assert(val == NULL);
        val = yyjson_mut_obj_get(obj, "a");
        yy_assert(val == NULL);
        val = yyjson_mut_obj_getn(obj, "a", 1);
        yy_assert(val == NULL);
        
        iter = yyjson_mut_obj_iter_with(obj);
        yy_assert(yyjson_mut_obj_iter_has_next(&iter) == false);
        while ((key = yyjson_mut_obj_iter_next(&iter))) {
            yy_assert(false);
        }
        yy_assert(yyjson_mut_obj_iter_has_next(&iter) == false);
        
        yyjson_mut_obj_foreach(obj, idx, max, key, val) {
            yy_assert(false);
        }
        
    } else {
        val = yyjson_mut_obj_get(obj, NULL);
        yy_assert(val == NULL);
        val = yyjson_mut_obj_getn(obj, NULL, 0);
        yy_assert(val == NULL);
        val = yyjson_mut_obj_get(obj, "not_exist");
        yy_assert(val == NULL);
        val = yyjson_mut_obj_getn(obj, "not_exist", 9);
        
        // test get() api
        for (usize i = 0; i < len; i++) {
            const char *str = keys[i];
            usize str_len = key_lens[i];

            i64 first_val = -9999;
            for (usize t = 0; t < len; t++) {
                if (str_len == key_lens[t] && memcmp(str, keys[t], str_len) == 0) {
                    first_val = vals[t];
                    break;
                }
            }
            
            if (strlen(str) == str_len) { // no '\0' inside string
                val = yyjson_mut_obj_get(obj, str);
                yy_assert(yyjson_mut_get_int(val) == first_val);
            }
            val = yyjson_mut_obj_getn(obj, str, str_len);
            yy_assert(yyjson_mut_get_int(val) == first_val);
        }
        
        // test all key-val pairs
        first_key = ((yyjson_mut_val *)obj->uni.ptr)->next->next;
        key = first_key;
        val = key->next;
        for (usize i = 0; i < len; i++) {
            const char *str = keys[i];
            usize str_len = key_lens[i];
            yy_assert(yyjson_mut_equals_strn(key, str, str_len));
            yy_assert(yyjson_mut_get_int(val) == vals[i]);
            key = val->next;
            val = key->next;
        }
        yy_assert(key == first_key);
        
        // test iterator api
        count = 0;
        memset(tmp, 0, sizeof(tmp));
        yyjson_mut_obj_iter_init(obj, &iter);
        yy_assert(yyjson_mut_obj_iter_has_next(&iter) == true);
        while ((key = yyjson_mut_obj_iter_next(&iter))) {
            val = yyjson_mut_obj_iter_get_val(key);
            yy_assert(yyjson_mut_equals_strn(key, keys[count], key_lens[count]));
            yy_assert(yyjson_mut_get_int(val) == vals[count]);
            yy_assert(tmp[count] == 0);
            tmp[count] = 1;
            count++;
            yy_assert(yyjson_mut_obj_iter_has_next(&iter) == count < len);
        }
        yy_assert(yyjson_mut_obj_iter_has_next(&iter) == false);
        yy_assert(count == len);
        
        // test foreach api
        count = 0;
        memset(tmp, 0, sizeof(tmp));
        yyjson_mut_obj_foreach(obj, idx, max, key, val) {
            yy_assert(yyjson_mut_equals_strn(key, keys[count], key_lens[count]));
            yy_assert(yyjson_mut_get_int(val) == vals[count]);
            yy_assert(tmp[count] == 0);
            yy_assert(count == idx);
            yy_assert(max == len);
            tmp[count] = 1;
            count++;
        }
        yy_assert(count == len);
    }
}


static void test_json_mut_obj_api(void) {
    yyjson_mut_doc *doc;
    yyjson_mut_val *obj, *key, *val;
    const char *keys[64];
    usize key_lens[64], idx;
    i64 vals[64];
    const char *str;
    yyjson_mut_obj_iter iter;
    
    
#define set_validate(idx, str, len, val) \
    keys[idx] = str; \
    key_lens[idx] = len; \
    vals[idx] = val;
    
#define new_key_val(idx) \
    key = yyjson_mut_strn(doc, keys[idx], key_lens[idx]); \
    val = yyjson_mut_int(doc, vals[idx]);
    
    
    //---------------------------------------------
    // memory pool
    doc = yyjson_mut_doc_new(NULL);
    
    yy_assert(!yyjson_mut_doc_set_str_pool_size(NULL, 0));
    yy_assert(!yyjson_mut_doc_set_str_pool_size(doc, 0));
    yy_assert(!yyjson_mut_doc_set_str_pool_size(doc, ~(size_t)0));
    
    yy_assert(yyjson_mut_doc_set_str_pool_size(doc, 100));
    yy_assert(doc->str_pool.chunk_size == 100 * sizeof(char) + sizeof(yyjson_str_chunk));
    
    yy_assert(!yyjson_mut_doc_set_val_pool_size(NULL, 0));
    yy_assert(!yyjson_mut_doc_set_val_pool_size(doc, 0));
    yy_assert(!yyjson_mut_doc_set_val_pool_size(doc, ~(size_t)0));
    
    yy_assert(yyjson_mut_doc_set_val_pool_size(doc, 100));
    yy_assert(doc->val_pool.chunk_size == 100 * sizeof(yyjson_mut_val) + sizeof(yyjson_mut_val));
    
    yyjson_mut_doc_free(doc);
    
    
    //---------------------------------------------
    // create
    doc = yyjson_mut_doc_new(NULL);
    obj = yyjson_mut_obj(doc);
    yy_assert(yyjson_mut_is_obj(obj));
    
    
    //---------------------------------------------
    // add()
    
    validate_mut_obj(obj, keys, key_lens, vals, 0);
    
    set_validate(0, "a", 1, 10);
    new_key_val(0);
    
    yy_assert(!yyjson_mut_obj_add(NULL, key, val));
    yy_assert(!yyjson_mut_obj_add(obj, NULL, val));
    yy_assert(!yyjson_mut_obj_add(obj, key, NULL));
    
    yy_assert(yyjson_mut_obj_add(obj, key, val));
    validate_mut_obj(obj, keys, key_lens, vals, 1);
    
    set_validate(1, "xxx", 3, 11);
    new_key_val(1);
    yy_assert(yyjson_mut_obj_add(obj, key, val));
    validate_mut_obj(obj, keys, key_lens, vals, 2);
    
    set_validate(2, "b", 1, 12);
    new_key_val(2);
    yy_assert(yyjson_mut_obj_add(obj, key, val));
    validate_mut_obj(obj, keys, key_lens, vals, 3);
    
    set_validate(3, "xxx", 3, 13);
    new_key_val(3);
    yy_assert(yyjson_mut_obj_add(obj, key, val));
    validate_mut_obj(obj, keys, key_lens, vals, 4);
    
    set_validate(4, "xxx\0xxx", 7, 20);
    new_key_val(4);
    yy_assert(yyjson_mut_obj_add(obj, key, val));
    validate_mut_obj(obj, keys, key_lens, vals, 5);
    
    yy_assert(!yyjson_mut_obj_remove(NULL, key));
    yy_assert(!yyjson_mut_obj_remove(obj, NULL));
    // validate the return val 
    yy_assert(yyjson_mut_equals(yyjson_mut_obj_remove(obj, key), val));
    yy_assert(yyjson_mut_obj_remove(yyjson_mut_obj(doc), key) == NULL);
    
    validate_mut_obj(obj, keys, key_lens, vals, 4);
    yyjson_mut_obj_clear(obj);
    validate_mut_obj(obj, keys, key_lens, vals, 0);
    
    //---------------------------------------------
    // put()
    
    // add(a) -> {a:10}
    set_validate(0, "a", 1, 10);
    new_key_val(0);
    yy_assert(yyjson_mut_obj_add(obj, key, val));
    validate_mut_obj(obj, keys, key_lens, vals, 1);
    
    // replace(a) -> {a:11}
    set_validate(0, "a", 1, 11);
    new_key_val(0);
    yy_assert(!yyjson_mut_obj_put(NULL, key, val));
    yy_assert(!yyjson_mut_obj_put(obj, NULL, val));
    yy_assert(yyjson_mut_obj_put(obj, key, val));
    validate_mut_obj(obj, keys, key_lens, vals, 1);
    
    // add(b) -> {a:11,b:20}
    set_validate(1, "b", 1, 20);
    new_key_val(1);
    yy_assert(yyjson_mut_obj_add(obj, key, val));
    validate_mut_obj(obj, keys, key_lens, vals, 2);
    
    // replace(b) -> {a:11,b:21}
    set_validate(1, "b", 1, 21);
    new_key_val(1);
    yy_assert(yyjson_mut_obj_put(obj, key, val));
    validate_mut_obj(obj, keys, key_lens, vals, 2);
    
    // replace(a) -> {a:30,b:21}
    set_validate(0, "a", 1, 30);
    set_validate(1, "b", 1, 21);
    new_key_val(0);
    yy_assert(yyjson_mut_obj_put(obj, key, val));
    validate_mut_obj(obj, keys, key_lens, vals, 2);
    
    // add(c) -> {a:30,b:21,c:40}
    set_validate(2, "c", 1, 40);
    new_key_val(2);
    yy_assert(yyjson_mut_obj_add(obj, key, val));
    validate_mut_obj(obj, keys, key_lens, vals, 3);
    
    // add(c) -> {a:30,b:21,c:40,c:41}
    set_validate(3, "c", 1, 41);
    new_key_val(3);
    yy_assert(yyjson_mut_obj_add(obj, key, val));
    validate_mut_obj(obj, keys, key_lens, vals, 4);
    
    // replace(duplicated) -> {a:30,b:21,c:42}
    set_validate(2, "c", 1, 42);
    new_key_val(2);
    yy_assert(yyjson_mut_obj_put(obj, key, val));
    validate_mut_obj(obj, keys, key_lens, vals, 3);
    
    // replace -> {a:30,b:21,c:43}
    set_validate(2, "c", 1, 43);
    new_key_val(2);
    yy_assert(!yyjson_mut_obj_replace(obj, key, NULL));
    yy_assert(!yyjson_mut_obj_replace(yyjson_mut_obj(doc), key, val));
    yy_assert(yyjson_mut_obj_replace(obj, key, val));
    validate_mut_obj(obj, keys, key_lens, vals, 3);
    
    // remove {a:30,b:21,c:43,c44} -> {a:30,b:21}
    set_validate(3, "c", 1, 44);
    new_key_val(3);
    yy_assert(yyjson_mut_obj_add(obj, key, val));
    yy_assert(yyjson_mut_obj_remove_key(obj, "c"));
    validate_mut_obj(obj, keys, key_lens, vals, 2);
    yy_assert(!yyjson_mut_obj_remove_key(obj, "c"));
    validate_mut_obj(obj, keys, key_lens, vals, 2);
    yy_assert(!yyjson_mut_obj_remove_key(NULL, "c"));
    
    // remove with len {a:30,b:21,c:43,c44} -> {a:30,b:21}
    set_validate(3, "c", 1, 44);
    new_key_val(3);
    yy_assert(yyjson_mut_obj_add(obj, key, val));
    yy_assert(yyjson_mut_obj_remove_keyn(obj, "cc", 1));
    validate_mut_obj(obj, keys, key_lens, vals, 2);
    yy_assert(!yyjson_mut_obj_remove_keyn(obj, "cc", 1));
    validate_mut_obj(obj, keys, key_lens, vals, 2);
    yy_assert(!yyjson_mut_obj_remove_keyn(NULL, "cc", 1));
    
    // replace(NULL)
    new_key_val(2);
    yy_assert(yyjson_mut_obj_put(obj, key, NULL));
    validate_mut_obj(obj, keys, key_lens, vals, 2);
    new_key_val(1);
    yy_assert(yyjson_mut_obj_put(obj, key, NULL));
    validate_mut_obj(obj, keys, key_lens, vals, 1);
    new_key_val(0);
    yy_assert(yyjson_mut_obj_put(obj, key, NULL));
    validate_mut_obj(obj, keys, key_lens, vals, 0);
    
    yyjson_mut_obj_clear(obj);
    
    
    //---------------------------------------------
    // rotate(idx)

    yy_assert(!yyjson_mut_obj_rotate(obj, 0));
    
    set_validate(0, "c", 1, 30);
    set_validate(1, "d", 1, 40);
    set_validate(2, "a", 1, 10);
    set_validate(3, "b", 1, 20);
    
    new_key_val(1); // d
    yy_assert(yyjson_mut_obj_add(obj, key, val));
    new_key_val(0); // c
    yy_assert(yyjson_mut_obj_add(obj, key, val));
    // {"d":40,"c":30}
    
    yy_assert(yyjson_mut_obj_rotate(obj, 1));
    // {"c":30,"d":40}
    
    validate_mut_obj(obj, keys, key_lens, vals, 2);
    
    yy_assert(yyjson_mut_obj_rotate(obj, 0));
    yy_assert(!yyjson_mut_obj_rotate(obj, 2));
    
    validate_mut_obj(obj, keys, key_lens, vals, 2);
    yyjson_mut_obj_clear(obj);
    
    new_key_val(2); // a
    yy_assert(yyjson_mut_obj_add(obj, key, val));
    new_key_val(3); // b
    yy_assert(yyjson_mut_obj_add(obj, key, val));
    new_key_val(0); // c
    yy_assert(yyjson_mut_obj_add(obj, key, val));
    new_key_val(1); // d
    yy_assert(yyjson_mut_obj_add(obj, key, val));
    // {"a":10,"b":20,"c":30,"d":40}
    
    yy_assert(yyjson_mut_obj_rotate(obj, 2));
    // {"c":30,"d":40,"a":10,"b":20}
    
    validate_mut_obj(obj, keys, key_lens, vals, 4);
    
    yy_assert(yyjson_mut_obj_rotate(obj, 0));
    yy_assert(yyjson_mut_obj_rotate(obj, 1));
    yy_assert(yyjson_mut_obj_rotate(obj, 3));
    yy_assert(!yyjson_mut_obj_rotate(obj, 4));
    
    validate_mut_obj(obj, keys, key_lens, vals, 4);
    yyjson_mut_obj_clear(obj);
    
    
    //---------------------------------------------
    // insert(idx)
    
    set_validate(0, "b", 1, 20); // insert at 0
    set_validate(1, "d", 1, 40); // insert at 1
    set_validate(2, "a", 1, 10); // insert at 0
    set_validate(3, "e", 1, 50); // insert at 3
    set_validate(4, "c", 1, 30); // insert at 2
    set_validate(5, "g", 1, 70); // insert at 5
    set_validate(6, "f", 1, 60); // insert at 5
    
    new_key_val(2); // a
    yy_assert(!yyjson_mut_obj_insert(obj, key, val, 1));
    yy_assert(yyjson_mut_obj_insert(obj, key, val, 0));
    yy_assert(yyjson_mut_obj_size(obj) == 1);
    val = yyjson_mut_obj_get(obj, "a");
    yy_assert(yyjson_mut_get_int(val) == 10);
    // {"a":10}
    
    new_key_val(0); // b
    yy_assert(!yyjson_mut_obj_insert(obj, key, val, 2));
    yy_assert(yyjson_mut_obj_insert(obj, key, val, 0));
    yy_assert(yyjson_mut_obj_size(obj) == 2);
    val = yyjson_mut_obj_get(obj, "b");
    yy_assert(yyjson_mut_get_int(val) == 20);
    // {"b":20,"a":10}
    
    new_key_val(4); // c
    yy_assert(!yyjson_mut_obj_insert(obj, key, val, 3));
    yy_assert(yyjson_mut_obj_insert(obj, key, val, 2));
    yy_assert(yyjson_mut_obj_size(obj) == 3);
    val = yyjson_mut_obj_get(obj, "c");
    yy_assert(yyjson_mut_get_int(val) == 30);
    // {"b":20,"a":10,"c":30}
    
    new_key_val(1); // d
    yy_assert(!yyjson_mut_obj_insert(obj, key, val, 4));
    yy_assert(yyjson_mut_obj_insert(obj, key, val, 1));
    yy_assert(yyjson_mut_obj_size(obj) == 4);
    val = yyjson_mut_obj_get(obj, "d");
    yy_assert(yyjson_mut_get_int(val) == 40);
    // {"b":20,"d":40,"a":10,"c":30}
    
    new_key_val(3); // e
    yy_assert(!yyjson_mut_obj_insert(obj, key, val, 5));
    yy_assert(yyjson_mut_obj_insert(obj, key, val, 3));
    yy_assert(yyjson_mut_obj_size(obj) == 5);
    val = yyjson_mut_obj_get(obj, "e");
    yy_assert(yyjson_mut_get_int(val) == 50);
    // {"b":20,"d":40,"a":10,"e":50,"c":30}
    
    new_key_val(6); // f
    yy_assert(!yyjson_mut_obj_insert(obj, key, val, 6));
    yy_assert(yyjson_mut_obj_insert(obj, key, val, 5));
    yy_assert(yyjson_mut_obj_size(obj) == 6);
    val = yyjson_mut_obj_get(obj, "f");
    yy_assert(yyjson_mut_get_int(val) == 60);
    // {"b":20,"d":40,"a":10,"e":50,"c":30,"f":60}
    
    new_key_val(5); // g
    yy_assert(!yyjson_mut_obj_insert(obj, key, val, 7));
    yy_assert(yyjson_mut_obj_insert(obj, key, val, 5));
    yy_assert(yyjson_mut_obj_size(obj) == 7);
    val = yyjson_mut_obj_get(obj, "g");
    yy_assert(yyjson_mut_get_int(val) == 70);
    // {"b":20,"d":40,"a":10,"e":50,"c":30,"g":70,"f":60}
    
    validate_mut_obj(obj, keys, key_lens, vals, 7);
    yyjson_mut_obj_clear(obj);
    
    yy_assert(!yyjson_mut_obj_insert(NULL, key, val, 0));
    yy_assert(!yyjson_mut_obj_insert(obj, NULL, val, 0));
    yy_assert(!yyjson_mut_obj_insert(obj, key, NULL, 0));
    yy_assert(!yyjson_mut_obj_insert(obj, NULL, NULL, 0));
    yy_assert(!yyjson_mut_obj_insert(NULL, NULL, NULL, 0));
    yy_assert(!yyjson_mut_obj_insert(obj, key, val, 1));
    yy_assert(yyjson_mut_obj_size(obj) == 0);
    
    
    //---------------------------------------------
    // add (convenience)
    
    yy_assert(!yyjson_mut_obj_add_null(NULL, obj, "a"));
    yy_assert(!yyjson_mut_obj_add_null(doc, NULL, "a"));
    yy_assert(!yyjson_mut_obj_add_null(doc, obj, NULL));
    yy_assert(yyjson_mut_obj_add_null(doc, obj, "a"));
    val = yyjson_mut_obj_get(obj, "a");
    yy_assert(yyjson_mut_is_null(val));
    
    yy_assert(yyjson_mut_obj_add_true(doc, obj, "b"));
    val = yyjson_mut_obj_get(obj, "b");
    yy_assert(yyjson_mut_is_true(val));
    
    yy_assert(yyjson_mut_obj_add_false(doc, obj, "c"));
    val = yyjson_mut_obj_get(obj, "c");
    yy_assert(yyjson_mut_is_false(val));
    
    yy_assert(yyjson_mut_obj_add_bool(doc, obj, "d", true));
    val = yyjson_mut_obj_get(obj, "d");
    yy_assert(yyjson_mut_is_true(val));
    
    yy_assert(yyjson_mut_obj_add_uint(doc, obj, "e", 123));
    val = yyjson_mut_obj_get(obj, "e");
    yy_assert(yyjson_mut_get_uint(val) == 123);
    
    yy_assert(yyjson_mut_obj_add_sint(doc, obj, "f", -123));
    val = yyjson_mut_obj_get(obj, "f");
    yy_assert(yyjson_mut_get_sint(val) == -123);
    
    yy_assert(yyjson_mut_obj_add_int(doc, obj, "g", -456));
    val = yyjson_mut_obj_get(obj, "g");
    yy_assert(yyjson_mut_get_int(val) == -456);
    
    yy_assert(yyjson_mut_obj_add_real(doc, obj, "h", 789.0));
    val = yyjson_mut_obj_get(obj, "h");
    yy_assert(yyjson_mut_get_real(val) == 789.0);
    
    str = "xxx";
    yy_assert(yyjson_mut_obj_add_str(doc, obj, "aa", str));
    val = yyjson_mut_obj_get(obj, "aa");
    yy_assert(yyjson_mut_get_str(val) == str);
    yy_assert(yyjson_mut_get_len(val) == 3);
    
    str = "xxx\0xxx";
    yy_assert(yyjson_mut_obj_add_strn(doc, obj, "bb", str, 7));
    val = yyjson_mut_obj_get(obj, "bb");
    yy_assert(yyjson_mut_get_str(val) == str);
    yy_assert(yyjson_mut_get_len(val) == 7);
    
    str = "xxx";
    yy_assert(yyjson_mut_obj_add_strcpy(doc, obj, "cc", str));
    val = yyjson_mut_obj_get(obj, "cc");
    yy_assert(yyjson_mut_get_str(val) != str);
    yy_assert(yyjson_mut_get_len(val) == 3);
    yy_assert(yyjson_mut_equals_strn(val, str, 3));
    
    str = "xxx\0xxx";
    yy_assert(yyjson_mut_obj_add_strncpy(doc, obj, "dd", str, 7));
    val = yyjson_mut_obj_get(obj, "dd");
    yy_assert(yyjson_mut_get_str(val) != str);
    yy_assert(yyjson_mut_get_len(val) == 7);
    yy_assert(yyjson_mut_equals_strn(val, str, 7));
    
    val = yyjson_mut_obj_add_arr(doc, obj, "ee");
    yy_assert(yyjson_mut_is_arr(val));
    yy_assert(yyjson_mut_obj_get(obj, "ee") == val);
    yy_assert(yyjson_mut_obj_add_arr(doc, obj, NULL) == NULL);
    
    val = yyjson_mut_obj_add_obj(doc, obj, "ff");
    yy_assert(yyjson_mut_is_obj(val));
    yy_assert(yyjson_mut_obj_get(obj, "ff") == val);
    yy_assert(yyjson_mut_obj_add_obj(doc, obj, NULL) == NULL);
    
    val = yyjson_mut_str(doc, "zzz");
    yy_assert(yyjson_mut_obj_add_val(doc, obj, "yyy", val));
    val = yyjson_mut_obj_get(obj, "yyy");
    yy_assert(yyjson_mut_equals_str(val, "zzz"));
    
    yyjson_mut_obj_clear(obj);
    
    
    //---------------------------------------------
    // remove (convenience)
    
    set_validate(0, "a", 1, 10);
    new_key_val(0);
    yy_assert(yyjson_mut_obj_add(obj, key, val));
    set_validate(1, "b", 1, 11);
    new_key_val(1);
    yy_assert(yyjson_mut_obj_add(obj, key, val));
    set_validate(2, "c", 1, 12);
    new_key_val(2);
    yy_assert(yyjson_mut_obj_add(obj, key, val));
    yy_assert(!yyjson_mut_obj_remove_str(NULL, "b"));
    yy_assert(!yyjson_mut_obj_remove_str(obj, NULL));
    yy_assert(yyjson_mut_obj_remove_str(obj, "b"));
    set_validate(1, "c", 1, 12);
    validate_mut_obj(obj, keys, key_lens, vals, 2);
    yyjson_mut_obj_clear(obj);
    
    set_validate(0, "a", 1, 10);
    new_key_val(0);
    yy_assert(yyjson_mut_obj_add(obj, key, val));
    set_validate(1, "xxx\0xxx", 7, 11);
    new_key_val(1);
    yy_assert(yyjson_mut_obj_add(obj, key, val));
    set_validate(2, "xxx", 3, 12);
    new_key_val(2);
    yy_assert(yyjson_mut_obj_add(obj, key, val));
    yyjson_mut_obj_remove_strn(obj, "xxx\0xxx", 7);
    set_validate(1, "xxx", 3, 12);
    validate_mut_obj(obj, keys, key_lens, vals, 2);
    yyjson_mut_obj_clear(obj);
    
    
    //---------------------------------------------
    // create (convenience)
    {
        const char *keys_str[3] = {"a", "b", "c"};
        const char *vals_str[3] = {"x", "y", "z"};
        obj = yyjson_mut_obj_with_str(NULL, keys_str, vals_str, 3);
        yy_assert(!obj);
        obj = yyjson_mut_obj_with_str(doc, keys_str, vals_str, 3);
        yy_assert(yyjson_mut_is_obj(obj));
        yy_assert(yyjson_mut_obj_size(obj) == 3);
        val = yyjson_mut_obj_get(obj, "a");
        yy_assert(yyjson_mut_equals_str(val, "x"));
        val = yyjson_mut_obj_get(obj, "b");
        yy_assert(yyjson_mut_equals_str(val, "y"));
        val = yyjson_mut_obj_get(obj, "c");
        yy_assert(yyjson_mut_equals_str(val, "z"));
        yyjson_mut_obj_clear(obj);
    }
    {
        const char *pairs[6] = {"a", "x", "b", "y", "c", "z"};
        obj = yyjson_mut_obj_with_kv(NULL, pairs, 3);
        yy_assert(!obj);
        obj = yyjson_mut_obj_with_kv(doc, pairs, 3);
        yy_assert(yyjson_mut_is_obj(obj));
        yy_assert(yyjson_mut_obj_size(obj) == 3);
        val = yyjson_mut_obj_get(obj, "a");
        yy_assert(yyjson_mut_equals_str(val, "x"));
        val = yyjson_mut_obj_get(obj, "b");
        yy_assert(yyjson_mut_equals_str(val, "y"));
        val = yyjson_mut_obj_get(obj, "c");
        yy_assert(yyjson_mut_equals_str(val, "z"));
        yyjson_mut_obj_clear(obj);
    }
    
    
    //---------------------------------------------
    // iterator
    yy_assert(yyjson_mut_obj_iter_init(obj, NULL) == false);
    yy_assert(yyjson_mut_obj_iter_init(NULL, &iter) == false);
    yy_assert(yyjson_mut_obj_iter_init(NULL, NULL) == false);
    
    
    //---------------------------------------------
    // iterator
    
    // obj(1)
    set_validate(0, "a", 1, 10);
    new_key_val(0);
    yy_assert(yyjson_mut_obj_add(obj, key, val));
    validate_mut_obj(obj, keys, key_lens, vals, 1);
    yyjson_mut_obj_iter_init(obj, &iter);
    idx = 0;
    while ((key = yyjson_mut_obj_iter_next(&iter))) {
        val = yyjson_mut_obj_iter_get_val(key);
        if (idx == 0) yy_assert(yyjson_mut_equals_str(key, "a"));
        if (idx == 0) yy_assert(yyjson_mut_get_int(val) == 10);
        idx++;
    }
    yy_assert(idx == 1);
    validate_mut_obj(obj, keys, key_lens, vals, 1);
    
    yy_assert(yyjson_mut_obj_iter_init(obj, &iter));
    yy_assert(yyjson_mut_get_int(yyjson_mut_obj_iter_get(&iter, "a")) == 10);
    yy_assert(!yyjson_mut_obj_iter_get(&iter, "x"));
    yy_assert(yyjson_mut_get_int(yyjson_mut_obj_iter_get(&iter, "a")) == 10);
    yy_assert(!yyjson_mut_obj_iter_get(&iter, "x"));
    
    
    // obj(2)
    set_validate(1, "b", 1, 11);
    new_key_val(1);
    yy_assert(yyjson_mut_obj_add(obj, key, val));
    yyjson_mut_obj_iter_init(obj, &iter);
    idx = 0;
    while ((key = yyjson_mut_obj_iter_next(&iter))) {
        val = yyjson_mut_obj_iter_get_val(key);
        if (idx == 0) yy_assert(yyjson_mut_equals_str(key, "a"));
        if (idx == 0) yy_assert(yyjson_mut_get_int(val) == 10);
        if (idx == 1) yy_assert(yyjson_mut_equals_str(key, "b"));
        if (idx == 1) yy_assert(yyjson_mut_get_int(val) == 11);
        idx++;
    }
    yy_assert(idx == 2);
    validate_mut_obj(obj, keys, key_lens, vals, 2);
    
    yy_assert(yyjson_mut_obj_iter_init(obj, &iter));
    yy_assert(yyjson_mut_get_int(yyjson_mut_obj_iter_get(&iter, "a")) == 10);
    yy_assert(yyjson_mut_get_int(yyjson_mut_obj_iter_get(&iter, "b")) == 11);
    yy_assert(!yyjson_mut_obj_iter_get(&iter, "x"));
    yy_assert(yyjson_mut_obj_iter_init(obj, &iter));
    yy_assert(yyjson_mut_get_int(yyjson_mut_obj_iter_get(&iter, "b")) == 11);
    yy_assert(yyjson_mut_get_int(yyjson_mut_obj_iter_get(&iter, "a")) == 10);
    yy_assert(!yyjson_mut_obj_iter_get(&iter, "x"));
    
    // obj(3)
    set_validate(2, "c", 1, 12);
    new_key_val(2);
    yy_assert(yyjson_mut_obj_add(obj, key, val));
    yyjson_mut_obj_iter_init(obj, &iter);
    idx = 0;
    while ((key = yyjson_mut_obj_iter_next(&iter))) {
        val = yyjson_mut_obj_iter_get_val(key);
        if (idx == 0) yy_assert(yyjson_mut_equals_str(key, "a"));
        if (idx == 0) yy_assert(yyjson_mut_get_int(val) == 10);
        if (idx == 1) yy_assert(yyjson_mut_equals_str(key, "b"));
        if (idx == 1) yy_assert(yyjson_mut_get_int(val) == 11);
        if (idx == 2) yy_assert(yyjson_mut_equals_str(key, "c"));
        if (idx == 2) yy_assert(yyjson_mut_get_int(val) == 12);
        idx++;
    }
    yy_assert(idx == 3);
    validate_mut_obj(obj, keys, key_lens, vals, 3);
    
    yy_assert(yyjson_mut_obj_iter_init(obj, &iter));
    yy_assert(yyjson_mut_get_int(yyjson_mut_obj_iter_get(&iter, "a")) == 10);
    yy_assert(yyjson_mut_get_int(yyjson_mut_obj_iter_get(&iter, "b")) == 11);
    yy_assert(yyjson_mut_get_int(yyjson_mut_obj_iter_get(&iter, "c")) == 12);
    yy_assert(!yyjson_mut_obj_iter_get(&iter, "x"));
    yy_assert(yyjson_mut_obj_iter_init(obj, &iter));
    yy_assert(yyjson_mut_get_int(yyjson_mut_obj_iter_get(&iter, "c")) == 12);
    yy_assert(yyjson_mut_get_int(yyjson_mut_obj_iter_get(&iter, "b")) == 11);
    yy_assert(yyjson_mut_get_int(yyjson_mut_obj_iter_get(&iter, "a")) == 10);
    yy_assert(!yyjson_mut_obj_iter_get(&iter, "x"));
    
    
    yyjson_mut_obj_clear(obj);
    
    
    
    //---------------------------------------------
    // iterator remove, size:1, remove:0
    
    set_validate(0, "a", 1, 10);
    new_key_val(0);
    yy_assert(yyjson_mut_obj_add(obj, key, val));
    validate_mut_obj(obj, keys, key_lens, vals, 1);
    yyjson_mut_obj_iter_init(obj, &iter);
    idx = 0;
    while ((key = yyjson_mut_obj_iter_next(&iter))) {
        val = yyjson_mut_obj_iter_get_val(key);
        if (yyjson_mut_equals_str(key, "a")) yyjson_mut_obj_iter_remove(&iter);
        idx++;
    }
    yy_assert(idx == 1);
    validate_mut_obj(obj, keys, key_lens, vals, 0);
    yyjson_mut_obj_clear(obj);
    
    
    //---------------------------------------------
    // iterator remove, size:2, remove:0
    
    set_validate(0, "a", 1, 10);
    new_key_val(0);
    yy_assert(yyjson_mut_obj_add(obj, key, val));
    set_validate(1, "b", 1, 11);
    new_key_val(1);
    yy_assert(yyjson_mut_obj_add(obj, key, val));
    validate_mut_obj(obj, keys, key_lens, vals, 2);
    yyjson_mut_obj_iter_init(obj, &iter);
    idx = 0;
    while ((key = yyjson_mut_obj_iter_next(&iter))) {
        val = yyjson_mut_obj_iter_get_val(key);
        if (yyjson_mut_equals_str(key, "a")) yyjson_mut_obj_iter_remove(&iter);
        idx++;
    }
    yy_assert(idx == 2);
    set_validate(0, "b", 1, 11);
    validate_mut_obj(obj, keys, key_lens, vals, 1);
    yyjson_mut_obj_clear(obj);
    
    
    //---------------------------------------------
    // iterator remove, size:2, remove:1
    
    set_validate(0, "a", 1, 10);
    new_key_val(0);
    yy_assert(yyjson_mut_obj_add(obj, key, val));
    set_validate(1, "b", 1, 11);
    new_key_val(1);
    yy_assert(yyjson_mut_obj_add(obj, key, val));
    validate_mut_obj(obj, keys, key_lens, vals, 2);
    yyjson_mut_obj_iter_init(obj, &iter);
    idx = 0;
    while ((key = yyjson_mut_obj_iter_next(&iter))) {
        val = yyjson_mut_obj_iter_get_val(key);
        if (yyjson_mut_equals_str(key, "b")) yyjson_mut_obj_iter_remove(&iter);
        idx++;
    }
    yy_assert(idx == 2);
    validate_mut_obj(obj, keys, key_lens, vals, 1);
    yyjson_mut_obj_clear(obj);
    
    
    //---------------------------------------------
    // iterator remove, size:3, remove:1
    
    set_validate(0, "a", 1, 10);
    new_key_val(0);
    yy_assert(yyjson_mut_obj_add(obj, key, val));
    set_validate(1, "b", 1, 11);
    new_key_val(1);
    yy_assert(yyjson_mut_obj_add(obj, key, val));
    set_validate(2, "c", 1, 12);
    new_key_val(2);
    yy_assert(yyjson_mut_obj_add(obj, key, val));
    validate_mut_obj(obj, keys, key_lens, vals, 3);
    yyjson_mut_obj_iter_init(obj, &iter);
    idx = 0;
    while ((key = yyjson_mut_obj_iter_next(&iter))) {
        val = yyjson_mut_obj_iter_get_val(key);
        if (yyjson_mut_equals_str(key, "b")) yyjson_mut_obj_iter_remove(&iter);
        idx++;
    }
    yy_assert(idx == 3);
    set_validate(1, "c", 1, 12);
    validate_mut_obj(obj, keys, key_lens, vals, 2);
    yyjson_mut_obj_clear(obj);
    
    
    //---------------------------------------------
    // iterator remove all
    
    set_validate(0, "a", 1, 10);
    new_key_val(0);
    yy_assert(yyjson_mut_obj_add(obj, key, val));
    set_validate(1, "b", 1, 11);
    new_key_val(1);
    yy_assert(yyjson_mut_obj_add(obj, key, val));
    set_validate(2, "c", 1, 12);
    new_key_val(2);
    yy_assert(yyjson_mut_obj_add(obj, key, val));
    validate_mut_obj(obj, keys, key_lens, vals, 3);
    yyjson_mut_obj_iter_init(obj, &iter);
    idx = 0;
    while ((key = yyjson_mut_obj_iter_next(&iter))) {
        val = yyjson_mut_obj_iter_get_val(key);
        yyjson_mut_obj_iter_remove(&iter);
        idx++;
    }
    yy_assert(idx == 3);
    validate_mut_obj(obj, keys, key_lens, vals, 0);
    yyjson_mut_obj_clear(obj);
    
    yy_assert(!yyjson_mut_obj_iter_remove(NULL));
    
    
    yyjson_mut_obj_clear(NULL);
    yyjson_mut_obj_clear(obj);
    //---------------------------------------------
    
    yyjson_mut_doc_free(doc);
}

#if !YYJSON_DISABLE_READER
static void test_json_mut_doc_api_one(const char *json_str) {
    yyjson_doc *json = yyjson_read(json_str, strlen(json_str), 0);
    yyjson_mut_doc *json_cp = yyjson_doc_mut_copy(json, NULL);
    yyjson_mut_doc *json_mut_cp = yyjson_mut_doc_mut_copy(json_cp, NULL);
    yy_assert(yyjson_mut_equals(json_cp->root, json_mut_cp->root) == true);
    yyjson_doc *idoc_cp = yyjson_mut_doc_imut_copy(json_cp, NULL);
    yy_assert(yyjson_equals(json->root, idoc_cp->root) == true);
    yyjson_doc *ival_cp = yyjson_mut_val_imut_copy(json_cp->root, NULL);
    yy_assert(yyjson_equals(json->root, idoc_cp->root) == true);
    yyjson_doc_free(json);
    yyjson_mut_doc_free(json_cp);
    yyjson_mut_doc_free(json_mut_cp);
    yyjson_doc_free(idoc_cp);
    yyjson_doc_free(ival_cp);
}
#endif

static void test_json_mut_doc_api(void) {
    {
        yyjson_mut_doc_set_root(NULL, NULL);
        yy_assert(yyjson_mut_doc_get_root(NULL) == NULL);
    }
    {
        yyjson_mut_doc *doc = yyjson_mut_doc_new(NULL);
        yy_assert(yyjson_mut_doc_get_root(doc) == NULL);
        yyjson_mut_doc *doc2 = yyjson_mut_doc_mut_copy(doc, NULL);
        yy_assert(doc2 != NULL && doc2->root == NULL);
        
        yyjson_mut_val *val = yyjson_mut_str(doc, "abc");
        yy_assert(yyjson_mut_is_str(val));
        yyjson_mut_doc_set_root(doc, val);
        yy_assert(yyjson_mut_doc_get_root(doc) == val);
        
        yyjson_mut_val *v1 = yyjson_mut_int(doc, 0);
        yyjson_mut_val *v2 = yyjson_mut_int(doc, 0);
        v1->tag = 0;
        v2->tag = 0;
        yy_assert(yyjson_mut_equals(v1, v2) == false);
        
        yyjson_mut_doc_free(doc);
        yyjson_mut_doc_free(doc2);
    }
    
#if !YYJSON_DISABLE_READER
    {
        yyjson_doc *idoc = yyjson_read("1", 1, 0);
        idoc->root = NULL;
        yy_assert(!yyjson_doc_mut_copy(idoc, NULL));
        yyjson_doc_free(idoc);
    }
    test_json_mut_doc_api_one("\"\"");
    test_json_mut_doc_api_one("\"abc\"");
    test_json_mut_doc_api_one("123");
    test_json_mut_doc_api_one("[1,2,3]");
    test_json_mut_doc_api_one("{\"a\":1}");
    test_json_mut_doc_api_one("{\"a\":{\"b\":[-1,2,1.0,2.0,true,false,null]}}");
#endif
#if !YYJSON_DISABLE_READER && !YYJSON_DISABLE_WRITER
    {
        const char *json_src = "{\"a\":1,\"b\":2}";
        const char *json_dst = "{\"c\":1,\"b\":2}";
        yyjson_doc *idoc = yyjson_read(json_src, strlen(json_src), 0);
        yyjson_mut_doc *mdoc = yyjson_doc_mut_copy(idoc, NULL);
        yyjson_mut_val *root = yyjson_mut_doc_get_root(mdoc);
        yyjson_mut_obj_rename_key(mdoc, root, "a", "c");
        char *new_json = yyjson_mut_write(mdoc, 0, NULL);
        yy_assert(strcmp(new_json, json_dst) == 0);
        yyjson_doc_free(idoc);
        yyjson_mut_doc_free(mdoc);
        free(new_json);
    }
#endif
}

static void validate_equals(const char *lhs_json, const char *rhs_json, bool equals) {
#if !YYJSON_DISABLE_READER
    yyjson_doc *lhs_doc = yyjson_read(lhs_json, strlen(lhs_json), 0);
    yyjson_doc *rhs_doc = yyjson_read(rhs_json, strlen(rhs_json), 0);

    yyjson_mut_doc *mut_lhs_doc = yyjson_doc_mut_copy(lhs_doc, NULL);
    yyjson_mut_doc *mut_rhs_doc = yyjson_doc_mut_copy(rhs_doc, NULL);

    yyjson_mut_val *mut_lhs_val = yyjson_mut_doc_get_root(mut_lhs_doc);
    yyjson_mut_val *mut_rhs_val = yyjson_mut_doc_get_root(mut_rhs_doc);
    
    yy_assert(yyjson_mut_equals(mut_lhs_val, mut_rhs_val) == equals);
    yy_assert(yyjson_mut_equals(mut_rhs_val, mut_lhs_val) == equals);

    yyjson_mut_doc_free(mut_rhs_doc);
    yyjson_mut_doc_free(mut_lhs_doc);

    yyjson_doc_free(rhs_doc);
    yyjson_doc_free(lhs_doc);
    
    // RAW type
    lhs_doc = yyjson_read(lhs_json, strlen(lhs_json), YYJSON_READ_NUMBER_AS_RAW);
    rhs_doc = yyjson_read(rhs_json, strlen(rhs_json), YYJSON_READ_NUMBER_AS_RAW);

    mut_lhs_doc = yyjson_doc_mut_copy(lhs_doc, NULL);
    mut_rhs_doc = yyjson_doc_mut_copy(rhs_doc, NULL);

    mut_lhs_val = yyjson_mut_doc_get_root(mut_lhs_doc);
    mut_rhs_val = yyjson_mut_doc_get_root(mut_rhs_doc);
    
    yy_assert(yyjson_mut_equals(mut_lhs_val, mut_rhs_val) == equals);
    yy_assert(yyjson_mut_equals(mut_rhs_val, mut_lhs_val) == equals);

    yyjson_mut_doc_free(mut_rhs_doc);
    yyjson_mut_doc_free(mut_lhs_doc);
    
    yyjson_doc_free(rhs_doc);
    yyjson_doc_free(lhs_doc);
#endif
}

static void test_json_mut_equals_api(void) {
    yy_assert(!yyjson_mut_equals(NULL, NULL));
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

yy_test_case(test_json_mut_val) {
    test_json_mut_val_api();
    test_json_mut_arr_api();
    test_json_mut_obj_api();
    test_json_mut_doc_api();
    test_json_mut_equals_api();
}
