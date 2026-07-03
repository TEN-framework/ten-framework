project(lws-api-test-secure-streams C)
cmake_minimum_required(VERSION 2.8.12)
include(CheckCSourceCompiles)
include(LwsCheckRequirements)

set(requirements 1)
require_lws_config(LWS_WITH_SECURE_STREAMS 1 requirements)
require_lws_config(LWS_WITH_TLS 1 requirements)
require_lws_config(LWS_WITH_SYS_STATE 1 requirements)
require_lws_config(LWS_WITH_SECURE_STREAMS_STATIC_POLICY_ONLY 0 requirements)

if (requirements)

	add_executable(${PROJECT_NAME} main.c)
	
	if (LWS_CTEST_INTERNET_AVAILABLE)
		add_test(NAME api-test-secure-streams COMMAND ${PROJECT_NAME})
		set_tests_properties(api-test-secure-streams
				     PROPERTIES
				     WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}/minimal-examples/api-tests/api-test-secure-streams
				     TIMEOUT 20)
	endif()

	if (websockets_shared)
		target_link_libraries(${PROJECT_NAME} websockets_shared ${LIBWEBSOCKETS_DEP_LIBS})
		add_dependencies(${PROJECT_NAME} websockets_shared)
	else()
		target_link_libraries(${PROJECT_NAME} websockets ${LIBWEBSOCKETS_DEP_LIBS})
	endif()

endif()
