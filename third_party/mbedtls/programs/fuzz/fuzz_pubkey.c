#define MBEDTLS_ALLOW_PRIVATE_ACCESS

#include <stdint.h>
#include <stdlib.h>
#include "mbedtls/pk.h"

int LLVMFuzzerTestOneInput(const uint8_t *Data, size_t Size) {
#ifdef MBEDTLS_PK_PARSE_C
    int ret;
    mbedtls_pk_context pk;

    mbedtls_pk_init( &pk );
    ret = mbedtls_pk_parse_public_key( &pk, Data, Size );
    if (ret == 0) {
#if defined(MBEDTLS_RSA_C)
        if( mbedtls_pk_get_type( &pk ) == MBEDTLS_PK_RSA )
        {
            mbedtls_mpi N, P, Q, D, E, DP, DQ, QP;
            mbedtls_rsa_context *rsa;

            mbedtls_mpi_init( &N ); mbedtls_mpi_init( &P ); mbedtls_mpi_init( &Q );
            mbedtls_mpi_init( &D ); mbedtls_mpi_init( &E ); mbedtls_mpi_init( &DP );
            mbedtls_mpi_init( &DQ ); mbedtls_mpi_init( &QP );

            rsa = mbedtls_pk_rsa( pk );
            if ( mbedtls_rsa_export( rsa, &N, NULL, NULL, NULL, &E ) != 0 ) {
                abort();
            }
            if ( mbedtls_rsa_export( rsa, &N, &P, &Q, &D, &E ) != MBEDTLS_ERR_RSA_BAD_INPUT_DATA ) {
                abort();
            }
            if ( mbedtls_rsa_export_crt( rsa, &DP, &DQ, &QP ) != MBEDTLS_ERR_RSA_BAD_INPUT_DATA ) {
                abort();
            }

            mbedtls_mpi_free( &N ); mbedtls_mpi_free( &P ); mbedtls_mpi_free( &Q );
            mbedtls_mpi_free( &D ); mbedtls_mpi_free( &E ); mbedtls_mpi_free( &DP );
            mbedtls_mpi_free( &DQ ); mbedtls_mpi_free( &QP );

        }
        else
#endif
#if defined(MBEDTLS_ECP_C)
        if( mbedtls_pk_get_type( &pk ) == MBEDTLS_PK_ECKEY ||
            mbedtls_pk_get_type( &pk ) == MBEDTLS_PK_ECKEY_DH )
        {
            mbedtls_ecp_keypair *ecp = mbedtls_pk_ec( pk );
            mbedtls_ecp_group_id grp_id = ecp->grp.id;
            const mbedtls_ecp_curve_info *curve_info =
                mbedtls_ecp_curve_info_from_grp_id( grp_id );

            /* If the curve is not supported, the key should not have been
             * accepted. */
            if( curve_info == NULL )
                abort( );

            /* It's a public key, so the private value should not have
             * been changed from its initialization to 0. */
            if( mbedtls_mpi_cmp_int( &ecp->d, 0 ) != 0 )
                abort( );
        }
        else
#endif
        {
            /* The key is valid but is not of a supported type.
             * This should not happen. */
            abort( );
        }
    }
    mbedtls_pk_free( &pk );
#else
    (void) Data;
    (void) Size;
#endif //MBEDTLS_PK_PARSE_C

    return 0;
}
