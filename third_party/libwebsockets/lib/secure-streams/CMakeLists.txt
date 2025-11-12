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

if (LWS_WITH_CLIENT)
	list(APPEND SOURCES
			secure-streams/secure-streams.c
			secure-streams/policy-common.c
			secure-streams/system/captive-portal-detect/captive-portal-detect.c
			secure-streams/protocols/ss-raw.c
		)
		if (NOT LWS_WITH_SECURE_STREAMS_STATIC_POLICY_ONLY)
			list(APPEND SOURCES
				secure-streams/policy-json.c
				secure-streams/system/fetch-policy/fetch-policy.c
			)
		endif()
		if (LWS_ROLE_H1)
			list(APPEND SOURCES
				secure-streams/protocols/ss-h1.c
			)
		endif()
		if (LWS_ROLE_H2)
			list(APPEND SOURCES
				secure-streams/protocols/ss-h2.c
			)
		endif()
		if (LWS_ROLE_WS)
			list(APPEND SOURCES
				secure-streams/protocols/ss-ws.c
			)
		endif()
		if (LWS_ROLE_MQTT)
			list(APPEND SOURCES
				secure-streams/protocols/ss-mqtt.c
			)
		endif()

		if (LWS_WITH_SECURE_STREAMS_PROXY_API)
			list(APPEND SOURCES
				secure-streams/secure-streams-serialize.c
				secure-streams/secure-streams-client.c
			)
		endif()

		if (LWS_WITH_SECURE_STREAMS_PROXY_API)
			list(APPEND SOURCES
				secure-streams/secure-streams-process.c
			)
		endif()

		if (LWS_WITH_SECURE_STREAMS_SYS_AUTH_API_AMAZON_COM AND
		    LWS_WITH_SYS_STATE)
			list(APPEND SOURCES
				secure-streams/system/auth-api.amazon.com/auth.c
			)
		endif()

		if (LWS_WITH_SECURE_STREAMS_AUTH_SIGV4)
			list(APPEND SOURCES
				secure-streams/system/auth-sigv4/sign.c
			)
		endif()


		if (LWS_WITH_SECURE_STREAMS_CPP)
			list(APPEND SOURCES secure-streams/cpp/lss.cxx)

			if (LWS_ROLE_H1 OR LWS_ROLE_H2)
				list(APPEND SOURCES secure-streams/cpp/lssFile.cxx)
			endif()

			if (LWS_ROLE_WS)
				list(APPEND SOURCES secure-streams/cpp/lssMsg.cxx)
			endif()
		endif()

	#
	# Helper function for adding a secure stream plugin
	#
	macro(create_ss_plugin NAME S2 S3 S4 S5 S6)

		set(SSP_SRCS)
		set(SSP_PUBLIC_HDR)
		set(SSP_HDR)

		if ("${S2}" STREQUAL "")
		else()
			list(APPEND SSP_SRCS plugins/${NAME}/${S2})
		endif()
		if ("${S3}" STREQUAL "")
		else()
			list(APPEND SSP_SRCS plugins/${NAME}/${S3})
		endif()
		if ("${S4}" STREQUAL "")
		else()
			list(APPEND SSP_SRCS plugins/${NAME}/${S4})
		endif()
		if ("${S5}" STREQUAL "")
		else()
			list(APPEND SSP_SRCS plugins/${NAME}/${S5})
		endif()
		if ("${S6}" STREQUAL "")
		else()
			list(APPEND SSP_SRCS plugins/${NAME}/${S6})
		endif()

		source_group("Headers Private"  FILES ${SSP_HDR})
		source_group("Sources"		FILES ${SSP_SRCS})

		add_library( ${NAME} STATIC
			     ${SSP_HDR} ${SSP_PUBLIC_HDR} ${SSP_SRCS} )

		target_include_directories(${NAME} PRIVATE "${LWS_LIB_INCLUDES}" ${LWS_LIB_BUILD_INC_PATHS})

		if (NOT LWS_PLAT_FREERTOS)
	                add_dependencies(${NAME} websockets_shared)
		endif()
		list(APPEND SS_PLUGINS_LIST ${NAME})
	endmacro()

	#	create_ss_plugin(ssp-h1url "h1url.c" "" "" "" "")
endif()

#
# Keep explicit parent scope exports at end
#

exports_to_parent_scope()
