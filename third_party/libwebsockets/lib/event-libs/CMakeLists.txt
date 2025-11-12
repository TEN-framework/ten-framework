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

include_directories(.)

macro(create_evlib_plugin PLUGIN_NAME MAIN_SRC PLUGIN_HDR EVLIB)

	set(PLUGIN_SRCS ${MAIN_SRC})

	source_group("Headers Private"   FILES ${PLUGIN_HDR})
	source_group("Sources"   FILES ${MAIN_SRC})
	add_library(websockets-${PLUGIN_NAME} SHARED ${MAIN_SRC} ${PLUGIN_HDR})

	if (APPLE)
		set_property(TARGET websockets-${PLUGIN_NAME} PROPERTY MACOSX_RPATH YES)
	endif()

	foreach(libpath ${LWS_DEP_LIB_PATHS})
		target_link_directories(${TEST_NAME} ${libpath})
	endforeach()

	target_link_libraries(websockets-${PLUGIN_NAME} websockets_shared ${EVLIB})
	add_dependencies(websockets-${PLUGIN_NAME} websockets_shared)
	target_compile_definitions(websockets-${PLUGIN_NAME} PRIVATE LWS_BUILDING_SHARED)

	target_include_directories(websockets-${PLUGIN_NAME} PRIVATE
			${PLUGIN_INCLUDE} ${LWS_LIB_BUILD_INC_PATHS})

	# Set test app specific defines.
	#	set_property(TARGET ${PLUGIN_NAME}
	#	     PROPERTY COMPILE_DEFINITIONS
	#	     INSTALL_DATADIR="${CMAKE_INSTALL_PREFIX}/evlib-plugins"
	#)

	set(CMAKE_POSITION_INDEPENDENT_CODE ON)

	install(TARGETS websockets-${PLUGIN_NAME}
		EXPORT LibwebsocketsTargets
		LIBRARY DESTINATION "${LWS_INSTALL_LIB_DIR}${LIB_SUFFIX}"
		COMPONENT ${PLUGIN_NAME})
		
	list(APPEND EVLIB_PLUGINS_LIST websockets-${PLUGIN_NAME})

endmacro()

#
# poll support gets built into the lib as the default
#

if (LWS_WITH_POLL)
	add_subdir_include_directories(poll)
endif()

if (LWS_WITH_LIBUV OR LWS_WITH_LIBUV_INTERNAL)
	add_subdir_include_directories(libuv)
	set(LWS_HAVE_UV_VERSION_H ${LWS_HAVE_UV_VERSION_H} PARENT_SCOPE)
	set(LWS_HAVE_NEW_UV_VERSION_H ${LWS_HAVE_NEW_UV_VERSION_H} PARENT_SCOPE)
endif()

if (LWS_WITH_LIBEVENT)
	add_subdir_include_directories(libevent)
endif()

if (LWS_WITH_GLIB)
	add_subdir_include_directories(glib)
endif()

if (LWS_WITH_LIBEV)
	add_subdir_include_directories(libev)
	set(LWS_HAVE_EVBACKEND_LINUXAIO ${LWS_HAVE_EVBACKEND_LINUXAIO} PARENT_SCOPE)
	set(LWS_HAVE_EVBACKEND_IOURING ${LWS_HAVE_EVBACKEND_IOURING} PARENT_SCOPE)
endif()

if (LWS_WITH_SDEVENT)
	add_subdir_include_directories(sdevent)
endif()

if (LWS_WITH_ULOOP)
	add_subdir_include_directories(uloop)
endif()

#
# Keep explicit parent scope exports at end
#

export_to_parent_intermediate()
set(EVLIB_PLUGINS_LIST ${EVLIB_PLUGINS_LIST} PARENT_SCOPE)

