enable_testing()
file(GLOB src    *.cpp)
file(GLOB header *.h)
add_executable(test_clasp ${header} ${src})
target_link_libraries(test_clasp libclasp)
# Catch2 requires C++11
set_target_properties(test_clasp PROPERTIES
    CXX_STANDARD 11
    CXX_EXTENSIONS OFF
    CXX_STANDARD_REQUIRED YES
)
set_target_properties(test_clasp PROPERTIES FOLDER test)
add_test(NAME test_clasp COMMAND test_clasp)
