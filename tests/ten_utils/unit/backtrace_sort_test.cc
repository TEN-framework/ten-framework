//
// Copyright © 2025 Agora
// This file is part of TEN Framework, an open source project.
// Licensed under the Apache License, Version 2.0, with certain conditions.
// Refer to the "LICENSE" file in the root directory for more information.
//
#include <cassert>
#include <climits>
#include <cstdlib>
#include <cstring>
#include <ctime>

#include "gtest/gtest.h"
#include "include_internal/ten_utils/backtrace/sort.h"
#include "ten_utils/macro/check.h"
#include "ten_utils/macro/memory.h"

// Compare function - integer ascending.
static int compare_int_asc(const void *a, const void *b) {
  int va = *(int *)a;
  int vb = *(int *)b;
  if (va < vb) return -1;
  if (va > vb) return 1;
  return 0;
}

// Compare function - integer descending.
static int compare_int_desc(const void *a, const void *b) {
  int va = *(int *)a;
  int vb = *(int *)b;
  if (va > vb) return -1;
  if (va < vb) return 1;
  return 0;
}

// Compare function - double.
static int compare_double(const void *a, const void *b) {
  double da = *(double *)a;
  double db = *(double *)b;
  if (da < db) {
    return -1;
  }
  if (da > db) {
    return 1;
  }
  return 0;
}

// Compare two arrays.
static bool arrays_equal(const void *arr1, const void *arr2, size_t count,
                         size_t size) {
  return memcmp(arr1, arr2, count * size) == 0;
}

// Test structure.
typedef struct {
  int key;
  char value[16];
} test_struct_t;

// Compare function for structure.
static int compare_struct(const void *a, const void *b) {
  int key_a = ((test_struct_t *)a)->key;
  int key_b = ((test_struct_t *)b)->key;
  if (key_a < key_b) return -1;
  if (key_a > key_b) return 1;
  return 0;
}

// Generate random integer array.
static void generate_random_array(int *arr, size_t count, int max_value) {
  for (size_t i = 0; i < count; i++) {
    arr[i] = rand() % max_value;
  }
}

TEST(BacktraceSortTest, Single) {
  int single_arr[] = {42};
  int expected_single[] = {42};

  backtrace_sort(single_arr, 1, sizeof(int), compare_int_asc);
  EXPECT_EQ(arrays_equal(single_arr, expected_single, 1, sizeof(int)), true);
}

TEST(BacktraceSortTest, SortedArray) {
  int arr[] = {1, 2, 3, 4, 5, 6, 7, 8, 9, 10};
  int expected[] = {1, 2, 3, 4, 5, 6, 7, 8, 9, 10};
  size_t count = sizeof(arr) / sizeof(arr[0]);

  backtrace_sort(arr, count, sizeof(int), compare_int_asc);
  EXPECT_EQ(arrays_equal(arr, expected, count, sizeof(int)), true);
}

TEST(BacktraceSortTest, ReverseSortedArray) {
  int arr[] = {10, 9, 8, 7, 6, 5, 4, 3, 2, 1};
  int expected[] = {1, 2, 3, 4, 5, 6, 7, 8, 9, 10};
  size_t count = sizeof(arr) / sizeof(arr[0]);

  backtrace_sort(arr, count, sizeof(int), compare_int_asc);
  EXPECT_EQ(arrays_equal(arr, expected, count, sizeof(int)), true);
}

TEST(BacktraceSortTest, SmallArray) {
  int arr[] = {3, 1, 4, 1, 5, 9, 2, 6};
  int expected[] = {1, 1, 2, 3, 4, 5, 6, 9};
  size_t count = sizeof(arr) / sizeof(arr[0]);

  backtrace_sort(arr, count, sizeof(int), compare_int_asc);
  EXPECT_EQ(arrays_equal(arr, expected, count, sizeof(int)), true);
}

TEST(BacktraceSortTest, LargeArray) {
#define LARGE_SIZE 100
  int arr[LARGE_SIZE];
  int expected[LARGE_SIZE];

  srand(42);
  generate_random_array(arr, LARGE_SIZE, 1000);

  memcpy(expected, arr, sizeof(arr));

  qsort(expected, LARGE_SIZE, sizeof(int), compare_int_asc);
  backtrace_sort(arr, LARGE_SIZE, sizeof(int), compare_int_asc);

  EXPECT_EQ(arrays_equal(arr, expected, LARGE_SIZE, sizeof(int)), true);
}

TEST(BacktraceSortTest, DescendingSort) {
  int arr[] = {3, 1, 4, 1, 5, 9, 2, 6};
  int expected[] = {9, 6, 5, 4, 3, 2, 1, 1};
  size_t count = sizeof(arr) / sizeof(arr[0]);

  backtrace_sort(arr, count, sizeof(int), compare_int_desc);
  EXPECT_EQ(arrays_equal(arr, expected, count, sizeof(int)), true);
}

TEST(BacktraceSortTest, DoubleSort) {
  double arr[] = {3.14, 1.41, 2.71, 0.0, -1.0, 42.0};
  double expected[] = {-1.0, 0.0, 1.41, 2.71, 3.14, 42.0};
  size_t count = sizeof(arr) / sizeof(arr[0]);

  backtrace_sort(arr, count, sizeof(double), compare_double);
  EXPECT_EQ(arrays_equal(arr, expected, count, sizeof(double)), true);
}

TEST(BacktraceSortTest, StructSort) {
  test_struct_t arr[] = {
      {5, "five"}, {3, "three"}, {1, "one"}, {4, "four"}, {2, "two"}};

  test_struct_t expected[] = {
      {1, "one"}, {2, "two"}, {3, "three"}, {4, "four"}, {5, "five"}};

  size_t count = sizeof(arr) / sizeof(arr[0]);

  backtrace_sort(arr, count, sizeof(test_struct_t), compare_struct);
  EXPECT_EQ(arrays_equal(arr, expected, count, sizeof(test_struct_t)), true);
}

TEST(BacktraceSortTest, DuplicateElements) {
  int arr[] = {3, 1, 3, 1, 3, 1, 3, 1};
  int expected[] = {1, 1, 1, 1, 3, 3, 3, 3};
  size_t count = sizeof(arr) / sizeof(arr[0]);

  backtrace_sort(arr, count, sizeof(int), compare_int_asc);
  EXPECT_EQ(arrays_equal(arr, expected, count, sizeof(int)), true);
}

TEST(BacktraceSortTest, ExtremeValues) {
  int arr[] = {INT_MAX, 0, INT_MIN, 42, -42};
  int expected[] = {INT_MIN, -42, 0, 42, INT_MAX};
  size_t count = sizeof(arr) / sizeof(arr[0]);

  backtrace_sort(arr, count, sizeof(int), compare_int_asc);
  EXPECT_EQ(arrays_equal(arr, expected, count, sizeof(int)), true);
}

TEST(BacktraceSortTest, Stress) {
#define STRESS_SIZE 10000
  int *arr = (int *)TEN_MALLOC(STRESS_SIZE * sizeof(int));
  int *expected = (int *)TEN_MALLOC(STRESS_SIZE * sizeof(int));

  if ((arr == nullptr) || (expected == nullptr)) {
    TEN_FREE(arr);
    TEN_FREE(expected);
    TEN_ASSERT(0, "Failed to allocate memory");
  }

  srand(time(nullptr));
  generate_random_array(arr, STRESS_SIZE, 1000000);
  memcpy(expected, arr, STRESS_SIZE * sizeof(int));

  qsort(expected, STRESS_SIZE, sizeof(int), compare_int_asc);
  backtrace_sort(arr, STRESS_SIZE, sizeof(int), compare_int_asc);

  EXPECT_EQ(arrays_equal(arr, expected, STRESS_SIZE, sizeof(int)), true);

  TEN_FREE(arr);
  TEN_FREE(expected);
}
