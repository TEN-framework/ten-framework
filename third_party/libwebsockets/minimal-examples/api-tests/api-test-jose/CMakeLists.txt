project(lws-api-test-jose C)
cmake_minimum_required(VERSION 2.8.12)
find_package(libwebsockets CONFIG REQUIRED)
list(APPEND CMAKE_MODULE_PATH ${LWS_CMAKE_DIR})
include(CheckCSourceCompiles)
include(LwsCheckRequirements)

set(SAMP lws-api-test-jose)
set(SRCS main.c jwk.c jws.c jwe.c)

set(requirements 1)
require_lws_config(LWS_WITH_JOSE 1 requirements)

if (requirements)

	add_executable(${SAMP} ${SRCS})

	if (NOT (LWS_WITH_MBEDTLS AND NOT LWS_HAVE_mbedtls_internal_aes_encrypt))
		add_test(NAME api-test-jose COMMAND lws-api-test-jose)
	endif()

	if (websockets_shared)
		target_link_libraries(${SAMP} websockets_shared ${LIBWEBSOCKETS_DEP_LIBS})
		add_dependencies(${SAMP} websockets_shared)
	else()
		target_link_libraries(${SAMP} websockets ${LIBWEBSOCKETS_DEP_LIBS})
	endif()
endif()
