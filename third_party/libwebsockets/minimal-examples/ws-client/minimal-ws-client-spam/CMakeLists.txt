project(lws-minimal-ws-client-spam C)
cmake_minimum_required(VERSION 2.8.12)
find_package(libwebsockets CONFIG REQUIRED)
list(APPEND CMAKE_MODULE_PATH ${LWS_CMAKE_DIR})
include(CheckIncludeFile)
include(CheckCSourceCompiles)
include(LwsCheckRequirements)

set(SAMP lws-minimal-ws-client-spam)
set(SRCS minimal-ws-client-spam.c)

set(requirements 1)
require_pthreads(requirements)
require_lws_config(LWS_ROLE_WS 1 requirements)
require_lws_config(LWS_WITH_CLIENT 1 requirements)
require_lws_config(LWS_WITH_TLS 1 requirements)

if (requirements)
	add_executable(${SAMP} ${SRCS})
	find_program(VALGRIND "valgrind")
       #
       # instantiate the server per sai builder instance, they are running in the same
       # machine context in parallel so they can tread on each other otherwise
       #
       set(PORT_WCS_SRV "7620")
       if ("$ENV{SAI_INSTANCE_IDX}" STREQUAL "0")
	       set(PORT_WCS_SRV 7621)
       endif()
       if ("$ENV{SAI_INSTANCE_IDX}" STREQUAL "1")
	       set(PORT_WCS_SRV 7622)
       endif()
       if ("$ENV{SAI_INSTANCE_IDX}" STREQUAL "2")
	       set(PORT_WCS_SRV 7623)
       endif()
       if ("$ENV{SAI_INSTANCE_IDX}" STREQUAL "3")
	       set(PORT_WCS_SRV 7624)
       endif()

# hack
if (WIN32)
else()

if (LWS_WITH_SERVER)
if (WIN32)
	add_test(NAME st_wcs_srv COMMAND cmd.exe /c start /b $<TARGET_FILE:test-server> -s --port ${PORT_WCS_SRV})
	add_test(NAME ki_wcs_srv COMMAND taskkill /F /IM $<TARGET_FILE_NAME:test-server> /T)
else()
	if (VALGRIND)
		add_test(NAME st_wcs_srv COMMAND
			${CMAKE_SOURCE_DIR}/scripts/ctest-background.sh
			wcs_srv ${VALGRIND} --tool=memcheck $<TARGET_FILE:test-server>
				-r ${CMAKE_SOURCE_DIR}/destdir/usr/local/share/libwebsockets-test-server/
				-s --port ${PORT_WCS_SRV} )
		add_test(NAME ki_wcs_srv COMMAND
			${CMAKE_SOURCE_DIR}/scripts/ctest-background-kill.sh
			wcs_srv ${VALGRIND} $<TARGET_FILE_NAME:test-server> --port ${PORT_WCS_SRV})
	else()
		add_test(NAME st_wcs_srv COMMAND
			${CMAKE_SOURCE_DIR}/scripts/ctest-background.sh
			wcs_srv $<TARGET_FILE:test-server>
				-r ${CMAKE_SOURCE_DIR}/destdir/usr/local/share/libwebsockets-test-server/
				-s --port ${PORT_WCS_SRV} )
		add_test(NAME ki_wcs_srv COMMAND
			${CMAKE_SOURCE_DIR}/scripts/ctest-background-kill.sh
			wcs_srv $<TARGET_FILE_NAME:test-server> --port ${PORT_WCS_SRV})
	endif()
endif()

	set_tests_properties(st_wcs_srv PROPERTIES WORKING_DIRECTORY . FIXTURES_SETUP wcs_srv TIMEOUT 800)
	set_tests_properties(ki_wcs_srv PROPERTIES FIXTURES_CLEANUP wcs_srv)

	add_test(NAME ws-client-spam COMMAND lws-minimal-ws-client-spam --server localhost --port ${PORT_WCS_SRV} -l 32 -c 3)
	set_tests_properties(ws-client-spam PROPERTIES
			     WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}/minimal-examples/ws-client/minimal-ws-client-spam
			     FIXTURES_REQUIRED "wcs_srv"
			     TIMEOUT 40)
endif()
endif()

	if (websockets_shared)
		target_link_libraries(${SAMP} websockets_shared ${PTHREAD_LIB} ${LIBWEBSOCKETS_DEP_LIBS})
		add_dependencies(${SAMP} websockets_shared)
	else()
		target_link_libraries(${SAMP} websockets ${PTHREAD_LIB} ${LIBWEBSOCKETS_DEP_LIBS})
	endif()
endif()
