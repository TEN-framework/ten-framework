// This file is used to test the `JSON Pointer` functions.

#include "yyjson.h"
#include "yy_test_utils.h"

#if !YYJSON_DISABLE_READER && !YYJSON_DISABLE_WRITER && !YYJSON_DISABLE_UTILS

#if defined(__clang__)
#pragma clang diagnostic ignored "-Wdeprecated-declarations"
#elif defined(__GNUC__)
#pragma GCC diagnostic ignored "-Wdeprecated-declarations"
#elif defined(_MSC_VER)
#pragma warning(disable:4996)
#endif

// JSON pointer operation
typedef enum {
    PTR_OP_GET,
    PTR_OP_ADD,
    PTR_OP_SET,
    PTR_OP_REPLACE,
    PTR_OP_REMOVE,
} ptr_op;

// JSON pointer data, expected:
//  val = src.get(ptr)
//  suc = src.add(ptr, val), src->dst
//  suc = src.set(ptr, val), src->dst
//  old = src.replace(ptr, val), src->dst
//  old = src.remove(ptr), src->dst
typedef struct {
    ptr_op op;          // operation
    
    bool create_parent; // param
    const char *src;    // source document, `empty_root` for empty root
    const char *ptr;    // pointer
    size_t ptr_len;     // pointer length, 0 to use strlen()
    const char *val;    // json value
    const char *dst;    // destination document, `empty_root` for empty root
    
    const char *key;    // used for ctx_append
    const char *ctn;    // expected ctx.ctn (target value's parent)
    const char *pre;    // expected ctx.pre (target value's previous)
    const char *old;    // expected ctx.old (removed value)
    yyjson_ptr_code err;// expected error code
    size_t pos;         // expected error pos
} ptr_data;

static const char *empty_root = "empty";

// -----------------------------------------------------------------------------
// assert (str == expected)
static void assert_str_eq(const char *str, const char *exp) {
    if (!str && !exp) return;
    if (str && !exp) yy_assertf(false, "expected NULL, but <%s>", str);
    if (!str && exp) yy_assertf(false, "expected <%s>, but NULL", exp);
    yy_assertf(strcmp(str, exp) == 0, "expected <%s>, but <%s>", exp, str);
}

// assert (str(val) == json)
static void assert_val_eq(yyjson_val *val, const char *json) {
    char *str = yyjson_val_write(val, 0, NULL);
    assert_str_eq(str, json);
    if (str) free(str);
}

// assert (str(val) == json)
static void assert_mut_val_eq(yyjson_mut_val *val, const char *json) {
    char *str = yyjson_mut_val_write(val, 0, NULL);
    assert_str_eq(str, json);
    if (str) free(str);
}

// assert (str(doc) == json)
static void assert_mut_doc_eq(yyjson_mut_doc *doc, const char *json) {
    if (doc && !doc->root) {
        assert_str_eq(empty_root, json);
    } else {
        char *str = yyjson_mut_write(doc, 0, NULL);
        assert_str_eq(str, json);
        if (str) free(str);
    }
}

// assert (ctx == expected)
static void assert_ctx_eq(yyjson_ptr_ctx *ctx, ptr_data *data) {
    if (data) {
        size_t invalid = SIZE_MAX;
        yy_assert(ctx->ctn != (void *)invalid);
        yy_assert(ctx->pre != (void *)invalid);
        yy_assert(ctx->old != (void *)invalid);
        assert_mut_val_eq(ctx->ctn, data->ctn);
        assert_mut_val_eq(ctx->pre, data->pre);
        assert_mut_val_eq(ctx->old, data->old);
    } else {
        yy_assert(ctx->ctn == NULL);
        yy_assert(ctx->pre == NULL);
        yy_assert(ctx->old == NULL);
    }
}

static void assert_err(yyjson_ptr_err *err, ptr_data *data) {
    yy_assert(err->code == data->err);
    yy_assert(err->pos == data->pos);
    if (err->code) yy_assert(err->msg != NULL);
    else yy_assert(err->msg == NULL);
}

static void assert_err_param(yyjson_ptr_err *err) {
    yy_assert(err->code == YYJSON_PTR_ERR_PARAMETER);
    yy_assert(err->pos == 0);
    yy_assert(err->msg != NULL);
}

// -----------------------------------------------------------------------------
// test single operation
static void test_ptr_op(ptr_data data) {
    // source
    yyjson_doc *idoc = yyjson_read(data.src, data.src ? strlen(data.src) : 0, 0);
    yyjson_val *iroot = yyjson_doc_get_root(idoc);
    yyjson_mut_doc *mdoc = yyjson_doc_mut_copy(idoc, NULL);
    yyjson_mut_val *mroot = yyjson_mut_doc_get_root(mdoc);
    if (data.src == empty_root) {
        idoc = yyjson_read("1", 1, 0);
        idoc->root = NULL;
        mdoc = yyjson_mut_doc_new(NULL);
    }
    
    // input value
    yyjson_doc *ival_doc = yyjson_read(data.val, data.val ? strlen(data.val) : 0, 0);
    yyjson_mut_doc *mval_doc = yyjson_doc_mut_copy(ival_doc, NULL);
    yyjson_mut_val *mval = yyjson_mut_doc_get_root(mval_doc);
    
    // pointer
    const char *ptr = data.ptr;
    size_t ptr_len = data.ptr_len;
    if (ptr && !ptr_len) ptr_len = strlen(ptr);
    
    // temp value
    yyjson_val *iret;
    yyjson_mut_doc *doc;
    yyjson_mut_val *ret, *val, *root;
    yyjson_ptr_err err;
    yyjson_ptr_ctx ctx;
    bool suc;
    
#define mut_before() do { \
    memset(&err, -1, sizeof(err)); \
    memset(&ctx, -1, sizeof(ctx)); \
    doc = yyjson_mut_doc_mut_copy(mdoc, NULL); \
    val = yyjson_mut_val_mut_copy(doc, mval); \
    root = doc? doc->root : NULL;\
} while(false)
    
#define mut_after() do { \
    yyjson_mut_doc_free(doc); \
} while (false)
    
    // val = src.get(ptr)
    if (data.op == PTR_OP_GET) {
        // -----------------------------
        // doc.get
        iret = yyjson_doc_ptr_get(NULL, NULL);
        yy_assert(iret == NULL);
        if (data.ptr_len == 0) {
            iret = yyjson_doc_ptr_get(idoc, ptr);
            assert_val_eq(iret, data.val);
            iret = yyjson_doc_get_pointer(idoc, ptr); // deprecated
            assert_val_eq(iret, data.val);
        }
        
        iret = yyjson_doc_ptr_getn(NULL, NULL, ptr_len);
        yy_assert(iret == NULL);
        iret = yyjson_doc_ptr_getn(idoc, ptr, ptr_len);
        assert_val_eq(iret, data.val);
        iret = yyjson_doc_get_pointern(idoc, ptr, ptr_len); // deprecated
        assert_val_eq(iret, data.val);
        
        iret = yyjson_doc_ptr_getx(NULL, NULL, ptr_len, NULL);
        yy_assert(iret == NULL);
        iret = yyjson_doc_ptr_getx(idoc, ptr, ptr_len, NULL);
        assert_val_eq(iret, data.val);
        
        memset(&err, -1, sizeof(err));
        iret = yyjson_doc_ptr_getx(NULL, NULL, ptr_len, &err);
        yy_assert(iret == NULL);
        assert_err_param(&err);
        
        memset(&err, -1, sizeof(err));
        iret = yyjson_doc_ptr_getx(idoc, ptr, ptr_len, &err);
        assert_val_eq(iret, data.val);
        assert_err(&err, &data);
        
        // -----------------------------
        // val.get
        iret = yyjson_ptr_get(NULL, NULL);
        yy_assert(iret == NULL);
        if (data.ptr_len == 0) {
            iret = yyjson_ptr_get(iroot, ptr);
            assert_val_eq(iret, data.val);
            iret = yyjson_get_pointer(iroot, ptr); // deprecated
            assert_val_eq(iret, data.val);
        }
        
        iret = yyjson_ptr_getn(NULL, NULL, ptr_len);
        assert_val_eq(iret, NULL);
        iret = yyjson_ptr_getn(iroot, ptr, ptr_len);
        assert_val_eq(iret, data.val);
        iret = yyjson_get_pointern(iroot, ptr, ptr_len); // deprecated
        assert_val_eq(iret, data.val);
        if (iroot && ptr && ptr_len && *ptr == '/') {
            iret = unsafe_yyjson_get_pointer(iroot, ptr, ptr_len); // deprecated
            assert_val_eq(iret, data.val);
        }
        
        iret = yyjson_ptr_getx(NULL, NULL, ptr_len, NULL);
        assert_val_eq(iret, NULL);
        iret = yyjson_ptr_getx(iroot, ptr, ptr_len, NULL);
        assert_val_eq(iret, data.val);
        
        memset(&err, -1, sizeof(err));
        iret = yyjson_ptr_getx(NULL, NULL, ptr_len, &err);
        yy_assert(iret == NULL);
        assert_err_param(&err);
        
        memset(&err, -1, sizeof(err));
        iret = yyjson_ptr_getx(iroot, ptr, ptr_len, &err);
        assert_val_eq(iret, data.val);
        if (data.src == empty_root) {
            assert_err_param(&err);
        } else {
            assert_err(&err, &data);
        }
        
        // -----------------------------
        // mut_doc.get
        ret = yyjson_mut_doc_ptr_get(NULL, NULL);
        yy_assert(ret == NULL);
        if (data.ptr_len == 0) {
            ret = yyjson_mut_doc_ptr_get(mdoc, ptr);
            assert_mut_val_eq(ret, data.val);
            ret = yyjson_mut_doc_get_pointer(mdoc, ptr); // deprecated
            assert_mut_val_eq(ret, data.val);
        }
        
        ret = yyjson_mut_doc_ptr_getn(NULL, NULL, ptr_len);
        yy_assert(ret == NULL);
        ret = yyjson_mut_doc_ptr_getn(mdoc, ptr, ptr_len);
        assert_mut_val_eq(ret, data.val);
        ret = yyjson_mut_doc_get_pointern(mdoc, ptr, ptr_len); // deprecated
        assert_mut_val_eq(ret, data.val);
        
        ret = yyjson_mut_doc_ptr_getx(NULL, NULL, ptr_len, NULL, NULL);
        yy_assert(ret == NULL);
        ret = yyjson_mut_doc_ptr_getx(mdoc, ptr, ptr_len, NULL, NULL);
        assert_mut_val_eq(ret, data.val);
        
        memset(&err, -1, sizeof(err));
        memset(&ctx, -1, sizeof(ctx));
        ret = yyjson_mut_doc_ptr_getx(NULL, NULL, ptr_len, &ctx, &err);
        yy_assert(ret == NULL);
        assert_ctx_eq(&ctx, NULL);
        assert_err_param(&err);
        
        memset(&err, -1, sizeof(err));
        memset(&ctx, -1, sizeof(ctx));
        ret = yyjson_mut_doc_ptr_getx(mdoc, ptr, ptr_len, &ctx, &err);
        assert_mut_val_eq(ret, data.val);
        assert_ctx_eq(&ctx, &data);
        assert_err(&err, &data);
        
        assert_mut_doc_eq(mdoc, data.src); /* doc should not be modified */
        
        // -----------------------------
        // mut_val.get
        ret = yyjson_mut_ptr_get(NULL, NULL);
        yy_assert(ret == NULL);
        if (data.ptr_len == 0) {
            ret = yyjson_mut_ptr_get(mroot, ptr);
            assert_mut_val_eq(ret, data.val);
            ret = yyjson_mut_get_pointer(mroot, ptr); // deprecated
            assert_mut_val_eq(ret, data.val);
        }
        
        ret = yyjson_mut_ptr_getn(NULL, NULL, ptr_len);
        yy_assert(ret == NULL);
        ret = yyjson_mut_ptr_getn(mroot, ptr, ptr_len);
        assert_mut_val_eq(ret, data.val);
        ret = yyjson_mut_get_pointern(mroot, ptr, ptr_len); // deprecated
        assert_mut_val_eq(ret, data.val);
        if (mroot && ptr && ptr_len && *ptr == '/') {
            ret = unsafe_yyjson_mut_get_pointer(mroot, ptr, ptr_len); // deprecated
            assert_mut_val_eq(ret, data.val);
        }
        
        ret = yyjson_mut_ptr_getx(NULL, NULL, ptr_len, NULL, NULL);
        yy_assert(ret == NULL);
        ret = yyjson_mut_ptr_getx(mroot, ptr, ptr_len, NULL, NULL);
        assert_mut_val_eq(ret, data.val);
        
        memset(&err, -1, sizeof(err));
        memset(&ctx, -1, sizeof(ctx));
        ret = yyjson_mut_ptr_getx(NULL, NULL, ptr_len, &ctx, &err);
        yy_assert(ret == NULL);
        assert_ctx_eq(&ctx, NULL);
        assert_err_param(&err);
        
        memset(&err, -1, sizeof(err));
        memset(&ctx, -1, sizeof(ctx));
        ret = yyjson_mut_ptr_getx(mroot, ptr, ptr_len, &ctx, &err);
        assert_mut_val_eq(ret, data.val);
        assert_ctx_eq(&ctx, &data);
        if (data.err == YYJSON_PTR_ERR_NULL_ROOT) {
            assert_err_param(&err);
        } else {
            assert_err(&err, &data);
        }
        
        assert_mut_doc_eq(mdoc, data.src); /* doc should not be modified */
    }
    
    // suc = src.add(ptr, val), src->dst
    if (data.op == PTR_OP_ADD) {
        // -----------------------------
        // mut_doc.add
        if (data.create_parent) {
            if (data.ptr_len == 0) { // can use strlen()
                mut_before();
                suc = yyjson_mut_doc_ptr_add(NULL, NULL, NULL);
                yy_assert(suc == false);
                mut_after();
                
                mut_before();
                suc = yyjson_mut_doc_ptr_add(doc, ptr, val);
                yy_assert(suc == (data.err == 0));
                assert_mut_doc_eq(doc, data.dst);
                mut_after();
            }
            
            mut_before();
            suc = yyjson_mut_doc_ptr_addn(NULL, NULL, ptr_len, NULL);
            yy_assert(suc == false);
            mut_after();
            
            mut_before();
            suc = yyjson_mut_doc_ptr_addn(doc, ptr, ptr_len, val);
            yy_assert(suc == (data.err == 0));
            assert_mut_doc_eq(doc, data.dst);
            mut_after();
        }
        
        mut_before();
        suc = yyjson_mut_doc_ptr_addx(NULL, NULL, ptr_len, NULL, data.create_parent, NULL, NULL);
        yy_assert(suc == false);
        mut_after();
        
        mut_before();
        suc = yyjson_mut_doc_ptr_addx(doc, ptr, ptr_len, val, data.create_parent, NULL, NULL);
        yy_assert(suc == (data.err == 0));
        assert_mut_doc_eq(doc, data.dst);
        mut_after();
        
        mut_before();
        suc = yyjson_mut_doc_ptr_addx(NULL, NULL, ptr_len, NULL, data.create_parent, &ctx, &err);
        yy_assert(suc == false);
        assert_err_param(&err);
        assert_ctx_eq(&ctx, NULL);
        mut_after();
        
        mut_before();
        suc = yyjson_mut_doc_ptr_addx(doc, ptr, ptr_len, val, data.create_parent, &ctx, &err);
        yy_assert(suc == (data.err == 0));
        assert_err(&err, &data);
        assert_mut_doc_eq(doc, data.dst);
        assert_ctx_eq(&ctx, &data);
        mut_after();
        
        // -----------------------------
        // mut_val.add
        if (data.create_parent) {
            if (data.ptr_len == 0) { // can use strlen()
                mut_before();
                suc = yyjson_mut_ptr_add(NULL, NULL, NULL, NULL);
                yy_assert(suc == false);
                mut_after();
                
                mut_before();
                suc = yyjson_mut_ptr_add(root, ptr, val, doc);
                if (root) {
                    yy_assert(suc == (data.err == 0));
                    assert_mut_val_eq(root, data.dst);
                } else {
                    yy_assert(suc == false);
                }
                mut_after();
            }
            
            mut_before();
            suc = yyjson_mut_ptr_addn(NULL, NULL, ptr_len, NULL, NULL);
            yy_assert(suc == false);
            mut_after();
            
            mut_before();
            suc = yyjson_mut_ptr_addn(root, ptr, ptr_len, val, doc);
            if (root) {
                yy_assert(suc == (data.err == 0));
                assert_mut_val_eq(root, data.dst);
            } else {
                yy_assert(suc == false);
            }
            mut_after();
        }
        
        mut_before();
        suc = yyjson_mut_ptr_addx(NULL, NULL, ptr_len, NULL, NULL, data.create_parent, NULL, NULL);
        yy_assert(suc == false);
        mut_after();
        
        mut_before();
        suc = yyjson_mut_ptr_addx(root, ptr, ptr_len, val, doc, data.create_parent, NULL, NULL);
        if (root) {
            yy_assert(suc == (data.err == 0));
            assert_mut_val_eq(root, data.dst);
        } else {
            yy_assert(suc == false);
        }
        mut_after();
        
        mut_before();
        suc = yyjson_mut_ptr_addx(NULL, NULL, ptr_len, NULL, doc, data.create_parent, &ctx, &err);
        yy_assert(suc == false);
        assert_ctx_eq(&ctx, NULL);
        assert_err_param(&err);
        mut_after();
        
        mut_before();
        suc = yyjson_mut_ptr_addx(root, ptr, ptr_len, val, doc, data.create_parent, &ctx, &err);
        if (root) {
            yy_assert(suc == (data.err == 0));
            assert_err(&err, &data);
            assert_mut_doc_eq(doc, data.dst);
            assert_ctx_eq(&ctx, &data);
        } else {
            yy_assert(suc == false);
            assert_err_param(&err);
        }
        mut_after();
    }
    
    // suc = src.set(ptr, val), src->dst
    if (data.op == PTR_OP_SET) {
        // -----------------------------
        // mut_doc.set
        if (data.create_parent) {
            if (data.ptr_len == 0) { // can use strlen()
                mut_before();
                suc = yyjson_mut_doc_ptr_set(NULL, NULL, NULL);
                yy_assert(suc == false);
                mut_after();
                
                mut_before();
                suc = yyjson_mut_doc_ptr_set(doc, ptr, val);
                yy_assert(suc == (data.err == 0));
                assert_mut_doc_eq(doc, data.dst);
                mut_after();
            }
            
            mut_before();
            suc = yyjson_mut_doc_ptr_setn(NULL, NULL, ptr_len, NULL);
            yy_assert(suc == false);
            mut_after();
            
            mut_before();
            suc = yyjson_mut_doc_ptr_setn(doc, ptr, ptr_len, val);
            yy_assert(suc == (data.err == 0));
            assert_mut_doc_eq(doc, data.dst);
            mut_after();
        }
        
        mut_before();
        suc = yyjson_mut_doc_ptr_setx(NULL, NULL, ptr_len, NULL, data.create_parent, NULL, NULL);
        yy_assert(suc == false);
        mut_after();
        
        mut_before();
        suc = yyjson_mut_doc_ptr_setx(doc, ptr, ptr_len, val, data.create_parent, NULL, NULL);
        yy_assert(suc == (data.err == 0));
        assert_mut_doc_eq(doc, data.dst);
        mut_after();
        
        mut_before();
        suc = yyjson_mut_doc_ptr_setx(NULL, NULL, ptr_len, NULL, data.create_parent, &ctx, &err);
        yy_assert(suc == false);
        assert_err_param(&err);
        assert_ctx_eq(&ctx, NULL);
        mut_after();
        
        mut_before();
        suc = yyjson_mut_doc_ptr_setx(doc, ptr, ptr_len, val, data.create_parent, &ctx, &err);
        yy_assert(suc == (data.err == 0));
        assert_err(&err, &data);
        assert_mut_doc_eq(doc, data.dst);
        assert_ctx_eq(&ctx, &data);
        mut_after();
        
        // -----------------------------
        // mut_val.set
        if (data.create_parent) {
            if (data.ptr_len == 0) { // can use strlen()
                mut_before();
                suc = yyjson_mut_ptr_set(NULL, NULL, NULL, NULL);
                yy_assert(suc == false);
                mut_after();
                
                mut_before();
                suc = yyjson_mut_ptr_set(root, ptr, val, doc);
                if (root) {
                    yy_assert(suc == (data.err == 0));
                    assert_mut_val_eq(root, data.dst);
                } else {
                    yy_assert(suc == false);
                }
                mut_after();
            }
            
            mut_before();
            suc = yyjson_mut_ptr_setn(NULL, NULL, ptr_len, NULL, NULL);
            yy_assert(suc == false);
            mut_after();
            
            mut_before();
            suc = yyjson_mut_ptr_setn(root, ptr, ptr_len, val, doc);
            if (root) {
                yy_assert(suc == (data.err == 0));
                assert_mut_val_eq(root, data.dst);
            } else {
                yy_assert(suc == false);
            }
            mut_after();
        }
        
        mut_before();
        suc = yyjson_mut_ptr_setx(NULL, NULL, ptr_len, NULL, NULL, data.create_parent, NULL, NULL);
        yy_assert(suc == false);
        mut_after();
        
        mut_before();
        suc = yyjson_mut_ptr_setx(root, ptr, ptr_len, val, doc, data.create_parent, NULL, NULL);
        if (root) {
            if (ptr_len) {
                yy_assert(suc == (data.err == 0));
                assert_mut_val_eq(root, data.dst);
            } else {
                yy_assert(suc == false);
                assert_mut_doc_eq(doc, data.src);
            }
        } else {
            yy_assert(suc == false);
        }
        mut_after();
        
        mut_before();
        suc = yyjson_mut_ptr_setx(NULL, NULL, ptr_len, NULL, doc, data.create_parent, &ctx, &err);
        yy_assert(suc == false);
        assert_ctx_eq(&ctx, NULL);
        assert_err_param(&err);
        mut_after();
         
        mut_before();
        suc = yyjson_mut_ptr_setx(root, ptr, ptr_len, val, doc, data.create_parent, &ctx, &err);
        if (root) {
            if (ptr_len) {
                yy_assert(suc == (data.err == 0));
                assert_err(&err, &data);
                assert_mut_doc_eq(doc, data.dst);
                assert_ctx_eq(&ctx, &data);
            } else {
                yy_assert(suc == false);
                yy_assert(err.code == YYJSON_PTR_ERR_SET_ROOT);
                assert_mut_doc_eq(doc, data.src);
                assert_ctx_eq(&ctx, NULL);
            }
        } else {
            yy_assert(suc == false);
            assert_err_param(&err);
        }
        mut_after();
    }
    
    // old = src.replace(ptr, val), src->dst
    if (data.op == PTR_OP_REPLACE) {
        // -----------------------------
        // mut_doc.replace
        if (data.ptr_len == 0) { // can use strlen()
            mut_before();
            ret = yyjson_mut_doc_ptr_replace(NULL, NULL, NULL);
            yy_assert(ret == NULL);
            mut_after();
            
            mut_before();
            ret = yyjson_mut_doc_ptr_replace(doc, ptr, val);
            assert_mut_val_eq(ret, data.old);
            assert_mut_doc_eq(doc, data.dst);
            mut_after();
        }
        
        mut_before();
        ret = yyjson_mut_doc_ptr_replacen(NULL, NULL, ptr_len, NULL);
        yy_assert(ret == NULL);
        mut_after();
        
        mut_before();
        ret = yyjson_mut_doc_ptr_replacen(doc, ptr, ptr_len, val);
        assert_mut_val_eq(ret, data.old);
        assert_mut_doc_eq(doc, data.dst);
        mut_after();
        
        mut_before();
        ret = yyjson_mut_doc_ptr_replacex(NULL, NULL, ptr_len, NULL, NULL, NULL);
        yy_assert(ret == NULL);
        mut_after();
        
        mut_before();
        ret = yyjson_mut_doc_ptr_replacex(doc, ptr, ptr_len, val, NULL, NULL);
        assert_mut_val_eq(ret, data.old);
        assert_mut_doc_eq(doc, data.dst);
        mut_after();
        
        mut_before();
        ret = yyjson_mut_doc_ptr_replacex(NULL, NULL, ptr_len, NULL, &ctx, &err);
        yy_assert(ret == NULL);
        assert_err_param(&err);
        assert_ctx_eq(&ctx, NULL);
        mut_after();
        
        mut_before();
        ret = yyjson_mut_doc_ptr_replacex(doc, ptr, ptr_len, val, &ctx, &err);
        assert_mut_val_eq(ret, data.old);
        assert_mut_doc_eq(doc, data.dst);
        assert_ctx_eq(&ctx, &data);
        assert_err(&err, &data);
        mut_after();
        
        // -----------------------------
        // mut_val.replace
        if (data.ptr_len == 0) { // can use strlen()
            mut_before();
            ret = yyjson_mut_ptr_replace(NULL, NULL, NULL);
            yy_assert(ret == NULL);
            mut_after();
            
            mut_before();
            ret = yyjson_mut_ptr_replace(root, ptr, val);
            if (root && val) {
                if (ptr_len > 0) {
                    assert_mut_val_eq(ret, data.old);
                    assert_mut_val_eq(root, data.dst);
                } else {
                    assert_mut_val_eq(ret, NULL);
                    assert_mut_val_eq(root, data.src);
                }
            } else {
                yy_assert(ret == NULL);
            }
            mut_after();
        }
        
        mut_before();
        ret = yyjson_mut_ptr_replacen(NULL, NULL, ptr_len, NULL);
        yy_assert(ret == NULL);
        mut_after();
        
        mut_before();
        ret = yyjson_mut_ptr_replacen(root, ptr, ptr_len, val);
        if (root && val) {
            if (ptr_len) {
                assert_mut_val_eq(ret, data.old);
                assert_mut_val_eq(root, data.dst);
            } else {
                assert_mut_val_eq(ret, NULL);
                assert_mut_val_eq(root, data.src);
            }
        } else {
            yy_assert(ret == NULL);
        }
        mut_after();
        
        mut_before();
        ret = yyjson_mut_ptr_replacex(NULL, NULL, ptr_len, NULL, NULL, NULL);
        yy_assert(ret == NULL);
        mut_after();
        
        mut_before();
        ret = yyjson_mut_ptr_replacex(root, ptr, ptr_len, val, NULL, NULL);
        if (root && val) {
            if (ptr_len) {
                assert_mut_val_eq(ret, data.old);
                assert_mut_val_eq(root, data.dst);
            } else {
                assert_mut_val_eq(ret, NULL);
                assert_mut_val_eq(root, data.src);
            }
        } else {
            yy_assert(ret == NULL);
        }
        mut_after();
        
        mut_before();
        ret = yyjson_mut_ptr_replacex(NULL, NULL, ptr_len, NULL, &ctx, &err);
        yy_assert(ret == NULL);
        assert_err_param(&err);
        assert_ctx_eq(&ctx, NULL);
        mut_after();
        
        mut_before();
        ret = yyjson_mut_ptr_replacex(root, ptr, ptr_len, val, &ctx, &err);
        if (root && val) {
            if (ptr_len) {
                assert_mut_val_eq(ret, data.old);
                assert_mut_val_eq(root, data.dst);
                assert_ctx_eq(&ctx, &data);
                assert_err(&err, &data);
            } else {
                assert_mut_val_eq(ret, NULL);
                assert_mut_val_eq(root, data.src);
                assert_ctx_eq(&ctx, NULL);
                yy_assert(err.code == YYJSON_PTR_ERR_SET_ROOT);
            }
        } else {
            yy_assert(ret == NULL);
            assert_err_param(&err);
        }
        mut_after();
    }
    
    // old = src.remove(ptr), src->dst
    if (data.op == PTR_OP_REMOVE) {
        // -----------------------------
        // mut_doc.remove
        if (data.ptr_len == 0) { // can use strlen()
            mut_before();
            ret = yyjson_mut_doc_ptr_remove(NULL, NULL);
            yy_assert(ret == NULL);
            mut_after();
            
            mut_before();
            ret = yyjson_mut_doc_ptr_remove(doc, ptr);
            assert_mut_val_eq(ret, data.old);
            assert_mut_doc_eq(doc, data.dst);
            mut_after();
        }
        
        mut_before();
        ret = yyjson_mut_doc_ptr_removen(NULL, NULL, ptr_len);
        yy_assert(ret == NULL);
        mut_after();
        
        mut_before();
        ret = yyjson_mut_doc_ptr_removen(doc, ptr, ptr_len);
        assert_mut_val_eq(ret, data.old);
        assert_mut_doc_eq(doc, data.dst);
        mut_after();
        
        mut_before();
        ret = yyjson_mut_doc_ptr_removex(NULL, NULL, ptr_len, NULL, NULL);
        yy_assert(ret == NULL);
        mut_after();
        
        mut_before();
        ret = yyjson_mut_doc_ptr_removex(doc, ptr, ptr_len, NULL, NULL);
        assert_mut_val_eq(ret, data.old);
        assert_mut_doc_eq(doc, data.dst);
        mut_after();
        
        mut_before();
        ret = yyjson_mut_doc_ptr_removex(NULL, NULL, ptr_len, &ctx, &err);
        yy_assert(ret == NULL);
        assert_err_param(&err);
        assert_ctx_eq(&ctx, NULL);
        mut_after();
        
        mut_before();
        ret = yyjson_mut_doc_ptr_removex(doc, ptr, ptr_len, &ctx, &err);
        assert_mut_val_eq(ret, data.old);
        assert_mut_doc_eq(doc, data.dst);
        assert_ctx_eq(&ctx, &data);
        assert_err(&err, &data);
        mut_after();
        
        // -----------------------------
        // mut_val.remove
        if (data.ptr_len == 0) { // can use strlen()
            mut_before();
            ret = yyjson_mut_ptr_remove(NULL, NULL);
            yy_assert(ret == NULL);
            mut_after();
            
            mut_before();
            ret = yyjson_mut_ptr_remove(root, ptr);
            if (root) {
                if (ptr_len) {
                    assert_mut_val_eq(ret, data.old);
                    assert_mut_val_eq(root, data.dst);
                } else {
                    assert_mut_val_eq(ret, NULL);
                    assert_mut_val_eq(root, data.src);
                }
            } else {
                yy_assert(ret == NULL);
            }
            mut_after();
        }
        
        mut_before();
        ret = yyjson_mut_ptr_removen(NULL, NULL, ptr_len);
        yy_assert(ret == NULL);
        mut_after();
        
        mut_before();
        ret = yyjson_mut_ptr_removen(root, ptr, ptr_len);
        if (root) {
            if (ptr_len) {
                assert_mut_val_eq(ret, data.old);
                assert_mut_val_eq(root, data.dst);
            } else {
                assert_mut_val_eq(ret, NULL);
                assert_mut_val_eq(root, data.src);
            }
        } else {
            yy_assert(ret == NULL);
        }
        mut_after();
        
        mut_before();
        ret = yyjson_mut_ptr_removex(NULL, NULL, ptr_len, NULL, NULL);
        yy_assert(ret == NULL);
        mut_after();
        
        mut_before();
        ret = yyjson_mut_ptr_removex(root, ptr, ptr_len, NULL, NULL);
        if (root) {
            if (ptr_len) {
                assert_mut_val_eq(ret, data.old);
                assert_mut_val_eq(root, data.dst);
            } else {
                assert_mut_val_eq(ret, NULL);
                assert_mut_val_eq(root, data.src);
            }
        } else {
            yy_assert(ret == NULL);
        }
        mut_after();
        
        mut_before();
        ret = yyjson_mut_ptr_removex(NULL, NULL, ptr_len, &ctx, &err);
        yy_assert(ret == NULL);
        assert_err_param(&err);
        assert_ctx_eq(&ctx, NULL);
        mut_after();
        
        mut_before();
        ret = yyjson_mut_ptr_removex(root, ptr, ptr_len, &ctx, &err);
        if (root) {
            if (ptr_len) {
                assert_mut_val_eq(ret, data.old);
                assert_mut_val_eq(root, data.dst);
                assert_ctx_eq(&ctx, &data);
                assert_err(&err, &data);
            } else {
                assert_mut_val_eq(ret, NULL);
                assert_mut_val_eq(root, data.src);
                assert_ctx_eq(&ctx, NULL);
                yy_assert(err.code == YYJSON_PTR_ERR_SET_ROOT);
            }
        } else {
            yy_assert(ret == NULL);
            assert_err_param(&err);
        }
        mut_after();
    }
    
#undef mut_before
#undef mut_after
    
    yyjson_doc_free(idoc);
    yyjson_mut_doc_free(mdoc);
    yyjson_doc_free(ival_doc);
    yyjson_mut_doc_free(mval_doc);
}

// -----------------------------------------------------------------------------
// test context operation: append, replace, remove
static void test_ctx_op(ptr_data data) {
    // source
    yyjson_doc *idoc = yyjson_read(data.src, data.src ? strlen(data.src) : 0, 0);
    yyjson_mut_doc *mdoc = yyjson_doc_mut_copy(idoc, NULL);
    
    // input value
    yyjson_doc *ival_doc = yyjson_read(data.val, data.val ? strlen(data.val) : 0, 0);
    yyjson_mut_doc *mval_doc = yyjson_doc_mut_copy(ival_doc, NULL);
    yyjson_mut_val *mval = yyjson_mut_doc_get_root(mval_doc);
    yyjson_doc *ikey_doc = yyjson_read(data.key, data.key ? strlen(data.key) : 0, 0);
    yyjson_mut_doc *mkey_doc = yyjson_doc_mut_copy(ikey_doc, NULL);
    yyjson_mut_val *mkey = yyjson_mut_doc_get_root(mkey_doc);
    
    // pointer
    const char *ptr = data.ptr;
    size_t ptr_len = data.ptr_len;
    if (ptr && !ptr_len) ptr_len = strlen(ptr);
    
    // temp value
    yyjson_mut_doc *doc;
    yyjson_mut_val *ret, *key, *val;
    yyjson_ptr_err err;
    yyjson_ptr_ctx ctx;
    bool suc;
    
#define mut_before() do { \
    memset(&err, -1, sizeof(err)); \
    memset(&ctx, -1, sizeof(ctx)); \
    doc = yyjson_mut_doc_mut_copy(mdoc, NULL); \
    val = yyjson_mut_val_mut_copy(doc, mval); \
    key = yyjson_mut_val_mut_copy(doc, mkey); \
} while(false)
    
#define mut_after() do { \
    yyjson_mut_doc_free(doc); \
} while (false)
    
    if (data.op == PTR_OP_ADD) {
        mut_before();
        ret = yyjson_mut_doc_ptr_getx(doc, ptr, ptr_len, &ctx, &err);
        suc = yyjson_ptr_ctx_append(&ctx, key, val);
        yy_assert(suc == !data.err);
        assert_mut_doc_eq(doc, data.dst);
        assert_ctx_eq(&ctx, &data);
        mut_after();
    }
    
    if (data.op == PTR_OP_REPLACE) {
        mut_before();
        ret = yyjson_mut_doc_ptr_getx(doc, ptr, ptr_len, &ctx, &err);
        suc = yyjson_ptr_ctx_replace(&ctx, val);
        yy_assert(suc == !data.err);
        assert_mut_doc_eq(doc, data.dst);
        assert_ctx_eq(&ctx, &data);
        mut_after();
    }
    
    if (data.op == PTR_OP_REMOVE) {
        mut_before();
        ret = yyjson_mut_doc_ptr_getx(doc, ptr, ptr_len, &ctx, &err);
        suc = yyjson_ptr_ctx_remove(&ctx);
        yy_assert(suc == !data.err);
        assert_mut_doc_eq(doc, data.dst);
        assert_ctx_eq(&ctx, &data);
        mut_after();
    }
    
#undef mut_before
#undef mut_after
    
    yyjson_doc_free(idoc);
    yyjson_mut_doc_free(mdoc);
    yyjson_doc_free(ival_doc);
    yyjson_mut_doc_free(mval_doc);
    yyjson_doc_free(ikey_doc);
    yyjson_mut_doc_free(mkey_doc);
}

// -----------------------------------------------------------------------------
// test cases from spec: https://www.rfc-editor.org/rfc/rfc6901
static void test_spec(void) {
    const char *json = "{"
        "\"foo\":[\"bar\",\"baz\"],"
        "\"\":0,"
        "\"a/b\":1,"
        "\"c%d\":2,"
        "\"e^f\":3,"
        "\"g|h\":4,"
        "\"i\\\\j\":5,"
        "\"k\\\"l\":6,"
        "\" \":7,"
        "\"m~n\":8"
    "}";
    test_ptr_op((ptr_data){
        .op = PTR_OP_GET,
        .src = json,
        .ptr = "",
        .val = json,
    });
    test_ptr_op((ptr_data){
        .op = PTR_OP_GET,
        .src = json,
        .ptr = "/foo",
        .val = "[\"bar\",\"baz\"]",
        .ctn = json,
        .pre = "\"m~n\"",
    });
    test_ptr_op((ptr_data){
        .op = PTR_OP_GET,
        .src = json,
        .ptr = "/foo/0",
        .val = "\"bar\"",
        .ctn = "[\"bar\",\"baz\"]",
        .pre = "\"baz\"",
    });
    test_ptr_op((ptr_data){
        .op = PTR_OP_GET,
        .src = json,
        .ptr = "/",
        .val = "0",
        .ctn = json,
        .pre = "\"foo\"",
    });
    test_ptr_op((ptr_data){
        .op = PTR_OP_GET,
        .src = json,
        .ptr = "/a~1b",
        .val = "1",
        .ctn = json,
        .pre = "\"\"",
    });
    test_ptr_op((ptr_data){
        .op = PTR_OP_GET,
        .src = json,
        .ptr = "/c%d",
        .val = "2",
        .ctn = json,
        .pre = "\"a/b\"",
    });
    test_ptr_op((ptr_data){
        .op = PTR_OP_GET,
        .src = json,
        .ptr = "/e^f",
        .val = "3",
        .ctn = json,
        .pre = "\"c%d\"",
    });
    test_ptr_op((ptr_data){
        .op = PTR_OP_GET,
        .src = json,
        .ptr = "/g|h",
        .val = "4",
        .ctn = json,
        .pre = "\"e^f\"",
    });
    test_ptr_op((ptr_data){
        .op = PTR_OP_GET,
        .src = json,
        .ptr = "/i\\j",
        .val = "5",
        .ctn = json,
        .pre = "\"g|h\"",
    });
    test_ptr_op((ptr_data){
        .op = PTR_OP_GET,
        .src = json,
        .ptr = "/k\"l",
        .val = "6",
        .ctn = json,
        .pre = "\"i\\\\j\"",
    });
    test_ptr_op((ptr_data){
        .op = PTR_OP_GET,
        .src = json,
        .ptr = "/ ",
        .val = "7",
        .ctn = json,
        .pre = "\"k\\\"l\"",
    });
    test_ptr_op((ptr_data){
        .op = PTR_OP_GET,
        .src = json,
        .ptr = "/m~0n",
        .val = "8",
        .ctn = json,
        .pre = "\" \"",
    });
}

// -----------------------------------------------------------------------------
// expected: val = src.get(ptr)
static void test_ptr_get(void) {
    
    // ---------------------------------
    // invalid parameter
    test_ptr_op((ptr_data){
        .op = PTR_OP_GET,
        .src = NULL,
        .ptr = NULL,
        .err = YYJSON_PTR_ERR_PARAMETER,
    });
    test_ptr_op((ptr_data){
        .op = PTR_OP_GET,
        .src = "1",
        .ptr = NULL,
        .err = YYJSON_PTR_ERR_PARAMETER,
    });
    test_ptr_op((ptr_data){
        .op = PTR_OP_GET,
        .src = NULL,
        .ptr = "/a",
        .err = YYJSON_PTR_ERR_PARAMETER,
    });
    
    // ---------------------------------
    // null root
    test_ptr_op((ptr_data){
        .op = PTR_OP_GET,
        .src = empty_root,
        .ptr = "",
        .err = YYJSON_PTR_ERR_NULL_ROOT,
    });
    test_ptr_op((ptr_data){
        .op = PTR_OP_GET,
        .src = empty_root,
        .ptr = "/a",
        .err = YYJSON_PTR_ERR_NULL_ROOT,
    });
    
    // ---------------------------------
    // single root
    test_ptr_op((ptr_data){
        .op = PTR_OP_GET,
        .src = "1",
        .ptr = "",
        .val = "1",
    });
    test_ptr_op((ptr_data){
        .op = PTR_OP_GET,
        .src = "1",
        .ptr = "/", // this matched to "" key
        .err = YYJSON_PTR_ERR_RESOLVE,
        .pos = 1,
    });
    
    // ---------------------------------
    // error syntax
    test_ptr_op((ptr_data){
        .op = PTR_OP_GET,
        .src = "{\"a\":[1,2,3]}",
        .ptr = "a", // no '/' prefix
        .err = YYJSON_PTR_ERR_SYNTAX,
        .pos = 0,
    });
    test_ptr_op((ptr_data){
        .op = PTR_OP_GET,
        .src = "{\"a\":[1,2,3]}",
        .ptr = "~", // no '/' prefix
        .err = YYJSON_PTR_ERR_SYNTAX,
        .pos = 0,
    });
    test_ptr_op((ptr_data){
        .op = PTR_OP_GET,
        .src = "{\"a\":[1,2,3]}",
        .ptr = "/~", // invalid escape
        .err = YYJSON_PTR_ERR_SYNTAX,
        .pos = 1,
    });
    test_ptr_op((ptr_data){
        .op = PTR_OP_GET,
        .src = "{\"a\":[1,2,3]}",
        .ptr = "/a/~", // invalid escape
        .err = YYJSON_PTR_ERR_SYNTAX,
        .pos = 3,
    });
    test_ptr_op((ptr_data){
        .op = PTR_OP_GET,
        .src = "{\"a\":[1,2,3]}",
        .ptr = "/a/~2", // invalid escape
        .err = YYJSON_PTR_ERR_SYNTAX,
        .pos = 3,
    });
    test_ptr_op((ptr_data){
        .op = PTR_OP_GET,
        .src = "{\"a\":[1,2,3]}",
        .ptr = "/a/~/", // invalid escape
        .err = YYJSON_PTR_ERR_SYNTAX,
        .pos = 3,
    });
    test_ptr_op((ptr_data){
        .op = PTR_OP_GET,
        .src = "{\"a\":[1,2,3]}",
        .ptr = "/a/~~", // invalid escape
        .err = YYJSON_PTR_ERR_SYNTAX,
        .pos = 3,
    });
    
    // ---------------------------------
    // array index
    test_ptr_op((ptr_data){
        .op = PTR_OP_GET,
        .src = "[]",
        .ptr = "/0", // out of range, but can be used to insert
        .ctn = "[]",
        .err = YYJSON_PTR_ERR_RESOLVE,
        .pos = 1,
    });
    test_ptr_op((ptr_data){
        .op = PTR_OP_GET,
        .src = "[]",
        .ptr = "/1", // out of range
        .err = YYJSON_PTR_ERR_RESOLVE,
        .pos = 1,
    });
    test_ptr_op((ptr_data){
        .op = PTR_OP_GET,
        .src = "[]",
        .ptr = "/", // no index
        .err = YYJSON_PTR_ERR_RESOLVE,
        .pos = 1,
    });
    test_ptr_op((ptr_data){
        .op = PTR_OP_GET,
        .src = "{\"a\":[1,2,3]}",
        .ptr = "/a/00", // leading zero
        .err = YYJSON_PTR_ERR_RESOLVE,
        .pos = 3,
    });
    test_ptr_op((ptr_data){
        .op = PTR_OP_GET,
        .src = "{\"a\":[1,2,3]}",
        .ptr = "/a/01", // leading zero
        .err = YYJSON_PTR_ERR_RESOLVE,
        .pos = 3,
    });
    test_ptr_op((ptr_data){
        .op = PTR_OP_GET,
        .src = "{\"a\":[1,2,3]}",
        .ptr = "/a/-1", // negative
        .err = YYJSON_PTR_ERR_RESOLVE,
        .pos = 3,
    });
    test_ptr_op((ptr_data){
        .op = PTR_OP_GET,
        .src = "{\"a\":[1,2,3]}",
        .ptr = "/a/ 1", // space
        .err = YYJSON_PTR_ERR_RESOLVE,
        .pos = 3,
    });
    test_ptr_op((ptr_data){
        .op = PTR_OP_GET,
        .src = "{\"a\":[1,2,3]}",
        .ptr = "/a/18446744073709551615", // big number
        .err = YYJSON_PTR_ERR_RESOLVE,
        .pos = 3,
    });
    test_ptr_op((ptr_data){
        .op = PTR_OP_GET,
        .src = "{\"a\":[]}",
        .ptr = "/a/0", // empty array, last index
        .ctn = "[]",
        .err = YYJSON_PTR_ERR_RESOLVE,
        .pos = 3,
    });
    test_ptr_op((ptr_data){
        .op = PTR_OP_GET,
        .src = "{\"a\":[]}",
        .ptr = "/a/-", // empty array, last index
        .ctn = "[]",
        .err = YYJSON_PTR_ERR_RESOLVE,
        .pos = 3,
    });
    test_ptr_op((ptr_data){
        .op = PTR_OP_GET,
        .src = "{\"a\":[]}",
        .ptr = "/a/1", // empty array, out of range
        .err = YYJSON_PTR_ERR_RESOLVE,
        .pos = 3,
    });
    test_ptr_op((ptr_data){
        .op = PTR_OP_GET,
        .src = "{\"a\":[1]}", // array with 1 value
        .ptr = "/a/0",
        .val = "1",
        .ctn = "[1]",
        .pre = "1",
    });
    test_ptr_op((ptr_data){
        .op = PTR_OP_GET,
        .src = "{\"a\":[1,2]}", // array with 2 values
        .ptr = "/a/0",
        .val = "1",
        .ctn = "[1,2]",
        .pre = "2",
    });
    test_ptr_op((ptr_data){
        .op = PTR_OP_GET,
        .src = "{\"a\":[1,2]}", // array with 2 values
        .ptr = "/a/1",
        .val = "2",
        .ctn = "[1,2]",
        .pre = "1",
    });
    test_ptr_op((ptr_data){
        .op = PTR_OP_GET,
        .src = "{\"a\":[1,2]}",
        .ptr = "/a/2", // last index
        .ctn = "[1,2]",
        .err = YYJSON_PTR_ERR_RESOLVE,
        .pos = 3,
    });
    test_ptr_op((ptr_data){
        .op = PTR_OP_GET,
        .src = "{\"a\":[1,2]}",
        .ptr = "/a/3", // out of range
        .err = YYJSON_PTR_ERR_RESOLVE,
        .pos = 3,
    });
    test_ptr_op((ptr_data){
        .op = PTR_OP_GET,
        .src = "{\"a\":[1,2]}",
        .ptr = "/a/-", // last index
        .ctn = "[1,2]",
        .err = YYJSON_PTR_ERR_RESOLVE,
        .pos = 3,
    });
    test_ptr_op((ptr_data){
        .op = PTR_OP_GET,
        .src = "{\"a\":[1,[2,3,4]]}",
        .ptr = "/a/-/2", // `-` cannot used to get value
        .err = YYJSON_PTR_ERR_RESOLVE,
        .pos = 3,
    });
    test_ptr_op((ptr_data){
        .op = PTR_OP_GET,
        .src = "{\"a\":[1,[2,3,4]]}",
        .ptr = "/a/1/2",  // 2 level
        .val = "4",
        .ctn = "[2,3,4]",
        .pre = "3",
    });
    test_ptr_op((ptr_data){
        .op = PTR_OP_GET,
        .src = "{\"a\":[1,[2,3,4]]}",
        .ptr = "/a/0/2", // 2 level, out of range
        .err = YYJSON_PTR_ERR_RESOLVE,
        .pos = 5,
    });
    test_ptr_op((ptr_data){
        .op = PTR_OP_GET,
        .src = "{\"a\":[1,[2,3,4]]}",
        .ptr = "/a/1/3", // 2 level, out of range
        .ctn = "[2,3,4]",
        .err = YYJSON_PTR_ERR_RESOLVE,
        .pos = 5,
    });
    test_ptr_op((ptr_data){
        .op = PTR_OP_GET,
        .src = "{\"a\":[1,[2,3,4]]}",
        .ptr = "/a/1/b", // 2 level, invalid index
        .err = YYJSON_PTR_ERR_RESOLVE,
        .pos = 5,
    });
    
    // ---------------------------------
    // object key
    test_ptr_op((ptr_data){
        .op = PTR_OP_GET,
        .src = "{}",
        .ptr = "/a", // no key
        .ctn = "{}",
        .err = YYJSON_PTR_ERR_RESOLVE,
        .pos = 1,
    });
    test_ptr_op((ptr_data){
        .op = PTR_OP_GET,
        .src = "{\"a\\u0000bc\":1,\"b\":2}",
        .ptr = "/a\0bc", // NUL inside key
        .ptr_len = 5,
        .val = "1",
        .ctn = "{\"a\\u0000bc\":1,\"b\":2}",
        .pre = "\"b\"",
    });
    test_ptr_op((ptr_data){
        .op = PTR_OP_GET,
        .src = "{\"a~b\":1,\"c\":2}",
        .ptr = "/a~0b", // escaped '~'
        .val = "1",
        .ctn = "{\"a~b\":1,\"c\":2}",
        .pre = "\"c\"",
    });
    test_ptr_op((ptr_data){
        .op = PTR_OP_GET,
        .src = "{\"a~b\":1,\"c\":2}",
        .ptr = "/a~1b", // escaped '~' not matched
        .ctn = "{\"a~b\":1,\"c\":2}",
        .err = YYJSON_PTR_ERR_RESOLVE,
        .pos = 1,
    });
    test_ptr_op((ptr_data){
        .op = PTR_OP_GET,
        .src = "{\"a/b\":1,\"c\":2}",
        .ptr = "/a~1b", // escaped '/'
        .val = "1",
        .ctn = "{\"a/b\":1,\"c\":2}",
        .pre = "\"c\"",
    });
    test_ptr_op((ptr_data){
        .op = PTR_OP_GET,
        .src = "{\"a/b\":1,\"c\":2}",
        .ptr = "/a~0b", // escaped '/' not matched
        .ctn = "{\"a/b\":1,\"c\":2}",
        .err = YYJSON_PTR_ERR_RESOLVE,
        .pos = 1,
    });
    test_ptr_op((ptr_data){
        .op = PTR_OP_GET,
        .src = "{\"a\":1,\"a\":2,\"b\":3}",
        .ptr = "/a", // duplcated key
        .val = "1",
        .ctn = "{\"a\":1,\"a\":2,\"b\":3}",
        .pre = "\"b\"",
    });
    test_ptr_op((ptr_data){
        .op = PTR_OP_GET,
        .src = "{\"a\":{\"b\":{\"c\":1}}}",
        .ptr = "/a/b/c", // 3 level, 3 token
        .val = "1",
        .ctn = "{\"c\":1}",
        .pre = "\"c\"",
    });
    test_ptr_op((ptr_data){
        .op = PTR_OP_GET,
        .src = "{\"a\":{\"b\":{\"c\":1}}}",
        .ptr = "/a/b", // 3 level, 2 token
        .val = "{\"c\":1}",
        .ctn = "{\"b\":{\"c\":1}}",
        .pre = "\"b\"",
    });
    test_ptr_op((ptr_data){
        .op = PTR_OP_GET,
        .src = "{\"a\":{\"b\":{\"c\":1}}}",
        .ptr = "/a/c", // 3 level, 2 token, nonexist
        .ctn = "{\"b\":{\"c\":1}}",
        .err = YYJSON_PTR_ERR_RESOLVE,
        .pos = 3,
    });
    test_ptr_op((ptr_data){
        .op = PTR_OP_GET,
        .src = "{\"a\":{\"b\":{\"c\":1}}}",
        .ptr = "/a/b/d", // 3 level, 3 token, nonexist
        .ctn = "{\"c\":1}",
        .err = YYJSON_PTR_ERR_RESOLVE,
        .pos = 5,
    });
    test_ptr_op((ptr_data){
        .op = PTR_OP_GET,
        .src = "{\"a\":{\"b\":{\"c\":1}}}",
        .ptr = "/a/b/c/d", // 3 level, 4 token, nonexist
        .err = YYJSON_PTR_ERR_RESOLVE,
        .pos = 7,
    });
    
    // ---------------------------------
    // pointer without null-terminator
    const char *ptr = "/a/b";
    char *ptr_alc = malloc(strlen(ptr));
    memcpy(ptr_alc, ptr, strlen(ptr));
    test_ptr_op((ptr_data){
        .op = PTR_OP_GET,
        .src = "{\"a\":{\"b\":{\"c\":1}}}",
        .ptr = ptr_alc,
        .ptr_len = strlen(ptr),
        .val = "{\"c\":1}",
        .ctn = "{\"b\":{\"c\":1}}",
        .pre = "\"b\"",
    });
    free(ptr_alc);
}

static void test_ptr_put(void) {
    // ---------------------------------
    // invalid parameter
    test_ptr_op((ptr_data){
        .op = PTR_OP_ADD,
        .src = NULL,
        .ptr = NULL,
        .val = NULL,
        .err = YYJSON_PTR_ERR_PARAMETER,
    });
    test_ptr_op((ptr_data){
        .op = PTR_OP_SET,
        .src = NULL,
        .ptr = NULL,
        .val = NULL,
        .err = YYJSON_PTR_ERR_PARAMETER,
    });
    test_ptr_op((ptr_data){
        .op = PTR_OP_REPLACE,
        .src = NULL,
        .ptr = NULL,
        .val = NULL,
        .err = YYJSON_PTR_ERR_PARAMETER,
    });
    test_ptr_op((ptr_data){
        .op = PTR_OP_REMOVE,
        .src = NULL,
        .ptr = NULL,
        .err = YYJSON_PTR_ERR_PARAMETER,
    });
    
    // ---------------------------------
    // error syntax (level 0)
    test_ptr_op((ptr_data){
        .op = PTR_OP_ADD,
        .src = "[0,1,2]",
        .ptr = "1",
        .val = "3",
        .dst = "[0,1,2]",
        .err = YYJSON_PTR_ERR_SYNTAX,
    });
    test_ptr_op((ptr_data){
        .op = PTR_OP_SET,
        .src = "[0,1,2]",
        .ptr = "1",
        .val = "3",
        .dst = "[0,1,2]",
        .err = YYJSON_PTR_ERR_SYNTAX,
    });
    test_ptr_op((ptr_data){
        .op = PTR_OP_REPLACE,
        .src = "[0,1,2]",
        .ptr = "1",
        .val = "3",
        .dst = "[0,1,2]",
        .err = YYJSON_PTR_ERR_SYNTAX,
    });
    test_ptr_op((ptr_data){
        .op = PTR_OP_REMOVE,
        .src = "[0,1,2]",
        .ptr = "1",
        .dst = "[0,1,2]",
        .err = YYJSON_PTR_ERR_SYNTAX,
    });
    
    // ---------------------------------
    // error syntax (level 1)
    test_ptr_op((ptr_data){
        .op = PTR_OP_ADD,
        .src = "[0,1,2]",
        .ptr = "/~2",
        .val = "3",
        .dst = "[0,1,2]",
        .err = YYJSON_PTR_ERR_SYNTAX,
        .pos = 1,
    });
    test_ptr_op((ptr_data){
        .op = PTR_OP_SET,
        .src = "[0,1,2]",
        .ptr = "/~2",
        .val = "3",
        .dst = "[0,1,2]",
        .err = YYJSON_PTR_ERR_SYNTAX,
        .pos = 1,
    });
    test_ptr_op((ptr_data){
        .op = PTR_OP_REPLACE,
        .src = "[0,1,2]",
        .ptr = "/~2",
        .val = "3",
        .dst = "[0,1,2]",
        .err = YYJSON_PTR_ERR_SYNTAX,
        .pos = 1,
    });
    test_ptr_op((ptr_data){
        .op = PTR_OP_REMOVE,
        .src = "[0,1,2]",
        .ptr = "/~2",
        .dst = "[0,1,2]",
        .err = YYJSON_PTR_ERR_SYNTAX,
        .pos = 1,
    });
    
    // ---------------------------------
    // error syntax (level 2)
    test_ptr_op((ptr_data){
        .op = PTR_OP_ADD,
        .src = "[0,1,2]",
        .ptr = "/1/~2",
        .val = "3",
        .dst = "[0,1,2]",
        .err = YYJSON_PTR_ERR_SYNTAX,
        .pos = 3,
        .create_parent = true,
    });
    test_ptr_op((ptr_data){
        .op = PTR_OP_SET,
        .src = "[0,1,2]",
        .ptr = "/1/~2",
        .val = "3",
        .dst = "[0,1,2]",
        .err = YYJSON_PTR_ERR_SYNTAX,
        .pos = 3,
        .create_parent = true,
    });
    test_ptr_op((ptr_data){
        .op = PTR_OP_REPLACE,
        .src = "[0,1,2]",
        .ptr = "/1/~2",
        .val = "3",
        .dst = "[0,1,2]",
        .err = YYJSON_PTR_ERR_SYNTAX,
        .pos = 3,
    });
    test_ptr_op((ptr_data){
        .op = PTR_OP_REMOVE,
        .src = "[0,1,2]",
        .ptr = "/1/~2",
        .dst = "[0,1,2]",
        .err = YYJSON_PTR_ERR_SYNTAX,
        .pos = 3,
    });
    
    // ---------------------------------
    // error syntax (level 3)
    test_ptr_op((ptr_data){
        .op = PTR_OP_ADD,
        .src = "[0,{},2]",
        .ptr = "/1/a/~2",
        .val = "3",
        .dst = "[0,{},2]",
        .err = YYJSON_PTR_ERR_SYNTAX,
        .pos = 5,
        .create_parent = true,
    });
    test_ptr_op((ptr_data){
        .op = PTR_OP_SET,
        .src = "[0,{},2]",
        .ptr = "/1/a/~2",
        .val = "3",
        .dst = "[0,{},2]",
        .err = YYJSON_PTR_ERR_SYNTAX,
        .pos = 5,
        .create_parent = true,
    });
    test_ptr_op((ptr_data){
        .op = PTR_OP_REPLACE,
        .src = "[0,{},2]",
        .ptr = "/1/a/~2",
        .val = "3",
        .dst = "[0,{},2]",
        .err = YYJSON_PTR_ERR_RESOLVE,
        .pos = 3,
    });
    test_ptr_op((ptr_data){
        .op = PTR_OP_REMOVE,
        .src = "[0,{},2]",
        .ptr = "/1/a/~2",
        .dst = "[0,{},2]",
        .err = YYJSON_PTR_ERR_RESOLVE,
        .pos = 3,
    });
    
    // ---------------------------------
    // no root (single value)
    test_ptr_op((ptr_data){
        .op = PTR_OP_ADD,
        .src = empty_root,
        .ptr = "",
        .val = "1",
        .dst = "1",
    });
    test_ptr_op((ptr_data){
        .op = PTR_OP_SET,
        .src = empty_root,
        .ptr = "",
        .val = "1",
        .dst = "1",
    });
    test_ptr_op((ptr_data){
        .op = PTR_OP_REPLACE,
        .src = empty_root,
        .ptr = "",
        .val = "1",
        .dst = empty_root,
        .err = YYJSON_PTR_ERR_RESOLVE,
    });
    test_ptr_op((ptr_data){
        .op = PTR_OP_REMOVE,
        .src = empty_root,
        .ptr = "",
        .dst = empty_root,
        .err = YYJSON_PTR_ERR_NULL_ROOT,
    });
    
    // ---------------------------------
    // no root (level 1)
    test_ptr_op((ptr_data){
        .op = PTR_OP_ADD,
        .src = empty_root,
        .ptr = "/a",
        .val = "1",
        .dst = empty_root,
        .err = YYJSON_PTR_ERR_NULL_ROOT,
        .create_parent = false,
    });
    test_ptr_op((ptr_data){
        .op = PTR_OP_ADD,
        .src = empty_root,
        .ptr = "/a",
        .val = "1",
        .dst = "{\"a\":1}",
        .ctn = "{\"a\":1}",
        .pre = "\"a\"",
        .create_parent = true,
    });
    test_ptr_op((ptr_data){
        .op = PTR_OP_SET,
        .src = empty_root,
        .ptr = "/a",
        .val = "1",
        .dst = empty_root,
        .err = YYJSON_PTR_ERR_NULL_ROOT,
        .create_parent = false,
    });
    test_ptr_op((ptr_data){
        .op = PTR_OP_SET,
        .src = empty_root,
        .ptr = "/a",
        .val = "1",
        .dst = "{\"a\":1}",
        .ctn = "{\"a\":1}",
        .pre = "\"a\"",
        .create_parent = true,
    });
    test_ptr_op((ptr_data){
        .op = PTR_OP_REPLACE,
        .src = empty_root,
        .ptr = "/a",
        .val = "1",
        .dst = empty_root,
        .err = YYJSON_PTR_ERR_NULL_ROOT,
    });
    test_ptr_op((ptr_data){
        .op = PTR_OP_REMOVE,
        .src = empty_root,
        .ptr = "/a",
        .dst = empty_root,
        .err = YYJSON_PTR_ERR_NULL_ROOT,
    });
    
    // ---------------------------------
    // no root (level 2)
    test_ptr_op((ptr_data){
        .op = PTR_OP_ADD,
        .src = empty_root,
        .ptr = "/a/0",
        .val = "1",
        .dst = empty_root,
        .create_parent = false,
        .err = YYJSON_PTR_ERR_NULL_ROOT,
    });
    test_ptr_op((ptr_data){
        .op = PTR_OP_ADD,
        .src = empty_root,
        .ptr = "/a/0",
        .val = "1",
        .dst = "{\"a\":{\"0\":1}}",
        .ctn = "{\"0\":1}",
        .pre = "\"0\"",
        .create_parent = true,
    });
    test_ptr_op((ptr_data){
        .op = PTR_OP_SET,
        .src = empty_root,
        .ptr = "/a/0",
        .val = "1",
        .dst = empty_root,
        .create_parent = false,
        .err = YYJSON_PTR_ERR_NULL_ROOT,
    });
    test_ptr_op((ptr_data){
        .op = PTR_OP_SET,
        .src = empty_root,
        .ptr = "/a/0",
        .val = "1",
        .dst = "{\"a\":{\"0\":1}}",
        .ctn = "{\"0\":1}",
        .pre = "\"0\"",
        .create_parent = true,
    });
    test_ptr_op((ptr_data){
        .op = PTR_OP_REPLACE,
        .src = empty_root,
        .ptr = "/a/0",
        .val = "1",
        .dst = empty_root,
        .err = YYJSON_PTR_ERR_NULL_ROOT,
    });
    test_ptr_op((ptr_data){
        .op = PTR_OP_REMOVE,
        .src = empty_root,
        .ptr = "/a/0",
        .dst = empty_root,
        .err = YYJSON_PTR_ERR_NULL_ROOT,
    });
    
    // ---------------------------------
    // target is root
    test_ptr_op((ptr_data){
        .op = PTR_OP_ADD,
        .src = "[1,2]",
        .ptr = "",
        .val = "3",
        .dst = "[1,2]",
        .err = YYJSON_PTR_ERR_SET_ROOT,
    });
    test_ptr_op((ptr_data){
        .op = PTR_OP_SET,
        .src = "[1,2]",
        .ptr = "",
        .val = "3",
        .dst = "3",
        .old = "[1,2]",
    });
    test_ptr_op((ptr_data){
        .op = PTR_OP_REPLACE,
        .src = "[1,2]",
        .ptr = "",
        .val = "3",
        .dst = "3",
        .old = "[1,2]",
    });
    test_ptr_op((ptr_data){
        .op = PTR_OP_REMOVE,
        .src = "[1,2]",
        .ptr = "",
        .dst = empty_root,
        .old = "[1,2]",
    });
    
    // ---------------------------------
    // target is root, no value
    test_ptr_op((ptr_data){
        .op = PTR_OP_ADD,
        .src = "[1,2]",
        .ptr = "",
        .val = NULL,
        .dst = "[1,2]",
        .err = YYJSON_PTR_ERR_PARAMETER,
    });
    test_ptr_op((ptr_data){
        .op = PTR_OP_SET,
        .src = "[1,2]",
        .ptr = "",
        .val = NULL,
        .dst = empty_root,
        .old = "[1,2]",
    });
    test_ptr_op((ptr_data){
        .op = PTR_OP_REPLACE,
        .src = "[1,2]",
        .ptr = "",
        .val = NULL,
        .dst = "[1,2]",
        .err = YYJSON_PTR_ERR_PARAMETER,
    });
    test_ptr_op((ptr_data){
        .op = PTR_OP_REMOVE,
        .src = "[1,2]",
        .ptr = "",
        .dst = empty_root,
        .old = "[1,2]",
    });
    
    // ---------------------------------
    // no value
    test_ptr_op((ptr_data){
        .op = PTR_OP_ADD,
        .src = "[1,2]",
        .ptr = "/0",
        .val = NULL,
        .dst = "[1,2]",
        .err = YYJSON_PTR_ERR_PARAMETER,
    });
    test_ptr_op((ptr_data){
        .op = PTR_OP_SET,
        .src = "[1,2]",
        .ptr = "/0",
        .val = NULL,
        .dst = "[2]",
        .ctn = "[2]",
        .old = "1",
    });
    test_ptr_op((ptr_data){
        .op = PTR_OP_REPLACE,
        .src = "[1,2]",
        .ptr = "/0",
        .val = NULL,
        .dst = "[1,2]",
        .err = YYJSON_PTR_ERR_PARAMETER,
    });
    test_ptr_op((ptr_data){
        .op = PTR_OP_REMOVE,
        .src = "[1,2]",
        .ptr = "/0",
        .dst = "[2]",
        .old = "1",
        .ctn = "[2]",
    });
    
    // ---------------------------------
    // no parent (level 2)
    test_ptr_op((ptr_data){
        .op = PTR_OP_ADD,
        .src = "{}",
        .ptr = "/a/0",
        .val = "1",
        .dst = "{}",
        .create_parent = false,
        .err = YYJSON_PTR_ERR_RESOLVE,
        .pos = 1,
    });
    test_ptr_op((ptr_data){
        .op = PTR_OP_ADD,
        .src = "{}",
        .ptr = "/a/0",
        .val = "1",
        .dst = "{\"a\":{\"0\":1}}",
        .ctn = "{\"0\":1}",
        .pre = "\"0\"",
        .create_parent = true,
    });
    test_ptr_op((ptr_data){
        .op = PTR_OP_SET,
        .src = "{}",
        .ptr = "/a/0",
        .val = "1",
        .dst = "{}",
        .create_parent = false,
        .err = YYJSON_PTR_ERR_RESOLVE,
        .pos = 1,
    });
    test_ptr_op((ptr_data){
        .op = PTR_OP_SET,
        .src = "{}",
        .ptr = "/a/0",
        .val = "1",
        .dst = "{\"a\":{\"0\":1}}",
        .ctn = "{\"0\":1}",
        .pre = "\"0\"",
        .create_parent = true,
    });
    test_ptr_op((ptr_data){
        .op = PTR_OP_REPLACE,
        .src = "{}",
        .ptr = "/a/0",
        .val = "1",
        .dst = "{}",
        .err = YYJSON_PTR_ERR_RESOLVE,
        .pos = 1,
    });
    test_ptr_op((ptr_data){
        .op = PTR_OP_REMOVE,
        .src = "{}",
        .ptr = "/a/0",
        .dst = "{}",
        .err = YYJSON_PTR_ERR_RESOLVE,
        .pos = 1,
    });
    
    // ---------------------------------
    // no parent (level 3)
    test_ptr_op((ptr_data){
        .op = PTR_OP_ADD,
        .src = "{}",
        .ptr = "/a/0/b",
        .val = "1",
        .dst = "{\"a\":{\"0\":{\"b\":1}}}",
        .ctn = "{\"b\":1}",
        .pre = "\"b\"",
        .create_parent = true,
    });
    test_ptr_op((ptr_data){
        .op = PTR_OP_SET,
        .src = "{}",
        .ptr = "/a/0/b",
        .val = "1",
        .dst = "{\"a\":{\"0\":{\"b\":1}}}",
        .ctn = "{\"b\":1}",
        .pre = "\"b\"",
        .create_parent = true,
    });
    test_ptr_op((ptr_data){
        .op = PTR_OP_REPLACE,
        .src = "{}",
        .ptr = "/a/0/b",
        .val = "1",
        .dst = "{}",
        .err = YYJSON_PTR_ERR_RESOLVE,
        .pos = 1,
    });
    test_ptr_op((ptr_data){
        .op = PTR_OP_REMOVE,
        .src = "{}",
        .ptr = "/a/0/b",
        .dst = "{}",
        .err = YYJSON_PTR_ERR_RESOLVE,
        .pos = 1,
    });
    
    // ---------------------------------
    // parent type not matched (array with no index)
    test_ptr_op((ptr_data){
        .op = PTR_OP_ADD,
        .src = "[]",
        .ptr = "/a/0",
        .val = "1",
        .dst = "[]",
        .create_parent = true,
        .err = YYJSON_PTR_ERR_RESOLVE,
        .pos = 1,
    });
    test_ptr_op((ptr_data){
        .op = PTR_OP_SET,
        .src = "[]",
        .ptr = "/a/0",
        .val = "1",
        .dst = "[]",
        .create_parent = true,
        .err = YYJSON_PTR_ERR_RESOLVE,
        .pos = 1,
    });
    test_ptr_op((ptr_data){
        .op = PTR_OP_REPLACE,
        .src = "[]",
        .ptr = "/a/0",
        .val = "1",
        .dst = "[]",
        .err = YYJSON_PTR_ERR_RESOLVE,
        .pos = 1,
    });
    test_ptr_op((ptr_data){
        .op = PTR_OP_REMOVE,
        .src = "[]",
        .ptr = "/a/0",
        .dst = "[]",
        .err = YYJSON_PTR_ERR_RESOLVE,
        .pos = 1,
    });
    test_ptr_op((ptr_data){
        .op = PTR_OP_ADD,
        .src = "[1]",
        .ptr = "/a/0",
        .val = "1",
        .dst = "[1]",
        .create_parent = true,
        .err = YYJSON_PTR_ERR_RESOLVE,
        .pos = 1,
    });
    test_ptr_op((ptr_data){
        .op = PTR_OP_SET,
        .src = "[1]",
        .ptr = "/a/0",
        .val = "1",
        .dst = "[1]",
        .create_parent = true,
        .err = YYJSON_PTR_ERR_RESOLVE,
        .pos = 1,
    });
    test_ptr_op((ptr_data){
        .op = PTR_OP_REPLACE,
        .src = "[1]",
        .ptr = "/a/0",
        .val = "1",
        .dst = "[1]",
        .err = YYJSON_PTR_ERR_RESOLVE,
        .pos = 1,
    });
    test_ptr_op((ptr_data){
        .op = PTR_OP_REMOVE,
        .src = "[1]",
        .ptr = "/a/0",
        .dst = "[1]",
        .err = YYJSON_PTR_ERR_RESOLVE,
        .pos = 1,
    });
    
    // ---------------------------------
    // parent type not matched (not container)
    test_ptr_op((ptr_data){
        .op = PTR_OP_ADD,
        .src = "[1]",
        .ptr = "/0/a",
        .val = "1",
        .dst = "[1]",
        .create_parent = true,
        .err = YYJSON_PTR_ERR_RESOLVE,
        .pos = 3,
    });
    test_ptr_op((ptr_data){
        .op = PTR_OP_SET,
        .src = "[1]",
        .ptr = "/0/a",
        .val = "1",
        .dst = "[1]",
        .create_parent = true,
        .err = YYJSON_PTR_ERR_RESOLVE,
        .pos = 3,
    });
    test_ptr_op((ptr_data){
        .op = PTR_OP_REPLACE,
        .src = "[1]",
        .ptr = "/0/a",
        .val = "1",
        .dst = "[1]",
        .err = YYJSON_PTR_ERR_RESOLVE,
        .pos = 3,
    });
    test_ptr_op((ptr_data){
        .op = PTR_OP_REMOVE,
        .src = "[1]",
        .ptr = "/0/a",
        .dst = "[1]",
        .err = YYJSON_PTR_ERR_RESOLVE,
        .pos = 3,
    });
    
    // ---------------------------------
    // array size 0[0]
    test_ptr_op((ptr_data){
        .op = PTR_OP_ADD,
        .src = "[]",
        .ptr = "/0",
        .val = "1",
        .dst = "[1]",
        .ctn = "[1]",
        .pre = "1",
    });
    test_ptr_op((ptr_data){
        .op = PTR_OP_SET,
        .src = "[]",
        .ptr = "/0",
        .val = "1",
        .dst = "[]",
        .ctn = "[]",
        .err = YYJSON_PTR_ERR_RESOLVE,
        .pos = 1,
    });
    test_ptr_op((ptr_data){
        .op = PTR_OP_REPLACE,
        .src = "[]",
        .ptr = "/0",
        .val = "1",
        .dst = "[]",
        .ctn = "[]",
        .err = YYJSON_PTR_ERR_RESOLVE,
        .pos = 1,
    });
    test_ptr_op((ptr_data){
        .op = PTR_OP_REMOVE,
        .src = "[]",
        .ptr = "/0",
        .val = "1",
        .dst = "[]",
        .ctn = "[]",
        .err = YYJSON_PTR_ERR_RESOLVE,
        .pos = 1,
    });
    
    // ---------------------------------
    // array size 0[-]
    test_ptr_op((ptr_data){
        .op = PTR_OP_ADD,
        .src = "[]",
        .ptr = "/-",
        .val = "1",
        .dst = "[1]",
        .ctn = "[1]",
        .pre = "1",
    });
    test_ptr_op((ptr_data){
        .op = PTR_OP_SET,
        .src = "[]",
        .ptr = "/-",
        .val = "1",
        .dst = "[]",
        .ctn = "[]",
        .err = YYJSON_PTR_ERR_RESOLVE,
        .pos = 1,
    });
    test_ptr_op((ptr_data){
        .op = PTR_OP_REPLACE,
        .src = "[]",
        .ptr = "/-",
        .val = "1",
        .dst = "[]",
        .ctn = "[]",
        .err = YYJSON_PTR_ERR_RESOLVE,
        .pos = 1,
    });
    test_ptr_op((ptr_data){
        .op = PTR_OP_REMOVE,
        .src = "[]",
        .ptr = "/-",
        .val = "1",
        .dst = "[]",
        .ctn = "[]",
        .err = YYJSON_PTR_ERR_RESOLVE,
        .pos = 1,
    });
    
    // ---------------------------------
    // array size 0[1]
    test_ptr_op((ptr_data){
        .op = PTR_OP_ADD,
        .src = "[]",
        .ptr = "/1",
        .val = "1",
        .dst = "[]",
        .err = YYJSON_PTR_ERR_RESOLVE,
        .pos = 1,
    });
    test_ptr_op((ptr_data){
        .op = PTR_OP_SET,
        .src = "[]",
        .ptr = "/1",
        .val = "1",
        .dst = "[]",
        .err = YYJSON_PTR_ERR_RESOLVE,
        .pos = 1,
    });
    test_ptr_op((ptr_data){
        .op = PTR_OP_REPLACE,
        .src = "[]",
        .ptr = "/1",
        .val = "1",
        .dst = "[]",
        .err = YYJSON_PTR_ERR_RESOLVE,
        .pos = 1,
    });
    test_ptr_op((ptr_data){
        .op = PTR_OP_REMOVE,
        .src = "[]",
        .ptr = "/1",
        .val = "1",
        .dst = "[]",
        .err = YYJSON_PTR_ERR_RESOLVE,
        .pos = 1,
    });
    
    // ---------------------------------
    // array size 1[0]
    test_ptr_op((ptr_data){
        .op = PTR_OP_ADD,
        .src = "[1]",
        .ptr = "/0",
        .val = "2",
        .dst = "[2,1]",
        .ctn = "[2,1]",
        .pre = "1",
    });
    test_ptr_op((ptr_data){
        .op = PTR_OP_SET,
        .src = "[1]",
        .ptr = "/0",
        .val = "2",
        .dst = "[2]",
        .ctn = "[2]",
        .pre = "2",
        .old = "1",
    });
    test_ptr_op((ptr_data){
        .op = PTR_OP_REPLACE,
        .src = "[1]",
        .ptr = "/0",
        .val = "2",
        .dst = "[2]",
        .ctn = "[2]",
        .pre = "2",
        .old = "1",
    });
    test_ptr_op((ptr_data){
        .op = PTR_OP_REMOVE,
        .src = "[1]",
        .ptr = "/0",
        .dst = "[]",
        .ctn = "[]",
        .old = "1",
    });
    
    // ---------------------------------
    // array size 1[1]
    test_ptr_op((ptr_data){
        .op = PTR_OP_ADD,
        .src = "[1]",
        .ptr = "/1",
        .val = "2",
        .dst = "[1,2]",
        .ctn = "[1,2]",
        .pre = "1",
    });
    test_ptr_op((ptr_data){
        .op = PTR_OP_SET,
        .src = "[1]",
        .ptr = "/1",
        .val = "2",
        .dst = "[1]",
        .ctn = "[1]",
        .err = YYJSON_PTR_ERR_RESOLVE,
        .pos = 1,
    });
    test_ptr_op((ptr_data){
        .op = PTR_OP_REPLACE,
        .src = "[1]",
        .ptr = "/1",
        .val = "2",
        .dst = "[1]",
        .ctn = "[1]",
        .err = YYJSON_PTR_ERR_RESOLVE,
        .pos = 1,
    });
    test_ptr_op((ptr_data){
        .op = PTR_OP_REMOVE,
        .src = "[1]",
        .ptr = "/1",
        .dst = "[1]",
        .ctn = "[1]",
        .err = YYJSON_PTR_ERR_RESOLVE,
        .pos = 1,
    });
    
    // ---------------------------------
    // array size 1[-]
    test_ptr_op((ptr_data){
        .op = PTR_OP_ADD,
        .src = "[1]",
        .ptr = "/-",
        .val = "2",
        .dst = "[1,2]",
        .ctn = "[1,2]",
        .pre = "1",
    });
    test_ptr_op((ptr_data){
        .op = PTR_OP_SET,
        .src = "[1]",
        .ptr = "/-",
        .val = "2",
        .dst = "[1]",
        .ctn = "[1]",
        .err = YYJSON_PTR_ERR_RESOLVE,
        .pos = 1,
    });
    test_ptr_op((ptr_data){
        .op = PTR_OP_REPLACE,
        .src = "[1]",
        .ptr = "/-",
        .val = "2",
        .dst = "[1]",
        .ctn = "[1]",
        .err = YYJSON_PTR_ERR_RESOLVE,
        .pos = 1,
    });
    test_ptr_op((ptr_data){
        .op = PTR_OP_REMOVE,
        .src = "[1]",
        .ptr = "/-",
        .dst = "[1]",
        .ctn = "[1]",
        .err = YYJSON_PTR_ERR_RESOLVE,
        .pos = 1,
    });
    
    // ---------------------------------
    // array size 1[2]
    test_ptr_op((ptr_data){
        .op = PTR_OP_ADD,
        .src = "[1]",
        .ptr = "/2",
        .val = "2",
        .dst = "[1]",
        .err = YYJSON_PTR_ERR_RESOLVE,
        .pos = 1,
    });
    test_ptr_op((ptr_data){
        .op = PTR_OP_SET,
        .src = "[1]",
        .ptr = "/2",
        .val = "2",
        .dst = "[1]",
        .err = YYJSON_PTR_ERR_RESOLVE,
        .pos = 1,
    });
    test_ptr_op((ptr_data){
        .op = PTR_OP_REPLACE,
        .src = "[1]",
        .ptr = "/2",
        .val = "2",
        .dst = "[1]",
        .err = YYJSON_PTR_ERR_RESOLVE,
        .pos = 1,
    });
    test_ptr_op((ptr_data){
        .op = PTR_OP_REMOVE,
        .src = "[1]",
        .ptr = "/2",
        .dst = "[1]",
        .err = YYJSON_PTR_ERR_RESOLVE,
        .pos = 1,
    });
    
    // ---------------------------------
    // array size 2[0]
    test_ptr_op((ptr_data){
        .op = PTR_OP_ADD,
        .src = "[0,1]",
        .ptr = "/0",
        .val = "2",
        .dst = "[2,0,1]",
        .ctn = "[2,0,1]",
        .pre = "1",
    });
    test_ptr_op((ptr_data){
        .op = PTR_OP_SET,
        .src = "[0,1]",
        .ptr = "/0",
        .val = "2",
        .dst = "[2,1]",
        .ctn = "[2,1]",
        .pre = "1",
        .old = "0",
    });
    test_ptr_op((ptr_data){
        .op = PTR_OP_REPLACE,
        .src = "[0,1]",
        .ptr = "/0",
        .val = "2",
        .dst = "[2,1]",
        .ctn = "[2,1]",
        .pre = "1",
        .old = "0",
    });
    test_ptr_op((ptr_data){
        .op = PTR_OP_REMOVE,
        .src = "[0,1]",
        .ptr = "/0",
        .dst = "[1]",
        .ctn = "[1]",
        .old = "0",
    });
    
    // ---------------------------------
    // array size 2[1]
    test_ptr_op((ptr_data){
        .op = PTR_OP_ADD,
        .src = "[0,1]",
        .ptr = "/1",
        .val = "2",
        .dst = "[0,2,1]",
        .ctn = "[0,2,1]",
        .pre = "0",
    });
    test_ptr_op((ptr_data){
        .op = PTR_OP_SET,
        .src = "[0,1]",
        .ptr = "/1",
        .val = "2",
        .dst = "[0,2]",
        .ctn = "[0,2]",
        .pre = "0",
        .old = "1",
    });
    test_ptr_op((ptr_data){
        .op = PTR_OP_REPLACE,
        .src = "[0,1]",
        .ptr = "/1",
        .val = "2",
        .dst = "[0,2]",
        .ctn = "[0,2]",
        .pre = "0",
        .old = "1",
    });
    test_ptr_op((ptr_data){
        .op = PTR_OP_REMOVE,
        .src = "[0,1]",
        .ptr = "/1",
        .dst = "[0]",
        .ctn = "[0]",
        .old = "1",
    });
    
    // ---------------------------------
    // array size 2[2]
    test_ptr_op((ptr_data){
        .op = PTR_OP_ADD,
        .src = "[0,1]",
        .ptr = "/2",
        .val = "2",
        .dst = "[0,1,2]",
        .ctn = "[0,1,2]",
        .pre = "1",
    });
    test_ptr_op((ptr_data){
        .op = PTR_OP_SET,
        .src = "[0,1]",
        .ptr = "/2",
        .val = "2",
        .dst = "[0,1]",
        .ctn = "[0,1]",
        .err = YYJSON_PTR_ERR_RESOLVE,
        .pos = 1,
    });
    test_ptr_op((ptr_data){
        .op = PTR_OP_REPLACE,
        .src = "[0,1]",
        .ptr = "/2",
        .val = "2",
        .dst = "[0,1]",
        .ctn = "[0,1]",
        .err = YYJSON_PTR_ERR_RESOLVE,
        .pos = 1,
    });
    test_ptr_op((ptr_data){
        .op = PTR_OP_REMOVE,
        .src = "[0,1]",
        .ptr = "/2",
        .dst = "[0,1]",
        .ctn = "[0,1]",
        .err = YYJSON_PTR_ERR_RESOLVE,
        .pos = 1,
    });
    
    // ---------------------------------
    // array size 2[3]
    test_ptr_op((ptr_data){
        .op = PTR_OP_ADD,
        .src = "[0,1]",
        .ptr = "/3",
        .val = "2",
        .dst = "[0,1]",
        .err = YYJSON_PTR_ERR_RESOLVE,
        .pos = 1,
    });
    test_ptr_op((ptr_data){
        .op = PTR_OP_SET,
        .src = "[0,1]",
        .ptr = "/3",
        .val = "2",
        .dst = "[0,1]",
        .err = YYJSON_PTR_ERR_RESOLVE,
        .pos = 1,
    });
    test_ptr_op((ptr_data){
        .op = PTR_OP_REPLACE,
        .src = "[0,1]",
        .ptr = "/3",
        .val = "2",
        .dst = "[0,1]",
        .err = YYJSON_PTR_ERR_RESOLVE,
        .pos = 1,
    });
    test_ptr_op((ptr_data){
        .op = PTR_OP_REMOVE,
        .src = "[0,1]",
        .ptr = "/3",
        .dst = "[0,1]",
        .err = YYJSON_PTR_ERR_RESOLVE,
        .pos = 1,
    });
    
    // ---------------------------------
    // parent's parent index is last
    test_ptr_op((ptr_data){
        .op = PTR_OP_ADD,
        .src = "[0,1,2]",
        .ptr = "/3/a",
        .val = "3",
        .dst = "[0,1,2,{\"a\":3}]",
        .ctn = "{\"a\":3}",
        .pre = "\"a\"",
        .create_parent = true,
    });
    test_ptr_op((ptr_data){
        .op = PTR_OP_SET,
        .src = "[0,1,2]",
        .ptr = "/3/a",
        .val = "3",
        .dst = "[0,1,2]",
        .create_parent = true,
        .err = YYJSON_PTR_ERR_RESOLVE,
        .pos = 1,
    });
    test_ptr_op((ptr_data){
        .op = PTR_OP_REPLACE,
        .src = "[0,1,2]",
        .ptr = "/3/a",
        .val = "3",
        .dst = "[0,1,2]",
        .create_parent = true,
        .err = YYJSON_PTR_ERR_RESOLVE,
        .pos = 1,
    });
    test_ptr_op((ptr_data){
        .op = PTR_OP_REMOVE,
        .src = "[0,1,2]",
        .ptr = "/3/a",
        .val = "3",
        .dst = "[0,1,2]",
        .create_parent = true,
        .err = YYJSON_PTR_ERR_RESOLVE,
        .pos = 1,
    });
    
    // ---------------------------------
    // key exist
    test_ptr_op((ptr_data){
        .op = PTR_OP_ADD,
        .src = "{\"a\":1,\"b\":2}",
        .ptr = "/a",
        .val = "3",
        .dst = "{\"a\":1,\"b\":2,\"a\":3}",
        .ctn = "{\"a\":1,\"b\":2,\"a\":3}",
        .pre = "\"b\"",
        .create_parent = true,
    });
    test_ptr_op((ptr_data){
        .op = PTR_OP_SET,
        .src = "{\"a\":1,\"b\":2}",
        .ptr = "/a",
        .val = "3",
        .dst = "{\"a\":3,\"b\":2}",
        .ctn = "{\"a\":3,\"b\":2}",
        .pre = "\"b\"",
        .old = "1",
        .create_parent = true,
    });
    test_ptr_op((ptr_data){
        .op = PTR_OP_REPLACE,
        .src = "{\"a\":1,\"b\":2}",
        .ptr = "/a",
        .val = "3",
        .dst = "{\"a\":3,\"b\":2}",
        .ctn = "{\"a\":3,\"b\":2}",
        .pre = "\"b\"",
        .old = "1",
    });
    test_ptr_op((ptr_data){
        .op = PTR_OP_REMOVE,
        .src = "{\"a\":1,\"b\":2}",
        .ptr = "/a",
        .dst = "{\"b\":2}",
        .ctn = "{\"b\":2}",
        .old = "1",
    });
    
    // ---------------------------------
    // duplcated key
    test_ptr_op((ptr_data){
        .op = PTR_OP_ADD,
        .src = "{\"a\":1,\"b\":2,\"a\":3,\"c\":4}",
        .ptr = "/a",
        .val = "5",
        .dst = "{\"a\":1,\"b\":2,\"a\":3,\"c\":4,\"a\":5}",
        .ctn = "{\"a\":1,\"b\":2,\"a\":3,\"c\":4,\"a\":5}",
        .pre = "\"c\"",
        .create_parent = true,
    });
    test_ptr_op((ptr_data){
        .op = PTR_OP_SET,
        .src = "{\"a\":1,\"b\":2,\"a\":3,\"c\":4}",
        .ptr = "/a",
        .val = "5",
        .dst = "{\"a\":5,\"b\":2,\"c\":4}",
        .ctn = "{\"a\":5,\"b\":2,\"c\":4}",
        .pre = "\"c\"",
        .old = "1",
        .create_parent = true,
    });
    test_ptr_op((ptr_data){
        .op = PTR_OP_REPLACE,
        .src = "{\"a\":1,\"b\":2,\"a\":3,\"c\":4}",
        .ptr = "/a",
        .val = "5",
        .dst = "{\"a\":5,\"b\":2,\"c\":4}",
        .ctn = "{\"a\":5,\"b\":2,\"c\":4}",
        .pre = "\"c\"",
        .old = "1",
    });
    test_ptr_op((ptr_data){
        .op = PTR_OP_REMOVE,
        .src = "{\"a\":1,\"b\":2,\"a\":3,\"c\":4}",
        .ptr = "/a",
        .dst = "{\"b\":2,\"c\":4}",
        .ctn = "{\"b\":2,\"c\":4}",
        .old = "1",
    });
    
    // ---------------------------------
    // special key (escaped character)
    test_ptr_op((ptr_data){
        .op = PTR_OP_ADD,
        .src = "{\"a\":1,\"b\":2}",
        .ptr = "/c~0d~1e",
        .val = "3",
        .dst = "{\"a\":1,\"b\":2,\"c~d/e\":3}",
        .ctn = "{\"a\":1,\"b\":2,\"c~d/e\":3}",
        .pre = "\"b\"",
    });
    test_ptr_op((ptr_data){
        .op = PTR_OP_SET,
        .src = "{\"a\":1,\"b\":2}",
        .ptr = "/c~0d~1e",
        .val = "3",
        .dst = "{\"a\":1,\"b\":2,\"c~d/e\":3}",
        .ctn = "{\"a\":1,\"b\":2,\"c~d/e\":3}",
        .pre = "\"b\"",
    });
    test_ptr_op((ptr_data){
        .op = PTR_OP_REPLACE,
        .src = "{\"a\":1,\"b\":2,\"c~d/e\":3}",
        .ptr = "/c~0d~1e",
        .val = "4",
        .dst = "{\"a\":1,\"b\":2,\"c~d/e\":4}",
        .ctn = "{\"a\":1,\"b\":2,\"c~d/e\":4}",
        .pre = "\"b\"",
        .old = "3",
    });
    test_ptr_op((ptr_data){
        .op = PTR_OP_REMOVE,
        .src = "{\"a\":1,\"b\":2,\"c~d/e\":3}",
        .ptr = "/c~0d~1e",
        .dst = "{\"a\":1,\"b\":2}",
        .ctn = "{\"a\":1,\"b\":2}",
        .old = "3",
    });
    
    // ---------------------------------
    // special key (empty string)
    test_ptr_op((ptr_data){
        .op = PTR_OP_ADD,
        .src = "{\"a\":1,\"b\":2}",
        .ptr = "/",
        .val = "3",
        .dst = "{\"a\":1,\"b\":2,\"\":3}",
        .ctn = "{\"a\":1,\"b\":2,\"\":3}",
        .pre = "\"b\"",
    });
    test_ptr_op((ptr_data){
        .op = PTR_OP_SET,
        .src = "{\"a\":1,\"b\":2}",
        .ptr = "/",
        .val = "3",
        .dst = "{\"a\":1,\"b\":2,\"\":3}",
        .ctn = "{\"a\":1,\"b\":2,\"\":3}",
        .pre = "\"b\"",
    });
    test_ptr_op((ptr_data){
        .op = PTR_OP_REPLACE,
        .src = "{\"a\":1,\"b\":2,\"\":3}",
        .ptr = "/",
        .val = "4",
        .dst = "{\"a\":1,\"b\":2,\"\":4}",
        .ctn = "{\"a\":1,\"b\":2,\"\":4}",
        .pre = "\"b\"",
        .old = "3",
    });
    test_ptr_op((ptr_data){
        .op = PTR_OP_REMOVE,
        .src = "{\"a\":1,\"b\":2,\"\":3}",
        .ptr = "/",
        .dst = "{\"a\":1,\"b\":2}",
        .ctn = "{\"a\":1,\"b\":2}",
        .old = "3",
    });
    
    // ---------------------------------
    // special key (string with NUL character)
    test_ptr_op((ptr_data){
        .op = PTR_OP_ADD,
        .src = "{\"a\":1,\"b\":2}",
        .ptr = "/\0",
        .ptr_len = 2,
        .val = "3",
        .dst = "{\"a\":1,\"b\":2,\"\\u0000\":3}",
        .ctn = "{\"a\":1,\"b\":2,\"\\u0000\":3}",
        .pre = "\"b\"",
    });
    test_ptr_op((ptr_data){
        .op = PTR_OP_SET,
        .src = "{\"a\":1,\"b\":2}",
        .ptr = "/\0",
        .ptr_len = 2,
        .val = "3",
        .dst = "{\"a\":1,\"b\":2,\"\\u0000\":3}",
        .ctn = "{\"a\":1,\"b\":2,\"\\u0000\":3}",
        .pre = "\"b\"",
    });
    test_ptr_op((ptr_data){
        .op = PTR_OP_REPLACE,
        .src = "{\"a\":1,\"b\":2,\"\\u0000\":3}",
        .ptr = "/\0",
        .ptr_len = 2,
        .val = "4",
        .dst = "{\"a\":1,\"b\":2,\"\\u0000\":4}",
        .ctn = "{\"a\":1,\"b\":2,\"\\u0000\":4}",
        .pre = "\"b\"",
        .old = "3",
    });
    test_ptr_op((ptr_data){
        .op = PTR_OP_REMOVE,
        .src = "{\"a\":1,\"b\":2,\"\\u0000\":3}",
        .ptr = "/\0",
        .ptr_len = 2,
        .dst = "{\"a\":1,\"b\":2}",
        .ctn = "{\"a\":1,\"b\":2}",
        .old = "3",
    });
}

// ctx.append, replace, remove
static void test_ptr_ctx(void) {
    // ---------------------------------
    // invalid param (no ctx)
    test_ctx_op((ptr_data){
        .op = PTR_OP_ADD,
        .src = "[0,1,2]",
        .ptr = "/a",
        .dst = "[0,1,2]",
        .err = 1,
    });
    test_ctx_op((ptr_data){
        .op = PTR_OP_REPLACE,
        .src = "[0,1,2]",
        .ptr = "/a",
        .dst = "[0,1,2]",
        .err = 1,
    });
    test_ctx_op((ptr_data){
        .op = PTR_OP_REMOVE,
        .src = "[0,1,2]",
        .ptr = "/a",
        .dst = "[0,1,2]",
        .err = 1,
    });
    
    // ---------------------------------
    // invalid param (no val)
    test_ctx_op((ptr_data){
        .op = PTR_OP_ADD,
        .src = "[0,1,2]",
        .ptr = "/0",
        .dst = "[0,1,2]",
        .ctn = "[0,1,2]",
        .pre = "2",
        .err = 1,
    });
    test_ctx_op((ptr_data){
        .op = PTR_OP_REPLACE,
        .src = "[0,1,2]",
        .ptr = "/0",
        .dst = "[0,1,2]",
        .ctn = "[0,1,2]",
        .pre = "2",
        .err = 1,
    });
    
    // ---------------------------------
    // invalid param (no key)
    test_ctx_op((ptr_data){
        .op = PTR_OP_ADD,
        .src = "{\"a\":1}",
        .ptr = "/b",
        .val = "2",
        .dst = "{\"a\":1}",
        .ctn = "{\"a\":1}",
        .err = 1,
    });
    
    // ---------------------------------
    // no ctx.ctn
    test_ctx_op((ptr_data){
        .op = PTR_OP_ADD,
        .src = "[0,1,2]",
        .ptr = "",
        .dst = "[0,1,2]",
        .err = 1,
    });
    test_ctx_op((ptr_data){
        .op = PTR_OP_REPLACE,
        .src = "[0,1,2]",
        .ptr = "",
        .dst = "[0,1,2]",
        .err = 1,
    });
    test_ctx_op((ptr_data){
        .op = PTR_OP_REMOVE,
        .src = "[0,1,2]",
        .ptr = "",
        .dst = "[0,1,2]",
        .err = 1,
    });
    
    // ---------------------------------
    // array size 0[0]
    test_ctx_op((ptr_data){
        .op = PTR_OP_ADD,
        .src = "[]",
        .ptr = "/0",
        .val = "1",
        .dst = "[1]",
        .ctn = "[1]",
        .pre = "1",
    });
    test_ctx_op((ptr_data){
        .op = PTR_OP_REPLACE,
        .src = "[]",
        .ptr = "/0",
        .val = "1",
        .dst = "[]",
        .ctn = "[]",
        .err = 1,
    });
    test_ctx_op((ptr_data){
        .op = PTR_OP_REMOVE,
        .src = "[]",
        .ptr = "/0",
        .dst = "[]",
        .ctn = "[]",
        .err = 1,
    });
    
    // ---------------------------------
    // array size 1[0]
    test_ctx_op((ptr_data){
        .op = PTR_OP_ADD,
        .src = "[1]",
        .ptr = "/0",
        .val = "2",
        .dst = "[1,2]",
        .ctn = "[1,2]",
        .pre = "1",
    });
    test_ctx_op((ptr_data){
        .op = PTR_OP_REPLACE,
        .src = "[1]",
        .ptr = "/0",
        .val = "2",
        .dst = "[2]",
        .ctn = "[2]",
        .pre = "2",
        .old = "1",
    });
    test_ctx_op((ptr_data){
        .op = PTR_OP_REMOVE,
        .src = "[1]",
        .ptr = "/0",
        .dst = "[]",
        .ctn = "[]",
        .old = "1",
    });
    
    // ---------------------------------
    // array size 1[1]
    test_ctx_op((ptr_data){
        .op = PTR_OP_ADD,
        .src = "[1]",
        .ptr = "/1",
        .val = "2",
        .dst = "[1,2]",
        .ctn = "[1,2]",
        .pre = "1",
    });
    test_ctx_op((ptr_data){
        .op = PTR_OP_REPLACE,
        .src = "[1]",
        .ptr = "/1",
        .val = "2",
        .dst = "[1]",
        .ctn = "[1]",
        .err = 1,
    });
    test_ctx_op((ptr_data){
        .op = PTR_OP_REMOVE,
        .src = "[1]",
        .ptr = "/1",
        .dst = "[1]",
        .ctn = "[1]",
        .err = 1,
    });
    
    // ---------------------------------
    // array size 2[0]
    test_ctx_op((ptr_data){
        .op = PTR_OP_ADD,
        .src = "[1,2]",
        .ptr = "/0",
        .val = "3",
        .dst = "[1,3,2]",
        .ctn = "[1,3,2]",
        .pre = "1",
    });
    test_ctx_op((ptr_data){
        .op = PTR_OP_REPLACE,
        .src = "[1,2]",
        .ptr = "/0",
        .val = "3",
        .dst = "[3,2]",
        .ctn = "[3,2]",
        .pre = "2",
        .old = "1",
    });
    test_ctx_op((ptr_data){
        .op = PTR_OP_REMOVE,
        .src = "[1,2]",
        .ptr = "/0",
        .dst = "[2]",
        .ctn = "[2]",
        .old = "1",
    });
    
    // ---------------------------------
    // array size 2[1]
    test_ctx_op((ptr_data){
        .op = PTR_OP_ADD,
        .src = "[1,2]",
        .ptr = "/1",
        .val = "3",
        .dst = "[1,2,3]",
        .ctn = "[1,2,3]",
        .pre = "2",
    });
    test_ctx_op((ptr_data){
        .op = PTR_OP_REPLACE,
        .src = "[1,2]",
        .ptr = "/1",
        .val = "3",
        .dst = "[1,3]",
        .ctn = "[1,3]",
        .pre = "1",
        .old = "2",
    });
    test_ctx_op((ptr_data){
        .op = PTR_OP_REMOVE,
        .src = "[1,2]",
        .ptr = "/1",
        .dst = "[1]",
        .ctn = "[1]",
        .old = "2",
    });
    
    // ---------------------------------
    // array size 2[2]
    test_ctx_op((ptr_data){
        .op = PTR_OP_ADD,
        .src = "[1,2]",
        .ptr = "/2",
        .val = "3",
        .dst = "[1,2,3]",
        .ctn = "[1,2,3]",
        .pre = "2",
    });
    test_ctx_op((ptr_data){
        .op = PTR_OP_REPLACE,
        .src = "[1,2]",
        .ptr = "/2",
        .val = "3",
        .dst = "[1,2]",
        .ctn = "[1,2]",
        .err = 1,
    });
    test_ctx_op((ptr_data){
        .op = PTR_OP_REMOVE,
        .src = "[1,2]",
        .ptr = "/2",
        .dst = "[1,2]",
        .ctn = "[1,2]",
        .err = 1,
    });
    
    
    // ---------------------------------
    // object size 0[0]
    test_ctx_op((ptr_data){
        .op = PTR_OP_ADD,
        .src = "{}",
        .ptr = "/a",
        .key = "\"a\"",
        .val = "1",
        .dst = "{\"a\":1}",
        .ctn = "{\"a\":1}",
        .pre = "\"a\"",
    });
    test_ctx_op((ptr_data){
        .op = PTR_OP_REPLACE,
        .src = "{}",
        .ptr = "/a",
        .val = "1",
        .dst = "{}",
        .ctn = "{}",
        .err = 1,
    });
    test_ctx_op((ptr_data){
        .op = PTR_OP_REMOVE,
        .src = "{}",
        .ptr = "/a",
        .dst = "{}",
        .ctn = "{}",
        .err = 1,
    });
    
    // ---------------------------------
    // object size 1[0]
    test_ctx_op((ptr_data){
        .op = PTR_OP_ADD,
        .src = "{\"a\":1}",
        .ptr = "/a",
        .key = "\"b\"",
        .val = "2",
        .dst = "{\"a\":1,\"b\":2}",
        .ctn = "{\"a\":1,\"b\":2}",
        .pre = "\"a\"",
    });
    test_ctx_op((ptr_data){
        .op = PTR_OP_REPLACE,
        .src = "{\"a\":1}",
        .ptr = "/a",
        .val = "2",
        .dst = "{\"a\":2}",
        .ctn = "{\"a\":2}",
        .pre = "\"a\"",
        .old = "1",
    });
    test_ctx_op((ptr_data){
        .op = PTR_OP_REMOVE,
        .src = "{\"a\":1}",
        .ptr = "/a",
        .dst = "{}",
        .ctn = "{}",
        .old = "1",
    });
    
    // ---------------------------------
    // object size 1[1]
    test_ctx_op((ptr_data){
        .op = PTR_OP_ADD,
        .src = "{\"a\":1}",
        .ptr = "/b",
        .key = "\"b\"",
        .val = "2",
        .dst = "{\"a\":1,\"b\":2}",
        .ctn = "{\"a\":1,\"b\":2}",
        .pre = "\"a\"",
    });
    test_ctx_op((ptr_data){
        .op = PTR_OP_REPLACE,
        .src = "{\"a\":1}",
        .ptr = "/b",
        .val = "2",
        .dst = "{\"a\":1}",
        .ctn = "{\"a\":1}",
        .err = 1,
    });
    test_ctx_op((ptr_data){
        .op = PTR_OP_REMOVE,
        .src = "{\"a\":1}",
        .ptr = "/b",
        .dst = "{\"a\":1}",
        .ctn = "{\"a\":1}",
        .err = 1,
    });
    
    // ---------------------------------
    // object size 2[0]
    test_ctx_op((ptr_data){
        .op = PTR_OP_ADD,
        .src = "{\"a\":1,\"b\":2}",
        .ptr = "/a",
        .key = "\"c\"",
        .val = "3",
        .dst = "{\"a\":1,\"c\":3,\"b\":2}",
        .ctn = "{\"a\":1,\"c\":3,\"b\":2}",
        .pre = "\"a\"",
    });
    test_ctx_op((ptr_data){
        .op = PTR_OP_REPLACE,
        .src = "{\"a\":1,\"b\":2}",
        .ptr = "/a",
        .val = "3",
        .dst = "{\"a\":3,\"b\":2}",
        .ctn = "{\"a\":3,\"b\":2}",
        .pre = "\"b\"",
        .old = "1",
    });
    test_ctx_op((ptr_data){
        .op = PTR_OP_REMOVE,
        .src = "{\"a\":1,\"b\":2}",
        .ptr = "/a",
        .dst = "{\"b\":2}",
        .ctn = "{\"b\":2}",
        .old = "1",
    });
    
    // ---------------------------------
    // object size 2[1]
    test_ctx_op((ptr_data){
        .op = PTR_OP_ADD,
        .src = "{\"a\":1,\"b\":2}",
        .ptr = "/b",
        .key = "\"c\"",
        .val = "3",
        .dst = "{\"a\":1,\"b\":2,\"c\":3}",
        .ctn = "{\"a\":1,\"b\":2,\"c\":3}",
        .pre = "\"b\"",
    });
    test_ctx_op((ptr_data){
        .op = PTR_OP_REPLACE,
        .src = "{\"a\":1,\"b\":2}",
        .ptr = "/b",
        .val = "3",
        .dst = "{\"a\":1,\"b\":3}",
        .ctn = "{\"a\":1,\"b\":3}",
        .pre = "\"a\"",
        .old = "2",
    });
    test_ctx_op((ptr_data){
        .op = PTR_OP_REMOVE,
        .src = "{\"a\":1,\"b\":2}",
        .ptr = "/b",
        .dst = "{\"a\":1}",
        .ctn = "{\"a\":1}",
        .old = "2",
    });
    
    // ---------------------------------
    // object size 2[2]
    test_ctx_op((ptr_data){
        .op = PTR_OP_ADD,
        .src = "{\"a\":1,\"b\":2}",
        .ptr = "/c",
        .key = "\"c\"",
        .val = "3",
        .dst = "{\"a\":1,\"b\":2,\"c\":3}",
        .ctn = "{\"a\":1,\"b\":2,\"c\":3}",
        .pre = "\"b\"",
    });
    test_ctx_op((ptr_data){
        .op = PTR_OP_REPLACE,
        .src = "{\"a\":1,\"b\":2}",
        .ptr = "/c",
        .val = "3",
        .dst = "{\"a\":1,\"b\":2}",
        .ctn = "{\"a\":1,\"b\":2}",
        .err = 1,
    });
    test_ctx_op((ptr_data){
        .op = PTR_OP_REMOVE,
        .src = "{\"a\":1,\"b\":2}",
        .ptr = "/c",
        .dst = "{\"a\":1,\"b\":2}",
        .ctn = "{\"a\":1,\"b\":2}",
        .err = 1,
    });
}

static void test_ptr_get_type(void) {
    const char *json = "{ \
        \"answer\": {\"to\": {\"life\": 42}}, \
        \"true\": true, \
        \"-1\": -1, \
        \"1\": 1, \
        \"0\": 0, \
        \"i64_max\": 9223372036854775807, \
        \"i64_max+\": 9223372036854775808, \
        \"zero\": 0, \
        \"pi\": 3.14159, \
        \"pistr\": \"3.14159\" \
    }";
    
    bool bool_value;
    double real_value;
    int64_t sint_value;
    uint64_t uint_value;
    const char *string_value;
    
    yyjson_doc *doc = yyjson_read(json, strlen(json), 0);
    yyjson_val *root = yyjson_doc_get_root (doc);
    
    // successful gets
    yy_assert(yyjson_ptr_get_bool(root, "/true", &bool_value) == true && bool_value == true);
    yy_assert(yyjson_ptr_get_uint(root, "/answer/to/life", &uint_value) == true && uint_value == 42);
    yy_assert(yyjson_ptr_get_sint(root, "/-1", &sint_value) == true && sint_value == -1);
    yy_assert(yyjson_ptr_get_sint(root, "/1", &sint_value) == true && sint_value == 1);
    yy_assert(yyjson_ptr_get_uint(root, "/1", &uint_value) == true && uint_value == 1);
    yy_assert(yyjson_ptr_get_sint(root, "/0", &sint_value) == true && sint_value == 0);
    yy_assert(yyjson_ptr_get_uint(root, "/0", &uint_value) == true && uint_value == 0);
    yy_assert(yyjson_ptr_get_sint(root, "/i64_max", &sint_value) == true);
    yy_assert(yyjson_ptr_get_uint(root, "/i64_max", &uint_value) == true);
    yy_assert(yyjson_ptr_get_uint(root, "/i64_max+", &uint_value) == true);
    yy_assert(yyjson_ptr_get_real(root, "/pi", &real_value) == true && real_value == (double)3.14159);
    yy_assert(yyjson_ptr_get_num(root, "/-1", &real_value) == true && real_value == (double)-1.0);
    yy_assert(yyjson_ptr_get_num(root, "/zero", &real_value) == true && real_value == (double)0.0);
    yy_assert(yyjson_ptr_get_num(root, "/answer/to/life", &real_value) == true && real_value == (double)42.0);
    yy_assert(yyjson_ptr_get_num(root, "/pi", &real_value) == true && real_value == (double)3.14159);
    yy_assert(yyjson_ptr_get_str(root, "/pistr", &string_value) == true && strcmp(string_value, "3.14159") == 0);
    
    // unsuccessful gets
    yy_assert(yyjson_ptr_get_uint(root, "/-1", &uint_value) == false);  // type cast error
    yy_assert(yyjson_ptr_get_sint(root, "/i64_max+", &sint_value) == false);  // type cast error
    yy_assert(yyjson_ptr_get_num(root, "/pistr", &real_value) == false);  // wrong type
    yy_assert(yyjson_ptr_get_str(root, "/answer/to", &string_value) == false);  // wrong type
    yy_assert(yyjson_ptr_get_uint(root, "/nosuch", &uint_value) == false); // not exist
    yy_assert(yyjson_ptr_get_sint(root, "/nosuch", &sint_value) == false); // not exist
    yy_assert(yyjson_ptr_get_real(root, "/nosuch", &real_value) == false); // not exist
    
    // type mismatch
    yy_assert(yyjson_ptr_get_bool(root, "/pi", &bool_value) == false);
    yy_assert(yyjson_ptr_get_uint(root, "/pi", &uint_value) == false);
    yy_assert(yyjson_ptr_get_sint(root, "/pi", &sint_value) == false);
    yy_assert(yyjson_ptr_get_real(root, "/zero", &real_value) == false);
    yy_assert(yyjson_ptr_get_num(root, "/true", &real_value) == false);
    yy_assert(yyjson_ptr_get_str(root, "/pi", &string_value) == false);
    
    yyjson_doc_free(doc);
}

yy_test_case(test_json_pointer) {
    test_spec();
    test_ptr_get();
    test_ptr_put();
    test_ptr_ctx();
    test_ptr_get_type();
}

#else
yy_test_case(test_json_pointer) {}
#endif
