// This file is used to test the built-in pool memory allocator. 

#include "yyjson.h"
#include "yy_test_utils.h"

#define NUM_PTR 16
#define BUF_SIZE 1024

static void test_alc_pool_init(void) {
    yyjson_alc alc;
    usize size;
    void *buf;
    
    yy_assert(!yyjson_alc_pool_init(NULL, NULL, 0));
    
    memset(&alc, 0, sizeof(alc));
    yy_assert(!yyjson_alc_pool_init(&alc, NULL, 0));
    yy_assert(!alc.malloc(NULL, 1));
    yy_assert(!alc.realloc(NULL, NULL, 0, 1));
    alc.free(NULL, NULL);
    
    memset(&alc, 0, sizeof(alc));
    yy_assert(!yyjson_alc_pool_init(&alc, NULL, 1024));
    yy_assert(!alc.malloc(NULL, 1));
    yy_assert(!alc.realloc(NULL, NULL, 0, 1));
    alc.free(NULL, NULL);
    
    char small_buf[10];
    memset(&alc, 0, sizeof(alc));
    yy_assert(!yyjson_alc_pool_init(&alc, small_buf, sizeof(small_buf)));
    yy_assert(!alc.malloc(NULL, 1));
    yy_assert(!alc.realloc(NULL, NULL, 0, 1));
    alc.free(NULL, NULL);
    
    size = 8 * sizeof(void *) - 1;
    buf = malloc(size);
    yy_assert(!yyjson_alc_pool_init(&alc, buf, size));
    free(buf);
}

static void test_alc_pool_func(void) {
    yyjson_alc alc;
    void *ptr[NUM_PTR];
    usize ptr_size[NUM_PTR];
    void *buf = malloc(BUF_SIZE);
    yy_assert(yyjson_alc_pool_init(&alc, buf, BUF_SIZE));
    
    
    // suc and fail
    ptr[0] = alc.malloc(alc.ctx, BUF_SIZE / 2);
    yy_assert(ptr[0]);
    memset(ptr[0], 0, BUF_SIZE / 2);
    ptr[1] = alc.malloc(alc.ctx, BUF_SIZE / 2);
    yy_assert(!ptr[1]);
    alc.free(alc.ctx, ptr[0]);
    
    
    // alc large, free, alc again
    for (int i = 0; i < NUM_PTR; i++) {
        ptr[i] = alc.malloc(alc.ctx, 32);
        yy_assert(ptr[i]);
        memset(ptr[i], 0, 32);
    }
    for (int i = 0; i < NUM_PTR; i += 2) {
        alc.free(alc.ctx, ptr[i]);
    }
    for (int i = 0; i < NUM_PTR; i += 2) {
        ptr[i] = alc.malloc(alc.ctx, 16);
        yy_assert(ptr[i]);
        memset(ptr[i], 0, 16);
    }
    for (int i = NUM_PTR - 1; i >= 0; i--) {
        alc.free(alc.ctx, ptr[i]);
    }
    
    
    // alc large, free, alc small
    for (int i = 0; i < NUM_PTR; i++) {
        ptr[i] = alc.malloc(alc.ctx, 32);
        yy_assert(ptr[i]);
        memset(ptr[i], 0, 32);
    }
    for (int i = 0; i < NUM_PTR; i += 2) {
        alc.free(alc.ctx, ptr[i]);
    }
    for (int i = 0; i < NUM_PTR; i += 2) {
        ptr[i] = alc.malloc(alc.ctx, 1);
        yy_assert(ptr[i]);
        memset(ptr[i], 0, 1);
    }
    for (int i = NUM_PTR - 1; i >= 0; i--) {
        alc.free(alc.ctx, ptr[i]);
    }
    
    
    // alc small, free, alc large
    for (int i = 0; i < NUM_PTR; i++) {
        ptr[i] = alc.malloc(alc.ctx, 16);
        yy_assert(ptr[i]);
        memset(ptr[i], 0, 16);
    }
    for (int i = 0; i < NUM_PTR; i += 2) {
        alc.free(alc.ctx, ptr[i]);
    }
    for (int i = 0; i < NUM_PTR; i += 2) {
        ptr[i] = alc.malloc(alc.ctx, 32);
        yy_assert(ptr[i]);
        memset(ptr[i], 0, 32);
    }
    for (int i = 0; i < NUM_PTR; i++) {
        alc.free(alc.ctx, ptr[i]);
    }
    
    
    // alc small, realloc large
    for (int i = 0; i < NUM_PTR / 2; i++) {
        ptr[i] = alc.malloc(alc.ctx, 8);
        yy_assert(ptr[i]);
        memset(ptr[i], 0, 8);
    }
    for (int i = 0; i < NUM_PTR / 2; i += 2) {
        alc.free(alc.ctx, ptr[i]);
    }
    for (int i = 1; i < NUM_PTR / 2; i += 2) {
        ptr[i] = alc.realloc(alc.ctx, ptr[i], 8, 32);
        yy_assert(ptr[i]);
        memset(ptr[i], 0, 32);
    }
    for (int i = 0; i < NUM_PTR / 2; i += 2) {
        ptr[i] = alc.malloc(alc.ctx, 16);
        yy_assert(ptr[i]);
        memset(ptr[i], 0, 16);
    }
    for (int i = 0; i < NUM_PTR / 2; i++) {
        alc.free(alc.ctx, ptr[i]);
    }
    
    
    // same space realloc
    ptr[0] = alc.malloc(alc.ctx, 64);
    ptr[0] = alc.realloc(alc.ctx, ptr[0], 64, 128);
    yy_assert(ptr[0]);
    alc.free(alc.ctx, ptr[0]);
    
    
    // random
    memset(ptr, 0, sizeof(ptr));
    memset(ptr_size, 0, sizeof(ptr_size));
    yy_rand_reset(0);
    for (int p = 0; p < 10000; p++) {
        int i = yy_rand_u32_uniform(NUM_PTR);
        usize inc = yy_rand_u32_uniform(127) + 1;
        void *tmp = ptr[i];
        usize tmp_size = ptr_size[i];
        if (tmp) {
            bool is_realloc = (yy_rand_u32_uniform(4) == 0);
            if (is_realloc) {
                tmp = alc.realloc(alc.ctx, tmp, tmp_size, tmp_size + inc);
                if (tmp) {
                    ptr[i] = tmp;
                    ptr_size[i] += inc;
                }
            } else {
                alc.free(alc.ctx, tmp);
                ptr[i] = NULL;
                ptr_size[i] = 0;
            }
        } else {
            tmp = alc.malloc(alc.ctx, inc);
            if (tmp) memset(tmp, 0xFF, inc);
            ptr[i] = tmp;
            ptr_size[i] = tmp ? inc : 0;
        }
    }
    for (int i = 0; i < NUM_PTR; i++) {
        if (ptr[i]) alc.free(alc.ctx, ptr[i]);
    }
    
    
    // cleanup
    free(buf);
}

// test allocator for different length json
static void test_alc_pool_read(void) {
#if !YYJSON_DISABLE_READER
    yyjson_read_flag flg;
    size_t buf_len;
    void *buf;
    yyjson_alc alc;
    yyjson_doc *doc;
    
    for (size_t n = 1; n <= 1000; n++) {
        // e.g. n = 3: [1,1,1]
        size_t len = 1 + n * 2;
        char *str = malloc(len);
        str[0] = '[';
        for (size_t i = 0; i < n; i++) {
            str[i * 2 + 1] = '1';
            str[i * 2 + 2] = ',';
        }
        str[len - 1] = ']';
        
        
        // default flag
        flg = 0;
        buf_len = yyjson_read_max_memory_usage(len, flg);
        buf = malloc(buf_len);
        yyjson_alc_pool_init(&alc, buf, buf_len);
        
        doc = yyjson_read_opts(str, len, flg, &alc, NULL);
        yy_assert(doc);
        yy_assert(doc->val_read == n + 1);
        yyjson_doc_free(doc);

        free(buf);
        
        
        // instu flag
        str = realloc(str, len + YYJSON_PADDING_SIZE);
        memset(str + len, 0, YYJSON_PADDING_SIZE);
        
        flg = YYJSON_READ_INSITU;
        buf_len = yyjson_read_max_memory_usage(len, flg);
        buf = malloc(buf_len);
        yyjson_alc_pool_init(&alc, buf, buf_len);
        
        doc = yyjson_read_opts(str, len, flg, &alc, NULL);
        yy_assert(doc);
        yy_assert(doc->val_read == n + 1);
        yyjson_doc_free(doc);
        
        free(buf);
        
        
        // cleanup
        free(str);
    }
#endif
}

static void test_alc_dyn(void) {
    yyjson_alc *alc;
    void *ptr[NUM_PTR];
    usize ptr_size[NUM_PTR];
    
    
    // new and destroy
    alc = yyjson_alc_dyn_new();
    yy_assert(alc);
    yy_assert(!alc->malloc(alc->ctx, SIZE_MAX));
    yy_assert(!alc->malloc(alc->ctx, SIZE_MAX - 16));
    yyjson_alc_dyn_free(alc);
    yyjson_alc_dyn_free(NULL);
    
    
    // new, alloc, destroy
    alc = yyjson_alc_dyn_new();
    ptr[0] = alc->malloc(alc->ctx, 0x100);
    yy_assert(ptr[0]);
    memset(ptr[0], 0x100, 0xFF);
    alc->free(alc->ctx, ptr[0]);
    yyjson_alc_dyn_free(alc);
    
    
    // new, alloc-free, destroy
    alc = yyjson_alc_dyn_new();
    yy_rand_reset(0);
    for (int p = 0; p < 1000; p++) {
        usize len = yy_rand_u32_uniform(0x4000) + 1;
        ptr[0] = alc->malloc(alc->ctx, len);
        yy_assert(ptr[0]);
        memset(ptr[0], len, 0xFF);
        alc->free(alc->ctx, ptr[0]);
    }
    yyjson_alc_dyn_free(alc);
    
    
    // new, alloc-free, destroy
    alc = yyjson_alc_dyn_new();
    yy_rand_reset(0);
    for (int p = 0; p < 1000; p++) {
        usize len = yy_rand_u32_uniform(0x4000) + 1;
        ptr[0] = alc->malloc(alc->ctx, len);
        yy_assert(ptr[0]);
        memset(ptr[0], len, 0xFF);
        alc->free(alc->ctx, ptr[0]);
    }
    yyjson_alc_dyn_free(alc);
    
    
    // new, alloc-realloc-free, destroy
    alc = yyjson_alc_dyn_new();
    yy_rand_reset(0);
    for (int p = 0; p < 1000; p++) {
        usize len = yy_rand_u32_uniform(0x4000) + 1;
        usize inc = yy_rand_u32_uniform(0x4000) + 1;
        ptr[0] = alc->malloc(alc->ctx, len);
        yy_assert(ptr[0]);
        memset(ptr[0], len, 0xFF);
        ptr[0] = alc->realloc(alc->ctx, ptr[0], len, len + inc);
        yy_assert(ptr[0]);
        memset(ptr[0], len + inc, 0xFF);
        alc->free(alc->ctx, ptr[0]);
    }
    yyjson_alc_dyn_free(alc);
    
    
    // random
    alc = yyjson_alc_dyn_new();
    yy_rand_reset(0);
    memset(ptr, 0, sizeof(ptr));
    memset(ptr_size, 0, sizeof(ptr_size));
    for (int p = 0; p < 10000; p++) {
        int i = yy_rand_u32_uniform(NUM_PTR);
        usize inc = yy_rand_u32_uniform(0x4000) + 1;
        void *tmp = ptr[i];
        usize tmp_size = ptr_size[i];
        if (tmp) {
            bool is_realloc = (yy_rand_u32_uniform(4) == 0);
            if (is_realloc) {
                tmp = alc->realloc(alc->ctx, tmp, tmp_size, tmp_size + inc);
                if (tmp) {
                    memset(tmp, 0xFF, tmp_size + inc);
                    ptr[i] = tmp;
                    ptr_size[i] += inc;
                }
            } else {
                alc->free(alc->ctx, tmp);
                ptr[i] = NULL;
                ptr_size[i] = 0;
            }
        } else {
            tmp = alc->malloc(alc->ctx, inc);
            if (tmp) memset(tmp, 0xFF, inc);
            ptr[i] = tmp;
            ptr_size[i] = tmp ? inc : 0;
        }
    }
    yyjson_alc_dyn_free(alc);
}


yy_test_case(test_allocator) {
    test_alc_pool_init();
    test_alc_pool_func();
    test_alc_pool_read();
    test_alc_dyn();
}
