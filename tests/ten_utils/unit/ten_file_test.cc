//
// Copyright © 2025 Agora
// This file is part of TEN Framework, an open source project.
// Licensed under the Apache License, Version 2.0, with certain conditions.
// Refer to the "LICENSE" file in the root directory for more information.
//
#include <fcntl.h>
#include <gtest/gtest.h>

#include <cstring>

#include "include_internal/ten_utils/backtrace/file.h"

// Test path normalization
TEST(FileTest, PathNormalization) {
  struct TestCase {
    const char *input;
    const char *expected;
  };

  // Define test cases.
  TestCase test_cases[] = {
      // Basic tests.
      {"", "."},
      {".", "."},
      {"/", "/"},

      // Simple paths.
      {"/a/b/c", "/a/b/c"},
      {"a/b/c", "a/b/c"},

      // Current directory tests.
      {"/a/./b/./c", "/a/b/c"},
      {"a/./b/./c", "a/b/c"},
      {"./a/b/c", "a/b/c"},

      // Parent directory tests.
      {"/a/b/../c", "/a/c"},
      {"a/b/../c", "a/c"},
      {"/a/b/../../c", "/c"},
      {"a/b/../../c", "c"},
      {"/a/b/c/../../d", "/a/d"},
      {"a/b/c/../../d", "a/d"},

      // Complex paths.
      {"/home/wei/MyData/MyProject/ten_framework/out/linux/x64/../../../core/"
       "src/ten_utils/backtrace/platform/posix/linux/backtrace.c",
       "/home/wei/MyData/MyProject/ten_framework/core/src/ten_utils/backtrace/"
       "platform/posix/linux/backtrace.c"},

      // Edge cases.
      {"/../a", "/a"},
      {"../..", "../.."},
      {"/a/b/c/..", "/a/b"},
      {"a/b/c/..", "a/b"},
      {"a/../..", ".."},
      {"/a/../..", "/"},
      {"//a//b//c", "/a/b/c"},
      {"a//b//c", "a/b/c"},

      // Windows-specific path tests.
      {"C:\\Users\\user\\Documents\\..\\Downloads\\file.txt",
       "C:\\Users\\user\\Downloads\\file.txt"},
      {"C:\\Users\\user\\.\\Documents", "C:\\Users\\user\\Documents"},
      {"C:\\Users\\..\\Program Files\\App", "C:\\Program Files\\App"},
      {"C:\\a\\b\\..\\..\\c", "C:\\c"},
      {"C:", "C:\\"},
      {"D:\\a\\b\\c", "D:\\a\\b\\c"},
      {"c:\\windows\\system32", "c:\\windows\\system32"},
      {"\\\\server\\share\\folder\\file.txt",
       "\\\\server\\share\\folder\\file.txt"},
      {"\\\\server\\share\\folder\\..\\other", "\\\\server\\share\\other"},
      {"\\\\server\\share\\.\\folder", "\\\\server\\share\\folder"},
      {"\\\\server\\share", "\\\\server\\share"},
      {"\\\\server\\share\\a\\b\\..\\..\\c", "\\\\server\\share\\c"},
      {"C:/Users/user/Documents/../Downloads/file.txt",
       "C:\\Users\\user\\Downloads\\file.txt"},
  };

  // Test each case.
  for (const auto &test : test_cases) {
    char normalized[4096] = {0};
    ASSERT_TRUE(ten_backtrace_normalize_path(test.input, normalized,
                                             sizeof(normalized)))
        << "Failed to normalize: " << test.input;
    EXPECT_STREQ(normalized, test.expected)
        << "Incorrect normalization for input: " << test.input
        << ", got: " << normalized << ", expected: " << test.expected;
  }
}

TEST(FileTest, WindowsPathNormalization) {
  struct TestCase {
    const char *input;
    const char *expected;
  };

  // Windows-specific test cases focusing on edge cases.
  TestCase windows_test_cases[] = {
      // Multiple parent directories and nested edge cases.
      {"C:\\a\\..\\..\\b", "C:\\b"},
      {"C:\\..\\..\\..\\a", "C:\\a"},
      {"C:\\a\\b\\..\\..\\..\\..\\c", "C:\\c"},
      {"\\\\server\\share\\a\\..\\b", "\\\\server\\share\\b"},
      {"\\\\server\\share\\..\\other", "\\\\server\\share\\other"},
      {"C:\\temp\\..\\", "C:\\"},
      {"C:\\temp\\..", "C:\\"},
      {"C:\\.", "C:\\"},
      {"C:\\.\\", "C:\\"},
      {"C:\\a\\.\\b", "C:\\a\\b"},

      // Mixed slashes.
      {"C:/a\\b/c\\d", "C:\\a\\b\\c\\d"},
      {"\\\\server/share\\folder", "\\\\server\\share\\folder"},

      // Drive letter edge cases.
      {"C:", "C:\\"},
      {"C:\\", "C:\\"},
      {"C:.", "C:\\"},
      {"C:.\\temp", "C:\\temp"},
      {"c:\\windows", "c:\\windows"},

      // Test cases for relative paths with Windows backslashes.
      // Note: These are detected as Windows paths due to backslashes.
      {"a\\b\\..\\c", "a\\c"},
      {"..\\..\\a", "..\\..\\a"},
      {".\\a\\.\\b", "a\\b"}};

  // Test each Windows-specific case.
  for (const auto &test : windows_test_cases) {
    char normalized[4096] = {0};
    ASSERT_TRUE(ten_backtrace_normalize_path(test.input, normalized,
                                             sizeof(normalized)))
        << "Failed to normalize Windows path: " << test.input;
    EXPECT_STREQ(normalized, test.expected)
        << "Incorrect normalization for Windows path: " << test.input
        << ", got: " << normalized << ", expected: " << test.expected;
  }
}
