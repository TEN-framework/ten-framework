//
// Copyright © 2025 Agora
// This file is part of TEN Framework, an open source project.
// Licensed under the Apache License, Version 2.0, with certain conditions.
// Refer to the "LICENSE" file in the root directory for more information.
//
#include "ten_utils/io/general/transport/backend/factory.h"

#include <string.h>

#include "include_internal/ten_utils/io/runloop.h"
#include "ten_utils/io/runloop.h"
#include "ten_utils/io/stream.h"
#include "ten_utils/lib/string.h"
#include "ten_utils/lib/uri.h"
#include "ten_utils/log/log.h"

typedef struct ten_backend_map_t {
  const char *name;
  const ten_transportbackend_factory_t *factory;
} ten_backend_map_t;

typedef struct ten_factory_map_t {
  const char *name;
  const ten_backend_map_t *factory;
  const size_t size;
} ten_factory_map_t;

extern const ten_transportbackend_factory_t general_tp_backend_raw;

#if defined(TEN_USE_LIBUV)

extern const ten_transportbackend_factory_t uv_tp_backend_tcp;
extern const ten_transportbackend_factory_t uv_tp_backend_pipe;

static const ten_backend_map_t uv_backend_map[] = {
    {TEN_PROTOCOL_TCP, &uv_tp_backend_tcp},
    {TEN_PROTOCOL_RAW, &general_tp_backend_raw},
    {TEN_PROTOCOL_PIPE, &uv_tp_backend_pipe},
};

#endif

#if defined(TEN_USE_LIBEVENT)

extern const ten_transportbackend_factory_t event_tp_backend_tcp;
extern const ten_transportbackend_factory_t event_tp_backend_pipe;

static const ten_backend_map_t event_backend_map[] = {
    {TEN_PROTOCOL_TCP, &event_tp_backend_tcp},
    {TEN_PROTOCOL_RAW, &general_tp_backend_raw},
    {TEN_PROTOCOL_PIPE, &event_tp_backend_pipe},
};

#endif

static const ten_factory_map_t factory_map[] = {
#if defined(TEN_USE_LIBEVENT)
    {TEN_RUNLOOP_EVENT2, event_backend_map,
     sizeof(event_backend_map) / sizeof(event_backend_map[0])},
#endif

#if defined(TEN_USE_LIBUV)
    {TEN_RUNLOOP_UV, uv_backend_map,
     sizeof(uv_backend_map) / sizeof(uv_backend_map[0])},
#endif
};

ten_transportbackend_factory_t *ten_get_transportbackend_factory(
    const char *choice, const ten_string_t *uri) {
  const ten_factory_map_t *map = NULL;
  const size_t map_size = 0;

  for (size_t i = 0; i < sizeof(factory_map) / sizeof(ten_factory_map_t); i++) {
    if (strcmp(factory_map[i].name, choice) == 0) {
      map = &factory_map[i];
      break;
    }
  }

  if (map == NULL) {
    return NULL;
  }

  ten_string_t *protocol = ten_uri_get_protocol(ten_string_get_raw_str(uri));

  for (size_t i = 0; i < map->size; i++) {
    if (strcmp(map->factory[i].name, protocol->buf) == 0 &&
        map->factory[i].factory != NULL &&
        map->factory[i].factory->create != NULL) {
      ten_string_destroy(protocol);
      return (ten_transportbackend_factory_t *)map->factory[i].factory;
    }
  }

  ten_string_destroy(protocol);
  return NULL;
}

#if defined(TEN_USE_LIBUV)
extern int ten_stream_migrate_uv(ten_stream_t *, ten_runloop_t *,
                                 ten_runloop_t *, void **,
                                 void (*)(ten_stream_t *, void **));
#endif

#if defined(TEN_USE_LIBEVENT)
extern int ten_stream_migrate_ev(ten_stream_t *, ten_runloop_t *,
                                 ten_runloop_t *, void **,
                                 void (*)(ten_stream_t *, void **));
#endif

int ten_stream_migrate(ten_stream_t *self, ten_runloop_t *from,
                       ten_runloop_t *to, void **user_data,
                       void (*cb)(ten_stream_t *new_stream, void **user_data)) {
  if (!self || !from || !to) {
    TEN_LOGE("Invalid parameter, self %p, from %p, to %p", self, from, to);
    return -1;
  }

  if (strcmp(from->impl, to->impl) != 0) {
    return -1;
  }

#if defined(TEN_USE_LIBUV)
  if (strcmp(from->impl, TEN_RUNLOOP_UV) == 0) {
    return ten_stream_migrate_uv(self, from, to, user_data, cb);
  }
#endif

#if defined(TEN_USE_LIBEVENT)
  if (strcmp(from->impl, TEN_RUNLOOP_EVENT2) == 0) {
    return ten_stream_migrate_ev(self, from, to, user_data, cb);
  }
#endif

  return -1;
}
