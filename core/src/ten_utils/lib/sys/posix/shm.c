//
// Copyright © 2025 Agora
// This file is part of TEN Framework, an open source project.
// Licensed under the Apache License, Version 2.0, with certain conditions.
// Refer to the "LICENSE" file in the root directory for more information.
//
#include "ten_utils/lib/shm.h"

#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>
#include <unistd.h>

#include "ten_utils/lib/atomic.h"
#include "ten_utils/macro/check.h"
#include "ten_utils/macro/memory.h"

static char *__make_abs_path(const char *name) {
  char *abs_path = NULL;
  size_t abs_path_size = 0;

  if (name == NULL) {
    return NULL;
  }

  if (name[0] == '/') {
    abs_path_size = strlen(name) + 1;
    abs_path = (char *)TEN_MALLOC(abs_path_size);
    if (abs_path == NULL) {
      return NULL;
    }
    strncpy(abs_path, name, abs_path_size);
    return abs_path;
  }

  abs_path_size = strlen(name) + 2;
  abs_path = (char *)TEN_MALLOC(abs_path_size);
  if (abs_path == NULL) {
    return NULL;
  }

  int written = snprintf(abs_path, abs_path_size, "/%s", name);
  TEN_ASSERT(written > 0, "Should not happen.");

  return abs_path;
}

void *ten_shm_map(const char *name, size_t size) {
  int fd = -1;
  void *addr = NULL;
  char *abs_path = NULL;

  ten_atomic_t *cookie = NULL;
  int open_existing = 0;

  if (!name || !*name || !size) {
    goto done;
  }

  abs_path = __make_abs_path(name);
  if (abs_path == NULL) {
    goto done;
  }

  fd = shm_open(abs_path, O_RDWR | O_CREAT | O_EXCL, S_IRUSR | S_IWUSR);
  open_existing = (fd < 0 && errno == EEXIST);
  if (open_existing) {
    // this is to avoid truncate an existing shm file
    fd = shm_open(abs_path, O_RDWR, S_IRUSR | S_IWUSR);
  } else {
    int rc = ftruncate(fd, size + sizeof(ten_atomic_t));
    TEN_ASSERT(!rc, "Should not happen.");
  }

  if (fd < 0) {
    goto done;
  }

  addr = mmap(NULL, size + sizeof(ten_atomic_t), PROT_READ | PROT_WRITE,
              MAP_SHARED, fd, 0);
  if (addr == MAP_FAILED) {
    addr = NULL;
    goto done;
  }

  if (!open_existing) {
    cookie = (ten_atomic_t *)addr;
    ten_atomic_store(cookie, size);
  }

  addr = (char *)addr + sizeof(ten_atomic_t);

done:
  if (fd >= 0) {
    close(fd);
  }

  if (abs_path) {
    TEN_FREE(abs_path);
  }

  return addr;
}

size_t ten_shm_get_size(void *addr) {
  void *begin = (char *)addr - sizeof(ten_atomic_t);

  return addr ? ten_atomic_load((volatile ten_atomic_t *)begin) : 0;
}

void ten_shm_unmap(void *addr) {
  void *begin = (char *)addr - sizeof(ten_atomic_t);
  size_t size = ten_shm_get_size(addr);

  if (addr && size) {
    munmap(begin, size);
  }
}

void ten_shm_unlink(const char *name) {
  char *abs_path = NULL;

  if (!name || !*name) {
    return;
  }

  abs_path = __make_abs_path(name);
  if (abs_path == NULL) {
    return;
  }

  shm_unlink(abs_path);
  TEN_FREE(abs_path);
}
