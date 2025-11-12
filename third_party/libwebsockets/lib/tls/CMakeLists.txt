#
# libwebsockets - small server side websockets and web server implementation
#
# Copyright (C) 2010 - 2020 Andy Green <andy@warmcat.com>
#
# Permission is hereby granted, free of charge, to any person obtaining a copy
# of this software and associated documentation files (the "Software"), to
# deal in the Software without restriction, including without limitation the
# rights to use, copy, modify, merge, publish, distribute, sublicense, and/or
# sell copies of the Software, and to permit persons to whom the Software is
# furnished to do so, subject to the following conditions:
#
# The above copyright notice and this permission notice shall be included in
# all copies or substantial portions of the Software.
#
# THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
# IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
# FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
# AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
# LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
# FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS
# IN THE SOFTWARE.
#
#
# This converts everything about the tls support into
#
# - entries on SOURCES (modifications set back in PARENT_SCOPE)
# - entries on LIB_LIST (modifications set back in PARENT_SCOPE)
# - include_directories()
# - Api build-time discovery results set in PARENT_SCOPE
#
# Everything else is handled privately here.

include_directories(.)

# Allow the user to use the old CyaSSL options/library in stead of wolfSSL
if (LWS_WITH_CYASSL AND LWS_WITH_WOLFSSL)
	message(FATAL_ERROR "LWS_WITH_CYASSL and LWS_WITH_WOLFSSL are mutually exclusive!")
endif()

if (LWS_WITH_CYASSL)
	# Copy CyaSSL options to the wolfSSL options
	set(LWS_WITH_WOLFSSL ${LWS_WITH_CYASSL} CACHE BOOL "Use wolfSSL/CyaSSL instead of OpenSSL" FORCE PARENT_SCOPE)
	set(LWS_WOLFSSL_LIBRARIES ${LWS_CYASSL_LIBRARIES} CACHE PATH "Path to wolfSSL/CyaSSL libraries" FORCE PARENT_SCOPE)
	set(LWS_WOLFSSL_INCLUDE_DIRS ${LWS_CYASSL_INCLUDE_DIRS} CACHE PATH "Path to wolfSSL/CyaSSL header files" FORCE PARENT_SCOPE)
endif()

set(LWS_OPENSSL_LIBRARIES CACHE PATH "Path to the OpenSSL library" )
set(LWS_OPENSSL_INCLUDE_DIRS CACHE PATH "Path to the OpenSSL include directory" )
set(LWS_WOLFSSL_LIBRARIES CACHE PATH "Path to the wolfSSL library" )
set(LWS_WOLFSSL_INCLUDE_DIRS CACHE PATH "Path to the wolfSSL include directory" )


if (LWS_WITH_BORINGSSL)
	# boringssl deprecated EVP_PKEY
	set (LWS_WITH_GENHASH OFF PARENT_SCOPE)
endif()

if (LWS_WITH_SSL AND NOT LWS_WITH_WOLFSSL AND NOT LWS_WITH_MBEDTLS)
	if ("${LWS_OPENSSL_LIBRARIES}" STREQUAL "" OR "${LWS_OPENSSL_INCLUDE_DIRS}" STREQUAL "")
	else()
		if (NOT LWS_PLAT_FREERTOS)
			set(OPENSSL_LIBRARIES ${LWS_OPENSSL_LIBRARIES})
		endif()
		set(OPENSSL_INCLUDE_DIRS ${LWS_OPENSSL_INCLUDE_DIRS})
		set(OPENSSL_FOUND 1)
	endif()
endif()

if (LWS_WITH_SSL AND LWS_WITH_WOLFSSL)
	if ("${LWS_WOLFSSL_LIBRARIES}" STREQUAL "" OR "${LWS_WOLFSSL_INCLUDE_DIRS}" STREQUAL "")
		include (FindPkgConfig)
		PKG_SEARCH_MODULE(LWS_WOLFSSL wolfssl)

		if (NOT WOLFSSL_FOUND)
			if (LWS_WITH_CYASSL)
				message(FATAL_ERROR "You must set LWS_CYASSL_LIBRARIES and LWS_CYASSL_INCLUDE_DIRS when LWS_WITH_CYASSL is turned on.")
			else()
				message(FATAL_ERROR "You must set LWS_WOLFSSL_LIBRARIES and LWS_WOLFSSL_INCLUDE_DIRS when LWS_WITH_WOLFSSL is turned on.")
			endif()
		endif()
	else()
		set(WOLFSSL_LIBRARIES ${LWS_WOLFSSL_LIBRARIES})
		set(WOLFSSL_INCLUDE_DIRS ${LWS_WOLFSSL_INCLUDE_DIRS})
		set(WOLFSSL_FOUND 1)
	endif()
	set(USE_WOLFSSL 1)
	set(USE_WOLFSSL 1 PARENT_SCOPE)
	set(LWS_WITH_TLS 1 PARENT_SCOPE)
	if (LWS_WITH_CYASSL)
		set(USE_OLD_CYASSL 1)
	endif()
endif()

if (LWS_SSL_CLIENT_USE_OS_CA_CERTS)
	set(LWS_SSL_CLIENT_USE_OS_CA_CERTS 1 PARENT_SCOPE)
endif()

if (LWS_WITH_MBEDTLS)
	add_subdirectory(mbedtls)
	include_directories(${_CMAKE_INC_LIST})
endif()

# The base dir where the test-apps look for the SSL certs.
set(LWS_OPENSSL_CLIENT_CERTS ../share CACHE PATH "Server SSL certificate directory")
if (WIN32)
	set(LWS_OPENSSL_CLIENT_CERTS . CACHE PATH "Client SSL certificate directory" PARENT_SCOPE)
else()
	set(LWS_OPENSSL_CLIENT_CERTS /etc/pki/tls/certs/ CACHE PATH "Client SSL certificate directory")
endif()

if (LWS_WITH_SSL)
	list(APPEND SOURCES
		tls/tls.c)
	if (LWS_WITH_NETWORK)
		list(APPEND SOURCES
			tls/tls-network.c)
	endif()
	if (LWS_WITH_TLS_SESSIONS)
		list(APPEND SOURCES
			tls/tls-sessions.c)
	endif()
	if (LWS_WITH_TLS_JIT_TRUST)
		list(APPEND SOURCES
			tls/tls-jit-trust.c)
	endif()
	
	if (LWS_WITH_MBEDTLS)
		list(APPEND SOURCES
			tls/mbedtls/mbedtls-tls.c
			tls/mbedtls/mbedtls-extensions.c
			tls/mbedtls/mbedtls-x509.c)
		if (LWS_WITH_NETWORK)
			list(APPEND SOURCES
				tls/mbedtls/mbedtls-ssl.c)
		endif()
		if (LWS_WITH_TLS_JIT_TRUST)
			list(APPEND SOURCES
				tls/mbedtls/mbedtls-extensions.c)
		endif()
		if (LWS_WITH_TLS_SESSIONS)
			list(APPEND SOURCES
				tls/mbedtls/mbedtls-session.c)
		endif()
		if (LWS_WITH_GENCRYPTO)
			list(APPEND SOURCES
				tls/mbedtls/lws-genhash.c
				tls/mbedtls/lws-genrsa.c
				tls/mbedtls/lws-genaes.c
				tls/lws-genec-common.c
				tls/mbedtls/lws-genec.c
				tls/mbedtls/lws-gencrypto.c)
		endif()
	else()
		list(APPEND SOURCES
			tls/openssl/openssl-tls.c
			tls/openssl/openssl-x509.c)
		if (LWS_WITH_NETWORK)
			list(APPEND SOURCES
				tls/openssl/openssl-ssl.c)
		endif()
		if (LWS_WITH_TLS_SESSIONS)
			list(APPEND SOURCES
				tls/openssl/openssl-session.c)
		endif()
		if (LWS_WITH_GENCRYPTO)
			list(APPEND SOURCES
				tls/openssl/lws-genhash.c
				tls/openssl/lws-genrsa.c
				tls/openssl/lws-genaes.c
				tls/lws-genec-common.c
				tls/openssl/lws-genec.c
				tls/openssl/lws-gencrypto.c)
		endif()
	endif()
		
	if (NOT LWS_WITHOUT_SERVER)
		list(APPEND SOURCES
			tls/tls-server.c)
		if (LWS_WITH_MBEDTLS)
			list(APPEND SOURCES
				tls/mbedtls/mbedtls-server.c)
		else()
			list(APPEND SOURCES
				tls/openssl/openssl-server.c)
		endif()
	endif()
	if (NOT LWS_WITHOUT_CLIENT)
		list(APPEND SOURCES
			tls/tls-client.c)
		if (LWS_WITH_MBEDTLS)
			list(APPEND SOURCES
				tls/mbedtls/mbedtls-client.c)
		else()
			list(APPEND SOURCES
				tls/openssl/openssl-client.c)
		endif()
		
	endif()
endif()

set(SOURCES ${SOURCES} PARENT_SCOPE)

#
# OpenSSL
#
if (LWS_WITH_SSL)
	message("Compiling with SSL support")
	set(chose_ssl 0)
	if (LWS_WITH_WOLFSSL)
		# Use wolfSSL as OpenSSL replacement.
		# TODO: Add a find_package command for this also.
		message("wolfSSL include dir: ${WOLFSSL_INCLUDE_DIRS}")
		message("wolfSSL libraries: ${WOLFSSL_LIBRARIES}")

		# Additional to the root directory we need to include
		# the wolfssl/ subdirectory which contains the OpenSSL
		# compatibility layer headers.

		if (LWS_WITH_CYASSL)
			foreach(inc ${WOLFSSL_INCLUDE_DIRS})
				set(OPENSSL_INCLUDE_DIRS ${OPENSSL_INCLUDE_DIRS} ${inc} ${inc}/cyassl)
				set(LWS_PUBLIC_INCLUDES ${LWS_PUBLIC_INCLUDES} "${inc}" "${inc}/cyassl")
				set(LWS_PUBLIC_INCLUDES ${LWS_PUBLIC_INCLUDES} PARENT_SCOPE)
			endforeach()
		else()
			foreach(inc ${WOLFSSL_INCLUDE_DIRS})
				set(OPENSSL_INCLUDE_DIRS ${OPENSSL_INCLUDE_DIRS} ${inc} ${inc}/wolfssl)
				set(LWS_PUBLIC_INCLUDES ${LWS_PUBLIC_INCLUDES} "${inc}" "${inc}/wolfssl")
				set(LWS_PUBLIC_INCLUDES ${LWS_PUBLIC_INCLUDES} PARENT_SCOPE)
			endforeach()
		endif()
		set(CMAKE_REQUIRED_INCLUDES ${CMAKE_REQUIRED_INCLUDES} ${OPENSSL_INCLUDE_DIRS})
		set(CMAKE_REQUIRED_INCLUDES ${CMAKE_REQUIRED_INCLUDES} PARENT_SCOPE)
		set(OPENSSL_INCLUDE_DIRS ${OPENSSL_INCLUDE_DIRS} PARENT_SCOPE)
		set(VARIA wolfSSL_)

		list(INSERT LIB_LIST 0 "${WOLFSSL_LIBRARIES}")
		message("LIB_LIST ${LIB_LIST}")
		set(chose_ssl 1)
	endif()

	if (LWS_WITH_MBEDTLS AND DEFINED MBEDTLS_INCLUDE_DIRS AND DEFINED MBEDTLS_LIBRARIES)
		message("MBEDTLS include dir: ${MBEDTLS_INCLUDE_DIRS}")
		message("MBEDTLS libraries: ${MBEDTLS_LIBRARIES}")

		foreach(inc ${MBEDTLS_INCLUDE_DIRS})
				set(LWS_PUBLIC_INCLUDES ${LWS_PUBLIC_INCLUDES} "${inc}" "${inc}/mbedtls")
				set(LWS_PUBLIC_INCLUDES ${LWS_PUBLIC_INCLUDES} PARENT_SCOPE)
		endforeach()

		list(INSERT LIB_LIST 0 "${MBEDTLS_LIBRARIES}")
	endif()
	
	if (LWS_WITH_MBEDTLS)
		set(chose_ssl 1)
	endif()

	if (NOT chose_ssl)
		if (OPENSSL_FOUND AND "${OPENSSL_INCLUDE_DIRS}" STREQUAL "")
			set(OPENSSL_INCLUDE_DIRS "${OPENSSL_INCLUDE_DIR}")
		endif()

		if (NOT OPENSSL_FOUND AND NOT LWS_WITH_BORINGSSL)
			# TODO: Add support for STATIC also.
			if (NOT LWS_PLAT_FREERTOS)
				find_package(PkgConfig QUIET)
				pkg_check_modules(PC_OPENSSL openssl QUIET)
				find_package(OpenSSL REQUIRED)
				list(APPEND OPENSSL_LIBRARIES ${PC_OPENSSL_LIBRARIES})
				set(OPENSSL_LIBRARIES ${OPENSSL_LIBRARIES} PARENT_SCOPE)
			endif()
			set(OPENSSL_INCLUDE_DIRS "${OPENSSL_INCLUDE_DIR}")
		endif()

		message("OpenSSL include dir: ${OPENSSL_INCLUDE_DIRS}")
		if (NOT LWS_PLAT_FREERTOS)
			message("OpenSSL libraries: ${OPENSSL_LIBRARIES}")
		endif()

		if (OPENSSL_INCLUDE_DIRS)
			set(LWS_PUBLIC_INCLUDES ${LWS_PUBLIC_INCLUDES} "${OPENSSL_INCLUDE_DIRS}")
			set(LWS_PUBLIC_INCLUDES ${LWS_PUBLIC_INCLUDES} PARENT_SCOPE)
		endif()
		if (NOT LWS_PLAT_FREERTOS)
			list(INSERT LIB_LIST 0 ${OPENSSL_LIBRARIES})
		endif()

		if (NOT LWS_WITH_MBEDTLS)
			# older (0.98) Openssl lacks this
			set(CMAKE_REQUIRED_INCLUDES ${CMAKE_REQUIRED_INCLUDES} ${OPENSSL_INCLUDE_DIRS} PARENT_SCOPE)
			check_include_file(openssl/ecdh.h LWS_HAVE_OPENSSL_ECDH_H)

			if (LWS_SSL_SERVER_WITH_ECDH_CERT AND NOT LWS_HAVE_OPENSSL_ECDH_H)
				message(FATAL_ERROR "Missing openssl/ecdh.h, so cannot use LWS_SSL_SERVER_WITH_ECDH_CERT")
			endif()
		else()
			unset(LWS_HAVE_OPENSSL_ECDH_H PARENT_SCOPE)
		endif(NOT LWS_WITH_MBEDTLS)
	endif()

endif(LWS_WITH_SSL)

if (DEFINED OPENSSL_INCLUDE_DIRS)
	set(CMAKE_REQUIRED_INCLUDES ${OPENSSL_INCLUDE_DIRS})
endif()
if (DEFINED LIB_LIST)
	set(CMAKE_REQUIRED_LIBRARIES ${LIB_LIST})
endif()
if (UNIX AND NOT (${CMAKE_SYSTEM_NAME} MATCHES "QNX"))
	set(CMAKE_REQUIRED_LIBRARIES ${CMAKE_REQUIRED_LIBRARIES} ${CMAKE_DL_LIBS})
endif()
if ((CMAKE_COMPILER_IS_GNUCC OR CMAKE_COMPILER_IS_GNUCXX) AND NOT (${CMAKE_SYSTEM_NAME} MATCHES "QNX"))
	set(CMAKE_REQUIRED_LIBRARIES ${CMAKE_REQUIRED_LIBRARIES} pthread)
endif()

if (NOT VARIA)
	set(VARIA "")
endif()

CHECK_FUNCTION_EXISTS(${VARIA}SSL_CTX_set1_param LWS_HAVE_SSL_CTX_set1_param PARENT_SCOPE)
CHECK_FUNCTION_EXISTS(${VARIA}SSL_set_info_callback LWS_HAVE_SSL_SET_INFO_CALLBACK PARENT_SCOPE)
CHECK_FUNCTION_EXISTS(${VARIA}X509_VERIFY_PARAM_set1_host LWS_HAVE_X509_VERIFY_PARAM_set1_host PARENT_SCOPE)
CHECK_SYMBOL_EXISTS(${VARIA}X509_VERIFY_PARAM_set1_host LWS_HAVE_X509_VERIFY_PARAM_set1_host_sym PARENT_SCOPE)
if (LWS_HAVE_X509_VERIFY_PARAM_set1_host_sym)
	set(LWS_HAVE_X509_VERIFY_PARAM_set1_host 1 PARENT_SCOPE)
endif()

CHECK_FUNCTION_EXISTS(${VARIA}RSA_set0_key LWS_HAVE_RSA_SET0_KEY PARENT_SCOPE)
CHECK_FUNCTION_EXISTS(${VARIA}X509_get_key_usage LWS_HAVE_X509_get_key_usage PARENT_SCOPE)
CHECK_FUNCTION_EXISTS(${VARIA}SSL_CTX_EVP_PKEY_new_raw_private_key LWS_HAVE_SSL_CTX_EVP_PKEY_new_raw_private_key PARENT_SCOPE)
CHECK_FUNCTION_EXISTS(${VARIA}SSL_CTX_get0_certificate LWS_HAVE_SSL_CTX_get0_certificate PARENT_SCOPE)
CHECK_FUNCTION_EXISTS(${VARIA}SSL_get0_alpn_selected LWS_HAVE_SSL_get0_alpn_selected PARENT_SCOPE)
CHECK_FUNCTION_EXISTS(${VARIA}SSL_set_alpn_protos LWS_HAVE_SSL_set_alpn_protos PARENT_SCOPE)
CHECK_FUNCTION_EXISTS(${VARIA}EVP_aes_128_cfb8 LWS_HAVE_EVP_aes_128_cfb8 PARENT_SCOPE)
CHECK_FUNCTION_EXISTS(${VARIA}EVP_aes_128_cfb128 LWS_HAVE_EVP_aes_128_cfb128 PARENT_SCOPE)
CHECK_FUNCTION_EXISTS(${VARIA}EVP_aes_192_cfb8 LWS_HAVE_EVP_aes_192_cfb8 PARENT_SCOPE)
CHECK_FUNCTION_EXISTS(${VARIA}EVP_aes_192_cfb128 LWS_HAVE_EVP_aes_192_cfb128 PARENT_SCOPE)
CHECK_FUNCTION_EXISTS(${VARIA}EVP_aes_256_cfb8 LWS_HAVE_EVP_aes_256_cfb8 PARENT_SCOPE)
CHECK_FUNCTION_EXISTS(${VARIA}EVP_aes_256_cfb128 LWS_HAVE_EVP_aes_256_cfb128 PARENT_SCOPE)
CHECK_FUNCTION_EXISTS(${VARIA}EVP_aes_128_xts LWS_HAVE_EVP_aes_128_xts PARENT_SCOPE)
CHECK_FUNCTION_EXISTS(${VARIA}EVP_aes_128_ofb LWS_HAVE_EVP_aes_128_ofb PARENT_SCOPE)
CHECK_FUNCTION_EXISTS(${VARIA}EVP_aes_128_ecb LWS_HAVE_EVP_aes_128_ecb PARENT_SCOPE)
CHECK_FUNCTION_EXISTS(${VARIA}EVP_aes_128_ctr LWS_HAVE_EVP_aes_128_ctr PARENT_SCOPE)


CHECK_FUNCTION_EXISTS(${VARIA}EVP_aes_128_xts LWS_HAVE_EVP_aes_128_xts PARENT_SCOPE)
CHECK_FUNCTION_EXISTS(${VARIA}RSA_verify_pss_mgf1 LWS_HAVE_RSA_verify_pss_mgf1 PARENT_SCOPE)
CHECK_FUNCTION_EXISTS(${VARIA}HMAC_CTX_new LWS_HAVE_HMAC_CTX_new PARENT_SCOPE)
CHECK_SYMBOL_EXISTS(${VARIA}SSL_CTX_set_ciphersuites LWS_HAVE_SSL_CTX_set_ciphersuites PARENT_SCOPE)
CHECK_FUNCTION_EXISTS(${VARIA}EVP_PKEY_new_raw_private_key LWS_HAVE_EVP_PKEY_new_raw_private_key PARENT_SCOPE)
CHECK_FUNCTION_EXISTS(${VARIA}SSL_SESSION_set_time LWS_HAVE_SSL_SESSION_set_time PARENT_SCOPE)
CHECK_SYMBOL_EXISTS(${VARIA}SSL_SESSION_up_ref LWS_HAVE_SSL_SESSION_up_ref PARENT_SCOPE)


# deprecated in openssl v3
CHECK_FUNCTION_EXISTS(${VARIA}EC_KEY_new_by_curve_name LWS_HAVE_EC_KEY_new_by_curve_name PARENT_SCOPE)

if (LWS_WITH_SSL AND NOT LWS_WITH_MBEDTLS)
	# we don't want to confuse what's in or out of the wrapper with
	# what's in an openssl also installed on the build host
CHECK_C_SOURCE_COMPILES("#include <openssl/ssl.h>\nint main(void) { STACK_OF(X509) *c = NULL; SSL_CTX *ctx = NULL; return (int)SSL_CTX_get_extra_chain_certs_only(ctx, &c); }\n" LWS_HAVE_SSL_EXTRA_CHAIN_CERTS)
CHECK_C_SOURCE_COMPILES("#include <openssl/ssl.h>\nint main(void) { EVP_MD_CTX *md_ctx = NULL; EVP_MD_CTX_free(md_ctx); return 0; }\n" LWS_HAVE_EVP_MD_CTX_free)
CHECK_C_SOURCE_COMPILES("#include <openssl/ssl.h>\nint main(void) { OPENSSL_STACK *x = NULL; return !x; } \n" LWS_HAVE_OPENSSL_STACK)
set(LWS_HAVE_SSL_EXTRA_CHAIN_CERTS ${LWS_HAVE_SSL_EXTRA_CHAIN_CERTS} PARENT_SCOPE)
set(LWS_HAVE_EVP_MD_CTX_free ${LWS_HAVE_EVP_MD_CTX_free} PARENT_SCOPE)
CHECK_FUNCTION_EXISTS(${VARIA}ECDSA_SIG_set0 LWS_HAVE_ECDSA_SIG_set0 PARENT_SCOPE)
CHECK_FUNCTION_EXISTS(${VARIA}BN_bn2binpad LWS_HAVE_BN_bn2binpad PARENT_SCOPE)
CHECK_FUNCTION_EXISTS(${VARIA}EVP_aes_128_wrap LWS_HAVE_EVP_aes_128_wrap PARENT_SCOPE)
CHECK_FUNCTION_EXISTS(${VARIA}EC_POINT_get_affine_coordinates LWS_HAVE_EC_POINT_get_affine_coordinates PARENT_SCOPE)
CHECK_SYMBOL_EXISTS(${VARIA}SSL_CTX_load_verify_file LWS_HAVE_SSL_CTX_load_verify_file PARENT_SCOPE)
CHECK_SYMBOL_EXISTS(${VARIA}SSL_CTX_load_verify_dir LWS_HAVE_SSL_CTX_load_verify_dir PARENT_SCOPE)
endif()

if (LWS_WITH_MBEDTLS)
	set(LWS_HAVE_TLS_CLIENT_METHOD 1 PARENT_SCOPE)
	if (NOT LWS_PLAT_FREERTOS)
		# not supported in esp-idf openssl wrapper yet, but is in our version
		set(LWS_HAVE_X509_VERIFY_PARAM_set1_host 1 PARENT_SCOPE)
	endif()

	set(CMAKE_REQUIRED_LIBRARIES ${MBEDTLS_LIBRARY} ${MBEDX509_LIBRARY} ${MBEDCRYPTO_LIBRARY})
	set(CMAKE_REQUIRED_INCLUDES ${CMAKE_REQUIRED_INCLUDES} ${MBEDTLS_INCLUDE_DIRS})

	if (ESP_PLATFORM)
		# we know we should have things
		set(LWS_HAVE_MBEDTLS_AUTH_KEY_ID 1 CACHE BOOL x)
		set(LWS_HAVE_mbedtls_ssl_conf_alpn_protocols 1 CACHE BOOL x)
		set(LWS_HAVE_mbedtls_ssl_get_alpn_protocol 1 CACHE BOOL x)
		set(LWS_HAVE_mbedtls_ssl_conf_sni 1 CACHE BOOL x)
		set(LWS_HAVE_mbedtls_ssl_set_hs_ca_chain 1 CACHE BOOL x)
		set(LWS_HAVE_mbedtls_ssl_set_hs_own_cert 1 CACHE BOOL x)
		set(LWS_HAVE_mbedtls_ssl_set_hs_authmode 1 CACHE BOOL x)
		set(LWS_HAVE_mbedtls_net_init 1 CACHE BOOL x)
		set(LWS_HAVE_mbedtls_x509_crt_parse_file 1 CACHE BOOL x) # some embedded may lack filesystem
		set(LWS_HAVE_mbedtls_md_setup 1 CACHE BOOL x) # not on xenial 2.2
		set(LWS_HAVE_mbedtls_rsa_complete 1 CACHE BOOL x) # not on xenial 2.2
		set(LWS_HAVE_mbedtls_internal_aes_encrypt 1 CACHE BOOL x) # not on xenial 2.2
	else()
		CHECK_C_SOURCE_COMPILES("#include <mbedtls/x509_crt.h>\nint main(void) { struct mbedtls_x509_crt c; c.authority_key_id.keyIdentifier.tag = MBEDTLS_ASN1_OCTET_STRING; return c.authority_key_id.keyIdentifier.tag; }\n" LWS_HAVE_MBEDTLS_AUTH_KEY_ID)
		CHECK_C_SOURCE_COMPILES("#include <mbedtls/ssl.h>\nint main(void) { void *v = (void *)mbedtls_ssl_set_verify; return !!v; }\n" LWS_HAVE_mbedtls_ssl_set_verify)
		CHECK_SYMBOL_EXISTS(mbedtls_ssl_conf_alpn_protocols LWS_HAVE_mbedtls_ssl_conf_alpn_protocols PARENT_SCOPE)
		CHECK_FUNCTION_EXISTS(mbedtls_ssl_get_alpn_protocol LWS_HAVE_mbedtls_ssl_get_alpn_protocol PARENT_SCOPE)
		CHECK_FUNCTION_EXISTS(mbedtls_ssl_conf_sni LWS_HAVE_mbedtls_ssl_conf_sni PARENT_SCOPE)
		CHECK_FUNCTION_EXISTS(mbedtls_ssl_set_hs_ca_chain LWS_HAVE_mbedtls_ssl_set_hs_ca_chain PARENT_SCOPE)
		CHECK_FUNCTION_EXISTS(mbedtls_ssl_set_hs_own_cert LWS_HAVE_mbedtls_ssl_set_hs_own_cert PARENT_SCOPE)
		CHECK_FUNCTION_EXISTS(mbedtls_ssl_set_hs_authmode LWS_HAVE_mbedtls_ssl_set_hs_authmode PARENT_SCOPE)
		CHECK_FUNCTION_EXISTS(mbedtls_net_init LWS_HAVE_mbedtls_net_init PARENT_SCOPE)
		CHECK_FUNCTION_EXISTS(mbedtls_x509_crt_parse_file LWS_HAVE_mbedtls_x509_crt_parse_file PARENT_SCOPE) # some embedded may lack filesystem
		CHECK_FUNCTION_EXISTS(mbedtls_md_setup LWS_HAVE_mbedtls_md_setup PARENT_SCOPE) # not on xenial 2.2
		CHECK_FUNCTION_EXISTS(mbedtls_rsa_complete LWS_HAVE_mbedtls_rsa_complete PARENT_SCOPE) # not on xenial 2.2
		CHECK_FUNCTION_EXISTS(mbedtls_internal_aes_encrypt LWS_HAVE_mbedtls_internal_aes_encrypt PARENT_SCOPE) # not on xenial 2.2
	endif()
else()
CHECK_FUNCTION_EXISTS(${VARIA}TLS_client_method LWS_HAVE_TLS_CLIENT_METHOD PARENT_SCOPE)
CHECK_FUNCTION_EXISTS(${VARIA}TLSv1_2_client_method LWS_HAVE_TLSV1_2_CLIENT_METHOD PARENT_SCOPE)
endif()

# Generate self-signed SSL certs for the test-server.

if (LWS_WITH_SSL AND NOT LWS_WITH_WOLFSSL)
	message("Searching for OpenSSL executable and dlls")
	find_package(OpenSSLbins)
	if (DEFINED OPENSSL_EXECUTABLE)
		message("OpenSSL executable: ${OPENSSL_EXECUTABLE}")
		
		if (OPENSSL_EXECUTABLE MATCHES "^$")
			set(OPENSSL_EXECUTABLE openssl)
		endif()
	endif()
	if (NOT DEFINED OPENSSL_EXECUTABLE)
		set(OPENSSL_EXECUTABLE openssl)
	endif()

endif()

set(GENCERTS 0)

if (LWS_WITH_SSL AND OPENSSL_EXECUTABLE AND NOT LWS_WITHOUT_TEST_SERVER AND NOT LWS_WITHOUT_SERVER AND NOT LWS_WITHOUT_TESTAPPS)
	set(GENCERTS 1)
endif()
if (LWS_PLAT_FREERTOS AND LWS_WITH_SSL)
	set(GENCERTS 1)
endif()
message(" GENCERTS = ${GENCERTS}")
if (GENCERTS)
	message("Generating SSL Certificates for the test-server...")

	set(TEST_SERVER_SSL_KEY "${PROJECT_BINARY_DIR}/libwebsockets-test-server.key.pem")
	set(TEST_SERVER_SSL_CERT "${PROJECT_BINARY_DIR}/libwebsockets-test-server.pem")

	if (WIN32)
		if (MINGW)
			message("cmd = \"${OPENSSL_EXECUTABLE}\" req -new -newkey rsa:2048 -days 10000 -nodes -x509 -subj \"/C=GB/ST=Erewhon/L=All around/O=libwebsockets-test/CN=localhost\" -keyout \"${TEST_SERVER_SSL_KEY}\" -out \"${TEST_SERVER_SSL_CERT}\"")
			execute_process(
				COMMAND "${OPENSSL_EXECUTABLE}" req -new -newkey rsa:2048 -days 10000 -nodes -x509 -subj "/C=GB/ST=Erewhon/L=All around/O=libwebsockets-test/CN=localhost" -keyout "${TEST_SERVER_SSL_KEY}" -out "${TEST_SERVER_SSL_CERT}"
				RESULT_VARIABLE OPENSSL_RETURN_CODE)
		else()
			file(WRITE "${PROJECT_BINARY_DIR}/openssl_input.txt"
				"GB\n"
				"Erewhon\n"
				"All around\n"
				"libwebsockets-test\n"
				"localhost\n"
				"none@invalid.org\n\n"
				)

			# The "type" command is a bit picky with paths.
			file(TO_NATIVE_PATH "${PROJECT_BINARY_DIR}/openssl_input.txt" OPENSSL_INPUT_WIN_PATH)
			message("OPENSSL_INPUT_WIN_PATH = ${OPENSSL_INPUT_WIN_PATH}")
			message("cmd = \"${OPENSSL_EXECUTABLE}\" req -new -newkey rsa:2048 -days 10000 -nodes -x509 -keyout \"${TEST_SERVER_SSL_KEY}\" -out \"${TEST_SERVER_SSL_CERT}\"")

			if(OPENSSL_CONFIG_FILE)
				execute_process(
					COMMAND cmd /c type "${OPENSSL_INPUT_WIN_PATH}"
					COMMAND "${OPENSSL_EXECUTABLE}" req -config ${OPENSSL_CONFIG_FILE} -new -newkey rsa:2048 -days 10000 -nodes -x509 -keyout "${TEST_SERVER_SSL_KEY}" -out "${TEST_SERVER_SSL_CERT}"
					RESULT_VARIABLE OPENSSL_RETURN_CODE
					OUTPUT_QUIET ERROR_QUIET)
			else()
				execute_process(
					COMMAND cmd /c type "${OPENSSL_INPUT_WIN_PATH}"
					COMMAND "${OPENSSL_EXECUTABLE}" req -new -newkey rsa:2048 -days 10000 -nodes -x509 -keyout "${TEST_SERVER_SSL_KEY}" -out "${TEST_SERVER_SSL_CERT}"
					RESULT_VARIABLE OPENSSL_RETURN_CODE
					OUTPUT_QUIET ERROR_QUIET)
			endif()

			message("\n")
		endif()

		if (OPENSSL_RETURN_CODE)
			message(WARNING "!!! Failed to generate SSL certificate for Test Server using cmd.exe !!!:\nOpenSSL return code = ${OPENSSL_RETURN_CODE}")
		else()
			message("SUCCSESFULLY generated SSL certificate")
		endif()
	else()
               if (CMAKE_HOST_SYSTEM_NAME MATCHES "NetBSD")
                execute_process(
                        COMMAND "${OPENSSL_EXECUTABLE}"
                                req -new -newkey rsa:2048 -days 10000 -nodes -x509 -subj "/O=lws/CN=localhost" -keyout "${TEST_SERVER_SSL_KEY}" -out "${TEST_SERVER_SSL_CERT}"
                        RESULT_VARIABLE OPENSSL_RETURN_CODE
                        #               OUTPUT_QUIET ERROR_QUIET
                        )

               else()
	
		# Unix.
		execute_process(
			COMMAND printf "GB\\nErewhon\\nAll around\\nlibwebsockets-test\\n\\nlocalhost\\nnone@invalid.org\\n"
			COMMAND "${OPENSSL_EXECUTABLE}"
				req -new -newkey rsa:2048 -days 10000 -nodes -x509 -keyout "${TEST_SERVER_SSL_KEY}" -out "${TEST_SERVER_SSL_CERT}"
			RESULT_VARIABLE OPENSSL_RETURN_CODE
			#		OUTPUT_QUIET ERROR_QUIET
			)
			
		endif()

		if (OPENSSL_RETURN_CODE)
			message(WARNING "!!! Failed to generate SSL certificate for Test Server!!!:\nOpenSSL return code = ${OPENSSL_RETURN_CODE}")
		else()
			message("SUCCESSFULLY generated SSL certificate")
		endif()
	endif()

	list(APPEND TEST_SERVER_DATA 
		"${TEST_SERVER_SSL_KEY}"
		"${TEST_SERVER_SSL_CERT}")
endif()

#
# Copy OpenSSL dlls to the output directory on Windows.
# (Otherwise we'll get an error when trying to run)
#
if (MSVC AND LWS_WITH_SSL AND NOT LWS_WITH_WOLFSSL)
	if(OPENSSL_BIN_FOUND)
		message("OpenSSL dlls found:")
		message("  Libeay: ${LIBEAY_BIN}")
		message("  SSLeay: ${SSLEAY_BIN}")

		foreach(TARGET_BIN ${TEST_APP_LIST})
			add_custom_command(TARGET ${TARGET_BIN}
				POST_BUILD
				COMMAND "${CMAKE_COMMAND}" -E copy "${LIBEAY_BIN}" "$<TARGET_FILE_DIR:${TARGET_BIN}>" VERBATIM)
			add_custom_command(TARGET ${TARGET_BIN}
				POST_BUILD
				COMMAND "${CMAKE_COMMAND}" -E copy "${SSLEAY_BIN}" "$<TARGET_FILE_DIR:${TARGET_BIN}>" VERBATIM)

			#
			# Win32: if we are using libuv, also need to copy it in the output dir
			#
			if (MSVC AND LWS_WITH_LIBUV)
				STRING(REPLACE ".lib" ".dll" LIBUV_BIN ${LIBUV_LIBRARIES})
				add_custom_command(TARGET ${TARGET_BIN}
					POST_BUILD
					COMMAND "${CMAKE_COMMAND}" -E copy "${LIBUV_BIN}" "$<TARGET_FILE_DIR:${TARGET_BIN}>" VERBATIM)
			endif()
		endforeach()
	endif()
endif()

if (LWS_WITH_TLS AND (LWS_WITH_JOSE OR LWS_WITH_GENCRYPTO))
	list(APPEND SOURCES
		tls/lws-gencrypto-common.c)
endif()

#
# Keep explicit parent scope exports at end
#

exports_to_parent_scope()
set(LWS_HAVE_MBEDTLS_NET_SOCKETS ${LWS_HAVE_MBEDTLS_NET_SOCKETS} PARENT_SCOPE)
set(TEST_SERVER_SSL_KEY "${TEST_SERVER_SSL_KEY}" PARENT_SCOPE)
set(TEST_SERVER_SSL_CERT "${TEST_SERVER_SSL_CERT}" PARENT_SCOPE)
set(TEST_SERVER_DATA ${TEST_SERVER_DATA} PARENT_SCOPE)

