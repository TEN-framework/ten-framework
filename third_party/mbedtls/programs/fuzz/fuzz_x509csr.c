#define MBEDTLS_ALLOW_PRIVATE_ACCESS

#include <stdint.h>
#include "mbedtls/x509_csr.h"

int LLVMFuzzerTestOneInput(const uint8_t *Data, size_t Size) {
#ifdef MBEDTLS_X509_CSR_PARSE_C
    int ret;
    mbedtls_x509_csr csr;
    unsigned char buf[4096];

    mbedtls_x509_csr_init( &csr );
    ret = mbedtls_x509_csr_parse( &csr, Data, Size );
#if !defined(MBEDTLS_X509_REMOVE_INFO)
    if (ret == 0) {
        ret = mbedtls_x509_csr_info( (char *) buf, sizeof( buf ) - 1, " ", &csr );
    }
#else
    ((void) ret);
    ((void) buf);
#endif /* !MBEDTLS_X509_REMOVE_INFO */
    mbedtls_x509_csr_free( &csr );
#else
    (void) Data;
    (void) Size;
#endif

    return 0;
}
