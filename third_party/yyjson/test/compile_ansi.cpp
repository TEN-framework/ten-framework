/* 
 This file is used to test the compatibility of C++. 
 It should be able to compile successfully in C++ environments.
*/

#include "yyjson.c"

int main(void) {
    yyjson_mut_doc *mdoc = yyjson_mut_doc_new(NULL);
    yyjson_mut_val *root = yyjson_mut_int(mdoc, 0);
    yyjson_mut_doc_set_root(mdoc, root);
    yyjson_mut_doc_free(mdoc);
    return 0;
}
