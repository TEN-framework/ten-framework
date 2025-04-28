project(lws-minimal-secure-streams-sigv4 C)
cmake_minimum_required(VERSION 2.8.12)
find_package(libwebsockets CONFIG REQUIRED)
list(APPEND CMAKE_MODULE_PATH ${LWS_CMAKE_DIR})
include(CheckCSourceCompiles)
include(LwsCheckRequirements)

set(SAMP lws-minimal-secure-streams-sigv4)

set(requirements 1)
require_lws_config(LWS_ROLE_H1 1 requirements)
require_lws_config(LWS_WITHOUT_CLIENT 0 requirements)
require_lws_config(LWS_WITH_SECURE_STREAMS 1 requirements)
require_lws_config(LWS_WITH_SECURE_STREAMS_AUTH_SIGV4 1 requirements)

if (requirements)
	add_executable(${SAMP} ss-s3-main.c ss-s3-ss.c)
	
	find_program(VALGRIND "valgrind")

	if (LWS_CTEST_INTERNET_AVAILABLE AND NOT WIN32 AND 0)
		if (VALGRIND)
			message("testing via valgrind")
			add_test(NAME ss-sigv4 COMMAND
				${VALGRIND} --tool=memcheck --leak-check=yes --num-callers=20
				$<TARGET_FILE:lws-minimal-secure-streams-sigv4>)
		else()
			add_test(NAME ss-sigv4 COMMAND lws-minimal-secure-streams-sigv4)
		endif()

		set_tests_properties(ss-sigv4
				     PROPERTIES
				     WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}/minimal-examples/secure-streams/minimal-secure-streams-sigv4
				     TIMEOUT 20)

		if (HAS_LWS_WITH_SECURE_STREAMS_PROXY_API OR LWS_WITH_SECURE_STREAMS_PROXY_API)

			#
			# Define test dep to bring up and take down the test
			# proxy
			#

			if (${CMAKE_SYSTEM_NAME} MATCHES "Linux")
				# uds abstract namespace for linux
				set(CTEST_SOCKET_PATH "@ctest-sspsigv4-$ENV{SAI_PROJECT}-$ENV{SAI_OVN}")
			else()
				# filesystem socket for others
				set(CTEST_SOCKET_PATH "/tmp/ctest-sspsigv4-$ENV{SAI_PROJECT}-$ENV{SAI_OVN}")
			endif()
			add_test(NAME st_ssproxysigv4 COMMAND
				${CMAKE_SOURCE_DIR}/scripts/ctest-background.sh
				ssproxysigv4 $<TARGET_FILE:lws-minimal-secure-streams-proxy>
				-i ${CTEST_SOCKET_PATH} )
			set_tests_properties(st_ssproxysigv4 PROPERTIES WORKING_DIRECTORY . FIXTURES_SETUP ssproxysigv4 TIMEOUT 800)

			add_test(NAME ki_ssproxysigv4 COMMAND
				${CMAKE_SOURCE_DIR}/scripts/ctest-background-kill.sh
				ssproxysigv4 $<TARGET_FILE:lws-minimal-secure-streams-proxy>
				-i ${CTEST_SOCKET_PATH})
			set_tests_properties(ki_ssproxysigv4 PROPERTIES FIXTURES_CLEANUP ssproxysigv4)

			#
			# the client part that will connect to the proxy
			#

			if (VALGRIND)
				message("testing via valgrind")
				add_test(NAME sspc-sigv4 COMMAND
					${VALGRIND} --tool=memcheck --leak-check=yes --num-callers=20
					$<TARGET_FILE:lws-minimal-secure-streams-sigv4-client> -i +${CTEST_SOCKET_PATH})
			else()
				add_test(NAME sspc-sigv4 COMMAND lws-minimal-secure-streams-sigv4-client -i +${CTEST_SOCKET_PATH})
			endif()
			set_tests_properties(sspc-sigv4 PROPERTIES
				WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}/minimal-examples/secure-streams/minimal-secure-streams-sigv4
				FIXTURES_REQUIRED "ssproxysigv4"
				TIMEOUT 40)

		endif()

	endif()

	if (websockets_shared)
		target_link_libraries(${SAMP} websockets_shared)
		add_dependencies(${SAMP} websockets_shared)
	else()
		target_link_libraries(${SAMP} websockets)
	endif()

	if (LWS_WITH_SECURE_STREAMS_PROXY_API)
		add_compile_options(-DLWS_SS_USE_SSPC)

		add_executable(${SAMP}-client ss-s3-main.c ss-s3-ss.c)
		if (websockets_shared)
			target_link_libraries(${SAMP}-client websockets_shared)
			add_dependencies(${SAMP}-client websockets_shared)
		else()
			target_link_libraries(${SAMP}-client websockets)
		endif()
	endif()

endif()
