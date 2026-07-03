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

#
# Test applications
#

set(TEST_APP_LIST)
if ((LWS_ROLE_H1 OR LWS_ROLE_H2))
	#
	# Helper function for adding a test app.
	#
	macro(create_test_app TEST_NAME MAIN_SRC S2 S3 S4 S5 S6)

		set(TEST_SRCS ${MAIN_SRC})
		set(TEST_HDR)
		if ("${S2}" STREQUAL "")
		else()
			list(APPEND TEST_SRCS ${S2})
		endif()
		if ("${S3}" STREQUAL "")
		else()
			list(APPEND TEST_SRCS ${S3})
		endif()
		if ("${S4}" STREQUAL "")
		else()
			list(APPEND TEST_SRCS ${S4})
		endif()
		if ("${S5}" STREQUAL "")
		else()
			list(APPEND TEST_SRCS ${S5})
		endif()
		if ("${S6}" STREQUAL "")
		else()
			list(APPEND TEST_SRCS ${S6})
		endif()
		if (WIN32)
			list(APPEND TEST_SRCS
				${WIN32_HELPERS_PATH}/getopt.c
				${WIN32_HELPERS_PATH}/getopt_long.c
				${WIN32_HELPERS_PATH}/gettimeofday.c
			)

			list(APPEND TEST_HDR
				${WIN32_HELPERS_PATH}/getopt.h
				${WIN32_HELPERS_PATH}/gettimeofday.h
			)
		endif(WIN32)

		source_group("Headers Private"   FILES ${TEST_HDR})
		source_group("Sources"   FILES ${TEST_SRCS})
		add_executable(${TEST_NAME} ${TEST_SRCS} ${TEST_HDR})

		foreach(libpath ${LWS_DEP_LIB_PATHS})
			target_link_directories(${TEST_NAME} ${libpath})
		endforeach()

		if (LWS_LINK_TESTAPPS_DYNAMIC)
			if (NOT LWS_WITH_SHARED)
				message(FATAL_ERROR "Build of the shared library is disabled. LWS_LINK_TESTAPPS_DYNAMIC must be combined with LWS_WITH_SHARED.")
			endif()
			target_link_libraries(${TEST_NAME} websockets_shared)
			add_dependencies(${TEST_NAME} websockets_shared)
		else()
			if (NOT LWS_WITH_STATIC)
				message(FATAL_ERROR "Build of the static library is disabled. Disabled LWS_LINK_TESTAPPS_DYNAMIC must be combined with LWS_WITH_STATIC.")
			endif()
			target_link_libraries(${TEST_NAME} websockets)
			add_dependencies(${TEST_NAME} websockets)
			if (UNIX AND LWS_WITH_SSL AND NOT LWS_WITH_MBEDTLS)
				target_link_libraries(${TEST_NAME} ${CMAKE_DL_LIBS})
			endif()
		endif()
		
		if (LWS_LIB_INCLUDES)
			target_include_directories(${TEST_NAME} PRIVATE "${LWS_LIB_INCLUDES}" ${LWS_LIB_BUILD_INC_PATHS})
		else()
			target_include_directories(${TEST_NAME} PRIVATE ${LWS_LIB_BUILD_INC_PATHS})
		endif()
		target_compile_options(${TEST_NAME} PRIVATE ${LWS_PTHR_FLAGS})

		if (LWS_WITH_HTTP_STREAM_COMPRESSION)
			target_link_libraries(${TEST_NAME} z)
		endif()

		# Set test app specific defines.
		set_property(TARGET ${TEST_NAME}
					PROPERTY COMPILE_DEFINITIONS
						INSTALL_DATADIR="${CMAKE_INSTALL_PREFIX}/share"
					)

		# Prefix the binary names with libwebsockets.
		set_target_properties(${TEST_NAME}
			PROPERTIES
			OUTPUT_NAME libwebsockets-${TEST_NAME})
			
		target_link_libraries(${TEST_NAME} ${LIB_LIST_AT_END})

		# Add to the list of tests.
		list(APPEND TEST_APP_LIST ${TEST_NAME})
	endmacro()

	if (NOT LWS_WITHOUT_SERVER)
		#
		# test-server
		#
		if (NOT LWS_WITHOUT_TEST_SERVER)
			create_test_app(test-server "test-server.c"
				""
				""
				""
				""
				"")
			target_compile_definitions(test-server PRIVATE LWS_BUILDING_SHARED)

			if (LWS_WITH_CGI AND (LWS_WITH_PLUGINS OR LWS_WITH_PLUGINS_BUILTIN) AND LWS_WITH_TLS)
			create_test_app(test-sshd "test-sshd.c"
				""
				""
				""
				""
				"")
			target_include_directories(test-sshd PRIVATE "${PROJECT_SOURCE_DIR}/plugins/ssh-base/include")
			target_compile_definitions(test-sshd PRIVATE LWS_BUILDING_SHARED)
			endif()

		endif()

		#
		# test-server-extpoll
		#
		if (NOT LWS_WITHOUT_TEST_SERVER_EXTPOLL AND NOT WIN32)
			create_test_app(test-server-extpoll
				"test-server.c"
				""
				""
				""
				""
				"")
			target_compile_definitions(test-server-extpoll PRIVATE LWS_BUILDING_SHARED)
			# Set defines for this executable only.
			set_property(
				TARGET test-server-extpoll
				PROPERTY COMPILE_DEFINITIONS 
					EXTERNAL_POLL 
					INSTALL_DATADIR="${CMAKE_INSTALL_PREFIX}/share"
				)

			# We need to link against winsock code.
			if (WIN32)
				target_link_libraries(test-server-extpoll ws2_32.lib)
			endif(WIN32)
		endif()

		if (LWS_WITH_LEJP)
			create_test_app(
				test-lejp
				"test-lejp.c"
				""
				""
				""
				""
				"")
			target_compile_definitions(test-lejp PRIVATE LWS_BUILDING_STATIC)
		endif()

		if (LWS_WITH_CBOR)
			create_test_app(
				test-lecp
				"test-lecp.c"
				""
				""
				""
				""
				"")
			target_compile_definitions(test-lecp PRIVATE LWS_BUILDING_STATIC)
		endif()


		# Data files for running the test server.
		list(APPEND TEST_SERVER_DATA
			"${PROJECT_SOURCE_DIR}/test-apps/favicon.ico"
			"${PROJECT_SOURCE_DIR}/test-apps/leaf.jpg"
			"${PROJECT_SOURCE_DIR}/test-apps/candide.zip"
			"${PROJECT_SOURCE_DIR}/test-apps/candide-uncompressed.zip"
			"${PROJECT_SOURCE_DIR}/test-apps/libwebsockets.org-logo.svg"
			"${PROJECT_SOURCE_DIR}/test-apps/http2.png"
			"${PROJECT_SOURCE_DIR}/test-apps/wss-over-h2.png"
			"${PROJECT_SOURCE_DIR}/test-apps/lws-common.js"
			"${PROJECT_SOURCE_DIR}/test-apps/test.html"
			"${PROJECT_SOURCE_DIR}/test-apps/test.css"
			"${PROJECT_SOURCE_DIR}/test-apps/test.js")

		add_custom_command(TARGET test-server
						POST_BUILD 
						COMMAND "${CMAKE_COMMAND}" -E make_directory "$<TARGET_FILE_DIR:test-server>/../share/libwebsockets-test-server")

		# Copy the file needed to run the server so that the test apps can
		# reach them from their default output location
		foreach (TEST_FILE ${TEST_SERVER_DATA})
			if (EXISTS ${TEST_FILE})
				add_custom_command(TARGET test-server
							POST_BUILD 
							COMMAND "${CMAKE_COMMAND}" -E copy "${TEST_FILE}" "$<TARGET_FILE_DIR:test-server>/../share/libwebsockets-test-server" VERBATIM)
			endif()
		endforeach()
	endif(NOT LWS_WITHOUT_SERVER)

	if (NOT LWS_WITHOUT_CLIENT)
		#
		# test-client
		#
		if (NOT LWS_WITHOUT_TEST_CLIENT)
			create_test_app(test-client "test-client.c" "" "" "" "" "")
		endif()

	endif(NOT LWS_WITHOUT_CLIENT)
endif((LWS_ROLE_H1 OR LWS_ROLE_H2))

# Install test apps.

install(TARGETS ${TEST_APP_LIST}
		RUNTIME DESTINATION ${LWS_INSTALL_EXAMPLES_DIR}
		COMPONENT examples)
set(CPACK_COMPONENT_EXAMPLES_DISPLAY_NAME "Example files")

# Programs shared files used by the test-server

if (NOT LWS_WITHOUT_SERVER)
	install(FILES ${TEST_SERVER_DATA}
			DESTINATION share/libwebsockets-test-server
		COMPONENT examples)

		install(FILES "${PROJECT_SOURCE_DIR}/test-apps/private/index.html"
			DESTINATION share/libwebsockets-test-server/private
			COMPONENT examples)
if (LWS_WITH_CGI)
	set(CGI_TEST_SCRIPT "${PROJECT_SOURCE_DIR}/test-apps/lws-cgi-test.sh")
	install(FILES ${CGI_TEST_SCRIPT}
			PERMISSIONS  OWNER_EXECUTE GROUP_EXECUTE WORLD_EXECUTE OWNER_READ GROUP_READ WORLD_READ
			DESTINATION share/libwebsockets-test-server
			COMPONENT examples)
	endif()
endif()


if (NOT LWS_WITHOUT_TEST_SERVER AND NOT LWS_WITHOUT_SERVER)
	install(FILES lws-ssh-test-keys;lws-ssh-test-keys.pub
		DESTINATION share/libwebsockets-test-server
		COMPONENT examples)
endif()
