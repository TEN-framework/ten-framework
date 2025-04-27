enable_testing()
find_package(Threads)
if (Boost_FOUND)
  add_executable(test_uintwide_t
    test_uintwide_t_boost_backend.cpp
    test_uintwide_t_edge_cases.cpp
    test_uintwide_t_examples.cpp
    test_uintwide_t_float_convert.cpp
    test_uintwide_t_int_convert.cpp
    test_uintwide_t_n_base.cpp
    test_uintwide_t_n_binary_ops_base.cpp
    test_uintwide_t_spot_values.cpp
    test.cpp)
  target_compile_features(test_uintwide_t PRIVATE cxx_std_11)
  target_include_directories(test_uintwide_t PRIVATE ${PROJECT_SOURCE_DIR})
  target_link_libraries(test_uintwide_t Examples ${CMAKE_THREAD_LIBS_INIT})
  add_test(test test_uintwide_t)
endif()
