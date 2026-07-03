project(lws-api-test-lws_struct-sqlite C)
cmake_minimum_required(VERSION 2.8.12)
find_package(libwebsockets CONFIG REQUIRED)
list(APPEND CMAKE_MODULE_PATH ${LWS_CMAKE_DIR})
include(CheckCSourceCompiles)
include(LwsCheckRequirements)

set(SAMP lws-api-test-lws_struct-sqlite)
set(SRCS main.c)

set(requirements 1)
require_lws_config(LWS_WITH_STRUCT_SQLITE3 1 requirements)

if (requirements)

	add_executable(${SAMP} ${SRCS})
	add_test(NAME api-test-lws_struct_sqlite COMMAND lws-api-test-lws_struct-sqlite)

	if (websockets_shared)
		target_link_libraries(${SAMP} websockets_shared sqlite3 ${LIBWEBSOCKETS_DEP_LIBS})
		add_dependencies(${SAMP} websockets_shared)
	else()
		target_link_libraries(${SAMP} websockets sqlite3 ${LIBWEBSOCKETS_DEP_LIBS})
	endif()
endif()
