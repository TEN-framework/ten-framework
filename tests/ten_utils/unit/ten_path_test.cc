//
// Copyright © 2025 Agora
// This file is part of TEN Framework, an open source project.
// Licensed under the Apache License, Version 2.0, with certain conditions.
// Refer to the "LICENSE" file in the root directory for more information.
//
#include "common/test_utils.h"
#include "gtest/gtest.h"
#include "ten_utils/lang/cpp/lib/string.h"
#include "ten_utils/lib/path.h"
#include "ten_utils/lib/string.h"

static int foo() { return 0; }

TEST(PathTest, positive) {
  ten::TenString str = ten_path_get_cwd();
  const ten::TenString cwd = str;
  EXPECT_FALSE(str.empty());
  AGO_LOG("Current working directory: %s\n", str.c_str());
  str = ten_path_get_home_path();
  EXPECT_FALSE(str.empty());
  AGO_LOG("Current user home directory: %s\n", str.c_str());
  str = ten_path_get_module_path((const void *)foo);
  EXPECT_FALSE(str.empty());
  AGO_LOG("Module path: %s\n", str.c_str());
  str = ten_path_get_executable_path();
  EXPECT_FALSE(str.empty());
  AGO_LOG("Executable file path: %s\n", str.c_str());
  str = "/aaa/bbb/ccc.txt";
  ten::TenString leaf = ten_path_get_filename(str);
  AGO_LOG("Leaf of %s is: %s\n", str.c_str(), leaf.c_str());
  ten::TenString dir = ten_path_get_dirname(str);
  AGO_LOG("Dir of %s is: %s\n", str.c_str(), dir.c_str());
  EXPECT_EQ(leaf == "ccc.txt", true);
  EXPECT_EQ(dir == "/aaa/bbb", true);
  str = "/aaa";
  leaf = ten_path_get_filename(str);
  AGO_LOG("Leaf of %s is: %s\n", str.c_str(), leaf.c_str());
  dir = ten_path_get_dirname(str);
  AGO_LOG("Dir of %s is: %s\n", str.c_str(), dir.c_str());
  EXPECT_EQ(leaf == "aaa", true);
  EXPECT_EQ(dir == "/", true);
  str = "/";
  leaf = ten_path_get_filename(str);
  AGO_LOG("Leaf of %s is: %s\n", str.c_str(), leaf.c_str());
  dir = ten_path_get_dirname(str);
  AGO_LOG("Dir of %s is: %s\n", str.c_str(), dir.c_str());
  EXPECT_TRUE(leaf.empty());
  EXPECT_EQ(dir == "/", true);
  str = cwd + "/.";
  ten::TenString abs = ten_path_realpath(str);
  AGO_LOG("Absolute path of %s is: %s\n", str.c_str(), abs.c_str());
  EXPECT_EQ(abs == cwd, true);
  str = cwd + "/..";
  abs = ten_path_realpath(str);
  dir = ten_path_get_dirname(cwd);
  AGO_LOG("Absolute path of %s is: %s\n", str.c_str(), abs.c_str());
  EXPECT_EQ(abs == dir, true);
  str = cwd + "/../.";
  abs = ten_path_realpath(str);
  AGO_LOG("Absolute path of %s is: %s\n", str.c_str(), abs.c_str());
  EXPECT_EQ(abs == dir, true);
  str = ten_path_get_cwd();
  EXPECT_EQ(ten_path_is_dir(str), true);
  str = "aaa/bbb/.";
  EXPECT_EQ(ten_path_is_special_dir(str), true);
  str = "aaa/bbb/..";
  EXPECT_EQ(ten_path_is_special_dir(str), true);
  str = "aaa/bbb/../../ccc.txt";
  EXPECT_EQ(ten_path_is_special_dir(str), false);
  str = ".";
  EXPECT_EQ(ten_path_is_special_dir(str), true);
  str = "..";
  EXPECT_EQ(ten_path_is_special_dir(str), true);
  str = "aaa/bbb/ccc.so";
  EXPECT_EQ(ten_path_is_shared_library(str), true);
  str = "aaa/bbb/ccc.dll";
  EXPECT_EQ(ten_path_is_shared_library(str), true);
  str = "aaa/bbb/ccc.dylib";
  EXPECT_EQ(ten_path_is_shared_library(str), true);
  str = "aaa/bbb/ccc.txt";
  EXPECT_EQ(ten_path_is_shared_library(str), false);
  str = ten_path_get_cwd();
  EXPECT_EQ(ten_path_exists(str.c_str()), true);
  str += "/definitly_not_existing";
  EXPECT_EQ(ten_path_exists(str.c_str()), false);
  str = ten_path_get_cwd();
  auto *fd = ten_path_open_dir(ten_string_get_raw_str(str));
  EXPECT_NE(fd, nullptr);
  EXPECT_EQ(ten_path_close_dir(fd), 0);
}
