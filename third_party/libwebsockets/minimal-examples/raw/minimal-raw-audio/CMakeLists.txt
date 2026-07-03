project(lws-minimal-raw-audio C)
cmake_minimum_required(VERSION 2.8.12)
find_package(libwebsockets CONFIG REQUIRED)
list(APPEND CMAKE_MODULE_PATH ${LWS_CMAKE_DIR})
include(CheckCSourceCompiles)
include(LwsCheckRequirements)

set(SAMP lws-minimal-raw-audio)
set(SRCS audio.c)

set(requirements 1)
require_lws_config(LWS_WITH_ALSA 1 requirements)
require_lws_config(LWS_WITH_NETWORK 1 requirements)

if (requirements)
	add_executable(${SAMP} ${SRCS})

	if (websockets_shared)
		target_link_libraries(${SAMP} websockets_shared asound ${LIBWEBSOCKETS_DEP_LIBS})
		add_dependencies(${SAMP} websockets_shared)
	else()
		target_link_libraries(${SAMP} websockets asound ${LIBWEBSOCKETS_DEP_LIBS})
	endif()
endif()
