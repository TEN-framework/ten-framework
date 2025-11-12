//
// Copyright © 2025 Agora
// This file is part of TEN Framework, an open source project.
// Licensed under the Apache License, Version 2.0, with certain conditions.
// Refer to the "LICENSE" file in the root directory for more information.
//
#include <emmintrin.h>

#include "ten_utils/lib/thread.h"

void ten_thread_pause_cpu() { _mm_pause(); }
