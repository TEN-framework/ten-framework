# nghttp2
#
# Copyright (c) 2023 nghttp2 contributors
#
# Permission is hereby granted, free of charge, to any person obtaining
# a copy of this software and associated documentation files (the
# "Software"), to deal in the Software without restriction, including
# without limitation the rights to use, copy, modify, merge, publish,
# distribute, sublicense, and/or sell copies of the Software, and to
# permit persons to whom the Software is furnished to do so, subject to
# the following conditions:
#
# The above copyright notice and this permission notice shall be
# included in all copies or substantial portions of the Software.
#
# THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
# EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
# MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND
# NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE
# LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION
# OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION
# WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.

# C++

include(CheckCXXCompilerFlag)

if(CMAKE_COMPILER_IS_GNUCC OR CMAKE_COMPILER_IS_GNUCXX OR CMAKE_CXX_COMPILER_ID MATCHES "Clang")

  # https://clang.llvm.org/docs/DiagnosticsReference.html
  # https://gcc.gnu.org/onlinedocs/gcc/Warning-Options.html

  # WPICKY_ENABLE = Options we want to enable as-is.
  # WPICKY_DETECT = Options we want to test first and enable if available.

  set(WPICKY_ENABLE "-Wall")

  # ----------------------------------
  # Add new options here, if in doubt:
  # ----------------------------------
  set(WPICKY_DETECT
  )

  # Assume these options always exist with both clang and gcc.
  # Require clang 3.0 / gcc 2.95 or later.
  list(APPEND WPICKY_ENABLE
  )

  # Always enable with clang, version dependent with gcc
  set(WPICKY_COMMON_OLD
    -Wformat-security                    # clang  3.0  gcc  4.1
  )

  set(WPICKY_COMMON
  )

  if(CMAKE_CXX_COMPILER_ID MATCHES "Clang")
    list(APPEND WPICKY_ENABLE
      ${WPICKY_COMMON_OLD}
    )
    list(APPEND WPICKY_ENABLE
      # clang++-18 warns this when building with wolfSSL >= v5.7.6-stable.
      -Wno-extern-c-compat
    )
    # Enable based on compiler version
    if((CMAKE_CXX_COMPILER_ID STREQUAL "Clang"      AND NOT CMAKE_CXX_COMPILER_VERSION VERSION_LESS 3.6) OR
       (CMAKE_CXX_COMPILER_ID STREQUAL "AppleClang" AND NOT CMAKE_CXX_COMPILER_VERSION VERSION_LESS 6.3))
      list(APPEND WPICKY_ENABLE
        ${WPICKY_COMMON}
      )
    endif()
    if((CMAKE_CXX_COMPILER_ID STREQUAL "Clang"      AND NOT CMAKE_CXX_COMPILER_VERSION VERSION_LESS 3.9) OR
       (CMAKE_CXX_COMPILER_ID STREQUAL "AppleClang" AND NOT CMAKE_CXX_COMPILER_VERSION VERSION_LESS 8.3))
      list(APPEND WPICKY_ENABLE
      )
    endif()
    if((CMAKE_CXX_COMPILER_ID STREQUAL "Clang"      AND NOT CMAKE_CXX_COMPILER_VERSION VERSION_LESS 5.0) OR
       (CMAKE_CXX_COMPILER_ID STREQUAL "AppleClang" AND NOT CMAKE_CXX_COMPILER_VERSION VERSION_LESS 9.4))
      list(APPEND WPICKY_ENABLE
      )
    endif()
    if((CMAKE_CXX_COMPILER_ID STREQUAL "Clang"      AND NOT CMAKE_CXX_COMPILER_VERSION VERSION_LESS 7.0) OR
       (CMAKE_CXX_COMPILER_ID STREQUAL "AppleClang" AND NOT CMAKE_CXX_COMPILER_VERSION VERSION_LESS 10.3))
      list(APPEND WPICKY_ENABLE
      )
    endif()
  else()  # gcc
    list(APPEND WPICKY_DETECT
      ${WPICKY_COMMON}
    )
    # Enable based on compiler version
    if(NOT CMAKE_CXX_COMPILER_VERSION VERSION_LESS 4.3)
      list(APPEND WPICKY_ENABLE
        ${WPICKY_COMMON_OLD}
      )
    endif()
  endif()

  #

  unset(_wpicky)

  foreach(_CCOPT IN LISTS WPICKY_ENABLE)
    set(_wpicky "${_wpicky} ${_CCOPT}")
  endforeach()

  foreach(_CCOPT IN LISTS WPICKY_DETECT)
    # surprisingly, CHECK_CXX_COMPILER_FLAG needs a new variable to store each new
    # test result in.
    string(MAKE_C_IDENTIFIER "OPT${_CCOPT}" _optvarname)
    # GCC only warns about unknown -Wno- options if there are also other diagnostic messages,
    # so test for the positive form instead
    string(REPLACE "-Wno-" "-W" _CCOPT_ON "${_CCOPT}")
    check_cxx_compiler_flag(${_CCOPT_ON} ${_optvarname})
    if(${_optvarname})
      set(_wpicky "${_wpicky} ${_CCOPT}")
    endif()
  endforeach()

  set(WARNCXXFLAGS "${WARNCXXFLAGS} ${_wpicky}")
endif()
