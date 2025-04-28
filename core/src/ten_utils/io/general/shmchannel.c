//
// Copyright © 2025 Agora
// This file is part of TEN Framework, an open source project.
// Licensed under the Apache License, Version 2.0, with certain conditions.
// Refer to the "LICENSE" file in the root directory for more information.
//
#include "ten_utils/io/shmchannel.h"

#include <stdlib.h>
#include <string.h>

#include "ten_utils/lib/shared_event.h"
#include "ten_utils/lib/shm.h"
#include "ten_utils/lib/spinlock.h"
#include "ten_utils/lib/string.h"
#include "ten_utils/macro/mark.h"
#include "ten_utils/macro/memory.h"

#if defined(_WIN32)
#define TEN_ANYSIZE_ARRAY 1
#else
#define TEN_ANYSIZE_ARRAY 0
#endif

typedef struct ten_shm_layout_t {
  ten_atomic_t id;
  ten_atomic_t ref_count;
  ten_atomic_t read_index;
  ten_atomic_t write_index;
  ten_atomic_t channel_lock;
  struct {
    uint32_t sig;
    uint32_t dummy;
    ten_atomic_t lock;
  } reader_active, writer_active, not_full, not_empty;

  uint8_t data[TEN_ANYSIZE_ARRAY];
} ten_shm_layout_t;

typedef struct ten_shm_channel_t {
  ten_shm_layout_t *region;
  ten_string_t name;
  ten_atomic_t active;
  int as_reader;
  ten_spinlock_t *channel_lock;
  ten_shared_event_t *reader_active;
  ten_shared_event_t *writer_active;
  ten_shared_event_t *not_full;
  ten_shared_event_t *not_empty;
  ten_runloop_async_t *read_sig;
  ten_runloop_async_t *write_sig;
} ten_shm_channel_t;

#define TEN_SHM_MEM_SIZE (1 * 1024 * 1024)
#define TEN_SHM_CHANNEL_SIZE (TEN_SHM_MEM_SIZE - sizeof(ten_shm_layout_t))
#define TEN_SHM_NAME_FORMAT "%s_%d"

int ten_shm_channel_create(const char *name, ten_shm_channel_t *channel[2]) {
  if (UNLIKELY(!name || !*name || !channel)) {
    return -1;
  }

  for (int i = 0; i < 2; i++) {
    channel[i] = (ten_shm_channel_t *)TEN_MALLOC(sizeof(ten_shm_channel_t));
    TEN_ASSERT(channel[i], "Failed to allocate memory.");
    memset(channel[i], 0, sizeof(ten_shm_channel_t));

    ten_string_init_formatted(&channel[i]->name, TEN_SHM_NAME_FORMAT, name, i);

    channel[i]->region =
        (ten_shm_layout_t *)ten_shm_map(channel[i]->name.buf, TEN_SHM_MEM_SIZE);
    TEN_ASSERT(channel[i]->region, "Failed to map shared memory.");
    ten_atomic_store(&channel[i]->region->id, i);

    ten_atomic_add_fetch(&channel[i]->region->ref_count, 1);

    channel[i]->channel_lock =
        ten_spinlock_from_addr(&channel[i]->region->channel_lock);
    TEN_ASSERT(channel[i]->channel_lock, "Failed to create spinlock.");

    channel[i]->reader_active =
        ten_shared_event_create(&channel[i]->region->reader_active.sig,
                                &channel[i]->region->reader_active.lock, 0, 0);
    TEN_ASSERT(channel[i]->reader_active, "Failed to create shared event.");

    channel[i]->writer_active =
        ten_shared_event_create(&channel[i]->region->writer_active.sig,
                                &channel[i]->region->writer_active.lock, 0, 0);
    TEN_ASSERT(channel[i]->writer_active, "Failed to create shared event.");

    channel[i]->not_full =
        ten_shared_event_create(&channel[i]->region->not_full.sig,
                                &channel[i]->region->not_full.lock, 0, 1);
    TEN_ASSERT(channel[i]->not_full, "Failed to create shared event.");

    channel[i]->not_empty =
        ten_shared_event_create(&channel[i]->region->not_empty.sig,
                                &channel[i]->region->not_empty.lock, 0, 1);
    TEN_ASSERT(channel[i]->not_empty, "Failed to create shared event.");
  }

  return 0;
}

void ten_shm_channel_close(ten_shm_channel_t *channel) {
  if (UNLIKELY(!channel || !channel->region)) {
    return;
  }

  void *region = channel->region;
  int64_t ref_count = ten_atomic_fetch_sub(&channel->region->ref_count, 1);
  int64_t id = ten_atomic_load(&channel->region->id);

  if (ten_atomic_load(&channel->active)) {
    ten_shm_channel_inactive(channel, channel->as_reader);
  }

  if (channel->reader_active) {
    ten_shared_event_destroy(channel->reader_active);
    channel->reader_active = NULL;
  }

  if (channel->writer_active) {
    ten_shared_event_destroy(channel->writer_active);
    channel->writer_active = NULL;
  }

  if (channel->not_full) {
    ten_shared_event_destroy(channel->not_full);
    channel->not_full = NULL;
  }

  if (channel->not_empty) {
    ten_shared_event_destroy(channel->not_empty);
    channel->not_empty = NULL;
  }

  ten_shm_unmap(region);

  if (ref_count == 1) {
    ten_shm_unlink(channel->name.buf);
  }

  TEN_FREE(channel);
}

int ten_shm_channel_active(ten_shm_channel_t *channel, int read) {
  if (UNLIKELY(!channel || !channel->region)) {
    return -1;
  }

  ten_atomic_store(&channel->active, 1);
  channel->as_reader = read;
  if (read) {
    ten_shared_event_set(channel->reader_active);
  } else {
    ten_shared_event_set(channel->writer_active);
  }

  return 0;
}

int ten_shm_channel_inactive(ten_shm_channel_t *channel, int read) {
  if (UNLIKELY(!channel || !channel->region)) {
    return -1;
  }

  if (!ten_atomic_load(&channel->active)) {
    return -1;
  }

  if (read) {
    ten_shared_event_reset(channel->reader_active);
    ten_shared_event_set(channel->not_full);
  } else {
    ten_shared_event_reset(channel->writer_active);
    ten_shared_event_set(channel->not_empty);
  }

  ten_atomic_store(&channel->active, 0);

  return 0;
}

static inline int __ten_shm_channel_get_capacity_unsafe(
    ten_shm_channel_t *channel) {
  return (int)((channel->region->write_index + TEN_SHM_CHANNEL_SIZE -
                channel->region->read_index) %
               TEN_SHM_CHANNEL_SIZE);
}

static inline int __ten_shm_channel_is_full_unsafe(ten_shm_channel_t *channel) {
  return __ten_shm_channel_get_capacity_unsafe(channel) ==
         (TEN_SHM_CHANNEL_SIZE - 1);
}

static inline int __ten_shm_channel_is_empty_unsafe(
    ten_shm_channel_t *channel) {
  return __ten_shm_channel_get_capacity_unsafe(channel) == 0;
}

static inline int __ten_shm_channel_reader_alive(ten_shm_channel_t *channel) {
  return ten_shared_event_wait(channel->reader_active, 0) == 0;
}

static inline int __ten_shm_channel_writer_alive(ten_shm_channel_t *channel) {
  return ten_shared_event_wait(channel->writer_active, 0) == 0;
}

int ten_shm_channel_send(ten_shm_channel_t *channel, void *data, size_t size,
                         int nonblock) {
  if (UNLIKELY(!channel || !channel->region || !data || !size)) {
    return -1;
  }

  if (!ten_atomic_load(&channel->active) || channel->as_reader != 0) {
    return -1;
  }

  size_t left = size;
  while (left) {
    if (!__ten_shm_channel_reader_alive(channel)) {
      return -1;
    }

    ten_spinlock_lock(channel->channel_lock);

    if (__ten_shm_channel_is_full_unsafe(channel)) {
      ten_spinlock_unlock(channel->channel_lock);

      if (nonblock) {
        if (left != size && channel->read_sig) {
          ten_runloop_async_notify(channel->read_sig);
        }

        return (size - left);
      }

      ten_shared_event_wait(channel->not_full, -1);
      ten_spinlock_lock(channel->channel_lock);
    }

    if (!__ten_shm_channel_reader_alive(channel)) {
      ten_spinlock_unlock(channel->channel_lock);
      return -1;
    }

    int caps = TEN_SHM_CHANNEL_SIZE -
               __ten_shm_channel_get_capacity_unsafe(channel) - 1;

    size_t copy_size = left > caps ? caps : left;
    size_t first = TEN_SHM_CHANNEL_SIZE - channel->region->write_index;
    size_t second = copy_size - first;

    size_t copy_left = first > copy_size ? copy_size : first;
    if (copy_left) {
      ten_spinlock_unlock(channel->channel_lock);
      memmove(channel->region->data + channel->region->write_index, data,
              copy_left);
      ten_spinlock_lock(channel->channel_lock);
      channel->region->write_index += copy_left;
      channel->region->write_index %= TEN_SHM_CHANNEL_SIZE;
      left -= copy_left;
      // printf("w[1] cap %d, %d\n", caps, copy_left);
    }

    if (copy_size > copy_left && second) {
      ten_spinlock_unlock(channel->channel_lock);
      memmove(channel->region->data, (char *)data + copy_left, second);
      ten_spinlock_lock(channel->channel_lock);
      channel->region->write_index = second;
      channel->region->write_index %= TEN_SHM_CHANNEL_SIZE;
      left -= second;
      // printf("w[2] cap %d, %d\n", caps, second);
    }

    ten_spinlock_unlock(channel->channel_lock);

    ten_shared_event_set(channel->not_empty);
  }

  if (nonblock && channel->read_sig) {
    ten_runloop_async_notify(channel->read_sig);
  }

  return (int)size;
}

int ten_shm_channel_recv(ten_shm_channel_t *channel, void *data, size_t size,
                         int nonblock) {
  if (UNLIKELY(!channel || !channel->region || !data || !size)) {
    return -1;
  }

  if (!ten_atomic_load(&channel->active) || channel->as_reader != 1) {
    return -1;
  }

  size_t left = size;
  while (left) {
    if (!__ten_shm_channel_writer_alive(channel)) {
      return -1;
    }

    ten_spinlock_lock(channel->channel_lock);

    if (__ten_shm_channel_is_empty_unsafe(channel)) {
      ten_spinlock_unlock(channel->channel_lock);

      if (nonblock) {
        if (left != size && channel->write_sig) {
          ten_runloop_async_notify(channel->write_sig);
        }

        return (size - left);
      }

      ten_shared_event_wait(channel->not_empty, -1);
      ten_spinlock_lock(channel->channel_lock);
    }

    if (!__ten_shm_channel_writer_alive(channel)) {
      ten_spinlock_unlock(channel->channel_lock);
      return -1;
    }

    int caps = __ten_shm_channel_get_capacity_unsafe(channel);

    size_t copy_size = left > caps ? caps : left;
    size_t first = TEN_SHM_CHANNEL_SIZE - channel->region->read_index;
    size_t second = copy_size - first;

    size_t copy_left = first > copy_size ? copy_size : first;
    if (copy_left) {
      ten_spinlock_unlock(channel->channel_lock);
      memmove(data, channel->region->data + channel->region->read_index,
              copy_left);
      ten_spinlock_lock(channel->channel_lock);
      channel->region->read_index += copy_left;
      channel->region->read_index %= TEN_SHM_CHANNEL_SIZE;
      left -= copy_left;
      // printf("r[1] cap %d, %d\n", caps, copy_left);
    }

    if (copy_size > copy_left && second) {
      ten_spinlock_unlock(channel->channel_lock);
      memmove((char *)data + first, channel->region->data, second);
      ten_spinlock_lock(channel->channel_lock);
      channel->region->read_index = second;
      channel->region->read_index %= TEN_SHM_CHANNEL_SIZE;
      left -= second;
      // printf("r[2] cap %d, %d\n", caps, second);
    }

    ten_spinlock_unlock(channel->channel_lock);

    ten_shared_event_set(channel->not_full);
  }

  if (nonblock && channel->write_sig) {
    ten_runloop_async_notify(channel->write_sig);
  }

  return (int)size;
}

int ten_shm_channel_get_capacity(ten_shm_channel_t *channel) {
  if (UNLIKELY(!channel || !channel->channel_lock)) {
    return -1;
  }

  ten_spinlock_lock(channel->channel_lock);
  int diff = __ten_shm_channel_get_capacity_unsafe(channel);
  ten_spinlock_unlock(channel->channel_lock);

  return diff;
}

int ten_shm_channel_set_signal(ten_shm_channel_t *channel,
                               ten_runloop_async_t *signal, int read) {
  ten_memory_barrier();
  if (read) {
    channel->read_sig = signal;
  } else {
    channel->write_sig = signal;
  }
  ten_memory_barrier();
  return 0;
}

int ten_shm_channel_wait_remote(ten_shm_channel_t *channel, int wait_ms) {
  if (!channel || !channel->region) {
    return -1;
  }

  if (!ten_atomic_load(&channel->active)) {
    return -1;
  }

  return channel->as_reader
             ? ten_shared_event_wait(channel->writer_active, wait_ms)
             : ten_shared_event_wait(channel->reader_active, wait_ms);
}
