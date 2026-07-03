project(lws-api-test-async-dns C)
cmake_minimum_required(VERSION 2.8.12)
find_package(libwebsockets CONFIG REQUIRED)

set(SAMP lws-api-test-async-dns)
set(SRCS main.c)

require_lws_config(LWS_ROLE_H1 1 requirements)
require_lws_config(LWS_WITH_CLIENT 1 requirements)
require_lws_config(LWS_WITH_SYS_ASYNC_DNS 1 requirements)

if (requirements)
	add_executable(${SAMP} ${SRCS})
	add_test(NAME api-test-async-dns COMMAND lws-api-test-async-dns)
	set_tests_properties(api-test-async-dns
			     PROPERTIES
			     WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}/minimal-examples/api-tests/api-test-async-dns
			     TIMEOUT 60)

	if (websockets_shared)
		target_link_libraries(${SAMP} websockets_shared ${LIBWEBSOCKETS_DEP_LIBS})
		add_dependencies(${SAMP} websockets_shared)
	else()
		target_link_libraries(${SAMP} websockets ${LIBWEBSOCKETS_DEP_LIBS})
	endif()
endif()
