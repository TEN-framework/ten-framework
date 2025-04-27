cmake_minimum_required(VERSION 3.1)

file(READ "libclingo/clingo.h" clingoh)
string(REGEX MATCH "#define CLINGO_VERSION \"([^\"]*)\"" clingov ${clingoh})

project(CLINGO VERSION "${CMAKE_MATCH_1}" LANGUAGES C CXX)
if (POLICY CMP0063)
    cmake_policy(SET CMP0063 NEW)
endif()
if (NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
    message(STATUS "No build type selected - using 'Release'")
    set(CMAKE_BUILD_TYPE "Release")
endif()

include(CheckCXXSourceCompiles)
include(GNUInstallDirs)
include(CMakeDependentOption)
list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/cmake")
include(clingo_functions)

# Enable folders in IDEs like Visual Studio
set_property(GLOBAL PROPERTY USE_FOLDERS ON)

set(CMAKE_CXX_STANDARD 14)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# build targets
option(CLINGO_BUILD_WEB
"Enable the web target. This option is meant to be used with emscripten to \
compile a library exporting just one function to run clingo."
OFF)
option(CLINGO_BUILD_TESTS
"Enable unit and system tests."
OFF)
option(CLINGO_BUILD_EXAMPLES
"Build clingo C and C++ examples."
OFF)
option(CLINGO_BUILD_APPS
"Build applications including gringo, clingo, reify, clasp, and lpconvert."
ON)
set(CLINGO_CLINGOPATH "" CACHE STRING
"Set inbuilt global search directories for clingo's include statement.")
set(CLINGO_BUILD_REVISION "" CACHE STRING
"revision string to add to version information")

# build fine-tuning
option(CLINGO_MANAGE_RPATH
"Set RPATH if not installed into system directory on *NIX systems."
ON)
option(CLINGO_BUILD_STATIC
"Do not build any shared libraries and do not compile position independent \
code."
OFF)
CMAKE_DEPENDENT_OPTION(CLINGO_BUILD_SHARED
"Build clingo library shared."
ON "NOT CLINGO_BUILD_STATIC" OFF)
set(CLINGO_USE_LIB OFF CACHE BOOL
"Advanced option to build Python and Lua modules against an existing \
libclingo.")
option(CLINGO_INSTALL_LIB
"Advanced option to force installation of static libraries."
OFF)
option(CLINGO_USE_LOCAL_CLASP
"Advanced option to build against bundled or installed clasp."
ON)
option(CLINGO_USE_LOCAL_CATCH
"Advanced option to build against bundled or installed catch."
ON)
set(CLINGO_MAP_TYPE "hopscotch" CACHE STRING
"Select hash table implementation. (sparse, hopscotch)")


# Python configuration
set(CLINGO_BUILD_WITH_PYTHON "auto" CACHE STRING
"Whether to enable Python support. Can be set to \"ON\" (to enable Python \
support), \"OFF\" (to disable Python support), \"auto\" (to enable Python \
support if available), or \"pip\" (advanced configuration to build a Python \
module exporting clingo symbols).")
set(CLINGO_PYTHON_VERSION "3.6" CACHE STRING
"Set this to find a specific Python version. This can for example be set with \
`-DCLINGO_PYTHON_VERSION:LIST=\"3.6;EXACT\"` to require a specific version." )
set(PYCLINGO_INSTALL "prefix" CACHE STRING
"Configure where to install the Python module. Can be set to \"user\" (to \
install in the user prefix), \"system\" (to install in the system \
\"prefix\"), or \"prefix\" (to install into the installation prefix).")
set(PYCLINGO_INSTALL_DIR "" CACHE STRING
"Advanced variable to manually configure where to install the Python module.")
set(PYCLINGO_SUFFIX CACHE STRING
"Advanced variable to manually configure the suffix of the Python module.")
option(PYCLINGO_DYNAMIC_LOOKUP
"Pass linker option `-undefined link_dynamic`."
${APPLE})

# Lua configuration
set(CLINGO_BUILD_WITH_LUA "auto" CACHE STRING
"Whether to enable Lua support. Can be set to 'ON' (to enable Lua support), \
'OFF' (to disable Lua support), or 'auto' (to enable Python support if \
available).")
set(CLINGO_LUA_VERSION "5.0" CACHE STRING
"Set this to find a specific Lua version. This can for example be set with \
`-DCLINGO_LUA_VERSION:LIST=\"5.3;EXACT\"` to require a specific version." )
set(LUACLINGO_INSTALL_DIR "" CACHE STRING
"Advanced variable to manually configure where to install the Lua module.")
set(LUACLINGO_SUFFIX CACHE STRING
"Advanced variable to manually configure the suffix of the Lua module.")

mark_as_advanced(CLINGO_BUILD_WEB)
mark_as_advanced(CLINGO_BUILD_STATIC)
mark_as_advanced(CLINGO_BUILD_SHARED)
mark_as_advanced(CLINGO_USE_LIB)
mark_as_advanced(CLINGO_INSTALL_LIB)
mark_as_advanced(PYCLINGO_INSTALL_DIR)
mark_as_advanced(PYCLINGO_SUFFIX)
mark_as_advanced(PYCLINGO_DYNAMIC_LOOKUP)
mark_as_advanced(LUACLINGO_INSTALL_DIR)
mark_as_advanced(LUACLINGO_SUFFIX)

# workaround to set custom ar and ranlib
if (CLINGO_CMAKE_AR)
    set(CMAKE_AR "${CLINGO_CMAKE_AR}")
    set(CMAKE_CXX_ARCHIVE_CREATE "<CMAKE_AR> qc <TARGET> <LINK_FLAGS> <OBJECTS>")
    set(CMAKE_C_ARCHIVE_CREATE "<CMAKE_AR> qc <TARGET> <LINK_FLAGS> <OBJECTS>")
endif()
if (CLINGO_CMAKE_RANLIB)
    set(CMAKE_RANLIB "${CLINGO_CMAKE_RANLIB}")
    set(CMAKE_CXX_ARCHIVE_FINISH "<CMAKE_RANLIB> <TARGET>")
    set(CMAKE_C_ARCHIVE_FINISH "<CMAKE_RANLIB> <TARGET>")
endif()

if (CLINGO_MANAGE_RPATH)
    set(CMAKE_SKIP_BUILD_RPATH FALSE)
    set(CMAKE_BUILD_WITH_INSTALL_RPATH FALSE)
    set(CMAKE_INSTALL_RPATH_USE_LINK_PATH TRUE)
    list(FIND CMAKE_PLATFORM_IMPLICIT_LINK_DIRECTORIES "${CMAKE_INSTALL_FULL_LIBDIR}" isSystemDir)
    if ("${isSystemDir}" STREQUAL "-1")
        set(CMAKE_INSTALL_RPATH "${CMAKE_INSTALL_FULL_LIBDIR}")
    endif()
endif()

if (CLINGO_BUILD_WEB)
    unset(CMAKE_C_USE_RESPONSE_FILE_FOR_LIBRARIES)
    unset(CMAKE_CXX_USE_RESPONSE_FILE_FOR_LIBRARIES)
    unset(CMAKE_C_USE_RESPONSE_FILE_FOR_OBJECTS)
    unset(CMAKE_CXX_USE_RESPONSE_FILE_FOR_OBJECTS)
    unset(CMAKE_C_USE_RESPONSE_FILE_FOR_INCLUDES)
    unset(CMAKE_CXX_USE_RESPONSE_FILE_FOR_INCLUDES)
endif()

if (NOT CMAKE_ARCHIVE_OUTPUT_DIRECTORY)
    set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)
endif()
if (NOT CMAKE_LIBRARY_OUTPUT_DIRECTORY)
    set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)
endif()
if (NOT CMAKE_RUNTIME_OUTPUT_DIRECTORY)
    set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)
endif()
if (NOT CMAKE_IMPORT_LIBRARY_PREFIX)
    set(CMAKE_IMPORT_LIBRARY_PREFIX import_)
endif()

# NOTE: searching for the interpreter first increases the chance
#       that searching for the python libraries finds the matching libraries for the default python interpreter
#       python is also used to run the tests

if(NOT CLINGO_BUILD_WITH_PYTHON STREQUAL "pip" AND (CMAKE_VERSION VERSION_GREATER "3.15.0" OR CMAKE_VERSION VERSION_EQUAL "3.15.0"))
    if(DEFINED PYTHON_EXECUTABLE AND NOT DEFINED Python_EXECUTABLE)
        set(Python_EXECUTABLE "${PYTHON_EXECUTABLE}")
    endif()
    if(DEFINED PYTHON_INCLUDE_DIR AND NOT DEFINED Python_INCLUDE_DIR)
        set(Python_INCLUDE_DIR "${PYTHON_INCLUDE_DIR}")
    endif()
    if(DEFINED PYTHON_LIBRARY AND NOT DEFINED Python_LIBRARY)
        set(Python_LIBRARY "${PYTHON_LIBRARY}")
    endif()
    if (POLICY CMP0094)
        cmake_policy(SET CMP0094 NEW)
    endif()
    set(_args Interpreter)
    if (CLINGO_BUILD_WITH_PYTHON)
        set(_args ${_args} Development)
        if (NOT CLINGO_BUILD_WITH_PYTHON STREQUAL "auto")
            set(_args ${_args} REQUIRED)
        endif()
    endif()
    find_package(Python ${CLINGO_PYTHON_VERSION} COMPONENTS ${_args})
else()
    if (CLINGO_BUILD_WITH_PYTHON STREQUAL "pip")
        find_package(PythonExtensions REQUIRED)
    else()
        find_package(PythonInterp)
    endif()
    set(Python_EXECUTABLE "${PYTHON_EXECUTABLE}")
    set(Python_VERSION_MAJOR "${PYTHON_VERSION_MAJOR}")
    if (CLINGO_BUILD_WITH_PYTHON)
        if (CLINGO_BUILD_WITH_PYTHON STREQUAL "pip")
            # libraries already found
        elseif (NOT CLINGO_BUILD_WITH_PYTHON STREQUAL "auto")
            find_package(PythonLibs ${CLINGO_PYTHON_VERSION} REQUIRED)
        else()
            find_package(PythonLibs ${CLINGO_PYTHON_VERSION})
        endif()
        if(PYTHONLIBS_FOUND)
            add_library(Python::Python INTERFACE IMPORTED)
            add_library(Python::Module INTERFACE IMPORTED)
            set(Python_Development_FOUND ON)
            set(Python_INCLUDE_DIRS "${PYTHON_INCLUDE_DIRS}")
            set(Python_VERSION "${PYTHONLIBS_VERSION_STRING}")
            set_property(TARGET Python::Python PROPERTY INTERFACE_INCLUDE_DIRECTORIES "${PYTHON_INCLUDE_DIRS}")
            set_property(TARGET Python::Module PROPERTY INTERFACE_INCLUDE_DIRECTORIES "${PYTHON_INCLUDE_DIRS}")
            set_property(TARGET Python::Python PROPERTY INTERFACE_LINK_LIBRARIES "${PYTHON_LIBRARIES}")
            if (MSVC)
                set_property(TARGET Python::Module PROPERTY INTERFACE_LINK_LIBRARIES "${PYTHON_LIBRARIES}")
            endif()
        endif()
    endif()
endif()
if (Python_Development_FOUND AND Python_VERSION_MAJOR LESS 3)
    message(FATAL_ERROR "Clingo does not support Python 2.")
endif()
if (CLINGO_BUILD_WITH_LUA)
    if (NOT CLINGO_BUILD_WITH_LUA STREQUAL "auto")
        find_package(Lua ${CLINGO_LUA_VERSION} REQUIRED)
    else()
        find_package(Lua ${CLINGO_LUA_VERSION})
    endif()
    if(LUA_FOUND)
        add_library(Lua::Lua INTERFACE IMPORTED)
        set_property(TARGET Lua::Lua PROPERTY INTERFACE_LINK_LIBRARIES "${LUA_LIBRARIES}")
        set_property(TARGET Lua::Lua PROPERTY INTERFACE_INCLUDE_DIRECTORIES "${LUA_INCLUDE_DIR}")
    endif()
endif()
find_package(BISON "2.5")
find_package(RE2C "0.101")
if (Python_Development_FOUND)
    # When using CFFI the clingo module has to be compiled with -pthread to
    # avoid linker errors
    set(THREADS_PREFER_PTHREAD_FLAG ON)
    find_package(Threads REQUIRED)
endif()


if (POLICY CMP0063 AND (CLINGO_BUILD_SHARED OR Python_Development_FOUND OR LUA_FOUND))
    set(CMAKE_CXX_VISIBILITY_PRESET hidden)
    set(CMAKE_C_VISIBILITY_PRESET hidden)
endif()

# Test support for -Wl,--version-script as a linker flag
if (CLINGO_BUILD_SHARED AND UNIX AND NOT APPLE)
    file(WRITE "${CMAKE_CURRENT_BINARY_DIR}/version.map" "{global: f; local: *;};")
    set(CMAKE_REQUIRED_FLAGS_SAVE ${CMAKE_REQUIRED_FLAGS})
    set(CMAKE_REQUIRED_FLAGS ${CMAKE_REQUIRED_FLAGS} "-Wl,--version-script='${CMAKE_CURRENT_BINARY_DIR}/version.map'")
    check_cxx_source_compiles("void f(){} int main(){}" CLINGO_USE_VERSION_SCRIPT)
    set(CMAKE_REQUIRED_FLAGS ${CMAKE_REQUIRED_FLAGS_SAVE})
    file(REMOVE "${CMAKE_CURRENT_BINARY_DIR}/version.map")
endif()

enable_testing()

if (CLINGO_USE_LIB)
    set(clingo_library_targets)
    add_library(libclingo INTERFACE IMPORTED)
    set_property(TARGET libclingo PROPERTY INTERFACE_INCLUDE_DIRECTORIES "${CLINGO_SOURCE_DIR}/libclingo")
    set_property(TARGET libclingo PROPERTY INTERFACE_LINK_LIBRARIES "clingo")
else()
    add_subdirectory(third_party)
    set(clingo_library_targets libgringo libpotassco libclasp libreify)
    if (CLINGO_USE_LOCAL_CLASP)
        # NOTE: assumes that submodule has been initialized
        set(CLASP_BUILD_APP ${CLINGO_BUILD_APPS} CACHE BOOL "")
        set(CLASP_USE_LOCAL_LIB_POTASSCO ON CACHE BOOL "" FORCE)
        if (NOT CLINGO_BUILD_SHARED AND CLINGO_INSTALL_LIB)
            set(CLASP_INSTALL_LIB ${CLINGO_INSTALL_LIB} CACHE BOOL "" FORCE)
        endif()
        add_subdirectory(clasp)
    else()
        find_package(Clasp REQUIRED)
    endif()
    add_subdirectory(libreify)
    add_subdirectory(libgringo)
    add_subdirectory(libclingo)
endif()
if (Python_Development_FOUND)
    add_subdirectory(libpyclingo)
    list(APPEND clingo_library_targets libpyclingo)
else()
    add_library(libpyclingo INTERFACE IMPORTED)
    add_library(libpyclingom INTERFACE IMPORTED)
endif()
if (LUA_FOUND)
    add_subdirectory(libluaclingo)
    list(APPEND clingo_library_targets libluaclingo)
else()
    add_library(libluaclingo INTERFACE IMPORTED)
endif()
if (CLINGO_BUILD_APPS AND NOT CLINGO_USE_LIB)
    add_subdirectory(app/reify)
    add_subdirectory(app/gringo)
    add_subdirectory(app/clingo)
endif()
if (CLINGO_BUILD_EXAMPLES)
    add_subdirectory(examples/c)
    add_subdirectory(examples/cc)
endif()
if (NOT CLINGO_BUILD_STATIC AND Python_Development_FOUND)
    add_subdirectory(app/pyclingo)
endif()
if (NOT CLINGO_BUILD_STATIC AND LUA_FOUND)
    add_subdirectory(app/luaclingo)
endif()
if (CLINGO_BUILD_WEB)
    add_subdirectory(app/web)
endif()
if (NOT CLINGO_BUILD_STATIC AND (CLINGO_BUILD_SHARED OR Python_Development_FOUND OR LUA_FOUND))
    foreach(target ${clingo_library_targets})
        set_target_properties(${target} PROPERTIES POSITION_INDEPENDENT_CODE ON)
    endforeach()
endif()

