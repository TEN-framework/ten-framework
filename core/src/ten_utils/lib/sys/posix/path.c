//
// Copyright © 2025 Agora
// This file is part of TEN Framework, an open source project.
// Licensed under the Apache License, Version 2.0, with certain conditions.
// Refer to the "LICENSE" file in the root directory for more information.
//
#if defined(OS_LINUX)
#define _GNU_SOURCE
#endif

#include "ten_utils/lib/path.h"

#include <dirent.h>
#include <dlfcn.h>
#include <errno.h>
#include <stdlib.h>
#include <sys/param.h>
#include <sys/stat.h>
#include <unistd.h>

#include "include_internal/ten_utils/log/log.h"
#include "ten_utils/lib/alloc.h"
#include "ten_utils/lib/string.h"
#include "ten_utils/macro/check.h"
#include "ten_utils/macro/mark.h"

int ten_path_to_system_flavor(ten_string_t *path) {
  TEN_ASSERT(path, "Invalid argument.");

  if (!path || ten_string_is_empty(path)) {
    return -1;
  }

  size_t len = ten_string_len(path);

#if defined(OS_WINDOWS)
  // Platform-specific handling: Convert '\' to '/' on Windows
  for (size_t i = 0; i < len; i++) {
    if (path->buf[i] == '\\') {
      path->buf[i] = '/';
    }
  }
#endif

  return 0;
}

ten_string_t *ten_path_get_cwd(void) {
  char *buf = TEN_MALLOC(MAXPATHLEN);
  TEN_ASSERT(buf, "Failed to allocate memory.");

  if (!getcwd(buf, MAXPATHLEN)) {
    TEN_LOGE("Failed to get cwd: %d", errno);
    TEN_FREE(buf);
    return NULL;
  }

  ten_string_t *ret = ten_string_create_formatted(buf);
  TEN_FREE(buf);

  return ret;
}

ten_string_t *ten_path_get_home_path(void) {
  // NOLINTNEXTLINE(concurrency-mt-unsafe)
  char *home = getenv("HOME");
  if (!home) {
    TEN_LOGE("Failed to get HOME environment variable: %d", errno);
    return NULL;
  }

  return ten_string_create_formatted("%s", home);
}

ten_string_t *ten_path_get_module_path(const void *addr) {
  TEN_ASSERT(addr, "Invalid argument.");

  Dl_info info;

  if (dladdr(addr, &info) == 0) {
    TEN_LOGE("Failed to dladdr: %d", errno);
    TEN_ASSERT(0, "Should not happen.");
    return NULL;
  }

  ten_string_t *full_path = ten_string_create_formatted(info.dli_fname);
  if (!full_path) {
    TEN_LOGE("Failed to get full path of %s", info.dli_fname);
    TEN_ASSERT(0, "Should not happen.");
    return NULL;
  }

  ten_string_t *dir = ten_path_get_dirname(full_path);
  ten_string_destroy(full_path);
  if (!dir) {
    TEN_LOGE("Failed to get folder of %s", ten_string_get_raw_str(full_path));
    TEN_ASSERT(0, "Should not happen.");
    return NULL;
  }

  ten_string_t *abs = ten_path_realpath(dir);
  ten_string_destroy(dir);
  if (!abs) {
    TEN_LOGE("Failed to get realpath of %s", ten_string_get_raw_str(dir));
    TEN_ASSERT(0, "Should not happen.");
    return NULL;
  }

  return abs;
}

ten_string_t *ten_path_realpath(const ten_string_t *path) {
  TEN_ASSERT(path, "Invalid argument.");

  if (!path || ten_string_is_empty(path)) {
    TEN_LOGE("Invalid argument.");
    return NULL;
  }

  char *buf = realpath(ten_string_get_raw_str(path), NULL);
  if (!buf) {
    return NULL;
  }

  ten_string_t *ret = ten_string_create_formatted(buf);
  TEN_FREE(buf);
  return ret;
}

int ten_path_is_dir(const ten_string_t *path) {
  TEN_ASSERT(path, "Invalid argument.");

  if (!path || ten_string_is_empty(path)) {
    TEN_LOGE("Path is empty.");
    return 0;
  }

  struct stat st;
  if (stat(ten_string_get_raw_str(path), &st) != 0) {
    TEN_LOGE("Failed to stat path: %s, errno: %d", ten_string_get_raw_str(path),
             errno);
    return 0;
  }

  return S_ISDIR(st.st_mode);
}

int ten_path_mkdir(const ten_string_t *path, int recursive) {
  TEN_ASSERT(path, "Invalid argument.");

  if (!path || ten_string_is_empty(path)) {
    TEN_LOGE("Invalid argument.");
    return -1;
  }

  if (ten_path_is_dir(path)) {
    TEN_LOGW("Path %s is existed.", ten_string_get_raw_str(path));
    return 0;
  }

  if (recursive) {
    ten_string_t *parent = ten_path_get_dirname(path);
    if (!parent) {
      TEN_LOGE("Failed to get parent folder of %s",
               ten_string_get_raw_str(path));
      return -1;
    }

    // Recursively create the parent directory if it doesn't exist
    if (!ten_path_exists(ten_string_get_raw_str(parent))) {
      int ret = ten_path_mkdir(parent, 1);
      ten_string_destroy(parent);

      if (ret != 0) {
        TEN_LOGE("Failed to create parent folder %s",
                 ten_string_get_raw_str(parent));
        return ret;
      }
    } else {
      ten_string_destroy(parent);
    }
  }

  int rc = mkdir(ten_string_get_raw_str(path), 0755);
  if (rc) {
    TEN_LOGE("Failed to create %s: %d", ten_string_get_raw_str(path), errno);
  }

  return rc;
}

int ten_path_exists(const char *path) {
  TEN_ASSERT(path, "Invalid argument.");

  if (!path || !*path) {
    return 0;
  }

  struct stat st;
  if (stat(path, &st) != 0) {
    return 0;
  }

  return 1;
}

struct ten_path_itor_t {
  struct dirent *entry;
  ten_dir_fd_t *dir;
};

struct ten_dir_fd_t {
  DIR *dir;
  ten_path_itor_t itor;
  ten_string_t *path;
};

ten_dir_fd_t *ten_path_open_dir(const char *path) {
  if (!path || !strlen(path)) {
    TEN_LOGE("Invalid argument.");
    return NULL;
  }

  ten_dir_fd_t *dir = TEN_MALLOC(sizeof(ten_dir_fd_t));
  TEN_ASSERT(dir, "Failed to allocate memory.");
  if (!dir) {
    TEN_LOGE("Failed to allocate memory for ten_dir_fd_t.");
    return NULL;
  }

  memset(dir, 0, sizeof(ten_dir_fd_t));

  dir->dir = opendir(path);
  if (!dir->dir) {
    TEN_LOGE("Failed to opendir %s: %d", path, errno);
    return NULL;
  }

  dir->itor.entry = NULL;
  dir->itor.dir = dir;
  dir->path = ten_string_create_from_c_str(path);

  return dir;
}

int ten_path_close_dir(ten_dir_fd_t *dir) {
  TEN_ASSERT(dir, "Invalid argument.");
  if (!dir) {
    TEN_LOGE("Invalid argument.");
    return -1;
  }

  if (dir->dir) {
    closedir(dir->dir);
  }

  if (dir->path) {
    ten_string_destroy(dir->path);
  }

  TEN_FREE(dir);
  return 0;
}

ten_path_itor_t *ten_path_get_first(ten_dir_fd_t *dir) {
  TEN_ASSERT(dir, "Invalid argument.");
  if (!dir) {
    TEN_LOGE("Invalid argument.");
    return NULL;
  }

  // readdir is actually thread-safe in Linux.
  // NOLINTNEXTLINE(concurrency-mt-unsafe)
  dir->itor.entry = readdir(dir->dir);
  return &dir->itor;
}

ten_path_itor_t *ten_path_get_next(ten_path_itor_t *itor) {
  TEN_ASSERT(itor, "Invalid argument.");
  if (!itor) {
    TEN_LOGE("Invalid argument.");
    return NULL;
  }

  // readdir is actually thread-safe in Linux.
  // NOLINTNEXTLINE(concurrency-mt-unsafe)
  itor->entry = readdir(itor->dir->dir);
  if (!itor->entry) {
    return NULL;
  }

  return itor;
}

ten_string_t *ten_path_itor_get_name(ten_path_itor_t *itor) {
  TEN_ASSERT(itor, "Invalid argument.");
  if (!itor) {
    TEN_LOGE("Invalid argument.");
    return NULL;
  }

  return ten_string_create_formatted(itor->entry->d_name);
}

ten_string_t *ten_path_itor_get_full_name(ten_path_itor_t *itor) {
  TEN_ASSERT(itor, "Invalid argument.");
  if (!itor) {
    TEN_LOGE("Invalid argument.");
    return NULL;
  }

  ten_string_t *short_name = ten_path_itor_get_name(itor);
  if (!short_name) {
    return NULL;
  }

  ten_string_t *full_name = ten_string_clone(itor->dir->path);
  if (!full_name) {
    ten_string_destroy(short_name);
    return NULL;
  }

  ten_string_append_formatted(full_name, "/%s",
                              ten_string_get_raw_str(short_name));
  ten_string_destroy(short_name);

  ten_path_to_system_flavor(full_name);
  return full_name;
}

int ten_path_change_cwd(ten_string_t *dirname) {
  TEN_ASSERT(dirname, "Invalid argument.");

  int rc = chdir(ten_string_get_raw_str(dirname));
  if (rc) {
    TEN_LOGE("Failed to chdir to %s", ten_string_get_raw_str(dirname));
    return -1;
  }
  return 0;
}

int ten_path_is_absolute(const ten_string_t *path) {
  TEN_ASSERT(path, "Invalid argument.");

  if (ten_string_starts_with(path, "/")) {
    return 1;
  }
  return 0;
}

int ten_path_make_symlink(const char *target, const char *link_path) {
  TEN_ASSERT(target && strlen(target) && link_path && strlen(link_path),
             "Invalid argument.");

  return symlink(target, link_path);
}

int ten_path_is_symlink(const char *path) {
  TEN_ASSERT(path && strlen(path), "Invalid argument.");

  struct stat st;
  if (lstat(path, &st) != 0) {
    return 0;
  }

  return S_ISLNK(st.st_mode);
}

int ten_path_create_temp_dir(const char *base_path,
                             ten_string_t *tmp_dir_path) {
  TEN_ASSERT(base_path && tmp_dir_path, "Invalid argument.");

  ten_string_init_from_c_str_with_size(tmp_dir_path, base_path,
                                       strlen(base_path));

  ten_path_join_c_str(tmp_dir_path, "tmpdir.XXXXXX");
  TEN_UNUSED char *result =
      mkdtemp((char *)ten_string_get_raw_str(tmp_dir_path));
  TEN_ASSERT(result, "Should not happen.");

  return 0;
}
