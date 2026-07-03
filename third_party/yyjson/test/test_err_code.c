// This file is used to test the accuracy of the error codes of 
// json_read and json_write.

#include "yyjson.h"
#include "yy_test_utils.h"



#define is_json_space(x) \
    (((u8)x) == ' ' || ((u8)x) == '\r' || ((u8)x) == '\n' || ((u8)x) == '\t')

#define is_alphabet(x) \
    (('a' <= ((u8)x) && ((u8)x) <= 'z') || ('A' <= ((u8)x) && ((u8)x) <= 'Z'))



static void test_read_err_code(void) {
#if !YYJSON_DISABLE_READER
    yyjson_read_err err;
    const char *str;
    yyjson_alc alc;
    char buf[1024];
    usize len;
    
    
    
    // -------------------------------------------------------------------------
    // Success, no error.
    str = "[]";
    //     ^
    memset(&err, -1, sizeof(err));
    yyjson_doc_free(yyjson_read_opts((char *)str, strlen(str), 0, NULL, &err));
    yy_assert(err.code == YYJSON_READ_SUCCESS);
    yy_assert(err.pos == 0);
    
    
    
    // -------------------------------------------------------------------------
    // Invalid parameter, such as NULL input string or 0 input length.
    str = "";
    //     ^ input length is 0
    memset(&err, -1, sizeof(err));
    yyjson_doc_free(yyjson_read_opts((char *)str, 0, 0, NULL, &err));
    yy_assert(err.code == YYJSON_READ_ERROR_INVALID_PARAMETER);
    yy_assert(err.pos == 0);
    
    str = NULL;
    //    ^ input data is NULL
    memset(&err, -1, sizeof(err));
    yyjson_doc_free(yyjson_read_opts((char *)str, 0, 0, NULL, &err));
    yy_assert(err.code == YYJSON_READ_ERROR_INVALID_PARAMETER);
    yy_assert(err.pos == 0);
    
    str = NULL;
    //    ^ input path is NULL
    memset(&err, -1, sizeof(err));
    yyjson_doc_free(yyjson_read_file(str, 0, NULL, &err));
    yy_assert(err.code == YYJSON_READ_ERROR_INVALID_PARAMETER);
    yy_assert(err.pos == 0);
    
    
    
    // -------------------------------------------------------------------------
    // Memory allocation failure occurs.
    str = "[]";
    //     ^ memory allocation failed
    yyjson_alc_pool_init(&alc, NULL, 0);
    memset(&err, -1, sizeof(err));
    yyjson_doc_free(yyjson_read_opts((char *)str, strlen(str), 0, &alc, &err));
    yy_assert(err.code == YYJSON_READ_ERROR_MEMORY_ALLOCATION);
    yy_assert(err.pos == 0);
    
    
    
    // -------------------------------------------------------------------------
    // Input JSON string is empty.
    str = " ";
    //     ^ input data is empty
    memset(&err, -1, sizeof(err));
    yyjson_doc_free(yyjson_read_opts((char *)str, strlen(str), 0, NULL, &err));
    yy_assert(err.code == YYJSON_READ_ERROR_EMPTY_CONTENT);
    yy_assert(err.pos == 0);
    
    str = "\n\n\r\n";
    //     ^ input data is empty
    memset(&err, -1, sizeof(err));
    yyjson_doc_free(yyjson_read_opts((char *)str, strlen(str), 0, NULL, &err));
    yy_assert(err.code == YYJSON_READ_ERROR_EMPTY_CONTENT);
    yy_assert(err.pos == 0);
    
    
    
    // -------------------------------------------------------------------------
    // Unexpected content after document, such as `[1]abc`.
    str = "[1]abc";
    //        ^ unexpected content after document
    memset(&err, -1, sizeof(err));
    yyjson_doc_free(yyjson_read_opts((char *)str, strlen(str), 0, NULL, &err));
    yy_assert(err.code == YYJSON_READ_ERROR_UNEXPECTED_CONTENT);
    yy_assert(err.pos == strlen(str) - 3);
    
    str = "[1],";
    //        ^ unexpected content after document
    memset(&err, -1, sizeof(err));
    yyjson_doc_free(yyjson_read_opts((char *)str, strlen(str), 0, NULL, &err));
    yy_assert(err.code == YYJSON_READ_ERROR_UNEXPECTED_CONTENT);
    yy_assert(err.pos == strlen(str) - 1);
    
#if !YYJSON_DISABLE_NON_STANDARD
    str = "[1],";
    //        ^ unexpected content after document
    memset(&err, -1, sizeof(err));
    yyjson_doc_free(yyjson_read_opts((char *)str, strlen(str),
                                     YYJSON_READ_ALLOW_TRAILING_COMMAS, NULL, &err));
    yy_assert(err.code == YYJSON_READ_ERROR_UNEXPECTED_CONTENT);
    yy_assert(err.pos == strlen(str) - 1);
#endif
    
    
    
    // -------------------------------------------------------------------------
    // Unexpected ending, such as `[123`.
    
    // test truncated single value
    const char *truncated_single_values[] = {
        "-",
        "-1.",
        "123.",
        "123e",
        "123e-",
        "123.1e",
        "123.1e-",
        "t",
        "tr",
        "tru",
        "f",
        "fa",
        "fal",
        "fals",
        "n",
        "nu",
        "nul",
    };
    for (usize i = 0; i < yy_nelems(truncated_single_values); i++) {
        // check unexpected end
        str = truncated_single_values[i];
        len = strlen(str);
        memset(&err, -1, sizeof(err));
        yyjson_doc_free(yyjson_read_opts((char *)str, len, 0, NULL, &err));
        yy_assert(err.code == YYJSON_READ_ERROR_UNEXPECTED_END);
        yy_assert(err.pos == len);
        
        // add a space after invalid json
        memcpy(buf, str, len);
        memcpy(buf + len, " ", 2);
        memset(&err, -1, sizeof(err));
        yyjson_doc_free(yyjson_read_opts((char *)buf, len + 1, 0, NULL, &err));
        if (is_alphabet(*buf)) {
            yy_assert(err.code == YYJSON_READ_ERROR_LITERAL);
            yy_assert(err.pos == 0);
        } else {
            yy_assert(err.code == YYJSON_READ_ERROR_INVALID_NUMBER);
            yy_assert(err.pos == len);
        }
    }
    
    // test truncated nan/inf value
    const char *truncated_nan_inf_values[] = {
        "na",
        "-na",
        "in",
        "-in",
        "In",
        "-In",
        "infi",
        "-infi",
        "Infi",
        "-Infi",
        "Infinit",
        "-Infinit",
    };
    for (usize i = 0; i < yy_nelems(truncated_nan_inf_values); i++) {
        // check unexpected end
        str = truncated_nan_inf_values[i];
        len = strlen(str);
        memset(&err, -1, sizeof(err));
        yyjson_doc_free(yyjson_read_opts((char *)str, len, 0, NULL, &err));
        yy_assert(err.code);
        yy_assert(err.code != YYJSON_READ_ERROR_UNEXPECTED_END);
        
#if !YYJSON_DISABLE_NON_STANDARD
        memset(&err, -1, sizeof(err));
        yyjson_doc_free(yyjson_read_opts((char *)str, strlen(str),
                                         YYJSON_READ_ALLOW_INF_AND_NAN, NULL, &err));
        yy_assert(err.code == YYJSON_READ_ERROR_UNEXPECTED_END);
        yy_assert(err.pos == len);
#endif
    }
    
    // test truncated JSON
    const char *valid_jsons[] = {
        "[0]",
        "[\n  0\n]",
        "[123]",
        "[\n  123\n]",
        "[123e4]",
        "[\n  123e4\n]",
        "[-123.4e-56]",
        "[\n  -123.4e-56\n]",
        "\"Check✅©\\t2020®яблоко////แอปเปิ้ล\\\\\\\\リンゴ|تفاحة|蘋果|사과|\"",
        "\"Check\\u2705\\u00A9\\t2020\\u00AE\\u044F\\u0431\\u043B\\u043E\\u043A\\u043E\\/\\/\\/\\/\\u0E41\\u0E2D\\u0E1B\\u0E40\\u0E1B\\u0E34\\u0E49\\u0E25\\\\\\\\\\u30EA\\u30F3\\u30B4|\\u062A\\u0641\\u0627\\u062D\\u0629|\\u860B\\u679C|\\uC0AC\\uACFC|\\uF8FF\"",
        "[[[{}]]]",
        "[\n  [\n    [\n      {}\n    ]\n  ]\n]",
        "{\"name\":\"Harry\",\"id\":123,\"star\":[1,2,3]}",
        "{\n  \"name\": \"Harry\",\n  \"id\": 123,\n  \"star\": [\n    1,\n    2,\n    3\n  ]\n}",
    };
    for (usize i = 0; i < yy_nelems(valid_jsons); i++) {
        str = valid_jsons[i];
        len = strlen(str);
        for (usize l = 1; l <= len; l++) {
            memset(&err, -1, sizeof(err));
            yyjson_doc_free(yyjson_read_opts((char *)str, l, 0, NULL, &err));
            if (l == len) {
                yy_assert(err.code == YYJSON_READ_SUCCESS);
                yy_assert(err.pos == 0);
            } else {
                yy_assert(err.code == YYJSON_READ_ERROR_UNEXPECTED_END);
                yy_assert(err.pos == l);
            }
        }
    }
    
    // test with `JSONTestSuite` files
    char dir[YY_MAX_PATH];
    yy_path_combine(dir, YYJSON_TEST_DATA_PATH, "data", "json", "test_parsing", NULL);
    int count;
    char **names = yy_dir_read(dir, &count);
    for (int i = 0; i < count; i++) {
        char *name = names[i];
        char path[YY_MAX_PATH];
        yy_path_combine(path, dir, name, NULL);
        if (!yy_str_has_prefix(name, "y_")) continue;
        
        // read files, trim spaces, ignore too large files
        u8 *dat;
        if (!yy_file_read(path, &dat, &len)) continue;
        str = (char *)dat;
        while (len && is_json_space(str[0])) { str++; len--; }
        while (len && is_json_space(str[len - 1])) { len--; }
        if (len > 256) len = 0;
        
        // some numbers are still valid after being truncated
        // but other truncated JSON should report `unexpected end` errors
        for (usize l = 1; l < len; l++) {
            memset(&err, -1, sizeof(err));
            yyjson_doc_free(yyjson_read_opts((char *)str, l, 0, NULL, &err));
            if (err.code == YYJSON_READ_SUCCESS) {
                yy_assert(*str == '-' || ('0' <= *str && *str <= '9'));
            } else {
                yy_assert(err.code == YYJSON_READ_ERROR_UNEXPECTED_END);
                yy_assert(err.pos == l);
            }
        }
        free(dat);
    }
    yy_dir_free(names);
    
    // Both 'Infinity' and 'Inf' are valid literals here.
#if !YYJSON_DISABLE_NON_STANDARD
    str = "-Infini";
    //        ^ unexpected end of data
    memset(&err, -1, sizeof(err));
    yyjson_doc_free(yyjson_read_opts((char *)str, strlen(str),
                                     YYJSON_READ_ALLOW_INF_AND_NAN, NULL, &err));
    yy_assert(err.code == YYJSON_READ_ERROR_UNEXPECTED_END);
    yy_assert(err.pos == strlen(str));
#endif
    
    
    
    // -------------------------------------------------------------------------
    // Unexpected character inside the document, such as `[abc]`.
    str = "[abc]";
    //      ^ unexpected character
    memset(&err, -1, sizeof(err));
    yyjson_doc_free(yyjson_read_opts((char *)str, strlen(str), 0, NULL, &err));
    yy_assert(err.code == YYJSON_READ_ERROR_UNEXPECTED_CHARACTER);
    yy_assert(err.pos == 1);
    
    str = "inf";
    //     ^ unexpected character
    memset(&err, -1, sizeof(err));
    yyjson_doc_free(yyjson_read_opts((char *)str, strlen(str), 0, NULL, &err));
    yy_assert(err.code == YYJSON_READ_ERROR_UNEXPECTED_CHARACTER);
    yy_assert(err.pos == 0);
    
    
    
    // -------------------------------------------------------------------------
    // Invalid JSON structure, such as `[1,]`.
    str = "[1,]";
    //       ^ trailing comma is not allowed
    memset(&err, -1, sizeof(err));
    yyjson_doc_free(yyjson_read_opts((char *)str, strlen(str), 0, NULL, &err));
    yy_assert(err.code == YYJSON_READ_ERROR_JSON_STRUCTURE);
    yy_assert(err.pos == strlen(str) - 2);
    
    
    
    // -------------------------------------------------------------------------
    // Invalid comment, such as unclosed multi-line comment.
#if !YYJSON_DISABLE_NON_STANDARD
    str = "[123]/*";
    //          ^ unclosed multiline comment
    memset(&err, -1, sizeof(err));
    yyjson_doc_free(yyjson_read_opts((char *)str, strlen(str),
                                     YYJSON_READ_ALLOW_COMMENTS, NULL, &err));
    yy_assert(err.code == YYJSON_READ_ERROR_INVALID_COMMENT);
    yy_assert(err.pos == strlen(str) - 2);
    
    str = "[123/*";
    //         ^ unclosed multiline comment
    memset(&err, -1, sizeof(err));
    yyjson_doc_free(yyjson_read_opts((char *)str, strlen(str),
                                     YYJSON_READ_ALLOW_COMMENTS, NULL, &err));
    yy_assert(err.code == YYJSON_READ_ERROR_INVALID_COMMENT);
    yy_assert(err.pos == strlen(str) - 2);
#endif
    
    
    
    // -------------------------------------------------------------------------
    // Invalid number, such as `123.e12`, `000`.
    str = "123.e12";
    //         ^ no digit after decimal point
    memset(&err, -1, sizeof(err));
    yyjson_doc_free(yyjson_read_opts((char *)str, strlen(str), 0, NULL, &err));
    yy_assert(err.code == YYJSON_READ_ERROR_INVALID_NUMBER);
    yy_assert(err.pos == 4);
    
    str = "000";
    //     ^ number with leading zero is not allowed
    memset(&err, -1, sizeof(err));
    yyjson_doc_free(yyjson_read_opts((char *)str, strlen(str), 0, NULL, &err));
    yy_assert(err.code == YYJSON_READ_ERROR_INVALID_NUMBER);
    yy_assert(err.pos == 0);
    
    str = "[01";
    //      ^ number with leading zero is not allowed
    memset(&err, -1, sizeof(err));
    yyjson_doc_free(yyjson_read_opts((char *)str, strlen(str), 0, NULL, &err));
    yy_assert(err.code == YYJSON_READ_ERROR_INVALID_NUMBER);
    yy_assert(err.pos == 1);
    
    str = "[123.]";
    //          ^ no digit after decimal point
    memset(&err, -1, sizeof(err));
    yyjson_doc_free(yyjson_read_opts((char *)str, strlen(str), 0, NULL, &err));
    yy_assert(err.code == YYJSON_READ_ERROR_INVALID_NUMBER);
    yy_assert(err.pos == 5);
    
    
    
    // -------------------------------------------------------------------------
    // Invalid string, such as invalid escaped character inside a string.
#if !YYJSON_DISABLE_UTF8_VALIDATION
    
    str = "\"\\uD800\"";
    //              ^ no low surrogate in string
    memset(&err, -1, sizeof(err));
    yyjson_doc_free(yyjson_read_opts((char *)str, strlen(str), 0, NULL, &err));
    yy_assert(err.code == YYJSON_READ_ERROR_INVALID_STRING);
    yy_assert(err.pos == 7);
    
    // invalid 1-byte UTF-8
    memcpy(buf, "\"abcdefgh\"", 10);
    buf[1] = 0x01;
    for (len = 2; len < 10; len++) {
        memset(&err, -1, sizeof(err));
        yyjson_doc_free(yyjson_read_opts((char *)buf, len, 0, NULL, &err));
        yy_assert(err.code == YYJSON_READ_ERROR_INVALID_STRING);
        yy_assert(err.pos == 1);
    }
    buf[1] = 0xA0;
    for (len = 2; len < 10; len++) {
        memset(&err, -1, sizeof(err));
        yyjson_doc_free(yyjson_read_opts((char *)buf, len, 0, NULL, &err));
        yy_assert(err.code == YYJSON_READ_ERROR_INVALID_STRING);
        yy_assert(err.pos == 1);
    }
    buf[1] = 0xFF;
    for (len = 2; len < 10; len++) {
        memset(&err, -1, sizeof(err));
        yyjson_doc_free(yyjson_read_opts((char *)buf, len, 0, NULL, &err));
        yy_assert(err.code == YYJSON_READ_ERROR_INVALID_STRING);
        yy_assert(err.pos == 1);
    }
    
    // invalid 2-bytes UTF-8
    memcpy(buf, "\"abcdefgh\"", 10);
    buf[1] = 0xC0;
    buf[2] = 0x80;
    for (len = 2; len < 10; len++) {
        memset(&err, -1, sizeof(err));
        yyjson_doc_free(yyjson_read_opts((char *)buf, len, 0, NULL, &err));
        yy_assert(err.code == YYJSON_READ_ERROR_INVALID_STRING);
        yy_assert(err.pos == 1);
    }
    
    // invalid 3-bytes UTF-8
    memcpy(buf, "\"abcdefgh\"", 10);
    buf[1] = 0xE0;
    buf[2] = 0x80;
    buf[3] = 0x80;
    for (len = 2; len < 10; len++) {
        memset(&err, -1, sizeof(err));
        yyjson_doc_free(yyjson_read_opts((char *)buf, len, 0, NULL, &err));
        if (len == 2) {
            yy_assert(err.code == YYJSON_READ_ERROR_UNEXPECTED_END);
            yy_assert(err.pos == 2);
        } else {
            yy_assert(err.code == YYJSON_READ_ERROR_INVALID_STRING);
            yy_assert(err.pos == 1);
        }
    }
    memcpy(buf, "\"abcdefgh\"", 10);
    buf[1] = 0xED;
    buf[2] = 0xA0;
    buf[3] = 0x80;
    for (len = 2; len < 10; len++) {
        memset(&err, -1, sizeof(err));
        yyjson_doc_free(yyjson_read_opts((char *)buf, len, 0, NULL, &err));
        if (len == 2) {
            yy_assert(err.code == YYJSON_READ_ERROR_UNEXPECTED_END);
            yy_assert(err.pos == 2);
        } else {
            yy_assert(err.code == YYJSON_READ_ERROR_INVALID_STRING);
            yy_assert(err.pos == 1);
        }
    }
    
    // invalid 4-bytes UTF-8
    memcpy(buf, "\"abcdefgh\"", 10);
    buf[1] = 0xF0;
    buf[2] = 0x80;
    buf[3] = 0x80;
    buf[3] = 0x80;
    for (len = 2; len < 10; len++) {
        memset(&err, -1, sizeof(err));
        yyjson_doc_free(yyjson_read_opts((char *)buf, len, 0, NULL, &err));
        if (len == 2) {
            yy_assert(err.code == YYJSON_READ_ERROR_UNEXPECTED_END);
            yy_assert(err.pos == len);
        } else {
            yy_assert(err.code == YYJSON_READ_ERROR_INVALID_STRING);
            yy_assert(err.pos == 1);
        }
    }
    memcpy(buf, "\"abcdefgh\"", 10);
    buf[1] = 0xF4;
    buf[2] = 0xA0;
    buf[3] = 0x80;
    buf[3] = 0x80;
    for (len = 2; len < 10; len++) {
        memset(&err, -1, sizeof(err));
        yyjson_doc_free(yyjson_read_opts((char *)buf, len, 0, NULL, &err));
        if (len == 2) {
            yy_assert(err.code == YYJSON_READ_ERROR_UNEXPECTED_END);
            yy_assert(err.pos == len);
        } else {
            yy_assert(err.code == YYJSON_READ_ERROR_INVALID_STRING);
            yy_assert(err.pos == 1);
        }
    }
    memcpy(buf, "\"abcdefgh\"", 10);
    buf[1] = 0xF5;
    buf[2] = 0x80;
    buf[3] = 0x80;
    buf[3] = 0x80;
    for (len = 2; len < 10; len++) {
        memset(&err, -1, sizeof(err));
        yyjson_doc_free(yyjson_read_opts((char *)buf, len, 0, NULL, &err));
        yy_assert(err.code == YYJSON_READ_ERROR_INVALID_STRING);
        yy_assert(err.pos == 1);
    }
    memcpy(buf, "\"abcdefgh\"", 10);
    buf[1] = 0xF8;
    buf[2] = 0x80;
    buf[3] = 0x80;
    buf[3] = 0x80;
    for (len = 2; len < 10; len++) {
        memset(&err, -1, sizeof(err));
        yyjson_doc_free(yyjson_read_opts((char *)buf, len, 0, NULL, &err));
        yy_assert(err.code == YYJSON_READ_ERROR_INVALID_STRING);
        yy_assert(err.pos == 1);
    }
    memcpy(buf, "\"abcdefgh\"", 10);
    buf[1] = 0xF9;
    buf[2] = 0x80;
    buf[3] = 0xC0;
    buf[3] = 0x80;
    for (len = 2; len < 10; len++) {
        memset(&err, -1, sizeof(err));
        yyjson_doc_free(yyjson_read_opts((char *)buf, len, 0, NULL, &err));
        yy_assert(err.code == YYJSON_READ_ERROR_INVALID_STRING);
        yy_assert(err.pos == 1);
    }
#endif
    
    
    
    // -------------------------------------------------------------------------
    // Invalid JSON literal, such as `truu`.
    str = "[truu]";
    //      ^ invalid literal
    memset(&err, -1, sizeof(err));
    yyjson_doc_free(yyjson_read_opts((char *)str, strlen(str), 0, NULL, &err));
    yy_assert(err.code == YYJSON_READ_ERROR_LITERAL);
    yy_assert(err.pos == 1);
    
    str = "truu";
    //     ^ invalid literal
    memset(&err, -1, sizeof(err));
    yyjson_doc_free(yyjson_read_opts((char *)str, strlen(str), 0, NULL, &err));
    yy_assert(err.code == YYJSON_READ_ERROR_LITERAL);
    yy_assert(err.pos == 0);
    
    str = "nan";
    //     ^ invalid literal
    memset(&err, -1, sizeof(err));
    yyjson_doc_free(yyjson_read_opts((char *)str, strlen(str), 0, NULL, &err));
    yy_assert(err.code == YYJSON_READ_ERROR_LITERAL);
    yy_assert(err.pos == 0);
    
    
    
    // -------------------------------------------------------------------------
    // Failed to open a file.
    str = "/yyjson/no_such_file.test";
    //     ^ file opening failed
    memset(&err, -1, sizeof(err));
    yyjson_doc_free(yyjson_read_file(str, 0, NULL, &err));
    yy_assert(err.code == YYJSON_READ_ERROR_FILE_OPEN);
    yy_assert(err.pos == 0);
    
#endif
}



static void test_write_err_code(void) {
#if !YYJSON_DISABLE_WRITER
    yyjson_mut_doc *doc;
    yyjson_mut_val *val;
    yyjson_write_err err;
    char *json;
    yyjson_alc alc;
    
    
    
    // -------------------------------------------------------------------------
    // Success, no error.
    memset(&err, -1, sizeof(err));
    doc = yyjson_mut_doc_new(NULL);
    val = yyjson_mut_int(doc, 123);
    yyjson_mut_doc_set_root(doc, val);
    json = yyjson_mut_write_opts(doc, 0, NULL, NULL, &err);
    yy_assert(strcmp(json, "123") == 0);
    yyjson_mut_doc_free(doc);
    free(json);
    yy_assert(err.code == YYJSON_WRITE_SUCCESS);
    
    
    
    // -------------------------------------------------------------------------
    // Invalid parameter, such as NULL document.
    memset(&err, -1, sizeof(err));
    json = yyjson_mut_write_opts(NULL, 0, NULL, NULL, &err);
    yy_assert(json == NULL);
    yy_assert(err.code == YYJSON_WRITE_ERROR_INVALID_PARAMETER);
    
    
    
    // -------------------------------------------------------------------------
    // Memory allocation failure occurs.
    yyjson_alc_pool_init(&alc, NULL, 0);
    memset(&err, -1, sizeof(err));
    doc = yyjson_mut_doc_new(NULL);
    val = yyjson_mut_int(doc, 123);
    yyjson_mut_doc_set_root(doc, val);
    json = yyjson_mut_write_opts(doc, 0, &alc, NULL, &err);
    yy_assert(json == NULL);
    yyjson_mut_doc_free(doc);
    yy_assert(err.code == YYJSON_WRITE_ERROR_MEMORY_ALLOCATION);
    
    
    
    // -------------------------------------------------------------------------
    // Invalid value type in JSON document.
    memset(&err, -1, sizeof(err));
    doc = yyjson_mut_doc_new(NULL);
    val = yyjson_mut_int(doc, 123);
    unsafe_yyjson_set_type(val, YYJSON_TYPE_NONE, YYJSON_SUBTYPE_NONE);
    yyjson_mut_doc_set_root(doc, val);
    json = yyjson_mut_write_opts(doc, 0, NULL, NULL, &err);
    yy_assert(json == NULL);
    yyjson_mut_doc_free(doc);
    yy_assert(err.code == YYJSON_WRITE_ERROR_INVALID_VALUE_TYPE);
    
    
    
    // -------------------------------------------------------------------------
    // NaN or Infinity number occurs.
    memset(&err, -1, sizeof(err));
    doc = yyjson_mut_doc_new(NULL);
    val = yyjson_mut_real(doc, INFINITY);
    yyjson_mut_doc_set_root(doc, val);
    json = yyjson_mut_write_opts(doc, 0, NULL, NULL, &err);
    yy_assert(json == NULL);
    yyjson_mut_doc_free(doc);
    yy_assert(err.code == YYJSON_WRITE_ERROR_NAN_OR_INF);
    
    
    
    // -------------------------------------------------------------------------
    // Invalid unicode in string.
    memset(&err, -1, sizeof(err));
    doc = yyjson_mut_doc_new(NULL);
    val = yyjson_mut_strn(doc, "abc\x80", 4);
    yyjson_mut_doc_set_root(doc, val);
    json = yyjson_mut_write_opts(doc, 0, NULL, NULL, &err);
    yy_assert(json == NULL);
    yyjson_mut_doc_free(doc);
    yy_assert(err.code == YYJSON_WRITE_ERROR_INVALID_STRING);
    
#endif
}



static void test_locate_pos(void) {
    const char *str;
    size_t len, pos, line, col, chr;
    
    // -------------------------------------------------------------------------
    // Invalid input.
    yy_assert(!yyjson_locate_pos(NULL, 0, 0, NULL, NULL, NULL));
    
    line = col = chr = SIZE_MAX;
    yy_assert(!yyjson_locate_pos(NULL, 0, 0, &line, &col, &chr));
    yy_assert(line == 0 && col == 0 && chr == 0);
    
    yy_assert(!yyjson_locate_pos("abc", 3, 4, NULL, NULL, NULL));
    
    line = col = chr = SIZE_MAX;
    yy_assert(!yyjson_locate_pos("abc", 3, 4, &line, &col, &chr));
    yy_assert(line == 0 && col == 0 && chr == 0);
    
    // -------------------------------------------------------------------------
    // Empty.
    yy_assert(yyjson_locate_pos("", 0, 0, &line, &col, &chr));
    yy_assert(line == 1 && col == 1 && chr == 0);
    
    // -------------------------------------------------------------------------
    // Empty new line.
    yy_assert(yyjson_locate_pos("\n", 1, 0, &line, &col, &chr));
    yy_assert(line == 1 && col == 1 && chr == 0);
    yy_assert(yyjson_locate_pos("\n", 1, 1, &line, &col, &chr));
    yy_assert(line == 2 && col == 1 && chr == 1);
    yy_assert(yyjson_locate_pos("\n\n", 2, 1, &line, &col, &chr));
    yy_assert(line == 2 && col == 1 && chr == 1);
    yy_assert(yyjson_locate_pos("\n\n", 2, 2, &line, &col, &chr));
    yy_assert(line == 3 && col == 1 && chr == 2);
    
    // -------------------------------------------------------------------------
    // 1 line.
    str = "abc";
    len = strlen(str);
    for (pos = 0; pos <= len; pos++) {
        yy_assert(yyjson_locate_pos(str, len, pos, &line, &col, &chr));
        yy_assert(line == 1 && col == pos + 1 && chr == pos);
    }
    
    // -------------------------------------------------------------------------
    // 2 lines.
    str = "abc\ndef";
    len = strlen(str);
    for (pos = 0; pos <= len; pos++) {
        yy_assert(yyjson_locate_pos(str, len, pos, &line, &col, &chr));
        if (pos <= 3) {
            yy_assert(line == 1 && col == pos + 1 && chr == pos);
        } else {
            yy_assert(line == 2 && col == pos - 4 + 1 && chr == pos);
        }
    }
    
    // -------------------------------------------------------------------------
    // 3 lines.
    str = "abc\ndef\nghijklmn";
    len = strlen(str);
    for (pos = 0; pos <= len; pos++) {
        yy_assert(yyjson_locate_pos(str, len, pos, &line, &col, &chr));
        if (pos <= 3) {
            yy_assert(line == 1 && col == pos + 1 && chr == pos);
        } else if (pos <= 7) {
            yy_assert(line == 2 && col == pos - 4 + 1 && chr == pos);
        } else {
            yy_assert(line == 3 && col == pos - 8 + 1 && chr == pos);
        }
    }
    
    // -------------------------------------------------------------------------
    // Unicode.
    str = "abcé果😀"; // 1-4 byte UTF-8
    len = strlen(str);
    for (pos = 0; pos <= len; pos++) {
        size_t pos_uni = pos;
        if (4 <= pos && pos <= 5) pos_uni = 4;
        if (6 <= pos && pos <= 8) pos_uni = 5;
        if (9 <= pos && pos <= 12) pos_uni = 6;
        yy_assert(yyjson_locate_pos(str, len, pos, &line, &col, &chr));
        yy_assert(line == 1 && col == pos_uni + 1 && chr == pos_uni);
    }
    str = "abcdef"; // invalid UTF-8
    len = strlen(str);
    char buf[7] = { 0 };
    memcpy(buf, str, len + 1);
    buf[1] = 0x80;
    buf[2] = 0xF8;
    for (pos = 0; pos <= len; pos++) {
        yy_assert(yyjson_locate_pos(buf, len, pos, &line, &col, &chr));
        yy_assert(line == 1 && col == pos + 1 && chr == pos);
    }
}



yy_test_case(test_err_code) {
    test_read_err_code();
    test_write_err_code();
    test_locate_pos();
}
