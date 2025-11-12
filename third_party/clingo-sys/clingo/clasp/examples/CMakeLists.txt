set(files
	example.h
	example1.cpp
	example2.cpp
	example3.cpp
	example4.cpp
	main.cpp)
add_executable(clasp_examples ${files})
target_link_libraries(clasp_examples libclasp)
set_target_properties(clasp_examples PROPERTIES FOLDER exe)
