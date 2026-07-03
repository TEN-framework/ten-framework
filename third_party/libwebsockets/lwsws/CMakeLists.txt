#
# libwebsockets - small server side websockets and web server implementation
#
# Copyright (C) 2010 - 2020 Andy Green <andy@warmcat.com>
#
# Permission is hereby granted, free of charge, to any person obtaining a copy
# of this software and associated documentation files (the "Software"), to
# deal in the Software without restriction, including without limitation the
# rights to use, copy, modify, merge, publish, distribute, sublicense, and/or
# sell copies of the Software, and to permit persons to whom the Software is
# furnished to do so, subject to the following conditions:
#
# The above copyright notice and this permission notice shall be included in
# all copies or substantial portions of the Software.
#
# THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
# IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
# FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
# AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
# LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
# FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS
# IN THE SOFTWARE.
#

if (LWS_WITH_LWSWS)
	list(APPEND LWSWS_SRCS
		"main.c"
	)

	if (WIN32)
		list(APPEND LWSWS_SRCS
			${WIN32_HELPERS_PATH}/getopt.c
			${WIN32_HELPERS_PATH}/getopt_long.c
			${WIN32_HELPERS_PATH}/gettimeofday.c
		)

		list(APPEND LWSWS_HDR
			${WIN32_HELPERS_PATH}/getopt.h
			${WIN32_HELPERS_PATH}/gettimeofday.h
		)
	endif(WIN32)

	source_group("Headers Private"   FILES ${LWSWS_HDR})
	source_group("Sources"   FILES ${LWSWS_SRCS})
	add_executable(lwsws ${LWSWS_SRCS} ${LWSWS_HDR})

	if (LWS_WITH_SHARED)
		target_link_libraries(lwsws websockets_shared ${LIB_LIST_AT_END})
		add_dependencies(lwsws websockets_shared)
	else()
		target_link_libraries(lwsws websockets ${LIB_LIST_AT_END})
		add_dependencies(lwsws websockets)
	endif()
	target_include_directories(lwsws PRIVATE "${LWS_LIB_INCLUDES}" ${LWS_LIB_BUILD_INC_PATHS})
	# Set test app specific defines.
	set_property(TARGET lwsws
		     PROPERTY COMPILE_DEFINITIONS
		     INSTALL_DATADIR="${CMAKE_INSTALL_PREFIX}/share"
	)

	install(TARGETS lwsws
		RUNTIME DESTINATION "${LWS_INSTALL_BIN_DIR}" COMPONENT lwsws )
	target_compile_definitions(lwsws PRIVATE LWS_BUILDING_SHARED)


endif (LWS_WITH_LWSWS)


