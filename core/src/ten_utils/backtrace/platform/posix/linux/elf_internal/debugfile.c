//
// Copyright © 2025 Agora
// This file is part of TEN Framework, an open source project.
// Licensed under the Apache License, Version 2.0, with certain conditions.
// Refer to the "LICENSE" file in the root directory for more information.
//
// This file is modified from
// https://github.com/ianlancetaylor/libbacktrace [BSD license]
//
#include "include_internal/ten_utils/backtrace/platform/posix/linux/elf_internal/debugfile.h"

#include <assert.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

#include "include_internal/ten_utils/backtrace/platform/posix/file.h"
#include "include_internal/ten_utils/backtrace/platform/posix/linux/elf_internal/crc32.h"

/**
 * @brief Path to the system-wide directory containing debug files organized by
 * build ID.
 *
 * When a binary is built with debug information, it can include a build ID,
 * which is a unique identifier for the binary. Debug information can be
 * stripped from the binary and stored in a separate file in this directory.
 *
 * The structure of this directory is:
 * /usr/lib/debug/.build-id/XX/YYYY...YY.debug
 * where XX is the first byte of the build ID in hex, and YYYY...YY is the
 * rest of the build ID in hex.
 *
 * This is the standard location where GDB and other debugging tools look for
 * separate debug info files when using build IDs.
 */
#define SYSTEM_BUILD_ID_DIR "/usr/lib/debug/.build-id/"

/**
 * @brief Open a separate debug info file, using the build ID to find it.
 *
 * This function constructs a path to a debug file based on the build ID
 * and attempts to open it. Debug files are typically stored in the
 * /usr/lib/debug/.build-id directory with a specific naming convention:
 * - First two characters of the build ID (in hex) form a subdirectory name
 * - The remaining characters form the filename with a ".debug" suffix
 *
 * For example, a build ID "abcdef" would be looked up at:
 * /usr/lib/debug/.build-id/ab/cdef.debug
 *
 * @param self The backtrace context.
 * @param build_id_data Pointer to the build ID data.
 * @param build_id_size Size of the build ID in bytes.
 * @return An open file descriptor on success, or -1 on failure.
 *
 * @note The GDB manual states that the only place gdb looks for a debug file
 * when the build ID is known is in /usr/lib/debug/.build-id.
 */
int elf_open_debug_file_by_build_id(ten_backtrace_t *self,
                                    const char *build_id_data,
                                    size_t build_id_size) {
  assert(self && "Invalid argument.");

  if (build_id_data == NULL || build_id_size == 0) {
    assert(0 && "Invalid build ID data.");
    return -1;
  }

  const char *const prefix = SYSTEM_BUILD_ID_DIR;
  const size_t prefix_len = strlen(prefix);
  const char *const suffix = ".debug";
  const size_t suffix_len = strlen(suffix);
  bool does_not_exist = false;

  // Calculate path length:
  // prefix + 2 chars per byte of build ID + 1 for '/' after first byte + suffix
  // + null terminator.
  size_t len = prefix_len + (build_id_size * 2) + 1 + suffix_len + 1;

  char *build_id_filename = malloc(len);
  if (build_id_filename == NULL) {
    assert(0 && "Failed to allocate memory.");
    return -1;
  }

  char *t = build_id_filename;
  memcpy(t, prefix, prefix_len);
  t += prefix_len;

  // Convert build ID bytes to hex characters and insert directory separator.
  for (size_t i = 0; i < build_id_size; i++) {
    unsigned char b = (unsigned char)build_id_data[i];
    unsigned char nib = (b & 0xf0) >> 4;

    *t++ = (char)(nib < 10 ? '0' + nib : 'a' + nib - 10);

    nib = b & 0x0f;
    *t++ = (char)(nib < 10 ? '0' + nib : 'a' + nib - 10);

    // Insert directory separator after the first byte (2 hex chars).
    if (i == 0) {
      *t++ = '/';
    }
  }

  memcpy(t, suffix, suffix_len);
  t[suffix_len] = '\0';

  int ret = ten_backtrace_open_file(build_id_filename, &does_not_exist);

  free(build_id_filename);

  // Note: gdb checks that the debuginfo file has the same build ID note,
  // but we skip this check since the file path is derived from the build ID
  // itself.

  return ret;
}

/**
 * @brief Check if a file is a symbolic link.
 *
 * This function determines whether the specified file is a symbolic link by
 * using lstat() to get file information without following symlinks.
 *
 * @param filename Path to the file to check. Must not be NULL.
 * @return 1 if the file is a symlink, 0 if it's not a symlink or if an error
 * occurred.
 *
 * @note If lstat() fails (e.g., file doesn't exist or permission issues),
 *       the function returns 0 rather than reporting the error.
 */
static int elf_is_symlink(const char *filename) {
  struct stat st;

  if (filename == NULL) {
    assert(0 && "Invalid argument: NULL filename provided.");
    return 0;
  }

  if (lstat(filename, &st) < 0) {
    // lstat failed - file might not exist or other error.
    return 0;
  }

  return S_ISLNK(st.st_mode) ? 1 : 0;
}

/**
 * @brief Reads the target of a symbolic link into a dynamically allocated
 * buffer.
 *
 * This function attempts to read the contents of a symbolic link specified by
 * @a filename. It starts with a reasonably sized buffer and doubles it if
 * needed until the entire link target can be read.
 *
 * @param self The backtrace context. Must not be NULL.
 * @param filename Path to the symbolic link to read. Must not be NULL.
 * @param plen Pointer to store the allocated buffer size (not the string
 * length). Must not be NULL.
 * @return A pointer to a newly allocated buffer containing the link target as a
 *         null-terminated string, or NULL if the link couldn't be read or
 *         memory allocation failed.
 *
 * @note The caller is responsible for freeing the returned buffer.
 * @note This function properly null-terminates the returned string.
 * @note On success, *plen contains the size of the allocated buffer, not the
 *       actual string length of the link target.
 */
static char *elf_readlink(ten_backtrace_t *self, const char *filename,
                          size_t *plen) {
  assert(self && "Invalid argument: NULL backtrace context.");
  assert(filename && "Invalid argument: NULL filename.");
  assert(plen && "Invalid argument: NULL plen pointer.");

  if (!self || !filename || !plen) {
    return NULL;
  }

  size_t len = 128;  // Initial buffer size.

  while (1) {
    char *buf = malloc(len);
    if (buf == NULL) {
      assert(0 && "Failed to allocate memory.");
      return NULL;
    }

    ssize_t rl = readlink(filename, buf, len);
    if (rl < 0) {
      // readlink failed (e.g., not a symlink, permission denied).
      free(buf);
      return NULL;
    }

    if ((size_t)rl < len - 1) {
      // We have enough space for the result plus null terminator.
      buf[rl] = '\0';  // readlink doesn't null-terminate the buffer.
      *plen = len;
      return buf;
    }

    free(buf);

    // Buffer was too small, double its size and try again.
    len *= 2;

    // Guard against potential overflow.
    if (len == 0) {
      return NULL;
    }
  }
}

/**
 * @brief Try to open a debug file by concatenating path components.
 *
 * This function attempts to open a debug file by constructing a path from three
 * components:
 * - prefix (base directory path)
 * - prefix2 (subdirectory or additional path component)
 * - debug_link_name (actual debug file name)
 *
 * The function concatenates these components and attempts to open the resulting
 * path.
 *
 * @param self The backtrace context.
 * @param prefix First part of the path (typically a directory).
 * @param prefix_len Length of the prefix string.
 * @param prefix2 Second part of the path (typically a subdirectory).
 * @param prefix2_len Length of the prefix2 string.
 * @param debug_link_name Name of the debug file to open.
 * @return An open file descriptor on success, or -1 on failure.
 *
 * @note All input pointers must be valid, non-NULL pointers.
 * @note The function handles memory allocation and cleanup internally.
 */
static int elf_try_debug_file(ten_backtrace_t *self, const char *prefix,
                              size_t prefix_len, const char *prefix2,
                              size_t prefix2_len, const char *debug_link_name) {
  assert(self && "Invalid argument: NULL backtrace context.");
  assert(prefix && "Invalid argument: NULL prefix.");
  assert(prefix2 && "Invalid argument: NULL prefix2.");
  assert(debug_link_name && "Invalid argument: NULL debug_link_name.");

  bool does_not_exist = false;
  int ret = -1;

  // Calculate the length of the debug link name and the total path.
  size_t debug_link_len = strlen(debug_link_name);
  size_t try_len = prefix_len + prefix2_len + debug_link_len + 1;

  // Allocate memory for the full path.
  char *try = malloc(try_len);
  if (try == NULL) {
    assert(0 && "Failed to allocate memory for debug file path.");
    return -1;
  }

  // Construct the full path by concatenating the components.
  memcpy(try, prefix, prefix_len);
  memcpy(try + prefix_len, prefix2, prefix2_len);
  memcpy(try + prefix_len + prefix2_len, debug_link_name, debug_link_len);
  try[prefix_len + prefix2_len + debug_link_len] = '\0';

  // Attempt to open the file.
  ret = ten_backtrace_open_file(try, &does_not_exist);

  // Clean up allocated memory.
  free(try);

  return ret;
}

/**
 * @brief Find a separate debug info file, using the debug_link section data to
 * find it.
 *
 * This function attempts to locate a debug info file by trying several standard
 * locations where debug files might be stored. It follows these steps:
 * 1. Resolves any symlinks in the original filename.
 * 2. Looks in the same directory as the executable.
 * 3. Looks in a .debug subdirectory of the executable's directory.
 * 4. Looks in /usr/lib/debug with the same relative path.
 *
 * @param self The backtrace context.
 * @param filename Path to the original executable file.
 * @param debug_link_name Name of the debug file to find (from .gnu_debuglink
 * section).
 * @return An open file descriptor on success, or -1 if the debug file cannot be
 * found.
 */
static int elf_find_debug_file_by_debug_link(ten_backtrace_t *self,
                                             const char *filename,
                                             const char *debug_link_name) {
  assert(self && "Invalid argument: NULL backtrace context.");
  assert(filename && "Invalid argument: NULL filename.");
  assert(debug_link_name && "Invalid argument: NULL debug_link_name.");

  const char *slash = NULL;

  // Resolve symlinks in @a filename. Since @a filename is fairly likely to be
  // /proc/self/exe, symlinks are common. We don't try to resolve the whole
  // path name, just the base name.

  int ret = -1;
  char *alc = NULL;
  size_t alc_len = 0;
  while (elf_is_symlink(filename)) {
    size_t new_len = 0;

    char *new_buf = elf_readlink(self, filename, &new_len);
    if (new_buf == NULL) {
      break;
    }

    if (new_buf[0] == '/') {
      // Absolute path. Use it directly, and no more filename handling is needed
      // for this round.
      filename = new_buf;
    } else {
      slash = strrchr(filename, '/');
      if (slash == NULL) {
        // Basename only. Use it directly, and no more filename handling is
        // needed for this round.
        filename = new_buf;
      } else {
        slash++;

        // Construct a new path by combining the directory of the original path
        // with the relative path from the symlink.
        // Example:
        //   filename: a/b/c/d
        //   symlink content: x/y/z
        //   => result: a/b/c/x/y/z

        size_t clen = slash - filename + strlen(new_buf) + 1;
        char *c = malloc(clen);
        if (c == NULL) {
          assert(0 && "Failed to allocate memory for path resolution.");
          free(new_buf);
          goto done;
        }

        memcpy(c, filename, slash - filename);
        memcpy(c + (slash - filename), new_buf, strlen(new_buf));
        c[slash - filename + strlen(new_buf)] = '\0';

        free(new_buf);

        filename = c;
        new_buf = c;
        new_len = clen;
      }
    }

    // Free previous allocation before assigning the new one.
    if (alc != NULL) {
      free(alc);
    }

    alc = new_buf;
    alc_len = new_len;
  }

  const char *prefix = NULL;
  size_t prefix_len = 0;

  // Alternative 1: Look for debug_link_name in the same directory as filename.
  slash = strrchr(filename, '/');
  if (slash == NULL) {
    prefix = "";
    prefix_len = 0;
  } else {
    slash++;
    prefix = filename;
    prefix_len = slash - filename;
  }

  int debug_file_fd =
      elf_try_debug_file(self, prefix, prefix_len, "", 0, debug_link_name);
  if (debug_file_fd >= 0) {
    ret = debug_file_fd;
    goto done;
  }

  // Alternative 2: Look for debug_link_name in a .debug subdirectory of
  // filename's directory.
  debug_file_fd = elf_try_debug_file(self, prefix, prefix_len, ".debug/",
                                     strlen(".debug/"), debug_link_name);
  if (debug_file_fd >= 0) {
    ret = debug_file_fd;
    goto done;
  }

  // Alternative 3: Look for debug_link_name in /usr/lib/debug with the same
  // relative path.
  debug_file_fd =
      elf_try_debug_file(self, "/usr/lib/debug/", strlen("/usr/lib/debug/"),
                         prefix, prefix_len, debug_link_name);
  if (debug_file_fd >= 0) {
    ret = debug_file_fd;
  }

done:
  // Clean up allocated memory.
  if (alc != NULL) {
    free(alc);
  }

  return ret;
}

/**
 * @brief Open a separate debug info file, using the debug_link section data to
 * find it.
 *
 * This function attempts to locate a debug file using the debug_link_name and
 * validates it against the provided CRC if one is specified.
 *
 * @param self The backtrace context.
 * @param filename The original ELF file path.
 * @param debug_link_name The name of the debug file from the .gnu_debuglink
 * section.
 * @param debug_link_crc The CRC checksum from the .gnu_debuglink section, or 0
 * if not available.
 * @param on_error Error callback function.
 * @param data User data to pass to the error callback.
 *
 * @return An open file descriptor to the debug file if found and valid, or -1
 * on failure
 */
int elf_open_debug_file_by_debug_link(ten_backtrace_t *self,
                                      const char *filename,
                                      const char *debug_link_name,
                                      uint32_t debug_link_crc,
                                      ten_backtrace_on_error_func_t on_error,
                                      void *data) {
  assert(self && "Invalid argument.");
  assert(filename && "Invalid filename argument.");
  assert(debug_link_name && "Invalid debug_link_name argument.");

  int debug_file_fd =
      elf_find_debug_file_by_debug_link(self, filename, debug_link_name);
  if (debug_file_fd < 0) {
    return -1;
  }

  if (debug_link_crc != 0) {
    uint32_t got_crc = elf_crc32_file(self, debug_file_fd, on_error, data);
    if (got_crc != debug_link_crc) {
      // CRC checksum error, the found debug file is not the correct one for
      // the original ELF file.
      ten_backtrace_close_file(debug_file_fd);
      return -1;
    }
  }

  return debug_file_fd;
}
