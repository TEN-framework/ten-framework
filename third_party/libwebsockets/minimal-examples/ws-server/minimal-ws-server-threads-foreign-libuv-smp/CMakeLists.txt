project(lws-minimal-ws-server-threads-foreign-libuv-smp C)
cmake_minimum_required(VERSION 2.8.12)
find_package(libwebsockets CONFIG REQUIRED)
list(APPEND CMAKE_MODULE_PATH ${LWS_CMAKE_DIR})
include(CheckIncludeFile)
include(CheckCSourceCompiles)
include(LwsCheckRequirements)

set(SAMP lws-minimal-ws-server-threads-foreign-smp)
set(SRCS minimal-ws-server.c)

set(requirements 1)
require_pthreads(requirements)
require_lws_config(LWS_ROLE_WS 1 requirements)
require_lws_config(LWS_WITH_SERVER 1 requirements)
require_lws_config(LWS_WITH_TLS 1 requirements)
require_lws_config(LWS_WITH_LIBUV 1 requirements)

CHECK_C_SOURCE_COMPILES("#include <libwebsockets.h>\nint main(void) {\n#if defined(LWS_WITH_LIBUV)\n return 0;\n#else\n fail;\n#endif\n return 0;\n}\n" LWS_WITH_LIBUV)

if (NOT LWS_WITH_LIBUV)
	set(requirements 0)
endif()


if (requirements)
	add_executable(${SAMP} ${SRCS})
	
	find_path(LIBUV_INCLUDE_DIRS NAMES uv.h)
	find_library(LIBUV_LIBRARIES NAMES uv)
	message("libuv include dir: ${LIBUV_INCLUDE_DIRS}")
	message("libuv libraries: ${LIBUV_LIBRARIES}")
	include_directories("${LIBUV_INCLUDE_DIRS}")
	set(extralibs ${extralibs} ${LIBUV_LIBRARIES})
	
	message("Extra libs: ${extralibs}")

		if (websockets_shared)
			target_link_libraries(${SAMP} websockets_shared ${extralibs} ${PTHREAD_LIB} ${LIBWEBSOCKETS_DEP_LIBS})
			add_dependencies(${SAMP} websockets_shared)
		else()
			target_link_libraries(${SAMP} websockets ${extralibs} ${PTHREAD_LIB} ${LIBWEBSOCKETS_DEP_LIBS})
		endif()
endif()
