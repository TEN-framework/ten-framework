# If we are being built as part of lws, confirm current build config supports
# reqconfig, else skip building ourselves.
#
# If we are being built externally, confirm installed lws was configured to
# support reqconfig, else error out with a helpful message about the problem.
#

include(CheckIncludeFile)

MACRO(require_lws_config reqconfig _val result)

	if (DEFINED ${reqconfig})
	if (${reqconfig})
		set (rq 1)
	else()
		set (rq 0)
	endif()
	else()
		set(rq 0)
	endif()

	if (${_val} EQUAL ${rq})
		set(SAME 1)
	else()
		set(SAME 0)
	endif()

	string(COMPARE EQUAL "${result}" requirements _cmp)

	# we go in the first clause if in-tree
	if (LWS_WITH_MINIMAL_EXAMPLES AND NOT ${SAME})
		if (${_val})
			message("${SAMP}: skipping as lws being built without ${reqconfig}")
		else()
			message("${SAMP}: skipping as lws built with ${reqconfig}")
		endif()
		set(${result} 0)
	else()
		if (LWS_WITH_MINIMAL_EXAMPLES)
			set(MET ${SAME})
		else()
			CHECK_C_SOURCE_COMPILES("#include <libwebsockets.h>\nint main(void) {\n#if defined(${reqconfig})\n return 0;\n#else\n fail;\n#endif\n return 0;\n}\n" HAS_${reqconfig})
			if (NOT DEFINED HAS_${reqconfig} OR NOT HAS_${reqconfig})
				set(HAS_${reqconfig} 0)
			else()
				set(HAS_${reqconfig} 1)
			endif()
			if ((HAS_${reqconfig} AND ${_val}) OR (NOT HAS_${reqconfig} AND NOT ${_val}))
				set(MET 1)
			else()
				set(MET 0)
			endif()
		endif()
		if (NOT MET AND _cmp)
			if (${_val})
				message(FATAL_ERROR "This project requires lws must have been configured with ${reqconfig}")
			else()
				message(FATAL_ERROR "Lws configuration of ${reqconfig} is incompatible with this project")
			endif()
		endif()

	endif()
ENDMACRO()

MACRO(require_pthreads result)
	CHECK_INCLUDE_FILE(pthread.h LWS_HAVE_PTHREAD_H)
	if (NOT LWS_HAVE_PTHREAD_H)
		if (LWS_WITH_MINIMAL_EXAMPLES)
			set(${result} 0)
			message("${SAMP}: skipping as no pthreads")
		else()
			message(FATAL_ERROR "threading support requires pthreads")
		endif()
	else()
		if (WIN32)
			set(PTHREAD_LIB ${LWS_EXT_PTHREAD_LIBRARIES})
		else()
			if (NOT ${CMAKE_SYSTEM_NAME} MATCHES "QNX")
				set(PTHREAD_LIB pthread)
			endif()
		endif()
	endif()
ENDMACRO()

MACRO(sai_resource SR_NAME SR_AMOUNT SR_LEASE SR_SCOPE)
	if (DEFINED ENV{SAI_OVN})

		site_name(HOST_NAME)
		
		#
		# Creates a "test" called res_${SR_SCOPE} that waits to be
		# given a lease on ${SR_AMOUNT} of a resource ${SR_NAME}, for at
		# most $SR_LEASE seconds, until the test dependent on it can
		# proceed.
		#
		# We need to keep this sai-resource instance up for the
		# duration of the actual test it is authorizing, when it
		# is killed, the resource is then immediately released.
		#
		# The resource cookie has to be globally unique within the
		# distributed builder sessions, so it includes the builder
		# hostname and builder instance information
		#

		add_test(NAME st_res_${SR_SCOPE} COMMAND
			 ${CMAKE_SOURCE_DIR}/scripts/ctest-background.sh
			 res_${SR_SCOPE}
			 sai-resource ${SR_NAME} ${SR_AMOUNT} ${SR_LEASE}
			 ${HOST_NAME}-res_${SR_SCOPE}-$ENV{SAI_PROJECT}-$ENV{SAI_OVN})

		# allow it to wait for up to 100s for the resource lease

		set_tests_properties(st_res_${SR_SCOPE} PROPERTIES
				     WORKING_DIRECTORY .
				     FIXTURES_SETUP res_sspcmin
				     TIMEOUT 100)

		add_test(NAME ki_res_${SR_SCOPE} COMMAND
			 ${CMAKE_SOURCE_DIR}/scripts/ctest-background-kill.sh
			 res_${SR_SCOPE} sai-resource )

		set_tests_properties(ki_res_${SR_SCOPE} PROPERTIES
					FIXTURES_CLEANUP res_${SR_SCOPE})

	endif()
ENDMACRO()

