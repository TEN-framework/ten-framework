//
// Copyright © 2025 Agora
// This file is part of TEN Framework, an open source project.
// Licensed under the Apache License, Version 2.0, with certain conditions.
// Refer to the "LICENSE" file in the root directory for more information.
//
#include <fcntl.h>
#include <gtest/gtest.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

#include "include_internal/ten_utils/backtrace/platform/posix/mmap.h"

namespace {

// Test fixture for MMAP tests.
class TenMmapTest : public ::testing::Test {
 protected:
  void SetUp() override {
    // Create a temporary test file.
    const char *test_filename = "ten_mmap_test_file.txt";
    test_data_ = "This is test data for the MMAP functionality test.";
    test_data_size_ = strlen(test_data_);

    // Create and write to test file.
    fd_ = open(test_filename, O_RDWR | O_CREAT | O_TRUNC, 0644);
    ASSERT_GE(fd_, 0) << "Failed to create test file: " << strerror(errno);

    // Write test data to the file.
    ssize_t bytes_written = write(fd_, test_data_, test_data_size_);
    ASSERT_EQ(bytes_written, static_cast<ssize_t>(test_data_size_))
        << "Failed to write test data: " << strerror(errno);

    // Store the filename for cleanup.
    test_filename_ = test_filename;
  }

  void TearDown() override {
    // Close file if open.
    if (fd_ >= 0) {
      close(fd_);
    }

    // Remove the test file.
    if (!test_filename_.empty()) {
      unlink(test_filename_.c_str());
    }
  }

  int fd_ = -1;
  std::string test_filename_;
  const char *test_data_ = nullptr;
  size_t test_data_size_ = 0;
};

// Test successful mapping of a file.
TEST_F(TenMmapTest, SuccessfulMapping) {
  ten_mmap_t mmap;
  bool result = ten_mmap_init(&mmap, fd_, 0, test_data_size_);

  ASSERT_TRUE(result) << "Failed to initialize mmap: " << strerror(errno);
  ASSERT_NE(mmap.data, nullptr) << "Mapped data pointer is null";
  ASSERT_NE(mmap.base, nullptr) << "Mapped base pointer is null";
  ASSERT_GT(mmap.len, 0U) << "Mapped length is zero";

  // Verify the mapped data matches the original.
  EXPECT_EQ(0, memcmp(mmap.data, test_data_, test_data_size_))
      << "Mapped data does not match original data";

  // Clean up.
  ten_mmap_deinit(&mmap);
}

// Test mapping with an offset.
TEST_F(TenMmapTest, MappingWithOffset) {
  const off_t offset = 5;  // Skip the first 5 bytes.
  ten_mmap_t mmap;
  bool result = ten_mmap_init(&mmap, fd_, offset, test_data_size_ - offset);

  ASSERT_TRUE(result) << "Failed to initialize mmap with offset: "
                      << strerror(errno);
  ASSERT_NE(mmap.data, nullptr) << "Mapped data pointer is null";

  // Verify the mapped data matches the original with offset.
  EXPECT_EQ(0, memcmp(mmap.data, test_data_ + offset, test_data_size_ - offset))
      << "Mapped data with offset does not match expected section of original "
         "data";

  // Clean up.
  ten_mmap_deinit(&mmap);
}

// Test multiple mappings and deinitialization.
TEST_F(TenMmapTest, MultipleMapAndDeinit) {
  ten_mmap_t mmap1;
  ten_mmap_t mmap2;

  // Create two mappings of the same file.
  bool result1 = ten_mmap_init(&mmap1, fd_, 0, test_data_size_);
  bool result2 = ten_mmap_init(&mmap2, fd_, 0, test_data_size_);

  ASSERT_TRUE(result1) << "Failed to initialize first mmap";
  ASSERT_TRUE(result2) << "Failed to initialize second mmap";

  // Verify both mappings are valid and contain correct data.
  EXPECT_EQ(0, memcmp(mmap1.data, test_data_, test_data_size_));
  EXPECT_EQ(0, memcmp(mmap2.data, test_data_, test_data_size_));

  // Clean up both mappings.
  ten_mmap_deinit(&mmap1);
  ten_mmap_deinit(&mmap2);

  // Verify the structure was cleaned up correctly.
  EXPECT_EQ(nullptr, mmap1.data);
  EXPECT_EQ(nullptr, mmap1.base);
  EXPECT_EQ(0U, mmap1.len);
}

}  // namespace
