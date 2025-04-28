//
// Copyright © 2025 Agora
// This file is part of TEN Framework, an open source project.
// Licensed under the Apache License, Version 2.0, with certain conditions.
// Refer to the "LICENSE" file in the root directory for more information.
//
#pragma once

#include "ten_utils/ten_config.h"

#if INTPTR_MAX == INT64_MAX
// 64-bit
#define BACKTRACE_ELF_SIZE 64
#elif INTPTR_MAX == INT32_MAX
// 32-bit
#define BACKTRACE_ELF_SIZE 32
#else
#error Unknown pointer size or missing size macros!
#endif

/* Define if dl_iterate_phdr is available. */
#define HAVE_DL_ITERATE_PHDR 1

/* Define to 1 if you have the <link.h> header file. */
#define HAVE_LINK_H 1

/* Define to 1 if you have the <mach-o/dyld.h> header file. */
/* #undef HAVE_MACH_O_DYLD_H */

/* Define if -lzstd is available. */
/* #undef HAVE_ZSTD */
