project(lws-minimal-secure-streams-perf C)
cmake_minimum_required(VERSION 2.8.12)
find_package(libwebsockets CONFIG REQUIRED)
list(APPEND CMAKE_MODULE_PATH ${LWS_CMAKE_DIR})
include(CheckCSourceCompiles)
include(LwsCheckRequirements)

set(SAMP lws-minimal-secure-streams-perf)

set(requirements 1)
require_lws_config(LWS_ROLE_H1 1 requirements)
require_lws_config(LWS_WITHOUT_CLIENT 0 requirements)
require_lws_config(LWS_WITH_SECURE_STREAMS 1 requirements)
require_lws_config(LWS_WITH_SECURE_STREAMS_STATIC_POLICY_ONLY 0 requirements)
require_lws_config(LWS_WITH_SYS_STATE 1 requirements)

if (requirements)
	add_executable(${SAMP} minimal-secure-streams.c)

	find_program(VALGRIND "valgrind")

	if (LWS_CTEST_INTERNET_AVAILABLE AND NOT WIN32)
	
		#
		# When running in CI, wait for a lease on the resources
		# before starting this test, so the server does not get
		# thousands of simultaneous tls connection attempts
		#
		# sai-resource holds the lease on the resources until
		# the time given in seconds or the sai-resource instance
		# exits, whichever happens first
		#
		# If running under Sai, creates a lock test called "res_sspcmin" 
		#
		
		sai_resource(warmcat_conns 1 40 ssperfpcmin)
		
		#
		# simple test not via proxy
		#
	
		if (VALGRIND)
			message("testing via valgrind")
			add_test(NAME ssperf-warmcat COMMAND
				${VALGRIND} --tool=memcheck --leak-check=yes --num-callers=20
				$<TARGET_FILE:lws-minimal-secure-streams-perf>)
		else()
			add_test(NAME ssperf-warmcat COMMAND lws-minimal-secure-streams-perf)
		endif()

		set_tests_properties(ssperf-warmcat
				     PROPERTIES
				     WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}/minimal-examples/secure-streams/minimal-secure-streams
				     TIMEOUT 40)
		if (DEFINED ENV{SAI_OVN})
			set_tests_properties(ssperf-warmcat PROPERTIES FIXTURES_REQUIRED "res_ssperfpcmin")
		endif()

		if (HAS_LWS_WITH_SECURE_STREAMS_PROXY_API OR LWS_WITH_SECURE_STREAMS_PROXY_API)

			#
			# Define test dep to bring up and take down the test
			# proxy
			#

			if (${CMAKE_SYSTEM_NAME} MATCHES "Linux")
				# uds abstract namespace for linux
				set(CTEST_SOCKET_PATH "@ctest-ssperfp-$ENV{SAI_PROJECT}-$ENV{SAI_OVN}")
			else()
				# filesystem socket for others
				set(CTEST_SOCKET_PATH "/tmp/ctest-ssperfp-$ENV{SAI_PROJECT}-$ENV{SAI_OVN}")
			endif()
			add_test(NAME st_ssperfproxy COMMAND
				${CMAKE_SOURCE_DIR}/scripts/ctest-background.sh
				ssperfproxy $<TARGET_FILE:lws-minimal-secure-streams-proxy>
				-i ${CTEST_SOCKET_PATH} )
			set_tests_properties(st_ssperfproxy PROPERTIES WORKING_DIRECTORY . FIXTURES_SETUP ssperfproxy TIMEOUT 800)

			add_test(NAME ki_ssperfproxy COMMAND
				${CMAKE_SOURCE_DIR}/scripts/ctest-background-kill.sh
				ssperfproxy $<TARGET_FILE:lws-minimal-secure-streams-proxy>
				-i ${CTEST_SOCKET_PATH})
			set_tests_properties(ki_ssperfproxy PROPERTIES FIXTURES_CLEANUP ssperfproxy)

			#
			# the client part that will connect to the proxy
			#

			if (VALGRIND)
				message("testing via valgrind")
				add_test(NAME ssperfpc-minimal COMMAND
					${VALGRIND} --tool=memcheck --leak-check=yes --num-callers=20
					$<TARGET_FILE:lws-minimal-secure-streams-perf-client> -i +${CTEST_SOCKET_PATH})
			else()
				add_test(NAME ssperfpc-minimal COMMAND lws-minimal-secure-streams-perf-client -i +${CTEST_SOCKET_PATH})
			endif()
			
			set(fixlist "ssperfproxy")
			if (DEFINED ENV{SAI_OVN})
				list(APPEND fixlist "res_ssperfproxy")
			endif()
			
			set_tests_properties(ssperfpc-minimal PROPERTIES
				WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}/minimal-examples/secure-streams/minimal-secure-streams-perf
				FIXTURES_REQUIRED "${fixlist}"
				TIMEOUT 40)

		endif()

	endif()

	if (websockets_shared)
		target_link_libraries(${SAMP} websockets_shared ${LIBWEBSOCKETS_DEP_LIBS})
		add_dependencies(${SAMP} websockets_shared)
	else()
		target_link_libraries(${SAMP} websockets ${LIBWEBSOCKETS_DEP_LIBS})
	endif()

	CHECK_C_SOURCE_COMPILES("#include <libwebsockets.h>\nint main(void) {\ni#if defined(LWS_WITH_SECURE_STREAMS_PROXY_API)\n return 0;\n #else\n fail\n #endif\n return 0;\n}\n" HAS_LWS_WITH_SECURE_STREAMS_PROXY_API)

	if (HAS_LWS_WITH_SECURE_STREAMS_PROXY_API OR LWS_WITH_SECURE_STREAMS_PROXY_API)
		add_compile_options(-DLWS_SS_USE_SSPC)

		add_executable(${SAMP}-client minimal-secure-streams.c)
		if (websockets_shared)
			target_link_libraries(${SAMP}-client websockets_shared ${LIBWEBSOCKETS_DEP_LIBS})
			add_dependencies(${SAMP}-client websockets_shared)
		else()
			target_link_libraries(${SAMP}-client websockets ${LIBWEBSOCKETS_DEP_LIBS})
		endif()
	endif()

endif()
