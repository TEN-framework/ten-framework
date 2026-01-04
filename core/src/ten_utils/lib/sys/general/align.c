//
// Copyright © 2025 Agora
// This file is part of TEN Framework, an open source project.
// Licensed under the Apache License, Version 2.0, with certain conditions.
// Refer to the "LICENSE" file in the root directory for more information.
//
#include "ten_utils/lib/align.h"

/*
TEN_UTILS_API is used for DLL export on Windows(MinGW). Without it,
error "undefined reference" will be raised.

According to GNU11 standard for inline functions, the "extern" keyword
should be used in .c file instead of .h file to prevent multiple definition.
So TEN_UTILS_API, which contains "extern" keyword, should be used here in .c
file.

Why only Windows(MinGW) needs this (not Linux/macOS/MSVC):
1. Linux/macOS: Global symbols declared with "extern" keyword are exported by
default.
2. Windows(MSVC): Each DLL generates and uses its own COMDAT copy of inline
functions, eliminating the need for cross-DLL imports.

Another solution:
MinGW uses GNU11 standard in this project, but we can use
__attribute__((gnu_inline)) in both .c and .h file to force the "inline"
keyword to work in GNU89 standard, which is exactly the opposite way.
("extern" keyword is used in .h file to prevent multiple definition)
And then TEN_UTILS_API can be used in .h file like the other functions.
*/
#if defined(__MINGW32__) || defined(__MINGW64__)
// Utility for aligning addresses.
TEN_UTILS_API inline size_t ten_align_forward(size_t addr, size_t align);
#else
extern inline size_t ten_align_forward(size_t addr, size_t align);
#endif
