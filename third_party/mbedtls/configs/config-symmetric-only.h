/**
 * \file config-symmetric-only.h
 *
 * \brief Configuration without any asymmetric cryptography.
 */
/*
 *  Copyright The Mbed TLS Contributors
 *  SPDX-License-Identifier: Apache-2.0
 *
 *  Licensed under the Apache License, Version 2.0 (the "License"); you may
 *  not use this file except in compliance with the License.
 *  You may obtain a copy of the License at
 *
 *  http://www.apache.org/licenses/LICENSE-2.0
 *
 *  Unless required by applicable law or agreed to in writing, software
 *  distributed under the License is distributed on an "AS IS" BASIS, WITHOUT
 *  WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 *  See the License for the specific language governing permissions and
 *  limitations under the License.
 */

/* System support */
//#define MBEDTLS_HAVE_ASM
#define MBEDTLS_HAVE_TIME
#define MBEDTLS_HAVE_TIME_DATE

/* Mbed Crypto feature support */
#define MBEDTLS_CIPHER_MODE_CBC
#define MBEDTLS_CIPHER_MODE_CFB
#define MBEDTLS_CIPHER_MODE_CTR
#define MBEDTLS_CIPHER_MODE_OFB
#define MBEDTLS_CIPHER_MODE_XTS
#define MBEDTLS_CIPHER_PADDING_PKCS7
#define MBEDTLS_CIPHER_PADDING_ONE_AND_ZEROS
#define MBEDTLS_CIPHER_PADDING_ZEROS_AND_LEN
#define MBEDTLS_CIPHER_PADDING_ZEROS
#define MBEDTLS_ERROR_STRERROR_DUMMY
#define MBEDTLS_FS_IO
#define MBEDTLS_ENTROPY_NV_SEED
#define MBEDTLS_SELF_TEST
#define MBEDTLS_USE_PSA_CRYPTO
#define MBEDTLS_VERSION_FEATURES

/* Mbed Crypto modules */
#define MBEDTLS_AES_C
#define MBEDTLS_ASN1_PARSE_C
#define MBEDTLS_ASN1_WRITE_C
#define MBEDTLS_BASE64_C
#define MBEDTLS_CAMELLIA_C
#define MBEDTLS_ARIA_C
#define MBEDTLS_CCM_C
#define MBEDTLS_CHACHA20_C
#define MBEDTLS_CHACHAPOLY_C
#define MBEDTLS_CIPHER_C
#define MBEDTLS_CMAC_C
#define MBEDTLS_CTR_DRBG_C
#define MBEDTLS_DES_C
#define MBEDTLS_ENTROPY_C
#define MBEDTLS_ERROR_C
#define MBEDTLS_GCM_C
#define MBEDTLS_HKDF_C
#define MBEDTLS_HMAC_DRBG_C
#define MBEDTLS_NIST_KW_C
#define MBEDTLS_MD_C
#define MBEDTLS_MD5_C
#define MBEDTLS_OID_C
#define MBEDTLS_PEM_PARSE_C
#define MBEDTLS_PEM_WRITE_C
#define MBEDTLS_PKCS5_C
#define MBEDTLS_PKCS12_C
#define MBEDTLS_PLATFORM_C
#define MBEDTLS_POLY1305_C
#define MBEDTLS_PSA_CRYPTO_C
#define MBEDTLS_PSA_CRYPTO_SE_C
#define MBEDTLS_PSA_CRYPTO_STORAGE_C
#define MBEDTLS_PSA_ITS_FILE_C
#define MBEDTLS_RIPEMD160_C
#define MBEDTLS_SHA1_C
/* The library does not currently support enabling SHA-224 without SHA-256.
 * A future version of the library will have this option disabled
 * by default. */
#define MBEDTLS_SHA224_C
#define MBEDTLS_SHA256_C
#define MBEDTLS_SHA384_C
#define MBEDTLS_SHA512_C
//#define MBEDTLS_THREADING_C
#define MBEDTLS_TIMING_C
#define MBEDTLS_VERSION_C
