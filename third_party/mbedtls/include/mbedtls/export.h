#if _MSC_VER
#define _DLL_EXPORT_FLAG __declspec(dllexport)
#define _DLL_IMPORT_FLAG __declspec(dllimport)
#elif __GNUC__ >= 4
#define _DLL_EXPORT_FLAG __attribute__((visibility("default")))
#define _DLL_IMPORT_FLAG
#else
#define _DLL_EXPORT_FLAG
#define _DLL_IMPORT_FLAG
#endif

#ifndef MBEDTLS_EXPORT
#if defined(MAKING_SHARED_MBEDTLS)
#define MBEDTLS_EXPORT _DLL_EXPORT_FLAG
#elif defined(USING_SHARED_MBEDTLS)
#define MBEDTLS_EXPORT _DLL_IMPORT_FLAG
#else
#define MBEDTLS_EXPORT
#endif
#endif

#ifndef MBEDX509_EXPORT
#if defined(MAKING_SHARED_MBEDX509)
#define MBEDX509_EXPORT _DLL_EXPORT_FLAG
#elif defined(USING_SHARED_MBEDTLS)
#define MBEDX509_EXPORT _DLL_IMPORT_FLAG
#else
#define MBEDX509_EXPORT
#endif
#endif

#ifndef MBEDCRYPTO_EXPORT
#if defined(MAKING_SHARED_MBEDCRYPTO)
#define MBEDCRYPTO_EXPORT _DLL_EXPORT_FLAG
#elif defined(USING_SHARED_MBEDTLS)
#define MBEDCRYPTO_EXPORT _DLL_IMPORT_FLAG
#else
#define MBEDCRYPTO_EXPORT
#endif
#endif
