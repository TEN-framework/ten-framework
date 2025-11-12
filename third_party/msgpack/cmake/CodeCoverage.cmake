# Check prereqs
FIND_PROGRAM(GCOV_PATH gcov)
FIND_PROGRAM(LCOV_PATH lcov)
FIND_PROGRAM(GENHTML_PATH genhtml)

IF(NOT GCOV_PATH)
	MESSAGE(FATAL_ERROR "gcov not found! Aborting...")
ENDIF()

IF(NOT CMAKE_COMPILER_IS_GNUCC AND NOT CMAKE_COMPILER_IS_GNUCXX)
	# Clang version 3.0.0 and greater now supports gcov as well.
	MESSAGE(STATUS "Compiler is not GNU gcc! Clang Version 3.0.0 and greater supports gcov as well, but older versions don't.")
	IF(NOT "${CMAKE_C_COMPILER_ID}" MATCHES "Clang" AND NOT "${CMAKE_CXX_COMPILER_ID}" MATCHES "Clang")
		MESSAGE(FATAL_ERROR "Compiler is not GNU gcc! Aborting...")
	ENDIF()
ENDIF()

SET(COVERAGE_FLAGS "-g -O0 --coverage")

FUNCTION(SETUP_TARGET_FOR_COVERAGE _targetname _testrunner _outputname)

	IF(NOT LCOV_PATH)
		MESSAGE(FATAL_ERROR "lcov not found! Aborting...")
	ENDIF()

	IF(NOT GENHTML_PATH)
		MESSAGE(FATAL_ERROR "genhtml not found! Aborting...")
	ENDIF()

	# Setup target
	ADD_CUSTOM_TARGET(${_targetname}

		# Cleanup lcov
		${LCOV_PATH} --directory . --zerocounters

		# Run tests
		COMMAND ${_testrunner} ${ARGV3}

		# Capturing lcov counters and generating report
		COMMAND ${LCOV_PATH} --directory . --capture --output-file ${_outputname}.info --base-directory ${CMAKE_SOURCE_DIR} --no-external --quiet
		COMMAND ${LCOV_PATH} --remove ${_outputname}.info '*/test/*' '*/fuzz/*' --output-file ${_outputname}.info.cleaned --quiet
		COMMAND ${GENHTML_PATH} -o ${_outputname} ${_outputname}.info.cleaned --prefix ${CMAKE_SOURCE_DIR}
		# COMMAND ${CMAKE_COMMAND} -E remove ${_outputname}.info ${_outputname}.info.cleaned

		WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
		COMMENT "Resetting code coverage counters to zero.\nProcessing code coverage counters and generating report."
	)

	# Show info where to find the report
	ADD_CUSTOM_COMMAND(TARGET ${_targetname} POST_BUILD
		COMMAND ;
		COMMENT "Open ./${_outputname}/index.html in your browser to view the coverage report."
	)

ENDFUNCTION()
