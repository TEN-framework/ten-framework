//
// Copyright © 2025 Agora
// This file is part of TEN Framework, an open source project.
// Licensed under the Apache License, Version 2.0, with certain conditions.
// Refer to the "LICENSE" file in the root directory for more information.
//
#include "ten_utils/lib/task.h"

#include <Windows.h>

ten_pid_t ten_task_get_id() { return (ten_pid_t)GetCurrentProcessId(); }