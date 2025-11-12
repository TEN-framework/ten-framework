project(lws-minimal-http-client-h2-rxflow C)
cmake_minimum_required(VERSION 2.8.12)
find_package(libwebsockets CONFIG REQUIRED)
list(APPEND CMAKE_MODULE_PATH ${LWS_CMAKE_DIR})
include(CheckCSourceCompiles)
include(LwsCheckRequirements)

set(SAMP lws-minimal-http-client-h2-rxflow)
set(SRCS minimal-http-client.c)

set(requirements 1)
require_lws_config(LWS_ROLE_H2 1 requirements)
require_lws_config(LWS_WITH_CLIENT 1 requirements)
require_lws_config(LWS_WITH_SYS_STATE 1 requirements)
require_lws_config(LWS_WITH_TLS 1 requirements)

if (requirements)
	add_executable(${SAMP} ${SRCS})
	if (LWS_CTEST_INTERNET_AVAILABLE)
		add_test(NAME http-client-h2-rxflow-warmcat COMMAND lws-minimal-http-client-h2-rxflow)
		add_test(NAME http-client-h2-rxflow-warmcat-h1 COMMAND lws-minimal-http-client-h2-rxflow --h1)
		set_tests_properties(http-client-h2-rxflow-warmcat
				     http-client-h2-rxflow-warmcat-h1
				     PROPERTIES
				     WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}/minimal-examples/http-client/minimal-http-client-h2-rxflow
				     TIMEOUT 30)
	endif()

	if (websockets_shared)
		target_link_libraries(${SAMP} websockets_shared ${LIBWEBSOCKETS_DEP_LIBS})
		add_dependencies(${SAMP} websockets_shared)
	else()
		target_link_libraries(${SAMP} websockets ${LIBWEBSOCKETS_DEP_LIBS})
	endif()
endif()
