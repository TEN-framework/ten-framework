//
// Copyright © 2025 Agora
// This file is part of TEN Framework, an open source project.
// Licensed under the Apache License, Version 2.0, with certain conditions.
// Refer to the "LICENSE" file in the root directory for more information.
//
#include <fcntl.h>
#include <gtest/gtest.h>

#include <cstring>

#include "include_internal/ten_utils/backtrace/platform/posix/file.h"

namespace {

// Test fixture for backtrace file operations.
class FileTestPosix : public ::testing::Test {
 protected:
  void SetUp() override {
    // Create a temporary test file.
    test_filename_ = "ten_file_test_file.txt";
    test_data_ = "This is test data for the file operations test.";

    // Create and write to test file.
    FILE *fp = fopen(test_filename_.c_str(), "w");
    ASSERT_NE(fp, nullptr) << "Failed to create test file: " << strerror(errno);

    // Write test data to the file.
    size_t bytes_written = fwrite(test_data_, 1, strlen(test_data_), fp);
    ASSERT_EQ(bytes_written, strlen(test_data_))
        << "Failed to write test data: " << strerror(errno);

    // Close the file.
    fclose(fp);
  }

  void TearDown() override {
    // Remove the test file.
    if (!test_filename_.empty()) {
      unlink(test_filename_.c_str());
    }
  }

  std::string test_filename_;
  const char *test_data_ = nullptr;
};

// Test successful file opening and closing
TEST_F(FileTestPosix, OpenAndCloseSuccess) {
  // Open the file.
  int fd = ten_backtrace_open_file(test_filename_.c_str(), nullptr);
  ASSERT_GE(fd, 0) << "Failed to open test file: " << strerror(errno);

  // Verify the file descriptor is valid by reading from it.
  char buffer[100] = {0};
  ssize_t bytes_read = read(fd, buffer, sizeof(buffer) - 1);
  ASSERT_GT(bytes_read, 0) << "Failed to read from file: " << strerror(errno);
  EXPECT_STREQ(buffer, test_data_)
      << "File content does not match expected data";

  // Close the file and verify success.
  bool close_result = ten_backtrace_close_file(fd);
  EXPECT_TRUE(close_result) << "Failed to close file: " << strerror(errno);

  // Verify file descriptor is closed by trying to use it (should fail).
  bytes_read = read(fd, buffer, 1);
  EXPECT_EQ(bytes_read, -1) << "File descriptor still valid after close";
  EXPECT_EQ(errno, EBADF) << "Expected 'bad file descriptor' error";
}

// Test the does_not_exist parameter.
TEST_F(FileTestPosix, DoesNotExistFlag) {
  const char *nonexistent_file = "file_that_does_not_exist.txt";
  bool does_not_exist = false;

  // Try to open a non-existent file.
  int fd = ten_backtrace_open_file(nonexistent_file, &does_not_exist);
  EXPECT_EQ(fd, -1) << "Expected failure when opening non-existent file";
  EXPECT_TRUE(does_not_exist) << "does_not_exist flag not set correctly";

  // Open an existing file.
  does_not_exist = true;  // Reset to opposite value.
  fd = ten_backtrace_open_file(test_filename_.c_str(), &does_not_exist);
  EXPECT_GE(fd, 0) << "Failed to open existing file: " << strerror(errno);
  EXPECT_FALSE(does_not_exist)
      << "does_not_exist flag incorrectly set for existing file";

  // Clean up.
  if (fd >= 0) {
    ten_backtrace_close_file(fd);
  }
}

}  // namespace
