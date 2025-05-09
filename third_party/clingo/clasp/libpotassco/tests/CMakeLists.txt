enable_testing()
set(test_lp
	test_aspif.cpp
	test_smodels.cpp
	test_text.cpp
	test_string_convert.cpp
	main.cpp
)
add_executable(test_potassco_lp ${test_lp})

set(test_opts
	test_application.cpp
	test_options.cpp
	test_value.cpp
	main.cpp
)

add_executable(test_potassco_opts ${test_opts})

foreach(target test_potassco_lp test_potassco_opts)
	message(STATUS "Processing ${target}")
	target_link_libraries(${target} libpotassco)
	target_include_directories(${target} PRIVATE $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}>)
	set_target_properties(${target} PROPERTIES
		CXX_STANDARD 11
		CXX_EXTENSIONS OFF
		CXX_STANDARD_REQUIRED YES
		FOLDER test
	)
endforeach()

add_test(NAME test_lp	COMMAND test_potassco_lp)
add_test(NAME test_opts COMMAND test_potassco_opts)
