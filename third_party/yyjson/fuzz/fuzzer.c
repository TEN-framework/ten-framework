#include <yyjson.h>

static void test_with_flags(const uint8_t *data, size_t size,
                            yyjson_read_flag rflg, yyjson_write_flag wflg) {
    yyjson_doc *idoc = yyjson_read((const char *)data, size, rflg);
    yyjson_mut_doc *mdoc = yyjson_doc_mut_copy(idoc, NULL);
    char *ijson = yyjson_write(idoc, wflg, NULL);
    if (ijson) free((void *)ijson);
    char *mjson = yyjson_mut_write(mdoc, wflg, NULL);
    if (mjson) free((void *)mjson);
    yyjson_doc_free(idoc);
    yyjson_mut_doc_free(mdoc);
}

int LLVMFuzzerTestOneInput(const uint8_t *data, size_t size) {
    test_with_flags(data, size,
                    YYJSON_READ_NOFLAG,
                    YYJSON_WRITE_NOFLAG);
    test_with_flags(data, size,
                    YYJSON_READ_ALLOW_TRAILING_COMMAS |
                    YYJSON_READ_ALLOW_COMMENTS |
                    YYJSON_READ_ALLOW_INF_AND_NAN,
                    YYJSON_WRITE_PRETTY |
                    YYJSON_WRITE_ESCAPE_UNICODE |
                    YYJSON_WRITE_ESCAPE_SLASHES |
                    YYJSON_WRITE_ALLOW_INF_AND_NAN);
    return 0;
}
