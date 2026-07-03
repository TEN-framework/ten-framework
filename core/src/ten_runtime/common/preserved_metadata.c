//
// Copyright © 2025 Agora
// This file is part of TEN Framework, an open source project.
// Licensed under the Apache License, Version 2.0, with certain conditions.
// Refer to the "LICENSE" file in the root directory for more information.
//
#include "include_internal/ten_runtime/common/preserved_metadata.h"

#include "include_internal/ten_runtime/common/version.h"

static char metadata[] = "version=" TEN_RUNTIME_VERSION;

void ten_preserved_metadata(void) {
  ((char volatile *)metadata)[0] = metadata[0];
}
