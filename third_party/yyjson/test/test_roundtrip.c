// This file is used to test the round-trip JSON reading and writing.

#include "yyjson.h"
#include "yy_test_utils.h"
#include <locale.h>



void test_roundtrip_locale(void) {
    
    // Check version
    yy_assert(yyjson_version() == YYJSON_VERSION_HEX);
    
    uint32_t ver = (YYJSON_VERSION_MAJOR << 16) +
                   (YYJSON_VERSION_MINOR << 8) +
                   (YYJSON_VERSION_PATCH);
    yy_assert(yyjson_version() == ver);
    
    char buf[16];
    snprintf(buf, sizeof(buf), "%d.%d.%d",
             YYJSON_VERSION_MAJOR, YYJSON_VERSION_MINOR, YYJSON_VERSION_PATCH);
    yy_assert(strcmp(YYJSON_VERSION_STRING, buf) == 0);
    ver = YYJSON_VERSION_MAJOR << 16;
    
    // Check JSON file roundtrip
#if !YYJSON_DISABLE_READER && !YYJSON_DISABLE_WRITER
    
    char dir[YY_MAX_PATH];
    yy_path_combine(dir, YYJSON_TEST_DATA_PATH, "data", "json", "test_roundtrip", NULL);
    
    int count;
    char **names = yy_dir_read(dir, &count);
    yy_assertf(names != NULL && count != 0, "read dir fail: %s\n", dir);
    
    for (int i = 0; i < count; i++) {
        char *name = names[i];
        char path[YY_MAX_PATH];
        yy_path_combine(path, dir, name, NULL);
        
        char *in_dat;
        usize in_len;
        bool read_suc = yy_file_read(path, (u8 **)&in_dat, &in_len);
        yy_assertf(read_suc == true, "file read fail: %s\n", path);
        yy_assertf(in_len > 0, "file is empty: %s\n", path);
        
        yyjson_doc *doc = yyjson_read((const char *)in_dat, in_len, 0);
        yy_assertf(doc != NULL, "json read fail: %s\n", path);
        
        usize out_len;
        char *out_dat = yyjson_write(doc, 0, &out_len);
        yy_assertf(out_dat != NULL, "json write fail: %s\n", path);
        
#if !YYJSON_DISABLE_FAST_FP_CONV
        yy_assertf(in_len == out_len && memcmp(in_dat, out_dat, in_len) == 0,
                   "roundtrip fail: %s\nin:  %.*s\nout: %.*s\n",
                   path, (int)in_len, in_dat, (int)out_len, out_dat);
#endif
        
        usize mout_len;
        yyjson_mut_doc *mdoc = yyjson_doc_mut_copy(doc, NULL);
        char *mout_dat = yyjson_mut_write(mdoc, 0, &mout_len);
        yy_assertf(mout_dat != NULL, "json mut write fail: %s\n", path);
        
#if !YYJSON_DISABLE_FAST_FP_CONV
        yy_assertf(in_len == mout_len && memcmp(in_dat, mout_dat, in_len) == 0,
                   "roundtrip (mut) fail: %s\nin:  %.*s\nout: %.*s\n",
                   path, (int)in_len, in_dat, (int)mout_len, mout_dat);
#endif
        
        yyjson_doc_free(doc);
        yyjson_mut_doc_free(mdoc);
        free(in_dat);
        free(out_dat);
        free(mout_dat);
    }
    
    yy_dir_free(names);
    
#endif
}


yy_test_case(test_roundtrip) {
    // decimal point is '.'
    setlocale(LC_ALL, "C");
    test_roundtrip_locale();
    
    // decimal point is ','
    setlocale(LC_ALL, "fr_FR");
    test_roundtrip_locale();
}
