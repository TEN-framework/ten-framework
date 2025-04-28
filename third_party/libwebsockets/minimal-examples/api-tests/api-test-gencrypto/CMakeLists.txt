project(lws-api-test-gencrypto C)
cmake_minimum_required(VERSION 2.8.12)
find_package(libwebsockets CONFIG REQUIRED)
list(APPEND CMAKE_MODULE_PATH ${LWS_CMAKE_DIR})
include(CheckCSourceCompiles)
include(LwsCheckRequirements)

set(SAMP lws-api-test-gencrypto)
set(SRCS main.c lws-genaes.c lws-genec.c)

set(requirements 1)
require_lws_config(LWS_WITH_GENCRYPTO 1 requirements)
require_lws_config(LWS_WITH_JOSE 1 requirements)
require_lws_config(USE_WOLFSSL 0 requirements)

if (requirements)

	add_executable(${SAMP} ${SRCS})
	add_test(NAME api-test-gencrypto COMMAND lws-api-test-gencrypto)

	if (websockets_shared)
		target_link_libraries(${SAMP} websockets_shared ${LIBWEBSOCKETS_DEP_LIBS})
		add_dependencies(${SAMP} websockets_shared)
	else()
		target_link_libraries(${SAMP} websockets ${LIBWEBSOCKETS_DEP_LIBS})
	endif()
endif()

