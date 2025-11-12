// This file is used to test the `JSON Merge Patch` functions.

#include "yyjson.h"
#include "yy_test_utils.h"

#if !YYJSON_DISABLE_UTILS

static void test_one(const char *orig_json,
                     const char *patch_json,
                     const char *expt_json) {
#if !YYJSON_DISABLE_READER
    yyjson_doc *i_orig_doc = yyjson_read(orig_json, strlen(orig_json), 0);
    yyjson_doc *i_patch_doc = yyjson_read(patch_json, strlen(patch_json), 0);
    yyjson_doc *i_expe_doc = yyjson_read(expt_json, strlen(expt_json), 0);
    yyjson_mut_doc *m_orig_doc = yyjson_doc_mut_copy(i_orig_doc, NULL);
    yyjson_mut_doc *m_patch_doc = yyjson_doc_mut_copy(i_patch_doc, NULL);
    yyjson_mut_doc *m_expe_doc = yyjson_doc_mut_copy(i_expe_doc, NULL);
    
    yyjson_mut_doc *doc = yyjson_mut_doc_new(NULL);
    yyjson_mut_val *ret1 = yyjson_merge_patch(doc, i_orig_doc->root, i_patch_doc->root);
    yyjson_mut_val *ret2 = yyjson_mut_merge_patch(doc, m_orig_doc->root, m_patch_doc->root);
    
#if !YYJSON_DISABLE_WRITER
    char *str1 = yyjson_mut_val_write(ret1, 0, NULL);
    char *str2 = yyjson_mut_val_write(ret2, 0, NULL);
    yy_assert(strcmp(expt_json, str1) == 0);
    yy_assert(strcmp(expt_json, str2) == 0);
    free(str1);
    free(str2);
#endif
    
    yy_assert(yyjson_mut_equals(m_expe_doc->root, ret1));
    yy_assert(yyjson_mut_equals(m_expe_doc->root, ret2));
    
    yy_assert(yyjson_merge_patch(NULL, NULL, NULL) == NULL);
    yy_assert(yyjson_merge_patch(NULL, i_orig_doc->root, NULL) == NULL);
    yy_assert(yyjson_merge_patch(NULL, NULL, i_patch_doc->root) == NULL);
    yy_assert(yyjson_merge_patch(NULL, i_orig_doc->root, i_patch_doc->root) == NULL);
    yy_assert(yyjson_merge_patch(doc, i_orig_doc->root, NULL) == NULL);
    yy_assert(yyjson_merge_patch(doc, NULL, i_patch_doc->root) != NULL);
    
    yy_assert(yyjson_mut_merge_patch(NULL, NULL, NULL) == NULL);
    yy_assert(yyjson_mut_merge_patch(NULL, m_orig_doc->root, NULL) == NULL);
    yy_assert(yyjson_mut_merge_patch(NULL, NULL, m_patch_doc->root) == NULL);
    yy_assert(yyjson_mut_merge_patch(NULL, m_orig_doc->root, m_patch_doc->root) == NULL);
    yy_assert(yyjson_mut_merge_patch(doc, m_orig_doc->root, NULL) == NULL);
    yy_assert(yyjson_mut_merge_patch(doc, NULL, m_patch_doc->root) != NULL);
    
    yyjson_mut_doc_free(doc);
    yyjson_mut_doc_free(m_expe_doc);
    yyjson_mut_doc_free(m_patch_doc);
    yyjson_mut_doc_free(m_orig_doc);
    yyjson_doc_free(i_expe_doc);
    yyjson_doc_free(i_patch_doc);
    yyjson_doc_free(i_orig_doc);
    
#endif
}

yy_test_case(test_json_merge_patch) {
    // test cases from spec: https://tools.ietf.org/html/rfc7386
    test_one("{\"a\":\"b\"}", "{\"a\":\"c\"}", "{\"a\":\"c\"}");
    test_one("{\"a\":\"b\"}", "{\"b\":\"c\"}", "{\"a\":\"b\",\"b\":\"c\"}");
    test_one("{\"a\":\"b\"}", "{\"a\":null }", "{}");
    test_one("{\"a\":\"b\"}", "{\"a\":null }", "{}");
    test_one("{\"a\":\"b\", \"b\":\"c\"}", "{\"a\":null }", "{\"b\":\"c\"}");
    test_one("{\"a\":[\"b\"] }", "{\"a\":\"c\"}", "{\"a\":\"c\"}");
    test_one("{\"a\":\"c\"}", "{\"a\":[\"b\"]}", "{\"a\":[\"b\"]}");
    test_one("{\"a\":{\"b\":\"c\"}}", "{\"a\":{\"b\":\"d\",\"c\":null}}", "{\"a\":{\"b\":\"d\"}}");
    test_one("{\"a\":{\"b\":\"c\"}}", "{\"a\":[1]}", "{\"a\":[1]}");
    test_one("[\"a\",\"b\"]", "[\"c\",\"d\"]", "[\"c\",\"d\"]");
    test_one("{\"a\":\"b\"}", "[\"c\"]", "[\"c\"]");
    test_one("{\"a\":\"foo\"}", "null", "null");
    test_one("{\"a\":\"foo\"}", "\"bar\"", "\"bar\"");
    test_one("{\"e\":null}", "{\"a\":1}", "{\"e\":null,\"a\":1}");
    test_one("[1,2]", "{\"a\":\"b\",\"c\":null}", "{\"a\":\"b\"}");
    test_one("{}", "{\"a\":{\"bb\":{\"ccc\":null}}}", "{\"a\":{\"bb\":{}}}");
}

#else
yy_test_case(test_json_merge_patch) {}
#endif
