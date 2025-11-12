// This file is used to test the `JSON Patch` functions.

#include "yyjson.h"
#include "yy_test_utils.h"

#if !YYJSON_DISABLE_READER && !YYJSON_DISABLE_WRITER && !YYJSON_DISABLE_UTILS

typedef struct {
    const char *src;
    const char *patch;
    const char *dst;
    yyjson_patch_err err;
} patch_data;

// -----------------------------------------------------------------------------
// assert (str == expected)
static void assert_str_eq(const char *str, const char *exp) {
    if (!str && !exp) return;
    if (str && !exp) yy_assertf(false, "expected NULL, but <%s>", str);
    if (!str && exp) yy_assertf(false, "expected <%s>, but NULL", exp);
    yy_assertf(strcmp(str, exp) == 0, "expected <%s>, but <%s>", exp, str);
}

// assert (str(val) == json)
static void assert_mut_val_eq(yyjson_mut_val *val, const char *json) {
    char *str = yyjson_mut_val_write(val, 0, NULL);
    assert_str_eq(str, json);
    if (str) free(str);
}

// assert (str(val) == json)
static void assert_err_eq(yyjson_patch_err *err, patch_data *data) {
    yy_assert(err->code == data->err.code);
    yy_assert(err->idx == data->err.idx);
    yy_assert(err->ptr.code == data->err.ptr.code);
    if (err->code) {
        yy_assert(err->msg != NULL);
    } else {
        yy_assert(err->msg == NULL);
    }
    if (err->code == YYJSON_PATCH_ERROR_POINTER) {
        yy_assert(err->ptr.code != 0);
        yy_assert(err->ptr.msg != NULL);
    } else {
        yy_assert(err->ptr.code == 0);
        yy_assert(err->ptr.msg == NULL);
    }
}

// -----------------------------------------------------------------------------
// test JSON patch
static void test_patch(patch_data data) {
    yyjson_mut_doc *doc = yyjson_mut_doc_new(NULL);
    yyjson_doc *src_doc = yyjson_read(data.src, data.src ? strlen(data.src) : 0, 0);
    yyjson_doc *pat_doc = yyjson_read(data.patch, data.patch ? strlen(data.patch) : 0, 0);
    yyjson_val *src = yyjson_doc_get_root(src_doc);
    yyjson_val *pat = yyjson_doc_get_root(pat_doc);
    yyjson_mut_val *msrc = yyjson_val_mut_copy(doc, src);
    yyjson_mut_val *mpat = yyjson_val_mut_copy(doc, pat);
    yyjson_mut_val *ret;
    yyjson_patch_err err;
    
    ret = yyjson_patch(doc, src, pat, NULL);
    assert_mut_val_eq(ret, data.dst);
    
    memset(&err, -1, sizeof(err));
    ret = yyjson_patch(doc, src, pat, &err);
    assert_mut_val_eq(ret, data.dst);
    assert_err_eq(&err, &data);
    
    ret = yyjson_mut_patch(doc, msrc, mpat, NULL);
    assert_mut_val_eq(ret, data.dst);
    
    memset(&err, -1, sizeof(err));
    ret = yyjson_mut_patch(doc, msrc, mpat, &err);
    assert_mut_val_eq(ret, data.dst);
    assert_err_eq(&err, &data);
    
    yyjson_mut_doc_free(doc);
    yyjson_doc_free(src_doc);
    yyjson_doc_free(pat_doc);
}

// -----------------------------------------------------------------------------
// test cases from https://www.rfc-editor.org/rfc/rfc6902
static void test_spec(void) {
    // A.1.  Adding an Object Member
    test_patch((patch_data){
        .src = "{\"foo\":\"bar\"}",
        .patch = "["
            "{\"op\":\"add\",\"path\":\"/baz\",\"value\":\"qux\"}"
        "]",
        .dst = "{\"foo\":\"bar\",\"baz\":\"qux\"}",
    });
    
    // A.2.  Adding an Array Element
    test_patch((patch_data){
        .src = "{\"foo\":[\"bar\",\"baz\"]}",
        .patch = "["
            "{\"op\":\"add\",\"path\":\"/foo/1\",\"value\":\"qux\"}"
        "]",
        .dst = "{\"foo\":[\"bar\",\"qux\",\"baz\"]}",
    });
    
    // A.3.  Removing an Object Member
    test_patch((patch_data){
        .src = "{\"foo\":\"bar\",\"baz\":\"qux\"}",
        .patch = "["
            "{\"op\":\"remove\",\"path\":\"/baz\"}"
        "]",
        .dst = "{\"foo\":\"bar\"}",
    });
    
    // A.4.  Removing an Array Element
    test_patch((patch_data){
        .src = "{\"foo\":[\"bar\",\"qux\",\"baz\"]}",
        .patch = "["
            "{\"op\":\"remove\",\"path\":\"/foo/1\"}"
        "]",
        .dst = "{\"foo\":[\"bar\",\"baz\"]}",
    });
    
    // A.5.  Replacing a Value
    test_patch((patch_data){
        .src = "{\"foo\":\"bar\",\"baz\":\"qux\"}",
        .patch = "["
            "{\"op\":\"replace\",\"path\":\"/baz\",\"value\":\"boo\"}"
        "]",
        .dst = "{\"foo\":\"bar\",\"baz\":\"boo\"}",
    });
    
    // A.6.  Moving a Value
    test_patch((patch_data){
        .src = "{\"foo\":{\"bar\":\"baz\",\"waldo\":\"fred\"},\"qux\":{\"corge\":\"grault\"}}",
        .patch = "["
            "{\"op\":\"move\",\"from\":\"/foo/waldo\",\"path\":\"/qux/thud\"}"
        "]",
        .dst = "{\"foo\":{\"bar\":\"baz\"},\"qux\":{\"corge\":\"grault\",\"thud\":\"fred\"}}",
    });
    
    // A.7.  Moving an Array Element
    test_patch((patch_data){
        .src = "{\"foo\":[\"all\",\"grass\",\"cows\",\"eat\"]}",
        .patch = "["
            "{\"op\":\"move\",\"from\":\"/foo/1\",\"path\":\"/foo/3\"}"
        "]",
        .dst = "{\"foo\":[\"all\",\"cows\",\"eat\",\"grass\"]}",
    });
    
    // A.8.  Testing a Value: Success
    test_patch((patch_data){
        .src = "{\"baz\":\"qux\",\"foo\":[\"a\",2,\"c\"]}",
        .patch = "["
            "{\"op\":\"test\",\"path\":\"/baz\",\"value\":\"qux\"},"
            "{\"op\":\"test\",\"path\":\"/foo/1\",\"value\":2}"
        "]",
        .dst = "{\"baz\":\"qux\",\"foo\":[\"a\",2,\"c\"]}",
    });
    
    // A.9.  Testing a Value: Error
    test_patch((patch_data){
        .src = "{\"baz\":\"qux\"}",
        .patch = "["
            "{\"op\":\"test\",\"path\":\"/baz\",\"value\":\"bar\"}"
        "]",
        .err = {
            .code = YYJSON_PATCH_ERROR_EQUAL,
            .idx = 0,
        },
    });
    
    // A.10.  Adding a Nested Member Object
    test_patch((patch_data){
        .src = "{\"foo\":\"bar\"}",
        .patch = "["
            "{\"op\":\"add\",\"path\":\"/child\",\"value\":{\"grandchild\":{}}}"
        "]",
        .dst = "{\"foo\":\"bar\",\"child\":{\"grandchild\":{}}}",
    });
    
    // A.11.  Ignoring Unrecognized Elements
    test_patch((patch_data){
        .src = "{\"foo\":\"bar\"}",
        .patch = "["
            "{\"op\":\"add\",\"path\":\"/baz\",\"value\":\"qux\",\"xyz\":123}"
        "]",
        .dst = "{\"foo\":\"bar\",\"baz\":\"qux\"}",
    });
    
    // A.12.  Adding to a Nonexistent Target
    test_patch((patch_data){
        .src = "{\"foo\":\"bar\"}",
        .patch = "["
            "{\"op\":\"add\",\"path\":\"/baz/bat\",\"value\":\"qux\"}"
        "]",
        .err = {
            .code = YYJSON_PATCH_ERROR_POINTER,
            .idx = 0,
            .ptr = YYJSON_PTR_ERR_RESOLVE,
        },
    });
    
    // A.13.  Invalid JSON Patch Document
    // Note:  yyjson allows duplicate keys, here only the first "op" is taken
    test_patch((patch_data){
        .src = "{\"foo\":\"bar\"}",
        .patch = "["
            "{\"op\":\"add\",\"path\":\"/baz\",\"value\":\"qux\",\"op\":\"remove\"}"
        "]",
        .dst = "{\"foo\":\"bar\",\"baz\":\"qux\"}",
    });
    
    // A.14.  ~ Escape Ordering
    test_patch((patch_data){
        .src = "{\"/\":9,\"~1\":10}",
        .patch = "["
            "{\"op\":\"test\",\"path\":\"/~01\",\"value\":10}"
        "]",
        .dst = "{\"/\":9,\"~1\":10}",
    });
    
    // A.15.  Comparing Strings and Numbers
    test_patch((patch_data){
        .src = "{\"/\":9,\"~1\":10}",
        .patch = "["
            "{\"op\":\"test\",\"path\":\"/~01\",\"value\":\"10\"}"
        "]",
        .err = {
            .code = YYJSON_PATCH_ERROR_EQUAL,
            .idx = 0,
        },
    });
    
    // A.16.  Adding an Array Value
    test_patch((patch_data){
        .src = "{\"foo\":[\"bar\"]}",
        .patch = "["
            "{\"op\":\"add\",\"path\":\"/foo/-\",\"value\":[\"abc\",\"def\"]}"
        "]",
        .dst = "{\"foo\":[\"bar\",[\"abc\",\"def\"]]}",
    });
}

static void test_more(void) {
    // ---------------------------------
    // invalid parameter
    test_patch((patch_data){
        .src = "",
        .patch = "[]",
        .err = {
            .code = YYJSON_PATCH_ERROR_INVALID_PARAMETER,
        }
    });
    test_patch((patch_data){
        .src = "[]",
        .patch = "",
        .err = {
            .code = YYJSON_PATCH_ERROR_INVALID_PARAMETER,
        }
    });
    test_patch((patch_data){
        .src = "",
        .patch = "",
        .err = {
            .code = YYJSON_PATCH_ERROR_INVALID_PARAMETER,
        }
    });
    test_patch((patch_data){
        .src = "[]",
        .patch = "{}",
        .err = {
            .code = YYJSON_PATCH_ERROR_INVALID_PARAMETER,
        }
    });
    
    // ---------------------------------
    // error with index
    test_patch((patch_data){
        .src = "[]",
        .patch = "["
            "{\"op\":\"add\",\"path\":\"/-\",\"value\":0},"
            "{\"op\":\"add\",\"path\":\"/-\",\"value\":1},"
            "123"
        "]",
        .err = {
            .code = YYJSON_PATCH_ERROR_INVALID_OPERATION,
            .idx = 2,
        }
    });
    test_patch((patch_data){
        .src = "[]",
        .patch = "["
            "{\"op\":\"add\",\"path\":\"/-\",\"value\":0},"
            "{\"op\":\"add\",\"path\":\"/-\",\"value\":1},"
            "{\"op\":\"err\",\"path\":\"/-\",\"value\":1}"
        "]",
        .err = {
            .code = YYJSON_PATCH_ERROR_INVALID_MEMBER,
            .idx = 2,
        }
    });
    test_patch((patch_data){
        .src = "[]",
        .patch = "["
            "{\"op\":\"add\",\"path\":\"/-\",\"value\":0},"
            "{\"op\":\"add\",\"path\":\"/-\",\"value\":1},"
            "{\"path\":\"/-\",\"value\":1}"
        "]",
        .err = {
            .code = YYJSON_PATCH_ERROR_MISSING_KEY,
            .idx = 2,
        }
    });
    test_patch((patch_data){
        .src = "[]",
        .patch = "["
            "{\"op\":\"add\",\"path\":\"/-\",\"value\":0},"
            "{\"op\":\"add\",\"path\":\"/-\",\"value\":1},"
            "{\"op\":\"add\",\"value\":2}"
        "]",
        .err = {
            .code = YYJSON_PATCH_ERROR_MISSING_KEY,
            .idx = 2,
        }
    });
    test_patch((patch_data){
        .src = "[]",
        .patch = "["
            "{\"op\":\"add\",\"path\":\"/-\",\"value\":0},"
            "{\"op\":\"add\",\"path\":\"/-\",\"value\":1},"
            "{\"op\":\"add\",\"path\":null,\"value\":2}"
        "]",
        .err = {
            .code = YYJSON_PATCH_ERROR_INVALID_MEMBER,
            .idx = 2,
        }
    });
    
    // ---------------------------------
    // error op
    test_patch((patch_data){
        .src = "[1]",
        .patch = "[{\"op\":0,\"path\":\"/0\",\"value\":0}]",
        .err = { .code = YYJSON_PATCH_ERROR_INVALID_MEMBER, }
    });
    test_patch((patch_data){
        .src = "[1]",
        .patch = "[{\"op\":\"\",\"path\":\"/0\",\"value\":0}]",
        .err = { .code = YYJSON_PATCH_ERROR_INVALID_MEMBER, }
    });
    test_patch((patch_data){
        .src = "[1]",
        .patch = "[{\"op\":\"at\",\"path\":\"/0\",\"value\":0}]",
        .err = { .code = YYJSON_PATCH_ERROR_INVALID_MEMBER, }
    });
    test_patch((patch_data){
        .src = "[1]",
        .patch = "[{\"op\":\"set\",\"path\":\"/0\",\"value\":0}]",
        .err = { .code = YYJSON_PATCH_ERROR_INVALID_MEMBER, }
    });
    test_patch((patch_data){
        .src = "[1]",
        .patch = "[{\"op\":\"puts\",\"path\":\"/0\",\"value\":0}]",
        .err = { .code = YYJSON_PATCH_ERROR_INVALID_MEMBER, }
    });
    test_patch((patch_data){
        .src = "[1]",
        .patch = "[{\"op\":\"delete\",\"path\":\"/0\",\"value\":0}]",
        .err = { .code = YYJSON_PATCH_ERROR_INVALID_MEMBER, }
    });
    test_patch((patch_data){
        .src = "[1]",
        .patch = "[{\"op\":\"unknown\",\"path\":\"/0\",\"value\":0}]",
        .err = { .code = YYJSON_PATCH_ERROR_INVALID_MEMBER, }
    });
    
    // ---------------------------------
    // add
    test_patch((patch_data){
        .src = "[1]",
        .patch = "[{\"op\":\"add\",\"value\":0}]",
        .err = { .code = YYJSON_PATCH_ERROR_MISSING_KEY, }
    });
    test_patch((patch_data){
        .src = "[0]",
        .patch = "[{\"op\":\"add\",\"path\":\"/1\"}]",
        .err = { .code = YYJSON_PATCH_ERROR_MISSING_KEY, }
    });
    test_patch((patch_data){
        .src = "[0]",
        .patch = "[{\"op\":\"add\",\"path\":\"/1\",\"value\":1}]",
        .dst = "[0,1]",
    });
    test_patch((patch_data){
        .src = "[0]",
        .patch = "[{\"op\":\"add\",\"path\":\"/2\",\"value\":1}]",
        .err = { .code = YYJSON_PATCH_ERROR_POINTER,
                 .ptr = { .code = YYJSON_PTR_ERR_RESOLVE } }
    });
    test_patch((patch_data){
        .src = "[0]",
        .patch = "[{\"op\":\"add\",\"path\":\"/~2\",\"value\":1}]",
        .err = { .code = YYJSON_PATCH_ERROR_POINTER,
                 .ptr = { .code = YYJSON_PTR_ERR_SYNTAX } }
    });
    test_patch((patch_data){
        .src = "[0]",
        .patch = "[{\"op\":\"add\",\"path\":\"\",\"value\":1}]",
        .dst = "1",
    });
    
    // ---------------------------------
    // remove
    test_patch((patch_data){
        .src = "[1]",
        .patch = "[{\"op\":\"remove\"}]",
        .err = { .code = YYJSON_PATCH_ERROR_MISSING_KEY, }
    });
    test_patch((patch_data){
        .src = "[1]",
        .patch = "[{\"op\":\"remove\",\"path\":0}]",
        .err = { .code = YYJSON_PATCH_ERROR_INVALID_MEMBER, }
    });
    test_patch((patch_data){
        .src = "[1]",
        .patch = "[{\"op\":\"remove\",\"path\":\"\"}]",
        .err = { .code = YYJSON_PATCH_ERROR_POINTER,
                 .ptr = { .code = YYJSON_PTR_ERR_SET_ROOT }
        }
    });
    test_patch((patch_data){
        .src = "[1]",
        .patch = "[{\"op\":\"remove\",\"path\":\"/-\"}]",
        .err = { .code = YYJSON_PATCH_ERROR_POINTER,
                 .ptr = { .code = YYJSON_PTR_ERR_RESOLVE, }
        }
    });
    test_patch((patch_data){
        .src = "[1]",
        .patch = "[{\"op\":\"remove\",\"path\":\"/0\"}]",
        .dst = "[]",
    });
    
    // ---------------------------------
    // replace
    test_patch((patch_data){
        .src = "[1]",
        .patch = "[{\"op\":\"replace\",\"value\":0}]",
        .err = { .code = YYJSON_PATCH_ERROR_MISSING_KEY, }
    });
    test_patch((patch_data){
        .src = "[0]",
        .patch = "[{\"op\":\"replace\",\"path\":\"/1\"}]",
        .err = { .code = YYJSON_PATCH_ERROR_MISSING_KEY, }
    });
    test_patch((patch_data){
        .src = "[0]",
        .patch = "[{\"op\":\"replace\",\"path\":\"/0\",\"value\":1}]",
        .dst = "[1]",
    });
    test_patch((patch_data){
        .src = "[0]",
        .patch = "[{\"op\":\"replace\",\"path\":\"/1\",\"value\":1}]",
        .err = { .code = YYJSON_PATCH_ERROR_POINTER,
                 .ptr = { .code = YYJSON_PTR_ERR_RESOLVE } }
    });
    test_patch((patch_data){
        .src = "[0]",
        .patch = "[{\"op\":\"replace\",\"path\":\"/~2\",\"value\":1}]",
        .err = { .code = YYJSON_PATCH_ERROR_POINTER,
                 .ptr = { .code = YYJSON_PTR_ERR_SYNTAX } }
    });
    test_patch((patch_data){
        .src = "[0]",
        .patch = "[{\"op\":\"replace\",\"path\":\"\",\"value\":1}]",
        .dst = "1",
    });
    
    // ---------------------------------
    // move
    test_patch((patch_data){
        .src = "[[1,2],[3,4]]",
        .patch = "[{\"op\":\"move\",\"from\":\"/0/0\"}]",
        .err = { .code = YYJSON_PATCH_ERROR_MISSING_KEY, }
    });
    test_patch((patch_data){
        .src = "[[1,2],[3,4]]",
        .patch = "[{\"op\":\"move\",\"path\":\"/1/0\"}]",
        .err = { .code = YYJSON_PATCH_ERROR_MISSING_KEY, }
    });
    test_patch((patch_data){
        .src = "[[1,2],[3,4]]",
        .patch = "[{\"op\":\"move\",\"from\":\"/0/0\",\"path\":\"/1/0\"}]",
        .dst = "[[2],[1,3,4]]",
    });
    test_patch((patch_data){
        .src = "[[1,2],[3,4]]",
        .patch = "[{\"op\":\"move\",\"from\":0,\"path\":\"/1/0\"}]",
        .err = { .code = YYJSON_PATCH_ERROR_INVALID_MEMBER }
    });
    test_patch((patch_data){
        .src = "[[1,2],[3,4]]",
        .patch = "[{\"op\":\"move\",\"from\":\"/0/a\",\"path\":\"/1/0\"}]",
        .err = { .code = YYJSON_PATCH_ERROR_POINTER,
                 .ptr = { .code = YYJSON_PTR_ERR_RESOLVE } }
    });
    test_patch((patch_data){
        .src = "[[1,2],[3,4]]",
        .patch = "[{\"op\":\"move\",\"from\":\"/0/0\",\"path\":\"/1/a\"}]",
        .err = { .code = YYJSON_PATCH_ERROR_POINTER,
                 .ptr = { .code = YYJSON_PTR_ERR_RESOLVE } }
    });
    test_patch((patch_data){
        .src = "[[1,2],[3,4]]",
        .patch = "[{\"op\":\"move\",\"from\":\"/0/~\",\"path\":\"/1/0\"}]",
        .err = { .code = YYJSON_PATCH_ERROR_POINTER,
                 .ptr = { .code = YYJSON_PTR_ERR_SYNTAX } }
    });
    test_patch((patch_data){
        .src = "[[1,2],[3,4]]",
        .patch = "[{\"op\":\"move\",\"from\":\"\",\"path\":\"\"}]",
        .dst = "[[1,2],[3,4]]",
    });
    test_patch((patch_data){
        .src = "[[1,2],[3,4]]",
        .patch = "[{\"op\":\"move\",\"from\":\"/0/0\",\"path\":\"\"}]",
        .dst = "1",
    });
    
    // ---------------------------------
    // copy
    test_patch((patch_data){
        .src = "[[1,2],[3,4]]",
        .patch = "[{\"op\":\"copy\",\"from\":\"/0/0\"}]",
        .err = { .code = YYJSON_PATCH_ERROR_MISSING_KEY, }
    });
    test_patch((patch_data){
        .src = "[[1,2],[3,4]]",
        .patch = "[{\"op\":\"copy\",\"path\":\"/1/0\"}]",
        .err = { .code = YYJSON_PATCH_ERROR_MISSING_KEY, }
    });
    test_patch((patch_data){
        .src = "[[1,2],[3,4]]",
        .patch = "[{\"op\":\"copy\",\"from\":\"/0/0\",\"path\":\"/1/0\"}]",
        .dst = "[[1,2],[1,3,4]]",
    });
    test_patch((patch_data){
        .src = "[[1,2],[3,4]]",
        .patch = "[{\"op\":\"copy\",\"from\":0,\"path\":\"/1/0\"}]",
        .err = { .code = YYJSON_PATCH_ERROR_INVALID_MEMBER }
    });
    test_patch((patch_data){
        .src = "[[1,2],[3,4]]",
        .patch = "[{\"op\":\"copy\",\"from\":\"/0/a\",\"path\":\"/1/0\"}]",
        .err = { .code = YYJSON_PATCH_ERROR_POINTER,
                 .ptr = { .code = YYJSON_PTR_ERR_RESOLVE } }
    });
    test_patch((patch_data){
        .src = "[[1,2],[3,4]]",
        .patch = "[{\"op\":\"copy\",\"from\":\"/0/0\",\"path\":\"/1/a\"}]",
        .err = { .code = YYJSON_PATCH_ERROR_POINTER,
                 .ptr = { .code = YYJSON_PTR_ERR_RESOLVE } }
    });
    test_patch((patch_data){
        .src = "[[1,2],[3,4]]",
        .patch = "[{\"op\":\"copy\",\"from\":\"/0/~\",\"path\":\"/1/0\"}]",
        .err = { .code = YYJSON_PATCH_ERROR_POINTER,
                 .ptr = { .code = YYJSON_PTR_ERR_SYNTAX } }
    });
    test_patch((patch_data){
        .src = "[[1,2],[3,4]]",
        .patch = "[{\"op\":\"copy\",\"from\":\"\",\"path\":\"\"}]",
        .dst = "[[1,2],[3,4]]",
    });
    test_patch((patch_data){
        .src = "[[1,2],[3,4]]",
        .patch = "[{\"op\":\"copy\",\"from\":\"/0/0\",\"path\":\"\"}]",
        .dst = "1",
    });
    
    // ---------------------------------
    // test
    test_patch((patch_data){
        .src = "[1]",
        .patch = "[{\"op\":\"test\",\"value\":1}]",
        .err = { .code = YYJSON_PATCH_ERROR_MISSING_KEY, }
    });
    test_patch((patch_data){
        .src = "[1]",
        .patch = "[{\"op\":\"test\",\"path\":\"/0\"}]",
        .err = { .code = YYJSON_PATCH_ERROR_MISSING_KEY, }
    });
    test_patch((patch_data){
        .src = "[1]",
        .patch = "[{\"op\":\"test\",\"path\":\"/0\",\"value\":1}]",
        .dst = "[1]",
    });
    test_patch((patch_data){
        .src = "[1]",
        .patch = "[{\"op\":\"test\",\"path\":\"/1\",\"value\":1}]",
        .err = { .code = YYJSON_PATCH_ERROR_POINTER,
                 .ptr = { .code = YYJSON_PTR_ERR_RESOLVE } }
    });
    test_patch((patch_data){
        .src = "[1]",
        .patch = "[{\"op\":\"test\",\"path\":\"/~2\",\"value\":1}]",
        .err = { .code = YYJSON_PATCH_ERROR_POINTER,
                 .ptr = { .code = YYJSON_PTR_ERR_SYNTAX } }
    });
    test_patch((patch_data){
        .src = "[1]",
        .patch = "[{\"op\":\"test\",\"path\":\"\",\"value\":2}]",
        .err = { .code = YYJSON_PATCH_ERROR_EQUAL }
    });
    
    // ---------------------------------
    // multiple ops
    test_patch((patch_data){
        .src = "[1,2,3]",
        .patch = "["
            "{\"op\":\"add\",\"path\":\"/3\",\"value\":4}," // [1,2,3,4]
            "{\"op\":\"remove\",\"path\":\"/1\"}," // [1,3,4]
            "{\"op\":\"replace\",\"path\":\"/0\",\"value\":{\"a\":0}}," // [{"a":0},3,4]
            "{\"op\":\"move\",\"from\":\"/0/a\",\"path\":\"/1\"}," // [{},0,3,4]
            "{\"op\":\"copy\",\"from\":\"/3\",\"path\":\"/0/b\"}," // [{"b":4},0,3,4]
            "{\"op\":\"test\",\"path\":\"/0\",\"value\":{\"b\":4}}" // [{"b":4},0,3,4]
        "]",
        .dst = "[{\"b\":4},0,3,4]"
    });
}

yy_test_case(test_json_patch) {
    test_spec();
    test_more();
}

#else
yy_test_case(test_json_patch) {}
#endif
