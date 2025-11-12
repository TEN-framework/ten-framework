project(lws-minimal-http-client-post C)
cmake_minimum_required(VERSION 2.8.12)
find_package(libwebsockets CONFIG REQUIRED)
list(APPEND CMAKE_MODULE_PATH ${LWS_CMAKE_DIR})
include(CheckCSourceCompiles)
include(LwsCheckRequirements)

set(SAMP lws-minimal-http-client-post)
set(SRCS minimal-http-client-post.c)

set(requirements 1)
set(MBEDTLS 0)
require_lws_config(LWS_ROLE_H1 1 requirements)
require_lws_config(LWS_WITH_CLIENT 1 requirements)

require_lws_config(LWS_WITH_MBEDTLS 1 MBEDTLS)

if (requirements)
	add_executable(${SAMP} ${SRCS})
	find_program(VALGRIND "valgrind")
	
	#
	# instantiate the server per sai builder instance, they are running in the same
	# machine context in parallel so they can tread on each other otherwise
	#
	set(PORT_HCP_SRV "7640")
	if ("$ENV{SAI_INSTANCE_IDX}" STREQUAL "0")
		set(PORT_HCP_SRV 7641)
	endif()
	if ("$ENV{SAI_INSTANCE_IDX}" STREQUAL "1")
		set(PORT_HCP_SRV 7642)
	endif()
	if ("$ENV{SAI_INSTANCE_IDX}" STREQUAL "2")
		set(PORT_HCP_SRV 7643)
	endif()
	if ("$ENV{SAI_INSTANCE_IDX}" STREQUAL "3")
		set(PORT_HCP_SRV 7644)
	endif()
	
# hack
if (NOT WIN32 AND LWS_WITH_SERVER)

	#
	# Tests against built server running locally (needs daemonization...)
	#

if (WIN32)
	add_test(NAME st_hcp_srv COMMAND cmd.exe /c start /b $<TARGET_FILE:test-server> -s --port ${PORT_HCP_SRV})
	add_test(NAME ki_hcp_srv COMMAND taskkill /F /IM $<TARGET_FILE_NAME:test-server> /T)
else()
	#
	# mbedtls is too slow to keep up on some targets, when ctest is in parallel
	#
	if (VALGRIND AND NOT MBEDTLS)
		add_test(NAME st_hcp_srv COMMAND
			${CMAKE_SOURCE_DIR}/scripts/ctest-background.sh
					hcp_srv ${VALGRIND} --tool=memcheck 
					$<TARGET_FILE:test-server>
					-r ${CMAKE_SOURCE_DIR}/destdir/usr/local/share/libwebsockets-test-server/
					-s --port ${PORT_HCP_SRV} -d1151)
		add_test(NAME ki_hcp_srv COMMAND
			${CMAKE_SOURCE_DIR}/scripts/ctest-background-kill.sh hcp_srv ${VALGRIND}
					$<TARGET_FILE_NAME:test-server> --port ${PORT_HCP_SRV})
	else()
		add_test(NAME st_hcp_srv COMMAND
			${CMAKE_SOURCE_DIR}/scripts/ctest-background.sh
					hcp_srv
					$<TARGET_FILE:test-server>
					-r ${CMAKE_SOURCE_DIR}/destdir/usr/local/share/libwebsockets-test-server/
					-s --port ${PORT_HCP_SRV} )
		add_test(NAME ki_hcp_srv COMMAND
			${CMAKE_SOURCE_DIR}/scripts/ctest-background-kill.sh hcp_srv
					$<TARGET_FILE_NAME:test-server> --port ${PORT_HCP_SRV})
	endif()
endif()

	set_tests_properties(st_hcp_srv PROPERTIES
					     WORKING_DIRECTORY .
					     FIXTURES_SETUP hcp_srv
					     TIMEOUT 800)
	set_tests_properties(ki_hcp_srv PROPERTIES
					     FIXTURES_CLEANUP hcp_srv)

	add_test(NAME http-client-post COMMAND
		lws-minimal-http-client-post -l --port ${PORT_HCP_SRV})
	add_test(NAME http-client-post-m COMMAND
		lws-minimal-http-client-post -l -m --port ${PORT_HCP_SRV})
	add_test(NAME http-client-post-h1 COMMAND
		lws-minimal-http-client-post -l --h1 --port ${PORT_HCP_SRV})
	add_test(NAME http-client-post-m-h1 COMMAND
		lws-minimal-http-client-post -l -m --h1 --port ${PORT_HCP_SRV})
	set_tests_properties(http-client-post
			     http-client-post-m
			     http-client-post-h1
			     http-client-post-m-h1
			     PROPERTIES
			     FIXTURES_REQUIRED "hcp_srv"
			     WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}/minimal-examples/http-client/minimal-http-client-post
			     TIMEOUT 20)
endif()


	if (websockets_shared)
		target_link_libraries(${SAMP} websockets_shared ${LIBWEBSOCKETS_DEP_LIBS})
		add_dependencies(${SAMP} websockets_shared)
	else()
		target_link_libraries(${SAMP} websockets ${LIBWEBSOCKETS_DEP_LIBS})
	endif()
endif()
