enable_testing()
set(test_lp
	test_aspif.cpp
	test_smodels.cpp
	test_text.cpp
	test_string_convert.cpp
	main.cpp
)
add_executable(test_potassco_lp ${test_lp})
target_link_libraries(test_potassco_lp libpotassco)
target_include_directories(test_potassco_lp	 PRIVATE
	$<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}>
)
set_target_properties(test_potassco_lp PROPERTIES
	CXX_STANDARD 11
	CXX_EXTENSIONS OFF
	CXX_STANDARD_REQUIRED YES
)
set(test_opts
	test_application.cpp
	test_options.cpp
	test_value.cpp
	main.cpp
)

add_executable(test_potassco_opts ${test_opts})
target_link_libraries(test_potassco_opts libpotassco)
target_include_directories(test_potassco_opts PRIVATE
	$<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}>
)

add_test(NAME test_lp	COMMAND test_potassco_lp)
add_test(NAME test_opts COMMAND test_potassco_opts)
set_target_properties(test_potassco_lp	 PROPERTIES FOLDER test)
set_target_properties(test_potassco_opts PROPERTIES FOLDER test)
