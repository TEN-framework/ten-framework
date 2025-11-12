//
// Copyright © 2025 Agora
// This file is part of TEN Framework, an open source project.
// Licensed under the Apache License, Version 2.0, with certain conditions.
// Refer to the "LICENSE" file in the root directory for more information.
//
#pragma once

#include "ten_utils/ten_config.h"

/* ELF size: 32 or 64 */
#define BACKTRACE_ELF_SIZE unused

/* Define if dl_iterate_phdr is available. */
/* #undef HAVE_DL_ITERATE_PHDR */

/* Define to 1 if you have the <link.h> header file. */
/* #undef HAVE_LINK_H */

/* Define to 1 if you have the <mach-o/dyld.h> header file. */
#define HAVE_MACH_O_DYLD_H 1

/* Define if -lzstd is available. */
#define HAVE_ZSTD 1

/* Enable large inode numbers on Mac OS X 10.5.  */
#ifndef _DARWIN_USE_64_BIT_INODE
#define _DARWIN_USE_64_BIT_INODE 1
#endif
