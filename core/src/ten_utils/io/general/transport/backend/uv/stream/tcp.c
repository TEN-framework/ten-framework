//
// Copyright © 2025 Agora
// This file is part of TEN Framework, an open source project.
// Licensed under the Apache License, Version 2.0, with certain conditions.
// Refer to the "LICENSE" file in the root directory for more information.
//
#include "include_internal/ten_utils/io/general/transport/backend/uv/stream/tcp.h"

#include <stdarg.h>
#include <stdlib.h>

#include "ten_utils/io/general/transport/backend/base.h"
#include "ten_utils/io/stream.h"  // IWYU pragma: keep
#include "ten_utils/lib/alloc.h"
#include "ten_utils/macro/field.h"  // IWYU pragma: keep
#include "ten_utils/macro/mark.h"

// Message write structure
typedef struct ten_uv_write_req_t {
  uv_write_t req;

  void *user_data;
} ten_uv_write_req_t;

static bool ten_streambackend_tcp_check_integrity(
    ten_streambackend_tcp_t *self) {
  TEN_ASSERT(self, "Invalid argument.");

  if (ten_atomic_load(&self->signature) != TEN_STREAMBACKEND_TCP_SIGNATURE) {
    return false;
  }
  return true;
}

void ten_streambackend_tcp_dump_info(ten_streambackend_tcp_t *tcp_stream,
                                     const char *fmt, ...) {
  TEN_ASSERT(tcp_stream && ten_streambackend_tcp_check_integrity(tcp_stream),
             "Invalid argument.");

  struct sockaddr_in socket_info;
  int socket_info_len = sizeof(socket_info);
  uv_tcp_getpeername((uv_tcp_t *)tcp_stream->uv_stream, &socket_info,
                     &socket_info_len);

  char ip_buf[INET_ADDRSTRLEN + 1];
  TEN_UNUSED int rc = uv_ip4_name(&socket_info, ip_buf, sizeof(ip_buf));
  TEN_ASSERT(!rc, "uv_ip4_name failed: %d", rc);

  ten_string_t new_fmt;
  TEN_STRING_INIT(new_fmt);

  const char *p = fmt;

  while (*p) {
    if ('^' != *p) {
      ten_string_append_formatted(&new_fmt, "%c", *p++);
      continue;
    }

    switch (*++p) {
    // The IP.
    case '1':
      ten_string_append_formatted(&new_fmt, "%s", ip_buf);
      p++;
      break;

    // The port.
    case '2':
      ten_string_append_formatted(&new_fmt, "%d", ntohs(socket_info.sin_port));
      p++;
      break;

    default:
      ten_string_append_formatted(&new_fmt, "%c", *p++);
      break;
    }
  }

  va_list ap;
  va_start(ap, fmt);

  ten_string_t description;
  TEN_STRING_INIT(description);
  ten_string_append_from_va_list(&description, ten_string_get_raw_str(&new_fmt),
                                 ap);
  ten_string_deinit(&new_fmt);

  va_end(ap);

  ten_string_deinit(&description);
}

static void on_tcp_alloc(uv_handle_t *uv_handle, size_t suggested_size,
                         uv_buf_t *buf) {
  TEN_ASSERT(uv_handle && suggested_size && buf, "Invalid argument.");

  buf->base = TEN_MALLOC(suggested_size);
  TEN_ASSERT(buf->base, "Failed to allocate memory.");

  buf->len = suggested_size;
}

static void on_tcp_read(uv_stream_t *uv_stream, ssize_t nread,
                        const uv_buf_t *buf) {
  TEN_ASSERT(uv_stream && uv_stream->data, "Invalid argument.");

  ten_streambackend_tcp_t *tcp_stream = uv_stream->data;
  TEN_ASSERT(tcp_stream && ten_streambackend_tcp_check_integrity(tcp_stream),
             "Invalid argument.");

  ten_stream_t *stream = tcp_stream->base.stream;
  TEN_ASSERT(stream && ten_stream_check_integrity(stream), "Invalid argument.");

  if (nread == 0) {
    // Nothing to read.
  } else if (nread < 0) {
    // Something bad happened, free the message.
    ten_streambackend_tcp_dump_info(
        tcp_stream, "libuv read tcp stream (^1:^2) failed: %ld", nread);

    if (buf->base) {
      // When nread < 0, the buf parameter might not point to a valid buffer; in
      // that case buf.len and buf.base are both set to 0.
      TEN_FREE_(buf->base);
    }

    // Notify that there is something bad happened.
    if (stream->on_message_read) {
      stream->on_message_read(stream, NULL, (int)nread);
    }
  } else {
    if (stream->on_message_read) {
      stream->on_message_read(stream, buf->base, (int)nread);
    }
    TEN_FREE_(buf->base);
  }
}

static int ten_streambackend_tcp_start_read(ten_streambackend_t *self_) {
  ten_streambackend_tcp_t *tcp_stream = (ten_streambackend_tcp_t *)self_;
  if (!tcp_stream) {
    // TEN_LOGE("Empty handle, treat as fail");
    return -1;
  }
  TEN_ASSERT(ten_streambackend_tcp_check_integrity(tcp_stream),
             "Invalid argument.");
  TEN_ASSERT(tcp_stream->uv_stream, "Invalid argument.");

  if (!tcp_stream->uv_stream) {
    // TEN_LOGE("Empty uv handle, treat as fail");
    return -1;
  }

  int rc = uv_read_start(tcp_stream->uv_stream, on_tcp_alloc, on_tcp_read);
  if (rc != 0) {
    // TEN_LOGE("uv_read_start return %d", rc);
    TEN_ASSERT(!rc, "uv_read_start() failed: %d", rc);
  }

  return rc;
}

static int ten_streambackend_tcp_stop_read(ten_streambackend_t *self_) {
  ten_streambackend_tcp_t *tcp_stream = (ten_streambackend_tcp_t *)self_;
  TEN_ASSERT(tcp_stream && ten_streambackend_tcp_check_integrity(tcp_stream),
             "Invalid argument.");

  if (!tcp_stream) {
    // TEN_LOGE("Empty handle, treat as fail");
    return -1;
  }
  TEN_ASSERT(ten_streambackend_tcp_check_integrity(tcp_stream),
             "Invalid argument.");

  if (!tcp_stream->uv_stream) {
    // TEN_LOGE("Empty uv handle, treat as fail");
    return -1;
  }

  int rc = uv_read_stop(tcp_stream->uv_stream);
  if (rc != 0) {
    // TEN_LOGE("uv_read_stop return %d", rc);
    TEN_ASSERT(!rc, "uv_read_stop() failed: %d", rc);
  }

  return rc;
}

static void on_tcp_write_done(uv_write_t *wreq, TEN_UNUSED int status) {
  ten_uv_write_req_t *req = (ten_uv_write_req_t *)wreq;

  ten_streambackend_tcp_t *tcp_stream = (ten_streambackend_tcp_t *)wreq->data;
  TEN_ASSERT(tcp_stream && ten_streambackend_tcp_check_integrity(tcp_stream),
             "Invalid argument.");

  ten_stream_t *stream = tcp_stream->base.stream;
  TEN_ASSERT(stream && ten_stream_check_integrity(stream), "Invalid argument.");

  if (stream->on_message_sent) {
    // Call the callback function.
    stream->on_message_sent(stream, status, req->user_data);
  }

  if (stream->on_message_free) {
    // Release the message data.
    stream->on_message_free(stream, status, req->user_data);
  }

  // Release the write request.
  TEN_FREE(req);
}

static int ten_streambackend_tcp_write(ten_streambackend_t *backend,
                                       const void *msg, size_t size,
                                       void *user_data) {
  ten_streambackend_tcp_t *tcp_stream = (ten_streambackend_tcp_t *)backend;
  TEN_ASSERT(tcp_stream, "Invalid argument.");

  ten_uv_write_req_t *req = TEN_MALLOC(sizeof(ten_uv_write_req_t));
  TEN_ASSERT(req, "Failed to allocate memory.");

  req->req.data = tcp_stream;
  req->user_data = user_data;

  uv_buf_t buf = uv_buf_init((char *)msg, size);

  int rc = uv_write((uv_write_t *)req, tcp_stream->uv_stream, &buf, 1,
                    on_tcp_write_done);
  if (rc != 0) {
    // TEN_LOGW("uv_write return %d", rc);
  }

  return rc;
}

static void ten_streambackend_tcp_destroy(ten_streambackend_tcp_t *tcp_stream) {
  TEN_ASSERT(tcp_stream && ten_streambackend_tcp_check_integrity(tcp_stream) &&
                 tcp_stream->uv_stream,
             "Invalid argument.");

  ten_streambackend_deinit(&tcp_stream->base);

  TEN_FREE(tcp_stream->uv_stream);
  TEN_FREE(tcp_stream);
}

static void ten_streambackend_tcp_on_close(uv_handle_t *uv_handle) {
  TEN_ASSERT(uv_handle && uv_handle->data, "Invalid argument.");

  // TEN_LOGD("Close stream.");

  ten_streambackend_tcp_t *tcp_stream =
      (ten_streambackend_tcp_t *)uv_handle->data;
  TEN_ASSERT(tcp_stream && ten_streambackend_tcp_check_integrity(tcp_stream),
             "Invalid argument.");

  ten_stream_t *stream = tcp_stream->base.stream;
  TEN_ASSERT(stream && ten_stream_check_integrity(stream), "Invalid argument.");

  ten_stream_on_close(stream);
  ten_streambackend_tcp_destroy(tcp_stream);
}

static int ten_streambackend_tcp_close(ten_streambackend_t *backend) {
  ten_streambackend_tcp_t *tcp_stream = (ten_streambackend_tcp_t *)backend;
  TEN_ASSERT(tcp_stream && ten_streambackend_tcp_check_integrity(tcp_stream),
             "Invalid argument.");

  if (ten_atomic_bool_compare_swap(&backend->is_close, 0, 1)) {
    // TEN_LOGD("Try to close stream TCP backend.");
    uv_close((uv_handle_t *)tcp_stream->uv_stream,
             ten_streambackend_tcp_on_close);
  }

  return 0;
}

static ten_streambackend_tcp_t *ten_streambackend_tcp_create(
    ten_stream_t *stream) {
  TEN_ASSERT(stream, "Invalid argument.");

  ten_streambackend_tcp_t *tcp_stream =
      (ten_streambackend_tcp_t *)TEN_MALLOC(sizeof(ten_streambackend_tcp_t));
  TEN_ASSERT(tcp_stream, "Failed to allocate memory.");

  memset(tcp_stream, 0, sizeof(ten_streambackend_tcp_t));

  ten_streambackend_init(TEN_RUNLOOP_UV, &tcp_stream->base, stream);
  ten_atomic_store(&tcp_stream->signature, TEN_STREAMBACKEND_TCP_SIGNATURE);

  tcp_stream->base.start_read = ten_streambackend_tcp_start_read;
  tcp_stream->base.stop_read = ten_streambackend_tcp_stop_read;
  tcp_stream->base.write = ten_streambackend_tcp_write;
  tcp_stream->base.close = ten_streambackend_tcp_close;

  tcp_stream->uv_stream = (uv_tcp_t *)TEN_MALLOC(sizeof(uv_tcp_t));
  TEN_ASSERT(tcp_stream->uv_stream, "Failed to allocate memory.");

  memset(tcp_stream->uv_stream, 0, sizeof(uv_tcp_t));

  tcp_stream->uv_stream->data = tcp_stream;

  return tcp_stream;
}

ten_stream_t *ten_stream_tcp_create_uv(uv_loop_t *loop) {
  ten_stream_t *stream = (ten_stream_t *)TEN_MALLOC(sizeof(*stream));
  TEN_ASSERT(stream, "Failed to allocate memory.");

  memset(stream, 0, sizeof(*stream));
  ten_stream_init(stream);

  ten_streambackend_tcp_t *tcp_stream = ten_streambackend_tcp_create(stream);

  int rc = uv_tcp_init(loop, tcp_stream->uv_stream);
  if (rc != 0) {
    // TEN_LOGE("uv_tcp_init return %d", rc);
    TEN_ASSERT(0, "uv_tcp_init() failed.");
    goto error;
  }
  return stream;

error:
  if (stream) {
    ten_stream_close(stream);
  }
  return NULL;
}
