//
// Copyright © 2025 Agora
// This file is part of TEN Framework, an open source project.
// Licensed under the Apache License, Version 2.0, with certain conditions.
// Refer to the "LICENSE" file in the root directory for more information.
//
#include "ten_utils/ten_config.h"

#include "include_internal/ten_utils/backtrace/file.h"

#include <assert.h>
#include <fcntl.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>

#if defined(_WIN32) || defined(_WIN64)
#include <windows.h>

#define strtok_r strtok_s
#endif

/**
 * @file file.c
 * @brief Platform-specific file operations for backtrace functionality.
 *
 * This file provides cross-platform compatible file handling operations used
 * by the backtrace system. It includes open and close operations with proper
 * error handling and platform-specific compatibility adjustments.
 */

/**
 * @brief Normalizes a file path by resolving '..' and '.' path components.
 *
 * This function processes a file path and simplifies it by resolving relative
 * path components like '..' (parent directory) and '.' (current directory).
 * It creates a cleaner, more readable canonical path.
 *
 * For example:
 *   "/a/b/../c"     becomes "/a/c"
 *   "/a/./b/./c"    becomes "/a/b/c"
 *   "/a/b/../../c"  becomes "/c"
 *   "C:\a\b\..\c"   becomes "C:\a\c" (Windows)
 *
 * The function handles both POSIX and Windows paths, including:
 * - POSIX absolute paths (starting with '/')
 * - POSIX relative paths
 * - Windows drive letter paths (e.g., "C:\path")
 * - Windows UNC paths (e.g., "\\server\share\path")
 *
 * @param path The input path to normalize (null-terminated string)
 * @param normalized_path Buffer to receive the normalized path
 * @param buffer_size Size of the normalized_path buffer
 * @return true on success, false if buffer is too small or path is invalid
 */
bool ten_backtrace_normalize_path(const char *path, char *normalized_path,
                                  size_t buffer_size) {
  if (!path || !normalized_path || buffer_size == 0) {
    assert(0 && "Invalid argument.");
    return false;
  }

  // Create a working copy of the path with enough space for null terminator.
  size_t path_len = strlen(path);
  if (path_len >= buffer_size) {
    assert(0 && "Buffer too small.");
    return false;  // Buffer too small
  }

  // Handle empty path.
  if (path_len == 0) {
    strcpy(normalized_path, ".");
    return true;
  }

  // Check if this path uses backslashes (Windows style).
  bool has_backslash = false;
  for (size_t i = 0; i < path_len; i++) {
    if (path[i] == '\\') {
      has_backslash = true;
      break;
    }
  }

  // Determine if this is a Windows path. Using backslashes indicates Windows
  // path.
  bool is_windows_path = has_backslash;
  bool has_drive_letter = false;

  // Check for Windows drive letter (e.g., "C:").
  if (path_len >= 2 &&
      ((path[0] >= 'A' && path[0] <= 'Z') ||
       (path[0] >= 'a' && path[0] <= 'z')) &&
      path[1] == ':') {
    is_windows_path = true;
    has_drive_letter = true;
  }

  // Check for Windows UNC path (e.g., "\\server\share").
  bool is_unc_path = false;
  if (path_len >= 2 && path[0] == '\\' && path[1] == '\\') {
    is_windows_path = true;
    is_unc_path = true;
  }

  // Create a temporary copy of the path where we convert backslashes to
  // forward slashes.
  char *temp_path = (char *)malloc(path_len + 1);
  if (!temp_path) {
    assert(0 && "Memory allocation failed.");
    return false;
  }

  memcpy(temp_path, path, path_len + 1);

  // Convert backslashes to forward slashes for uniform processing.
  for (size_t i = 0; i < path_len; i++) {
    if (temp_path[i] == '\\') {
      temp_path[i] = '/';
    }
  }

  // Preserve the original path type (absolute or relative).
  bool is_absolute = false;
  if (temp_path[0] == '/') {
    is_absolute = true;
  }
  // Drive letters are considered absolute.
  if (has_drive_letter) {
    is_absolute = true;
  }

  // Copy the path to normalize.
  strncpy(normalized_path, temp_path, buffer_size);
  normalized_path[buffer_size - 1] = '\0';
  free(temp_path);

  // Special handling for UNC paths: keep the first two components (server and
  // share) as a fixed prefix.
  char *path_to_tokenize = normalized_path;
  char unc_prefix[buffer_size];
  memset(unc_prefix, 0, buffer_size);  // Initialize to zero

  // For UNC paths, we need special handling.
  if (is_unc_path) {
    // Skip the initial "//" in UNC path (which was "\\").
    path_to_tokenize = normalized_path + 2;

    // Find the server name.
    char *server_end = strchr(path_to_tokenize, '/');
    if (server_end) {
      // Save server name.
      size_t server_len = server_end - path_to_tokenize;
      char server_name[buffer_size];
      memset(server_name, 0, buffer_size);
      strncpy(server_name, path_to_tokenize, server_len);

      // Now find the share name.
      char *share_start = server_end + 1;
      char *share_end = strchr(share_start, '/');

      if (share_end) {
        // Save share name.
        size_t share_len = share_end - share_start;
        char share_name[buffer_size];
        memset(share_name, 0, buffer_size);
        strncpy(share_name, share_start, share_len);

        // Construct UNC prefix.
        if (is_windows_path) {
          int ret = snprintf(unc_prefix, buffer_size, "\\\\%s\\%s", server_name,
                             share_name);
          if (ret < 0 || (size_t)ret >= buffer_size) {
            assert(0 && "Buffer too small or encoding error.");
            return false;  // Buffer too small or encoding error.
          }
        } else {
          int ret = snprintf(unc_prefix, buffer_size, "//%s/%s", server_name,
                             share_name);
          if (ret < 0 || (size_t)ret >= buffer_size) {
            assert(0 && "Buffer too small or encoding error.");
            return false;  // Buffer too small or encoding error.
          }
        }

        path_to_tokenize = share_end;
      } else {
        // No path after the share - just keep the whole string as-is.
        // Convert back to original separator style.
        char *result = strdup(path);  // Use original path.
        if (!result) {
          return false;  // Memory allocation failed.
        }
        strcpy(normalized_path, result);
        free(result);
        return true;
      }
    } else {
      // Just server name, no share - keep as-is.
      // Convert back to original separator style.
      char *result = strdup(path);  // Use original path.
      if (!result) {
        assert(0 && "Memory allocation failed.");
        return false;  // Memory allocation failed
      }
      strcpy(normalized_path, result);
      free(result);
      return true;
    }
  }

  // Handle drive letter prefix for Windows.
  char drive_prefix[4] = {0};
  if (has_drive_letter) {
    // Save the drive prefix (e.g., "C:").
    strncpy(drive_prefix, normalized_path, 2);
    drive_prefix[2] = '\0';
    path_to_tokenize = normalized_path + 2;

    // If there's a separator after the drive letter, skip it.
    if (*path_to_tokenize == '/') {
      path_to_tokenize++;
    }
  }

  // Make a copy of path_to_tokenize since strtok modifies it.
  char *path_copy = strdup(path_to_tokenize);
  if (!path_copy) {
    assert(0 && "Memory allocation failed.");
    return false;  // Memory allocation failed
  }

  // Use a stack to track directories.
  char **stack = (char **)malloc(
      buffer_size * sizeof(char *));  // Max possible depth is path length
  if (!stack) {
    assert(0 && "Memory allocation failed.");
    free(path_copy);
    return false;  // Memory allocation failed
  }

  // Initialize stack to NULL pointers to make cleanup easier.
  for (size_t i = 0; i < buffer_size; i++) {
    stack[i] = NULL;
  }

  int stack_size = 0;
  bool cleanup_failed = false;

  // Start tokenizing the path.
  char *saveptr = NULL;  // For thread-safe strtok_r.
  char *token = strtok_r(path_copy, "/", &saveptr);

  while (token != NULL && !cleanup_failed) {
    // Current directory (.) - skip it.
    if (strcmp(token, ".") == 0) {
      // Do nothing.
    }
    // Parent directory (..) - pop from stack if possible.
    else if (strcmp(token, "..") == 0) {
      if (stack_size > 0 && strcmp(stack[stack_size - 1], "..") != 0) {
        // Only pop if the top of stack is not a parent directory.
        free(stack[stack_size - 1]);  // Free popped item.
        stack[stack_size - 1] = NULL;
        stack_size--;  // Pop from stack.
      } else if (!is_absolute) {
        // For relative paths, add .. if we're at root or if top of stack is
        // already a ..
        stack[stack_size] = strdup("..");
        if (!stack[stack_size]) {
          cleanup_failed = true;
          break;
        }
        stack_size++;
      }
      // For absolute paths, we ignore .. at root.
    }
    // Regular directory component - push to stack.
    else {
      stack[stack_size] = strdup(token);
      if (!stack[stack_size]) {
        cleanup_failed = true;
        break;
      }
      stack_size++;
    }

    token = strtok_r(NULL, "/", &saveptr);
  }

  free(path_copy);

  if (cleanup_failed) {
    // Clean up all allocated memory in case of failure.
    for (size_t i = 0; i < buffer_size; i++) {
      if (stack[i]) {
        free(stack[i]);
      }
    }
    free((void *)stack);
    return false;
  }

  // Allocate result path buffer.
  char *result_path = malloc(buffer_size);
  if (!result_path) {
    // Memory allocation failed, clean up.
    for (size_t i = 0; i < buffer_size; i++) {
      if (stack[i]) {
        free(stack[i]);
      }
    }
    free((void *)stack);
    return false;
  }
  result_path[0] = '\0';

  // Choose the appropriate separator based on path type.
  char separator = (is_windows_path) ? '\\' : '/';

  // Reconstruct the path from the stack.
  // Handle drive letter or UNC prefix.
  if (has_drive_letter) {
    strcpy(result_path, drive_prefix);
    // Always add a separator after drive letter.
    char sep_str[2] = {separator, '\0'};
    strcat(result_path, sep_str);
  } else if (is_unc_path) {
    strcpy(result_path, unc_prefix);
    // Always add a separator after UNC prefix if it doesn't already end with
    // one.
    size_t prefix_len = strlen(result_path);
    if (prefix_len > 0 && result_path[prefix_len - 1] != separator) {
      char sep_str[2] = {separator, '\0'};
      strcat(result_path, sep_str);
    }
  } else if (is_absolute) {
    char sep_str[2] = {separator, '\0'};
    strcpy(result_path, sep_str);
  } else {
    result_path[0] = '\0';  // For relative paths, start with empty string.
  }

  // Join the remaining components.
  for (int i = 0; i < stack_size; i++) {
    if (i > 0) {
      // Add separator between components.
      char sep_str[2] = {separator, '\0'};
      strcat(result_path, sep_str);
    } else if (!has_drive_letter && !is_unc_path && !is_absolute) {
      // No separator needed for first component of relative path.
    } else if (i == 0 && result_path[0] != '\0' &&
               result_path[strlen(result_path) - 1] != separator) {
      // Add separator if needed after prefix.
      char sep_str[2] = {separator, '\0'};
      strcat(result_path, sep_str);
    }

    strcat(result_path, stack[i]);
    free(stack[i]);  // Free each stack item as we use it.
    stack[i] = NULL;
  }

  // Clean up any remaining stack items (shouldn't be any).
  for (size_t i = 0; i < buffer_size; i++) {
    if (stack[i]) {
      free(stack[i]);
    }
  }
  free((void *)stack);

  // If the result is empty, use "." for relative paths or proper root for
  // absolute paths.
  if (result_path[0] == '\0') {
    if (has_drive_letter) {
      strcpy(result_path, drive_prefix);
      char sep_str[2] = {separator, '\0'};
      strcat(result_path, sep_str);
    } else if (is_unc_path) {
      strcpy(result_path, unc_prefix);
    } else if (is_absolute) {
      char sep_str[2] = {separator, '\0'};
      strcpy(result_path, sep_str);
    } else {
      strcpy(result_path, ".");  // Empty relative path becomes "."
    }
  }

  // Copy result to output buffer.
  if (strlen(result_path) >= buffer_size) {
    free(result_path);
    return false;  // Result too large for buffer.
  }

  strcpy(normalized_path, result_path);
  free(result_path);

  return true;
}
