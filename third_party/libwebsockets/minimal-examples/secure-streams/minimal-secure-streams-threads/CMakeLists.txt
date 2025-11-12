project(lws-minimal-secure-streams-threads C)
cmake_minimum_required(VERSION 2.8.12)
find_package(libwebsockets CONFIG REQUIRED)
list(APPEND CMAKE_MODULE_PATH ${LWS_CMAKE_DIR})
include(CheckCSourceCompiles)
include(LwsCheckRequirements)

set(requirements 1)
require_pthreads(requirements)
require_lws_config(LWS_ROLE_H1 1 requirements)
require_lws_config(LWS_WITH_CLIENT 1 requirements)
require_lws_config(LWS_WITH_SECURE_STREAMS 1 requirements)
require_lws_config(LWS_WITH_SYS_SMD 1 requirements)
require_lws_config(LWS_WITH_SECURE_STREAMS_STATIC_POLICY_ONLY 0 requirements)
require_lws_config(LWS_WITH_SYS_STATE 1 requirements)

if (requirements AND NOT WIN32)
# win32 has problems with pthreads.h and timespec struct redef
	add_executable(${PROJECT_NAME} minimal-secure-streams-threads.c)

	find_program(VALGRIND "valgrind")

	if (LWS_CTEST_INTERNET_AVAILABLE AND NOT WIN32)

		if (VALGRIND)
			add_test(NAME ss-threads COMMAND
				${VALGRIND} --tool=memcheck --leak-check=yes --num-callers=20
				$<TARGET_FILE:lws-minimal-secure-streams-threads>)
		else()

			add_test(NAME ss-threads COMMAND lws-minimal-secure-streams-threads)
		endif()
		set_tests_properties(ss-threads
				     PROPERTIES
				     WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}/minimal-examples/secure-streams/minimal-secure-streams-threads
				     TIMEOUT 10)
	endif()

	if (websockets_shared)
		target_link_libraries(${PROJECT_NAME} websockets_shared ${LIBWEBSOCKETS_DEP_LIBS})
		add_dependencies(${PROJECT_NAME} websockets_shared)
	else()
		target_link_libraries(${PROJECT_NAME} websockets ${LIBWEBSOCKETS_DEP_LIBS})
	endif()

	CHECK_C_SOURCE_COMPILES("#include <libwebsockets.h>\nint main(void) {\ni#if defined(LWS_WITH_SECURE_STREAMS_PROXY_API)\n return 0;\n #else\n fail\n #endif\n return 0;\n}\n" HAS_LWS_WITH_SECURE_STREAMS_PROXY_API)

	if (HAS_LWS_WITH_SECURE_STREAMS_PROXY_API OR LWS_WITH_SECURE_STREAMS_PROXY_API)
		add_compile_options(-DLWS_SS_USE_SSPC)

		add_executable(${PROJECT_NAME}-client minimal-secure-streams-threads.c)

		if (websockets_shared)
			target_link_libraries(${PROJECT_NAME}-client websockets_shared ${LIBWEBSOCKETS_DEP_LIBS})
			add_dependencies(${PROJECT_NAME}-client websockets_shared)
		else()
			target_link_libraries(${PROJECT_NAME}-client websockets ${LIBWEBSOCKETS_DEP_LIBS})
		endif()

		#
		# Define test dep to bring up and take down the test
		# proxy
		#

		if (${CMAKE_SYSTEM_NAME} MATCHES "Linux")
			# uds abstract namespace for linux
			set(CTEST_SOCKET_PATH "@ctest-sspthreads_sspc-$ENV{SAI_PROJECT}-$ENV{SAI_OVN}")
		else()
			# filesystem socket for others
			set(CTEST_SOCKET_PATH "/tmp/ctest-sspthreads_sspc-$ENV{SAI_PROJECT}-$ENV{SAI_OVN}")
		endif()

		add_test(NAME st_ssprxthreads_sspc COMMAND
			${CMAKE_SOURCE_DIR}/scripts/ctest-background.sh
			ssproxythreads_sspc $<TARGET_FILE:lws-minimal-secure-streams-proxy>
			-i ${CTEST_SOCKET_PATH}  -d1039)
		set_tests_properties(st_ssprxthreads_sspc PROPERTIES WORKING_DIRECTORY . FIXTURES_SETUP ssproxythreads_sspc TIMEOUT 800)

		add_test(NAME ki_ssprxthreads_sspc COMMAND
			${CMAKE_SOURCE_DIR}/scripts/ctest-background-kill.sh
			ssproxythreads_sspc $<TARGET_FILE:lws-minimal-secure-streams-proxy>
			-i ${CTEST_SOCKET_PATH}  -d1039)
		set_tests_properties(ki_ssprxthreads_sspc PROPERTIES FIXTURES_CLEANUP ssproxythreads_sspc)

		#
		# the client part that will connect to the proxy
		#

		if (VALGRIND)
			message("testing via valgrind")
			add_test(NAME sspcthreads_sspc COMMAND
				${VALGRIND} --tool=memcheck --leak-check=yes --num-callers=20
				$<TARGET_FILE:lws-minimal-secure-streams-threads-client> -i +${CTEST_SOCKET_PATH})
		else()
			add_test(NAME sspcthreads_sspc COMMAND lws-minimal-secure-streams-threads-client -i +${CTEST_SOCKET_PATH})
		endif()
		set_tests_properties(sspcthreads_sspc PROPERTIES
			WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}/minimal-examples/secure-streams/minimal-secure-streams-threads
			FIXTURES_REQUIRED "ssproxythreads_sspc"
			TIMEOUT 80)


		#
		# Define test dep to bring up and take down the test
		# proxy
		#

		if (${CMAKE_SYSTEM_NAME} MATCHES "Linux")
			# uds abstract namespace for linux
			set(CTEST_SOCKET_PATH "@ctest-mul-sspthreads_sspc-$ENV{SAI_PROJECT}-$ENV{SAI_OVN}")
		else()
			# filesystem socket for others
			set(CTEST_SOCKET_PATH "/tmp/ctest-mul-sspthreads_sspc-$ENV{SAI_PROJECT}-$ENV{SAI_OVN}")
		endif()

		add_test(NAME st_mulssprxthreads_sspc COMMAND
			${CMAKE_SOURCE_DIR}/scripts/ctest-background.sh
			mulssproxythreads_sspc $<TARGET_FILE:lws-minimal-secure-streams-proxy>
			-i ${CTEST_SOCKET_PATH}  -d1039)
		set_tests_properties(st_mulssprxthreads_sspc PROPERTIES WORKING_DIRECTORY . FIXTURES_SETUP mulssproxythreads_sspc TIMEOUT 800)

		add_test(NAME ki_mulssprxthreads_sspc COMMAND
			${CMAKE_SOURCE_DIR}/scripts/ctest-background-kill.sh
			mulssproxythreads_sspc $<TARGET_FILE:lws-minimal-secure-streams-proxy>
			-i ${CTEST_SOCKET_PATH}  -d1039)
		set_tests_properties(ki_mulssprxthreads_sspc PROPERTIES FIXTURES_CLEANUP mulssproxythreads_sspc)

	endif()

endif()
