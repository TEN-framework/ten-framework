/*==============================================================================
 * A command line tool to print JSON info and rewrite JSON.
 * Copyright (C) 2020 Yaoyuan <ibireme@gmail.com>.
 *
 * Released under the MIT License:
 * https://github.com/ibireme/yyjson/blob/master/LICENSE
 *============================================================================*/

#include <stdio.h>
#include "yyjson.h"

void print_help(void) {
    printf("JSON info tool\n");
    printf("Usage: jsoninfo [options] file\n");
    printf("Example of printing info: jsoninfo twitter.json\n");
    printf("Example of minify json: jsoninfo -m -o twitter.min.json twitter.json\n");
    printf("Options:\n");
    printf("  -h --help         Print this help.\n");
    printf("  -p --pretty       Rewrite with pretty format (default).\n");
    printf("  -m --minify       Rewrite with minify format.\n");
    printf("  -e --escape       Escape unicode for rewrite.\n");
    printf("  -s --slash        Escape slashes for rewrite.\n");
    printf("  -o --output file  Output file path for rewrite.\n");
}

static const char *O_PATH = NULL;
static const char *O_OUT = NULL;
static bool O_PRETTY = false;
static bool O_MINIFY = false;
static bool O_ESCAPE = false;
static bool O_SLASH = false;

int main(int argc, const char * argv[]) {
    if (argc <= 1) {
        print_help();
        return 0;
    }
    
    for (int i = 1; i < argc - 1; i++) {
        const char *arg = argv[i];
        size_t len = strlen(arg);
        if (len < 2 || arg[0] != '-') {
            printf("unknown option: %s\n", arg);
            return 0;
        } else if (!strcmp(arg, "-h") || !strcmp(arg, "--help")) {
            print_help();
            return 0;
        } else if (!strcmp(arg, "-p") || !strcmp(arg, "--pretty")) {
            if (O_PRETTY) { printf("duplicated option: %s\n", arg); return 0; }
            O_PRETTY = true;
        } else if (!strcmp(arg, "-m") || !strcmp(arg, "--minify")) {
            if (O_MINIFY) { printf("duplicated option: %s\n", arg); return 0; }
            O_MINIFY = true;
        } else if (!strcmp(arg, "-e") || !strcmp(arg, "--escape")) {
            if (O_ESCAPE) { printf("duplicated option: %s\n", arg); return 0; }
            O_ESCAPE = true;
        } else if (!strcmp(arg, "-s") || !strcmp(arg, "--slash")) {
            if (O_SLASH) { printf("duplicated option: %s\n", arg); return 0; }
            O_SLASH = true;
        } else if (!strcmp(arg, "-o") || !strcmp(arg, "--output")) {
            if (O_OUT) { printf("duplicated option: %s\n", arg); return 0; }
            if (++i >= argc - 1) { printf("no input file\n"); return 0; }
            O_OUT = argv[i];
        } else {
            printf("unknown option: %s\n", arg);
            return 0;
        }
    }
    O_PATH = argv[argc - 1];
    if (!strcmp(O_PATH, "-h") || !strcmp(O_PATH, "--help")) {
        print_help();
        return 0;
    }
    if (strlen(O_PATH) == 0 || O_PATH[0] == '-') {
        printf("no input file\n");
        return 0;
    }
    if (O_MINIFY && O_PRETTY) {
        printf("conflict option --minify and --pretty\n");
        return 0;
    }
    if (O_MINIFY || O_PRETTY || O_ESCAPE || O_SLASH) {
        if (!O_OUT) {
            printf("no output file specified\n");
            return 0;
        }
    }
    if (!O_MINIFY && !O_PRETTY) {
        O_PRETTY = true;
    }
    
    yyjson_read_err err;
    yyjson_doc *doc = yyjson_read_file(O_PATH, 0 , NULL, &err);
    if (!doc) {
        printf("JSON read fail: %s, position:%ld\n", err.msg, (long)err.pos);
        return 0;
    }
    
    if (O_OUT) {
        yyjson_write_err werr;
        yyjson_write_flag flg = YYJSON_WRITE_NOFLAG;
        if (O_PRETTY) flg |= YYJSON_WRITE_PRETTY;
        if (O_ESCAPE) flg |= YYJSON_WRITE_ESCAPE_UNICODE;
        if (O_SLASH) flg |= YYJSON_WRITE_ESCAPE_SLASHES;
        bool suc = yyjson_write_file(O_OUT, doc, flg, NULL, &werr);
        if (!suc) {
            printf("Write fail: %s.\n", werr.msg);
        }
        yyjson_doc_free(doc);
        return 0;
    }
    
    long num_null = 0;
    long num_bool = 0;
    long num_int = 0;
    long num_real = 0;
    long num_str = 0;
    long num_obj = 0;
    long num_arr = 0;
    for (size_t i = 0, max = yyjson_doc_get_val_count(doc); i < max; i++) {
        yyjson_val *val = doc->root + i;
        switch (yyjson_get_type(val)) {
            case YYJSON_TYPE_NULL: num_null++; break;
            case YYJSON_TYPE_BOOL: num_bool++; break;
            case YYJSON_TYPE_STR: num_str++; break;
            case YYJSON_TYPE_OBJ: num_obj++; break;
            case YYJSON_TYPE_ARR: num_arr++; break;
            case YYJSON_TYPE_NUM:
                switch (yyjson_get_subtype(val)) {
                    case YYJSON_SUBTYPE_UINT: num_int++; break;
                    case YYJSON_SUBTYPE_SINT: num_int++; break;
                    case YYJSON_SUBTYPE_REAL: num_real++; break;
                    default: break;
                }
            default: break;
        }
    }
    
    size_t val_count = yyjson_doc_get_val_count(doc);
    size_t read_size = yyjson_doc_get_read_size(doc);
    
    const char *name = O_PATH;
    for (const char *tmp = name, *max = name + strlen(name); tmp < max; tmp++) {
        if ((*tmp == '/' || *tmp == '\\') && tmp + 1 < max) {
            name = tmp + 1;
        }
    }
    
    printf("name: %s\n", name);
    printf("size: %ld\n", (long)read_size);
    printf("vals: %ld\n", (long)val_count);
    printf("  arr:  %ld\n", num_arr);
    printf("  obj:  %ld\n", num_obj);
    printf("  str:  %ld\n", num_str);
    printf("  int:  %ld\n", num_int);
    printf("  real: %ld\n", num_real);
    printf("  bool: %ld\n", num_bool);
    printf("  null: %ld\n", num_null);
    
    return 0;
}
