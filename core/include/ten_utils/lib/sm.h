//
// Copyright © 2025 Agora
// This file is part of TEN Framework, an open source project.
// Licensed under the Apache License, Version 2.0, with certain conditions.
// Refer to the "LICENSE" file in the root directory for more information.
//
#pragma once

#include "ten_utils/ten_config.h"

#include <stddef.h>

#if !defined(NDEBUG)
#define TEN_SM_MAX_HISTORY 10
#else
#define TEN_SM_MAX_HISTORY 1
#endif

typedef struct ten_sm_t ten_sm_t;

typedef struct ten_sm_state_history_t {
  int from;
  int event;
  int reason;
  int to;
} ten_sm_state_history_t;

typedef void (*ten_sm_op)(ten_sm_t *sm, const ten_sm_state_history_t *top,
                          void *arg);

#define TEN_REASON_ANY (-1)

typedef struct ten_sm_state_entry_t {
  int current;
  int event;
  int reason;
  int next;
  ten_sm_op operation;
} ten_sm_state_entry_t;

typedef struct ten_sm_auto_trans_t {
  int from_state;
  int to_state;
  int auto_trigger;
  int trigger_reason;
} ten_sm_auto_trans_t;

TEN_UTILS_API ten_sm_t *ten_state_machine_create();

TEN_UTILS_API void ten_state_machine_destroy(ten_sm_t *sm);

TEN_UTILS_API int ten_state_machine_init(ten_sm_t *sm, int begin_state,
                                         ten_sm_op default_op,
                                         const ten_sm_state_entry_t *entries,
                                         size_t entry_count,
                                         const ten_sm_auto_trans_t *trans,
                                         size_t trans_count);

TEN_UTILS_API int ten_state_machine_reset_state(ten_sm_t *sm);

TEN_UTILS_API int ten_state_machine_trigger(ten_sm_t *sm, int event, int reason,
                                            void *arg);

TEN_UTILS_API int ten_state_machine_current_state(const ten_sm_t *sm);
