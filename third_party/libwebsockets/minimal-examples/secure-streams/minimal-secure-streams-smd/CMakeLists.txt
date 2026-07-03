project(lws-minimal-secure-streams-smd C)
cmake_minimum_required(VERSION 2.8.12)
find_package(libwebsockets CONFIG REQUIRED)
list(APPEND CMAKE_MODULE_PATH ${LWS_CMAKE_DIR})
include(CheckCSourceCompiles)
include(LwsCheckRequirements)

set(requirements 1)
require_lws_config(LWS_ROLE_H1 1 requirements)
require_lws_config(LWS_WITH_CLIENT 1 requirements)
require_lws_config(LWS_WITH_SECURE_STREAMS 1 requirements)
require_lws_config(LWS_WITH_SYS_SMD 1 requirements)
require_lws_config(LWS_WITH_SECURE_STREAMS_STATIC_POLICY_ONLY 0 requirements)
require_lws_config(LWS_WITH_SYS_STATE 1 requirements)

require_lws_config(LWS_WITH_SYS_FAULT_INJECTION 1 has_fault_injection)

if (requirements)
	add_executable(${PROJECT_NAME} minimal-secure-streams-smd.c)
	
	find_program(VALGRIND "valgrind")

	if (LWS_CTEST_INTERNET_AVAILABLE AND NOT WIN32)
	
		if (VALGRIND)
			add_test(NAME ss-smd COMMAND
				${VALGRIND} --tool=memcheck --leak-check=yes --num-callers=20
				$<TARGET_FILE:lws-minimal-secure-streams-smd>)
		else()
	
			add_test(NAME ss-smd COMMAND lws-minimal-secure-streams-smd)
		endif()
		set_tests_properties(ss-smd
				     PROPERTIES
				     WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}/minimal-examples/secure-streams/minimal-secure-streams-smd
				     TIMEOUT 10)

               if (has_fault_injection)
                       if (VALGRIND)
                               add_test(NAME ss-smd-fi1 COMMAND
                                       ${VALGRIND} --tool=memcheck --leak-check=yes --num-callers=20
                                       $<TARGET_FILE:lws-minimal-secure-streams-smd>
                                       --fault-injection "ss/ss_create_smd"
                                       --expected-exit 1)
                               add_test(NAME ss-smd-fi2 COMMAND
                                       ${VALGRIND} --tool=memcheck --leak-check=yes --num-callers=20
                                       $<TARGET_FILE:lws-minimal-secure-streams-smd>
                                       --fault-injection "ss/ss_create_smd_1"
                                       --expected-exit 1)
                               add_test(NAME ss-smd-fi3 COMMAND
                                       ${VALGRIND} --tool=memcheck --leak-check=yes --num-callers=20
                                       $<TARGET_FILE:lws-minimal-secure-streams-smd>
                                       --fault-injection "ss/ss_create_smd_2"
                                       --expected-exit 1)
		       else()
                               add_test(NAME ss-smd-fi1 COMMAND lws-minimal-secure-streams-smd
                                        --fault-injection "ss/ss_create_smd"
                                        --expected-exit 1)
                               add_test(NAME ss-smd-fi2 COMMAND lws-minimal-secure-streams-smd
                                        --fault-injection "ss/ss_create_smd_1"
                                        --expected-exit 1)
                                add_test(NAME ss-smd-fi3 COMMAND lws-minimal-secure-streams-smd
                                        --fault-injection "ss/ss_create_smd_2"
                                        --expected-exit 1) 
 			endif()

                       set_tests_properties(ss-smd-fi1
			       		    ss-smd-fi2
					    ss-smd-fi3
                                    PROPERTIES
                                    WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}/minimal-examples/secure-streams/minimal-secure-streams-smd
                                    TIMEOUT 5)
               endif()
     
	
	
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

		add_executable(${PROJECT_NAME}-client minimal-secure-streams-smd.c multi.c)

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
			set(CTEST_SOCKET_PATH "@ctest-sspsmd_sspc-$ENV{SAI_PROJECT}-$ENV{SAI_OVN}")
		else()
			# filesystem socket for others
			set(CTEST_SOCKET_PATH "/tmp/ctest-sspsmd_sspc-$ENV{SAI_PROJECT}-$ENV{SAI_OVN}")
		endif()

		add_test(NAME st_ssprxsmd_sspc COMMAND
			${CMAKE_SOURCE_DIR}/scripts/ctest-background.sh
			ssproxysmd_sspc $<TARGET_FILE:lws-minimal-secure-streams-proxy>
			-i ${CTEST_SOCKET_PATH}  -d1039)
		set_tests_properties(st_ssprxsmd_sspc PROPERTIES WORKING_DIRECTORY . FIXTURES_SETUP ssproxysmd_sspc TIMEOUT 800)

		add_test(NAME ki_ssprxsmd_sspc COMMAND
			${CMAKE_SOURCE_DIR}/scripts/ctest-background-kill.sh
			ssproxysmd_sspc $<TARGET_FILE:lws-minimal-secure-streams-proxy>
			-i ${CTEST_SOCKET_PATH}  -d1039)
		set_tests_properties(ki_ssprxsmd_sspc PROPERTIES FIXTURES_CLEANUP ssproxysmd_sspc)

		#
		# the client part that will connect to the proxy
		#

		if (VALGRIND)
			message("testing via valgrind")
			add_test(NAME sspcsmd_sspc COMMAND
				${VALGRIND} --tool=memcheck --leak-check=yes --num-callers=20
				$<TARGET_FILE:lws-minimal-secure-streams-smd-client> -i +${CTEST_SOCKET_PATH})
		else()
			add_test(NAME sspcsmd_sspc COMMAND lws-minimal-secure-streams-smd-client -i +${CTEST_SOCKET_PATH})
		endif()
		set_tests_properties(sspcsmd_sspc PROPERTIES
			WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}/minimal-examples/secure-streams/minimal-secure-streams-smd
			FIXTURES_REQUIRED "ssproxysmd_sspc"
			TIMEOUT 80)
			
		
		#
		# Define test dep to bring up and take down the test
		# proxy
		#

		if (${CMAKE_SYSTEM_NAME} MATCHES "Linux")
			# uds abstract namespace for linux
			set(CTEST_SOCKET_PATH "@ctest-mul-sspsmd_sspc-$ENV{SAI_PROJECT}-$ENV{SAI_OVN}")
		else()
			# filesystem socket for others
			set(CTEST_SOCKET_PATH "/tmp/ctest-mul-sspsmd_sspc-$ENV{SAI_PROJECT}-$ENV{SAI_OVN}")
		endif()

		add_test(NAME st_mulssprxsmd_sspc COMMAND
			${CMAKE_SOURCE_DIR}/scripts/ctest-background.sh
			mulssproxysmd_sspc $<TARGET_FILE:lws-minimal-secure-streams-proxy>
			-i ${CTEST_SOCKET_PATH}  -d1039)
		set_tests_properties(st_mulssprxsmd_sspc PROPERTIES WORKING_DIRECTORY . FIXTURES_SETUP mulssproxysmd_sspc TIMEOUT 800)

		add_test(NAME ki_mulssprxsmd_sspc COMMAND
			${CMAKE_SOURCE_DIR}/scripts/ctest-background-kill.sh
			mulssproxysmd_sspc $<TARGET_FILE:lws-minimal-secure-streams-proxy>
			-i ${CTEST_SOCKET_PATH}  -d1039)
		set_tests_properties(ki_mulssprxsmd_sspc PROPERTIES FIXTURES_CLEANUP mulssproxysmd_sspc)

		#
		# multi tests for the client part that will connect to the proxy
		#

		if (VALGRIND)
			message("testing via valgrind")
			add_test(NAME mulsspcsmd_sspc COMMAND
				${VALGRIND} --tool=memcheck --leak-check=yes --num-callers=20
				$<TARGET_FILE:lws-minimal-secure-streams-smd-client> -i +${CTEST_SOCKET_PATH} --multi -d1039)
		else()
			add_test(NAME mulsspcsmd_sspc COMMAND lws-minimal-secure-streams-smd-client -i +${CTEST_SOCKET_PATH} --multi -d1039)
		endif()
		set_tests_properties(mulsspcsmd_sspc PROPERTIES
			WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}/minimal-examples/secure-streams/minimal-secure-streams-smd
			FIXTURES_REQUIRED "mulssproxysmd_sspc"
			TIMEOUT 80)
		
	endif()

endif()
