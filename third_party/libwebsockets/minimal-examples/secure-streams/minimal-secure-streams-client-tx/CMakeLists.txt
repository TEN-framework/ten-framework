project(lws-minimal-secure-streams-client-tx C)
cmake_minimum_required(VERSION 2.8.12)
find_package(libwebsockets CONFIG REQUIRED)
list(APPEND CMAKE_MODULE_PATH ${LWS_CMAKE_DIR})
include(CheckCSourceCompiles)
include(LwsCheckRequirements)

set(SAMP lws-minimal-secure-streams-client-tx)
set(SRCS minimal-secure-streams-client-tx.c)

set(requirements 1)
require_lws_config(LWS_ROLE_H1 1 requirements)
require_lws_config(LWS_WITHOUT_CLIENT 0 requirements)
require_lws_config(LWS_WITH_SECURE_STREAMS 1 requirements)
require_lws_config(LWS_WITH_SECURE_STREAMS_PROXY_API 1 requirements)
require_lws_config(LWS_WITH_SYS_STATE 1 requirements)

if (requirements)
	add_executable(${SAMP} ${SRCS})
	add_compile_options(-DLWS_SS_USE_SSPC)
	
	find_program(VALGRIND "valgrind")

	if (LWS_CTEST_INTERNET_AVAILABLE AND NOT WIN32)

		#
		# Define test dep to bring up and take down the test
		# proxy
		#

		if (${CMAKE_SYSTEM_NAME} MATCHES "Linux")
			# uds abstract namespace for linux
			set(CTEST_SOCKET_PATH "@ctest-sspctx-$ENV{SAI_PROJECT}-$ENV{SAI_OVN}")
		else()
			# filesystem socket for others
			set(CTEST_SOCKET_PATH "/tmp/ctest-sspctx-$ENV{SAI_PROJECT}-$ENV{SAI_OVN}")
		endif()
		add_test(NAME st_ssproxyctx COMMAND
			${CMAKE_SOURCE_DIR}/scripts/ctest-background.sh
			ssproxyctx $<TARGET_FILE:lws-minimal-secure-streams-proxy>
			-i ${CTEST_SOCKET_PATH} )
		set_tests_properties(st_ssproxyctx PROPERTIES WORKING_DIRECTORY . FIXTURES_SETUP ssproxyctx TIMEOUT 800)

		add_test(NAME ki_ssproxyctx COMMAND
			${CMAKE_SOURCE_DIR}/scripts/ctest-background-kill.sh
			ssproxyctx $<TARGET_FILE:lws-minimal-secure-streams-proxy>
			-i ${CTEST_SOCKET_PATH})
		set_tests_properties(ki_ssproxyctx PROPERTIES FIXTURES_CLEANUP ssproxyctx)

		#
		# the client part that will connect to the proxy
		#

		if (VALGRIND)
			message("testing via valgrind")
			add_test(NAME sspc-minimaltx COMMAND
				${VALGRIND} --tool=memcheck --leak-check=yes --num-callers=20
				$<TARGET_FILE:lws-minimal-secure-streams-client-tx> -i +${CTEST_SOCKET_PATH})
		else()
			add_test(NAME sspc-minimaltx COMMAND lws-minimal-secure-streams-client-tx -i +${CTEST_SOCKET_PATH})
		endif()
		set_tests_properties(sspc-minimaltx PROPERTIES
			WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}/minimal-examples/secure-streams/minimal-secure-streams-client-tx
			FIXTURES_REQUIRED "ssproxyctx"
			TIMEOUT 40)

	endif()
	

	if (websockets_shared)
		target_link_libraries(${SAMP} websockets_shared ${LIBWEBSOCKETS_DEP_LIBS})
		add_dependencies(${SAMP} websockets_shared)
	else()
		target_link_libraries(${SAMP} websockets ${LIBWEBSOCKETS_DEP_LIBS})
	endif()
endif()
