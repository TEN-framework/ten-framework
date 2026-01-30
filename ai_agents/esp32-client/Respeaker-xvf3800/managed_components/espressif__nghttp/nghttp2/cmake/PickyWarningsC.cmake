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

# C

include(CheckCCompilerFlag)

if(CMAKE_COMPILER_IS_GNUCC OR CMAKE_COMPILER_IS_GNUCXX OR CMAKE_C_COMPILER_ID MATCHES "Clang")

  # https://clang.llvm.org/docs/DiagnosticsReference.html
  # https://gcc.gnu.org/onlinedocs/gcc/Warning-Options.html

  # WPICKY_ENABLE = Options we want to enable as-is.
  # WPICKY_DETECT = Options we want to test first and enable if available.

  # Prefer the -Wextra alias with clang.
  if(CMAKE_C_COMPILER_ID MATCHES "Clang")
    set(WPICKY_ENABLE "-Wextra")
  else()
    set(WPICKY_ENABLE "-W")
  endif()

  list(APPEND WPICKY_ENABLE
    -Wall
  )

  # ----------------------------------
  # Add new options here, if in doubt:
  # ----------------------------------
  set(WPICKY_DETECT
  )

  # Assume these options always exist with both clang and gcc.
  # Require clang 3.0 / gcc 2.95 or later.
  list(APPEND WPICKY_ENABLE
    -Wconversion                         # clang  3.0  gcc  2.95
    -Winline                             # clang  1.0  gcc  1.0
    -Wmissing-declarations               # clang  1.0  gcc  2.7
    -Wmissing-prototypes                 # clang  1.0  gcc  1.0
    -Wnested-externs                     # clang  1.0  gcc  2.7
    -Wpointer-arith                      # clang  1.0  gcc  1.4
    -Wshadow                             # clang  1.0  gcc  2.95
    -Wundef                              # clang  1.0  gcc  2.95
    -Wwrite-strings                      # clang  1.0  gcc  1.4
  )

  # Always enable with clang, version dependent with gcc
  set(WPICKY_COMMON_OLD
    -Waddress                            # clang  3.0  gcc  4.3
    -Wattributes                         # clang  3.0  gcc  4.1
    -Wcast-align                         # clang  1.0  gcc  4.2
    -Wdeclaration-after-statement        # clang  1.0  gcc  3.4
    -Wdiv-by-zero                        # clang  3.0  gcc  4.1
    -Wempty-body                         # clang  3.0  gcc  4.3
    -Wendif-labels                       # clang  1.0  gcc  3.3
    -Wfloat-equal                        # clang  1.0  gcc  2.96 (3.0)
    -Wformat-nonliteral                  # clang  3.0  gcc  4.1
    -Wformat-security                    # clang  3.0  gcc  4.1
    -Wmissing-field-initializers         # clang  3.0  gcc  4.1
    -Wmissing-noreturn                   # clang  3.0  gcc  4.1
    -Wno-format-nonliteral               # clang  1.0  gcc  2.96 (3.0)        # This is required because we pass format string as "const char*"
  # -Wpadded                             # clang  3.0  gcc  4.1               # Not used because we cannot change public structs
    -Wredundant-decls                    # clang  3.0  gcc  4.1
    -Wsign-conversion                    # clang  3.0  gcc  4.3
    -Wstrict-prototypes                  # clang  1.0  gcc  3.3
  # -Wswitch-enum                        # clang  3.0  gcc  4.1               # Not used because this basically disallows default case
    -Wunreachable-code                   # clang  3.0  gcc  4.1
    -Wunused-parameter                   # clang  3.0  gcc  4.1
    -Wvla                                # clang  2.8  gcc  4.3
  )

  set(WPICKY_COMMON
    -Wpragmas                            # clang  3.5  gcc  4.1  appleclang  6.0
  )

  if(CMAKE_C_COMPILER_ID MATCHES "Clang")
    list(APPEND WPICKY_ENABLE
      ${WPICKY_COMMON_OLD}
      -Wshorten-64-to-32                 # clang  1.0
      -Wlanguage-extension-token         # clang  3.0
    )
    # Enable based on compiler version
    if((CMAKE_C_COMPILER_ID STREQUAL "Clang"      AND NOT CMAKE_C_COMPILER_VERSION VERSION_LESS 3.6) OR
       (CMAKE_C_COMPILER_ID STREQUAL "AppleClang" AND NOT CMAKE_C_COMPILER_VERSION VERSION_LESS 6.3))
      list(APPEND WPICKY_ENABLE
        ${WPICKY_COMMON}
        -Wunreachable-code-break         # clang  3.5            appleclang  6.0
        -Wheader-guard                   # clang  3.4            appleclang  5.1
      )
    endif()
    if((CMAKE_C_COMPILER_ID STREQUAL "Clang"      AND NOT CMAKE_C_COMPILER_VERSION VERSION_LESS 3.9) OR
       (CMAKE_C_COMPILER_ID STREQUAL "AppleClang" AND NOT CMAKE_C_COMPILER_VERSION VERSION_LESS 8.3))
      list(APPEND WPICKY_ENABLE
        -Wmissing-variable-declarations  # clang  3.2            appleclang  4.6
      )
    endif()
    if((CMAKE_C_COMPILER_ID STREQUAL "Clang"      AND NOT CMAKE_C_COMPILER_VERSION VERSION_LESS 5.0) OR
       (CMAKE_C_COMPILER_ID STREQUAL "AppleClang" AND NOT CMAKE_C_COMPILER_VERSION VERSION_LESS 9.4))
      list(APPEND WPICKY_ENABLE
      )
    endif()
    if((CMAKE_C_COMPILER_ID STREQUAL "Clang"      AND NOT CMAKE_C_COMPILER_VERSION VERSION_LESS 7.0) OR
       (CMAKE_C_COMPILER_ID STREQUAL "AppleClang" AND NOT CMAKE_C_COMPILER_VERSION VERSION_LESS 10.3))
      list(APPEND WPICKY_ENABLE
      )
    endif()
  else()  # gcc
    list(APPEND WPICKY_DETECT
      ${WPICKY_COMMON}
    )
    # Enable based on compiler version
    if(NOT CMAKE_C_COMPILER_VERSION VERSION_LESS 4.3)
      list(APPEND WPICKY_ENABLE
        ${WPICKY_COMMON_OLD}
        -Wclobbered                      #             gcc  4.3
      )
    endif()
  endif()

  #

  unset(_wpicky)

  foreach(_CCOPT IN LISTS WPICKY_ENABLE)
    set(_wpicky "${_wpicky} ${_CCOPT}")
  endforeach()

  foreach(_CCOPT IN LISTS WPICKY_DETECT)
    # surprisingly, CHECK_C_COMPILER_FLAG needs a new variable to store each new
    # test result in.
    string(MAKE_C_IDENTIFIER "OPT${_CCOPT}" _optvarname)
    # GCC only warns about unknown -Wno- options if there are also other diagnostic messages,
    # so test for the positive form instead
    string(REPLACE "-Wno-" "-W" _CCOPT_ON "${_CCOPT}")
    check_c_compiler_flag(${_CCOPT_ON} ${_optvarname})
    if(${_optvarname})
      set(_wpicky "${_wpicky} ${_CCOPT}")
    endif()
  endforeach()

  set(WARNCFLAGS "${WARNCFLAGS} ${_wpicky}")
endif()
