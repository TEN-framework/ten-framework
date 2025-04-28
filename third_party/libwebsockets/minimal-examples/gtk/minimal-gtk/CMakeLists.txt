project(lws-minimal-gtk C)
cmake_minimum_required(VERSION 2.8.12)
find_package(libwebsockets CONFIG REQUIRED)
list(APPEND CMAKE_MODULE_PATH ${LWS_CMAKE_DIR})
include(CheckCSourceCompiles)
include(LwsCheckRequirements)

set(SAMP lws-minimal-gtk)
set(SRCS main.c)

set(requirements 1)
require_lws_config(LWS_ROLE_H1 1 requirements)
require_lws_config(LWS_WITH_SERVER 1 requirements)
require_lws_config(LWS_WITH_GLIB 1 requirements)
require_lws_config(LWS_WITH_GTK 1 requirements)

if (requirements)

# gtk pieces
	
	include (FindPkgConfig)
	
	set(LWS_GTK_INCLUDE_DIRS CACHE PATH "Path to the gtk include directory")
	set(LWS_GTK_LIBRARIES CACHE PATH "Path to the gtk library")
	PKG_SEARCH_MODULE(LWS_GTK2 gtk+-3.0)
	if (LWS_GTK2_FOUND)
		list(APPEND LWS_GTK_INCLUDE_DIRS "${LWS_GTK2_INCLUDE_DIRS}")
		list(APPEND LWS_GTK_LIBRARIES "${LWS_GTK2_LIBRARIES}")
	endif()
	message("gtk include dir: ${LWS_GTK_INCLUDE_DIRS}")
	message("gtk libraries: ${LWS_GTK_LIBRARIES}")
	include_directories("${LWS_GTK_INCLUDE_DIRS}")
	set(extralibs ${extralibs} ${LWS_GTK_LIBRARIES})
	
	
	
	message("Extra libs: ${extralibs}")

	add_executable(${SAMP} ${SRCS})

	if (websockets_shared)
		target_link_libraries(${SAMP} websockets_shared ${extralibs} ${LIBWEBSOCKETS_DEP_LIBS})
		add_dependencies(${SAMP} websockets_shared)
	else()
		target_link_libraries(${SAMP} websockets ${extralibs} ${LIBWEBSOCKETS_DEP_LIBS})
	endif()
endif()
