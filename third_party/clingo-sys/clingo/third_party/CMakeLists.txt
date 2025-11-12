if (CLINGO_BUILD_TESTS AND NOT TARGET Catch2::Catch2WithMain)
    if (CLINGO_USE_LOCAL_CATCH)
        set(CATCH_BUILD_STATIC_LIBRARY ON CACHE INTERNAL "" FORCE)
        add_subdirectory(catch)
    else()
        find_package(Catch2 3.1 REQUIRED)
    endif()
endif()

add_library(sparse_map INTERFACE)
target_include_directories(sparse_map INTERFACE $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/sparse-map/include>)
add_library(tsl::sparse_map ALIAS sparse_map)

add_library(hopscotch_map INTERFACE)
target_include_directories(hopscotch_map INTERFACE $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/hopscotch-map/include>)
add_library(tsl::hopscotch_map ALIAS hopscotch_map)

add_library(ordered_map INTERFACE)
target_include_directories(ordered_map INTERFACE $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/ordered-map/include>)
add_library(tsl::ordered_map ALIAS ordered_map)

add_library(optional INTERFACE)
target_include_directories(optional INTERFACE $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/optional/include>)
add_library(tl::optional ALIAS optional)

add_library(variant INTERFACE)
target_include_directories(variant INTERFACE $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/variant/include>)
add_library(mpark::variant ALIAS variant)

add_library(wide_integer INTERFACE)
target_include_directories(wide_integer INTERFACE $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/wide-integer>)
add_library(math::wide_integer ALIAS wide_integer)
