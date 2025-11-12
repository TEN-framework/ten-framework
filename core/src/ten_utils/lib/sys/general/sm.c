//
// Copyright © 2025 Agora
// This file is part of TEN Framework, an open source project.
// Licensed under the Apache License, Version 2.0, with certain conditions.
// Refer to the "LICENSE" file in the root directory for more information.
//
#include "ten_utils/lib/sm.h"

#include <string.h>

#include "ten_utils/container/list.h"
#include "ten_utils/container/list_node.h"
#include "ten_utils/lib/alloc.h"
#include "ten_utils/lib/mutex.h"

struct ten_sm_t {
  ten_mutex_t *lock;
  ten_sm_state_entry_t *entries;
  size_t entry_count;
  ten_sm_auto_trans_t *auto_trans;
  size_t trans_count;
  int current_state;
  int begin_state;
  ten_sm_op default_op;
  ten_list_t history;
  int initted;
};

ten_sm_t *ten_state_machine_create() {
  ten_sm_t *sm = NULL;

  sm = ten_malloc(sizeof(*sm));
  if (!sm) {
    goto error;
  }

  memset(sm, 0, sizeof(*sm));
  sm->lock = ten_mutex_create();
  if (!sm->lock) {
    goto error;
  }

  ten_list_init(&sm->history);

  return sm;

error:
  ten_state_machine_destroy(sm);

  return NULL;
}

void ten_state_machine_destroy(ten_sm_t *sm) {
  if (!sm) {
    return;
  }

  if (sm->entries && sm->entry_count) {
    ten_free(sm->entries);
  }

  if (sm->auto_trans && sm->trans_count) {
    ten_free(sm->auto_trans);
  }

  ten_list_clear(&sm->history);

  if (sm->lock) {
    ten_mutex_destroy(sm->lock);
  }

  ten_free(sm);
}

int ten_state_machine_init(ten_sm_t *sm, int begin_state, ten_sm_op default_op,
                           const ten_sm_state_entry_t *entries,
                           size_t entry_count, const ten_sm_auto_trans_t *trans,
                           size_t trans_count) {
  int index = 0;
  ten_sm_state_entry_t *entry_clone = NULL;
  ten_sm_auto_trans_t *trans_clone = NULL;

  if (!sm || !entries || !entry_count || !sm->lock) {
    goto error;
  }

  for (index = 0; index < entry_count; index++) {
    if (entries[index].current == begin_state) {
      break;
    }
  }

  if (index == entry_count) {
    goto error;
  }

  entry_clone = ten_malloc(entry_count * sizeof(ten_sm_state_entry_t));
  if (!entry_clone) {
    goto error;
  }

  memcpy(entry_clone, entries, entry_count * sizeof(ten_sm_state_entry_t));

  if (trans && trans_count) {
    trans_clone = ten_malloc(trans_count * sizeof(ten_sm_auto_trans_t));
    if (!trans_clone) {
      goto error;
    }

    memcpy(trans_clone, trans, trans_count * sizeof(ten_sm_auto_trans_t));
  }

  ten_mutex_lock(sm->lock);
  if (sm->entries || sm->entry_count || sm->initted) {
    goto leave_and_error;
  }

  sm->entries = entry_clone;
  sm->entry_count = entry_count;
  sm->auto_trans = trans_clone;
  sm->trans_count = trans_count;
  sm->current_state = begin_state;
  sm->begin_state = begin_state;
  sm->default_op = default_op;
  sm->initted = 1;
  ten_mutex_unlock(sm->lock);
  return 0;

leave_and_error:
  ten_mutex_unlock(sm->lock);
error:
  if (entry_clone) {
    ten_free(entry_clone);
  }

  if (trans_clone) {
    ten_free(trans_clone);
  }

  return -1;
}

int ten_state_machine_reset_state(ten_sm_t *sm) {
  if (!sm) {
    goto error;
  }

  ten_mutex_lock(sm->lock);
  if (!sm->entries || !sm->entry_count || !sm->initted) {
    goto leave_and_error;
  }

  sm->current_state = sm->begin_state;
  ten_mutex_unlock(sm->lock);
  return 0;

leave_and_error:
  ten_mutex_unlock(sm->lock);
error:
  return -1;
}

int ten_state_machine_trigger(ten_sm_t *sm, int event, int reason, void *arg) {
  ten_sm_op op = NULL;
  ten_sm_state_entry_t *entry = NULL;
  ten_sm_state_history_t *new_story = NULL;
  int origin_state = -1;
  int next_state = -1;
  int index = 0;

  if (!sm) {
    goto error;
  }

  new_story = ten_malloc(sizeof(*new_story));
  if (!new_story) {
    goto error;
  }

  ten_mutex_lock(sm->lock);
  if (!sm->entries || !sm->entry_count || !sm->initted) {
    goto leave_and_error;
  }

  for (index = 0; index < sm->entry_count; index++) {
    ten_sm_state_entry_t *e = &sm->entries[index];
    if (e->current == sm->current_state && e->event == event &&
        (e->reason == reason || e->reason == TEN_REASON_ANY)) {
      entry = e;
      break;
    }
  }

  if (entry) {
    next_state = entry->next;
    op = entry->operation;
  } else {
    next_state = sm->current_state;
    op = sm->default_op;
  }

  origin_state = sm->current_state;
  sm->current_state = next_state;
  ten_mutex_unlock(sm->lock);

  // perform action _without_ lock
  if (op) {
    new_story->event = event;
    new_story->from = origin_state;
    new_story->to = next_state;
    new_story->reason = reason;

    if (ten_list_size(&sm->history) >= TEN_SM_MAX_HISTORY) {
      ten_listnode_destroy(ten_list_pop_front(&sm->history));
    }

    ten_list_push_ptr_back(&sm->history, new_story, ten_free);
    op(sm, new_story, arg);
  }

  ten_mutex_lock(sm->lock);

  if (sm->trans_count && sm->auto_trans) {
    for (index = 0; index < sm->trans_count; index++) {
      ten_sm_auto_trans_t *trans = &sm->auto_trans[index];
      int auto_event = trans->auto_trigger;
      int auto_reason = trans->trigger_reason;
      if (trans->from_state != origin_state || trans->to_state != next_state) {
        continue;
      }

      ten_mutex_unlock(sm->lock);
      ten_state_machine_trigger(sm, auto_event, auto_reason, arg);
      ten_mutex_lock(sm->lock);
    }
  }

  ten_mutex_unlock(sm->lock);

  return 0;

leave_and_error:
  ten_mutex_unlock(sm->lock);

error:
  if (new_story) {
    ten_free(new_story);
  }

  return -1;
}

int ten_state_machine_current_state(const ten_sm_t *sm) {
  int state = -1;
  ten_mutex_lock(sm->lock);
  state = sm->current_state;
  ten_mutex_unlock(sm->lock);
  return state;
}