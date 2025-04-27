//
// Copyright © 2025 Agora
// This file is part of TEN Framework, an open source project.
// Licensed under the Apache License, Version 2.0, with certain conditions.
// Refer to the "LICENSE" file in the root directory for more information.
//
// This file is modified from
// https://github.com/ianlancetaylor/libbacktrace [BSD license]
//
#pragma once

#include "ten_utils/ten_config.h"

TEN_UTILS_PRIVATE_API void elf_uncompress_failed(void);

TEN_UTILS_PRIVATE_API int elf_fetch_bits(const unsigned char **ppin,
                                         const unsigned char *pinend,
                                         uint64_t *pval, unsigned int *pbits);
