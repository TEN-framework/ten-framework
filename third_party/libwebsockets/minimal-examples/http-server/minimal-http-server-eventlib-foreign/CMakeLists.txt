project(lws-minimal-http-server-eventlib-foreign C)
cmake_minimum_required(VERSION 2.8.12)
find_package(libwebsockets CONFIG REQUIRED)
list(APPEND CMAKE_MODULE_PATH ${LWS_CMAKE_DIR})
include(CheckIncludeFile)
include(CheckCSourceCompiles)
include(LwsCheckRequirements)

set(SAMP lws-minimal-http-server-eventlib-foreign)
set(SRCS minimal-http-server-eventlib-foreign.c)

set(requirements 1)
require_pthreads(requirements)
require_lws_config(LWS_ROLE_H1 1 requirements)
require_lws_config(LWS_WITH_SERVER 1 requirements)
require_lws_config(LWS_WITH_TLS 1 requirements)

CHECK_C_SOURCE_COMPILES("#include <libwebsockets.h>\nint main(void) {\n#if defined(LWS_WITH_LIBUV)\n return 0;\n#else\n fail;\n#endif\n return 0;\n}\n" LWS_WITH_LIBUV)
CHECK_C_SOURCE_COMPILES("#include <libwebsockets.h>\nint main(void) {\n#if defined(LWS_WITH_LIBEVENT)\n return 0;\n#else\n fail;\n#endif\n return 0;\n}\n" LWS_WITH_LIBEVENT)
CHECK_C_SOURCE_COMPILES("#include <libwebsockets.h>\nint main(void) {\n#if defined(LWS_WITH_LIBEV)\n return 0;\n#else\n fail;\n#endif\n return 0;\n}\n" LWS_WITH_LIBEV)
CHECK_C_SOURCE_COMPILES("#include <libwebsockets.h>\nint main(void) {\n#if defined(LWS_WITH_GLIB)\n return 0;\n#else\n fail;\n#endif\n return 0;\n}\n" LWS_WITH_GLIB)
CHECK_C_SOURCE_COMPILES("#include <libwebsockets.h>\nint main(void) {\n#if defined(LWS_WITH_SDEVENT)\n return 0;\n#else\n fail;\n#endif\n return 0;\n}\n" LWS_WITH_SDEVENT)
CHECK_C_SOURCE_COMPILES("#include <libwebsockets.h>\nint main(void) {\n#if defined(LWS_WITH_ULOOP)\n return 0;\n#else\n fail;\n#endif\n return 0;\n}\n" LWS_WITH_ULOOP)

if (LWS_WITH_LIBUV)
	find_path(LIBUV_INCLUDE_DIRS NAMES uv.h)
	find_library(LIBUV_LIBRARIES NAMES uv)
	message("libuv include dir: ${LIBUV_INCLUDE_DIRS}")
	message("libuv libraries: ${LIBUV_LIBRARIES}")
	include_directories("${LIBUV_INCLUDE_DIRS}")
	set(extralibs ${extralibs} ${LIBUV_LIBRARIES})
	list(APPEND SRCS libuv.c)
endif()
if (LWS_WITH_LIBEVENT)
	find_path(LIBEVENT_INCLUDE_DIRS NAMES event2/event.h)
	find_library(LIBEVENT_LIBRARIES NAMES event)
	message("libevent include dir: ${LIBEVENT_INCLUDE_DIRS}")
	message("libevent libraries: ${LIBEVENT_LIBRARIES}")
	include_directories("${LIBEVENT_INCLUDE_DIRS}")
	set(extralibs ${extralibs} ${LIBEVENT_LIBRARIES})
	list(APPEND SRCS libevent.c)
endif()
if (LWS_WITH_LIBEV)
	find_path(LIBEV_INCLUDE_DIRS NAMES ev.h)
	find_library(LIBEV_LIBRARIES NAMES ev)
	message("libev include dir: ${LIBEV_INCLUDE_DIRS}")
	message("libev libraries: ${LIBEV_LIBRARIES}")
	include_directories("${LIBEV_INCLUDE_DIRS}")
	set(extralibs ${extralibs} ${LIBEV_LIBRARIES})
	list(APPEND SRCS libev.c)
endif()
if (LWS_WITH_GLIB)
	set(LWS_GLIB_INCLUDE_DIRS CACHE PATH "Path to the glib include directory")
	set(LWS_GLIB_LIBRARIES CACHE PATH "Path to the glib library")
		include (FindPkgConfig)
	if (NOT GLIB_FOUND)
		find_path(GLIB_INCLUDE_DIRS NAMES glib-2.0/glib.h)
		find_library(GLIB_LIBRARIES NAMES glib-2.0)
		if(GLIB_INCLUDE_DIRS AND GLIB_LIBRARIES)
			set(GLIB_FOUND 1)
		endif()
		if (GLIB_INCLUDE_DIRS)
			set(GLIB_INCLUDE_DIRS "${GLIB_INCLUDE_DIRS}/glib-2.0")
		endif()
	endif()
	PKG_SEARCH_MODULE(LWS_GLIB2 glib-2.0)
	if (LWS_GLIB2_FOUND)
		list(APPEND GLIB_INCLUDE_DIRS "${LWS_GLIB2_INCLUDE_DIRS}")
	endif()
	message("glib include dir: ${GLIB_INCLUDE_DIRS}")
	message("glib libraries: ${GLIB_LIBRARIES}")
	include_directories("${GLIB_INCLUDE_DIRS}")
	set(extralibs ${extralibs} ${GLIB_LIBRARIES})
	list(APPEND SRCS glib.c)
endif()
if (LWS_WITH_SDEVENT)
	find_path(LIBSYSTEMD_INCLUDE_DIRS NAMES systemd/sd-event.h)
	find_library(LIBSYSTEMD_LIBRARIES NAMES systemd)
	message("libsystemd include dir: ${LIBSYSTEMD_INCLUDE_DIRS}")
	message("libsystemd libraries: ${LIBSYSTEMD_LIBRARIES}")
	include_directories("${LIBSYSTEMD_INCLUDE_DIRS}")
	set(extralibs ${extralibs} ${LIBSYSTEMD_LIBRARIES})
	list(APPEND SRCS libsdevent.c)
endif()
if (LWS_WITH_ULOOP)
	find_path(LIBUBOX_INCLUDE_DIRS NAMES libubox/uloop.h)
	find_library(LIBUBOX_LIBRARIES NAMES ubox)
	message("libubox include dir: ${LIBUBOX_INCLUDE_DIRS}")
	message("libubox libraries: ${LIBUBOX_LIBRARIES}")
	include_directories("${LIBUBOX_INCLUDE_DIRS}")
	set(extralibs ${extralibs} ${LIBUBOX_LIBRARIES})
	list(APPEND SRCS uloop.c)
endif()

message("Extra libs: ${extralibs}")

if (NOT LWS_WITH_LIBUV AND NOT LWS_WITH_LIBEVENT AND NOT LWS_WITH_LIBEV AND NOT LWS_WITH_GLIB AND NOT LWS_WITH_ULOOP)
	set(requirements 0)
endif()

if (requirements)
	add_executable(${SAMP} ${SRCS})
	
	#
	# tests are running in the same machine context in parallel so they
	# compete for the same ports.  Select a base port from which sai
	# instance we are running in, add another digit at the actual test
	# according to which subtest it is.  Then there can be no clashes
	# regardless of how many build and tests in parallel.
	#

	set(PORT_HSEF_SRV "961")
	if ("$ENV{SAI_INSTANCE_IDX}" STREQUAL "0")
		set(PORT_HSEF_SRV 962)
	endif()
	if ("$ENV{SAI_INSTANCE_IDX}" STREQUAL "1")
		set(PORT_HSEF_SRV 963)
	endif()
	if ("$ENV{SAI_INSTANCE_IDX}" STREQUAL "2")
		set(PORT_HSEF_SRV 964)
	endif()
	if ("$ENV{SAI_INSTANCE_IDX}" STREQUAL "3")
		set(PORT_HSEF_SRV 965)
	endif()

	if (websockets_shared)
		target_link_libraries(${SAMP} websockets_shared ${extralibs} ${PTHREAD_LIB} ${LIBWEBSOCKETS_DEP_LIBS})
		add_dependencies(${SAMP} websockets_shared)
	else()
		target_link_libraries(${SAMP} websockets ${extralibs} ${PTHREAD_LIB} ${LIBWEBSOCKETS_DEP_LIBS})
	endif()
	
	# notice we override the evlib plugin source via LD_LIBRARY_PATH so
	# we are using the evlibs we just built, if any
	
	if (LWS_WITH_LIBUV)
		add_test(NAME hs_evlib_foreign_uv COMMAND lws-minimal-http-server-eventlib-foreign --uv -p ${PORT_HSEF_SRV}1)
		set_tests_properties(hs_evlib_foreign_uv
			     PROPERTIES
			     ENVIRONMENT "LD_LIBRARY_PATH=${CMAKE_BINARY_DIR}/lib"
			     WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}/minimal-examples/http-server/minimal-http-server-eventlib-foreign
			     TIMEOUT 50)
	endif()
	if (LWS_WITH_LIBEVENT)
		add_test(NAME hs_evlib_foreign_event COMMAND lws-minimal-http-server-eventlib-foreign --event -p ${PORT_HSEF_SRV}2)
		set_tests_properties(hs_evlib_foreign_event
			     PROPERTIES
			     ENVIRONMENT "LD_LIBRARY_PATH=${CMAKE_BINARY_DIR}/lib"
			     WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}/minimal-examples/http-server/minimal-http-server-eventlib-foreign
			     TIMEOUT 50)
	endif()
	if (LWS_WITH_LIBEV)
		add_test(NAME hs_evlib_foreign_ev COMMAND lws-minimal-http-server-eventlib-foreign --ev -p ${PORT_HSEF_SRV}3)
		set_tests_properties(hs_evlib_foreign_ev
			     PROPERTIES
			     ENVIRONMENT "LD_LIBRARY_PATH=${CMAKE_BINARY_DIR}/lib"
			     WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}/minimal-examples/http-server/minimal-http-server-eventlib-foreign
			     TIMEOUT 50)
	endif()
	if (LWS_WITH_GLIB)
		add_test(NAME hs_evlib_foreign_glib COMMAND lws-minimal-http-server-eventlib-foreign --glib -p ${PORT_HSEF_SRV}4)
		set_tests_properties(hs_evlib_foreign_glib
			     PROPERTIES
			     ENVIRONMENT "LD_LIBRARY_PATH=${CMAKE_BINARY_DIR}/lib"
			     WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}/minimal-examples/http-server/minimal-http-server-eventlib-foreign
			     TIMEOUT 50)
	endif()
	if (LWS_WITH_SDEVENT)
		add_test(NAME hs_evlib_foreign_sd COMMAND lws-minimal-http-server-eventlib-foreign --sd -p ${PORT_HSEF_SRV}5)
		set_tests_properties(hs_evlib_foreign_sd
			     PROPERTIES
			     ENVIRONMENT "LD_LIBRARY_PATH=${CMAKE_BINARY_DIR}/lib"
			     WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}/minimal-examples/http-server/minimal-http-server-eventlib-foreign
			     TIMEOUT 50)
	endif()
	if (LWS_WITH_SDEVENT)
		add_test(NAME hs_evlib_foreign_uloop COMMAND lws-minimal-http-server-eventlib-foreign --uloop -p ${PORT_HSEF_SRV}5)
		set_tests_properties(hs_evlib_foreign_uloop
			     PROPERTIES
			     ENVIRONMENT "LD_LIBRARY_PATH=${CMAKE_BINARY_DIR}/lib"
			     WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}/minimal-examples/http-server/minimal-http-server-eventlib-foreign
			     TIMEOUT 50)
	endif()
	
endif()
