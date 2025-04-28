cmake_minimum_required(VERSION 3.1)
project(LIB_POTASSCO VERSION 1.0 LANGUAGES CXX)
if (POLICY CMP0063)
	cmake_policy(SET CMP0063 NEW)
endif()
# enable folders in IDEs like Visual Studio
set_property(GLOBAL PROPERTY USE_FOLDERS ON)
include(GNUInstallDirs)
option(LIB_POTASSCO_BUILD_TESTS       "whether or not to build tests"             OFF)
option(LIB_POTASSCO_BUILD_APP         "whether or not to build lpconvert tool"    ON)
option(LIB_POTASSCO_INSTALL_LIB       "whether or not to install libpotassco"     OFF)
option(LIB_POTASSCO_INSTALL_VERSIONED "whether to use a versioned install layout" OFF)

if (LIB_POTASSCO_INSTALL_VERSIONED)
	set(include_dest  "potassco-${LIB_POTASSCO_VERSION}")
	set(library_dest  "potassco-${LIB_POTASSCO_VERSION}")
	set(cmake_dest    "potassco-${LIB_POTASSCO_VERSION}/cmake")
else()
	set(include_dest  ".")
	set(library_dest  ".")
	set(cmake_dest    "cmake/Potassco")
endif()

if (LIB_POTASSCO_INSTALL_LIB AND NOT CMAKE_INSTALL_LIBDIR)
	message(STATUS "LIBDIR no set - using lib")
	set(CMAKE_INSTALL_LIBDIR lib)
endif()

if (NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
	message(STATUS "No build type selected - using 'Release'")
	set(CMAKE_BUILD_TYPE "Release")
endif()

if (NOT MSVC)
	if (NOT CMAKE_ARCHIVE_OUTPUT_DIRECTORY)
		set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)
	endif()
	if (NOT CMAKE_LIBRARY_OUTPUT_DIRECTORY)
		set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)
	endif()
	if (NOT CMAKE_RUNTIME_OUTPUT_DIRECTORY)
		set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)
	endif()
else()
    set(VC_RELEASE_LINK_OPTIONS /LTCG)
	SET(CMAKE_EXE_LINKER_FLAGS_RELEASE    "${CMAKE_EXE_LINKER_FLAGS_RELEASE}    ${VC_RELEASE_LINK_OPTIONS}")
	SET(CMAKE_MODULE_LINKER_FLAGS_RELEASE "${CMAKE_MODULE_LINKER_FLAGS_RELEASE} ${VC_RELEASE_LINK_OPTIONS}")
	SET(CMAKE_SHARED_LINKER_FLAGS_RELEASE "${CMAKE_SHARED_LINKER_FLAGS_RELEASE} ${VC_RELEASE_LINK_OPTIONS}")
	SET(CMAKE_STATIC_LINKER_FLAGS_RELEASE "${CMAKE_STATIC_LINKER_FLAGS_RELEASE} ${VC_RELEASE_LINK_OPTIONS}")
endif()

add_subdirectory(src)
if(LIB_POTASSCO_BUILD_TESTS)
	enable_testing()
	add_subdirectory(tests)
endif()
if(LIB_POTASSCO_BUILD_APP)
	add_subdirectory(app)
endif()

# optional doc target
find_package(Doxygen)
if(DOXYGEN_FOUND)
	set(doxyfile "${CMAKE_CURRENT_SOURCE_DIR}/doc/Doxyfile")
	add_custom_target(doc_potassco
		COMMAND ${DOXYGEN_EXECUTABLE} ${doxyfile}
		WORKING_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}/doc"
		COMMENT "Generating documentation..."
		VERBATIM)
	set_target_properties(doc_potassco PROPERTIES FOLDER doc)
endif()

# export
if(LIB_POTASSCO_INSTALL_LIB)
	include(CMakePackageConfigHelpers)
	configure_package_config_file(
		${PROJECT_SOURCE_DIR}/cmake/PotasscoConfig.cmake.in
		${CMAKE_CURRENT_BINARY_DIR}/PotasscoConfig.cmake
		INSTALL_DESTINATION ${CMAKE_INSTALL_LIBDIR}/${cmake_dest})
	write_basic_package_version_file(
		${CMAKE_CURRENT_BINARY_DIR}/PotasscoConfigVersion.cmake
		COMPATIBILITY SameMajorVersion)
	install(FILES
		${CMAKE_CURRENT_BINARY_DIR}/PotasscoConfig.cmake
		${CMAKE_CURRENT_BINARY_DIR}/PotasscoConfigVersion.cmake
		DESTINATION ${CMAKE_INSTALL_LIBDIR}/${cmake_dest})
	install(EXPORT PotasscoTargets DESTINATION "${CMAKE_INSTALL_LIBDIR}/${cmake_dest}")
endif()
