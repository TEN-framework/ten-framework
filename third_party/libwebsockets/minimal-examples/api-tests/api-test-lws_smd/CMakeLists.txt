project(lws-api-test-lws_smd C)
cmake_minimum_required(VERSION 2.8.12)
find_package(libwebsockets CONFIG REQUIRED)
list(APPEND CMAKE_MODULE_PATH ${LWS_CMAKE_DIR})
include(CheckCSourceCompiles)
include(LwsCheckRequirements)

set(requirements 1)
require_pthreads(requirements)
require_lws_config(LWS_WITH_SYS_SMD 1 requirements)
require_lws_config(LWS_WITH_SYS_STATE 1 requirements)

if (requirements)
	add_executable(${PROJECT_NAME} main.c)
	add_test(NAME api-test-lws_smd COMMAND lws-api-test-lws_smd -d1151)
	set_tests_properties(api-test-lws_smd
			     PROPERTIES
			     RUN_SERIAL TRUE
			     WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}/minimal-examples/api-tests/api-test-lws_smd
			     TIMEOUT 60)

	if (websockets_shared)
		target_link_libraries(${PROJECT_NAME} websockets_shared ${PTHREAD_LIB} ${LIBWEBSOCKETS_DEP_LIBS})
		add_dependencies(${PROJECT_NAME} websockets_shared)
	else()
		target_link_libraries(${PROJECT_NAME} websockets ${PTHREAD_LIB} ${LIBWEBSOCKETS_DEP_LIBS})
	endif()
endif()
