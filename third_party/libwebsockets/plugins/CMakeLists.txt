#
# libwebsockets - small server side websockets and web server implementation
#
# Copyright (C) 2010 - 2021 Andy Green <andy@warmcat.com>
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

if (DEFINED LIB_LIST_AT_END)
link_libraries(${LIB_LIST_AT_END})
endif()

if ((LWS_WITH_PLUGINS AND LWS_WITH_SHARED) OR LWS_WITH_PLUGINS_BUILTIN)

	#
	# Either build the plugins as separate dynamic libs (LWS_WITH_PLUGINS)
	# or build into the main lws library (LWS_WITH_PLUGINS_BUILTIN)
	#

	macro(create_plugin PLUGIN_NAME PLUGIN_INCLUDE MAIN_SRC S2 S3)

		if (NOT LWS_WITH_PLUGINS_BUILTIN)
			set(PLUGIN_SRCS ${MAIN_SRC})

			if ("${S2}" STREQUAL "")
			else()
				list(APPEND PLUGIN_SRCS ${S2})
			endif()
			if ("${S3}" STREQUAL "")
			else()
				list(APPEND PLUGIN_SRCS ${S3})
			endif()

			if (WIN32)
				list(APPEND PLUGIN_SRCS
					${WIN32_HELPERS_PATH}/getopt.c
					${WIN32_HELPERS_PATH}/getopt_long.c
					${WIN32_HELPERS_PATH}/gettimeofday.c
				)

				list(APPEND PLUGIN_HDR
					${WIN32_HELPERS_PATH}/getopt.h
					${WIN32_HELPERS_PATH}/gettimeofday.h
				)
			endif(WIN32)

			source_group("Headers Private"   FILES ${PLUGIN_HDR})
			source_group("Sources"   FILES ${PLUGIN_SRCS})
			add_library(${PLUGIN_NAME} SHARED ${PLUGIN_SRCS} ${PLUGIN_HDR})
			target_link_libraries(${PLUGIN_NAME} websockets_shared)
			add_dependencies(${PLUGIN_NAME} websockets_shared)

			# doesn't work inside macro :-O
			# target_compile_definitions(${PLUGIN_NAME} PRIVATE LWS_BUILDING_SHARED)
			target_include_directories(${PLUGIN_NAME} PRIVATE ${PLUGIN_INCLUDE}
									  ${LWS_LIB_BUILD_INC_PATHS})
			set_property(TARGET ${PLUGIN_NAME}
				     PROPERTY COMPILE_DEFINITIONS
				     INSTALL_DATADIR="${CMAKE_INSTALL_PREFIX}/plugins"
			)

			set(CMAKE_POSITION_INDEPENDENT_CODE ON)
			list(APPEND PLUGINS_LIST ${PLUGIN_NAME})
		else()
			# let's just build the things into the lib

			message("Building in plugin ${PLUGIN_NAME}")

			if ("${PLUGIN_INCLUDE}" STREQUAL "")
			else()
				list(APPEND LWS_LIB_BUILD_INC_PATHS ../plugins/${PLUGIN_INCLUDE})
			endif()

			if ("${MAIN_SRC}" STREQUAL "")
			else()
				foreach(A ${MAIN_SRC})
					list(APPEND SOURCES ../plugins/${A})
				endforeach()
			endif()
			if ("${S2}" STREQUAL "")
			else()
				foreach(A ${S2})
					list(APPEND SOURCES ../plugins/${A})
				endforeach()
			endif()
			if ("${S3}" STREQUAL "")
			else()
				foreach(A ${S3})
					list(APPEND SOURCES ../plugins/${A})
				endforeach()
			endif()

		endif(NOT LWS_WITH_PLUGINS_BUILTIN)
	endmacro()
		
if (LWS_ROLE_WS)
		create_plugin(protocol_dumb_increment ""
			      "protocol_dumb_increment.c" "" "")
		if (NOT LWS_WITH_PLUGINS_BUILTIN)
			target_compile_definitions(protocol_dumb_increment PRIVATE LWS_BUILDING_SHARED)
		endif()

		create_plugin(protocol_lws_mirror ""
			      "protocol_lws_mirror.c" "" "")
		if (NOT LWS_WITH_PLUGINS_BUILTIN)
			target_compile_definitions(protocol_lws_mirror PRIVATE LWS_BUILDING_SHARED)
		endif()

		create_plugin(protocol_lws_status ""
			      "protocol_lws_status.c" "" "")
		if (NOT LWS_WITH_PLUGINS_BUILTIN)
			target_compile_definitions(protocol_lws_status PRIVATE LWS_BUILDING_SHARED)
		endif()

		if (NOT WIN32)
			create_plugin(protocol_lws_raw_test ""
			      "protocol_lws_raw_test.c" "" "")
			if (NOT LWS_WITH_PLUGINS_BUILTIN)
				target_compile_definitions(protocol_lws_raw_test PRIVATE LWS_BUILDING_SHARED)
			endif()


		      if (UNIX AND LWS_HAVE_PTHREAD_H)
				create_plugin(protocol_deaddrop ""
				      "deaddrop/protocol_lws_deaddrop.c" "" "")
				if (NOT LWS_WITH_PLUGINS_BUILTIN)
					target_compile_definitions(protocol_deaddrop PRIVATE LWS_BUILDING_SHARED)
				endif()
			endif()
		endif()

	if (LWS_WITH_SYS_METRICS)
			create_plugin(protocol_lws_openmetrics_export ""
				      "protocol_lws_openmetrics_export.c" "" "")
			if (NOT LWS_WITH_PLUGINS_BUILTIN)
				target_compile_definitions(protocol_lws_openmetrics_export PRIVATE LWS_BUILDING_SHARED)
			endif()
	endif()

	if (NOT LWS_WITHOUT_CLIENT)
			create_plugin(protocol_client_loopback_test ""
        	                      "protocol_client_loopback_test.c" "" "")
			if (NOT LWS_WITH_PLUGINS_BUILTIN)
				target_compile_definitions(protocol_client_loopback_test PRIVATE LWS_BUILDING_SHARED)
			endif()

	endif()

endif(LWS_ROLE_WS)

	create_plugin(protocol_post_demo ""
		      "protocol_post_demo.c" "" "")
	if (NOT LWS_WITH_PLUGINS_BUILTIN)
		target_compile_definitions(protocol_post_demo PRIVATE LWS_BUILDING_SHARED)
	endif()


if (LWS_ROLE_RAW_PROXY)
	create_plugin(protocol_lws_raw_proxy ""
		      "raw-proxy/protocol_lws_raw_proxy.c" "" "")
	if (NOT LWS_WITH_PLUGINS_BUILTIN)
		target_compile_definitions(protocol_lws_raw_proxy PRIVATE LWS_BUILDING_SHARED)
	endif()

endif()

if (LWS_WITH_FTS)
	create_plugin(protocol_fulltext_demo ""
		      "protocol_fulltext_demo.c" "" "")
	if (NOT LWS_WITH_PLUGINS_BUILTIN)
		target_compile_definitions(protocol_fulltext_demo PRIVATE LWS_BUILDING_SHARED)
	endif()

endif()


if (LWS_WITH_SSL)
	create_plugin(protocol_lws_ssh_base "ssh-base/include"
		      "ssh-base/sshd.c;ssh-base/telnet.c;ssh-base/kex-25519.c" "ssh-base/crypto/chacha.c;ssh-base/crypto/ed25519.c;ssh-base/crypto/fe25519.c;ssh-base/crypto/ge25519.c;ssh-base/crypto/poly1305.c;ssh-base/crypto/sc25519.c;ssh-base/crypto/smult_curve25519_ref.c" "")
	if (NOT LWS_WITH_PLUGINS_BUILTIN)
		target_compile_definitions(protocol_lws_ssh_base PRIVATE LWS_BUILDING_SHARED)
	endif()

	create_plugin(protocol_lws_sshd_demo "ssh-base/include" "protocol_lws_sshd_demo.c" "" "")
	if (NOT LWS_WITH_PLUGINS_BUILTIN)
		target_compile_definitions(protocol_lws_sshd_demo PRIVATE LWS_BUILDING_SHARED)
	endif()

	include_directories("${PROJECT_SOURCE_DIR}/plugins/ssh-base/include")
endif()



if (LWS_WITH_ACME)
	create_plugin(protocol_lws_acme_client ""
		      "acme-client/protocol_lws_acme_client.c" "" "")
	if (NOT LWS_WITH_PLUGINS_BUILTIN)
		target_compile_definitions(protocol_lws_acme_client PRIVATE LWS_BUILDING_SHARED)
	endif()
endif()

endif((LWS_WITH_PLUGINS AND LWS_WITH_SHARED) OR LWS_WITH_PLUGINS_BUILTIN)


# plugins

if (LWS_WITH_PLUGINS AND NOT LWS_WITH_PLUGINS_BUILTIN)

	install(TARGETS ${PLUGINS_LIST}
		PERMISSIONS  OWNER_WRITE OWNER_EXECUTE GROUP_EXECUTE WORLD_EXECUTE OWNER_READ GROUP_READ WORLD_READ
		DESTINATION share/libwebsockets-test-server/plugins
		COMPONENT plugins)

	if (NOT WIN32)
		install(FILES deaddrop/assets/index.html;deaddrop/assets/deaddrop.js;deaddrop/assets/deaddrop.css;deaddrop/assets/drop.svg
			DESTINATION share/libwebsockets-test-server/deaddrop
			COMPONENT plugins)
	endif()


endif()

export_to_parent_intermediate()

