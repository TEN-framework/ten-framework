//
// Copyright © 2025 Agora
// This file is part of TEN Framework, an open source project.
// Licensed under the Apache License, Version 2.0, with certain conditions.
// Refer to the "LICENSE" file in the root directory for more information.
//
#include <limits.h>
#include <stdint.h>

#include <atomic>
#include <chrono>
#include <functional>
#include <mutex>
#include <thread>
#include <unordered_map>
#include <vector>

#include "common/test_utils.h"
#include "gtest/gtest.h"
#include "ten_utils/lib/event.h"
#include "ten_utils/lib/rwlock.h"
#include "ten_utils/lib/time.h"

#define TEST_THREAD_MAX 50

/**
 * Check the soundness of WrRdLock
 * Performance check should be done on micro benchmark
 */

struct ReaderLockGuard {
  explicit ReaderLockGuard(ten_rwlock_t *lock) : lock_(lock) {
    ten_rwlock_lock(lock_, 1);
  }

  ~ReaderLockGuard() { ten_rwlock_unlock(lock_, 1); }

 private:
  ten_rwlock_t *lock_;
};

struct WriterLockGuard {
  explicit WriterLockGuard(ten_rwlock_t *lock) : lock_(lock) {
    ten_rwlock_lock(lock_, 0);
  }

  ~WriterLockGuard() { ten_rwlock_unlock(lock_, 0); }

 private:
  ten_rwlock_t *lock_;
};

struct RWLockStatistic {
  struct Statistic {
    std::atomic<uint64_t> count = {0};
    std::atomic<uint64_t> total_us = {0};
    std::atomic<uint64_t> min_us = {UINT64_MAX};
    std::atomic<uint64_t> max_us = {0};
    std::atomic<int32_t> min_concurr = {INT32_MAX};
    std::atomic<int32_t> max_concurr = {0};
  } Reader, Writer;
};

struct ThreadBalancerStatistic {
  struct Balancer {
    int min = INT_MAX;
    int max = 0;
    int average = 0;
  } Reader, Writer;
};

static void RunRWLockTest(ten_rwlock_t *lut, const std::string &impl,
                          int test_ms, int write_threads, int read_threads,
                          bool perf = false) {
  RWLockStatistic stats;
  std::vector<std::thread> writers;
  std::vector<std::thread> readers;
  ten_event_t *start_event = ten_event_create(0, 0);
  std::atomic<bool> stop = {false};
  std::atomic<int32_t> read_concurr = {0};
  std::atomic<int32_t> write_concurr = {0};
  std::unordered_map<std::thread::id, int> writer_actives;
  std::mutex writer_actives_lock;
  std::unordered_map<std::thread::id, int> reader_actives;
  std::mutex reader_actives_lock;

  std::srand(static_cast<unsigned>(time(NULL)));

  auto writer_op = [&start_event, &lut, &stop, &read_concurr, &write_concurr,
                    &stats, &writer_actives, &writer_actives_lock, perf] {
    int *actives = nullptr;
    {
      std::lock_guard<std::mutex> _(writer_actives_lock);
      actives = &writer_actives[std::this_thread::get_id()];
    }

    // int id = writer_id++;
    // AGO_LOG("[writer] % 2d: waiting for start event", id);
    ten_event_wait(start_event, -1);

    while (!stop) {
      auto now = ten_current_time_ms();
      WriterLockGuard _(lut);
      auto dur = ten_current_time_ms() - now;
      if (actives) {
        (*actives)++;
      }

      // AGO_LOG("[writer] % 2d: now increase concurr cnt", id);
      ++write_concurr;
      // no reader if writer active
      // and only 1 writer can active
      if (!perf) {
        ASSERT_EQ(read_concurr, 0);
        ASSERT_EQ(write_concurr, 1);
      }

      ++stats.Writer.count;
      stats.Writer.total_us += dur;
      // not accurate but OK
      if (stats.Writer.min_us > dur) stats.Writer.min_us = dur;
      if (stats.Writer.max_us < dur) stats.Writer.max_us = dur;
      if (stats.Writer.min_concurr > write_concurr)
        stats.Writer.min_concurr = write_concurr.load();
      if (stats.Writer.max_concurr < write_concurr)
        stats.Writer.max_concurr = write_concurr.load();

      // no reader if writer active
      // and only 1 writer can active
      if (!perf) {
        ASSERT_EQ(read_concurr, 0);
        ASSERT_EQ(write_concurr, 1);
      }

      // AGO_LOG("[writer] % 2d: now decrease concurr cnt", id);
      --write_concurr;
      if (!perf) {
        ASSERT_EQ(read_concurr, 0);
        ASSERT_EQ(write_concurr, 0);
      }
    }
  };

  auto reader_op = [&start_event, &lut, &stop, &read_concurr, &write_concurr,
                    &stats, &reader_actives, &reader_actives_lock, perf] {
    // int id = reader_id++;
    // AGO_LOG("[reader] % 2d: waiting for start event", id);
    int *actives = nullptr;
    {
      std::lock_guard<std::mutex> _(reader_actives_lock);
      actives = &reader_actives[std::this_thread::get_id()];
    }

    ten_event_wait(start_event, -1);

    while (!stop) {
      auto now = ten_current_time_ms();
      ReaderLockGuard _(lut);
      auto dur = ten_current_time_ms() - now;
      if (actives) {
        (*actives)++;
      }

      // AGO_LOG("[reader] % 2d: now increase concurr cnt", id);
      ++read_concurr;
      // no writer if reader active
      if (!perf) {
        ASSERT_EQ(write_concurr, 0);
      }

      ++stats.Reader.count;
      stats.Reader.total_us += dur;
      // not accurate but OK (especially for 'read_concurr', it may change at
      // any time)
      if (stats.Reader.min_us > dur) stats.Reader.min_us = dur;
      if (stats.Reader.max_us < dur) stats.Reader.max_us = dur;
      if (stats.Reader.min_concurr > read_concurr)
        stats.Reader.min_concurr = read_concurr.load();
      if (stats.Reader.max_concurr < read_concurr)
        stats.Reader.max_concurr = read_concurr.load();

      // no writer if reader active
      if (!perf) {
        ASSERT_EQ(write_concurr, 0);
      }

      // AGO_LOG("[reader] % 2d: now decrease concurr cnt", id);
      --read_concurr;
      // no writer if reader active
      if (!perf) {
        ASSERT_EQ(write_concurr, 0);
      }
    }
  };

  // prepare
  writers.reserve(write_threads);
  for (int i = 0; i < write_threads; i++) {
    writers.emplace_back(std::thread(writer_op));
  }

  readers.reserve(read_threads);
  for (int i = 0; i < read_threads; i++) {
    readers.emplace_back(std::thread(reader_op));
  }

  // go
  ten_event_set(start_event);

  std::this_thread::sleep_for(std::chrono::milliseconds(test_ms));

  // done
  stop = true;

  std::for_each(writers.begin(), writers.end(),
                std::mem_fn(&std::thread::join));
  std::for_each(readers.begin(), readers.end(),
                std::mem_fn(&std::thread::join));

  ThreadBalancerStatistic balancer;
  int total = 0;
  for (auto &c : writer_actives) {
    if (balancer.Writer.min > c.second) balancer.Writer.min = c.second;
    if (balancer.Writer.max < c.second) balancer.Writer.max = c.second;
    total += c.second;
  }
  if (balancer.Writer.min == INT_MAX) balancer.Writer.min = 0;
  if (!writer_actives.empty()) {
    balancer.Writer.average = total / writer_actives.size();
  }

  total = 0;
  for (auto &c : reader_actives) {
    if (balancer.Reader.min > c.second) balancer.Reader.min = c.second;
    if (balancer.Reader.max < c.second) balancer.Reader.max = c.second;
    total += c.second;
  }
  if (balancer.Reader.min == INT_MAX) balancer.Reader.min = 0;
  if (!reader_actives.empty()) {
    balancer.Reader.average = total / reader_actives.size();
  }

  if (perf) {
    double phase_balance =
        stats.Reader.count == 0   ? 100.0
        : stats.Writer.count == 0 ? 100.0
        : stats.Reader.count > stats.Writer.count
            ? (100.0 * (double)stats.Writer.count / (double)stats.Reader.count)
            : (100.0 * (double)stats.Reader.count / (double)stats.Writer.count);

    double task_balance = balancer.Writer.max == 0
                              ? 100.0
                              : (100.0 * (double)balancer.Writer.min /
                                 (double)balancer.Writer.max);
    // impl|reader_threads|writer_threads|reader_acquires|writer_acquires|task_balance|phase_balance
    printf("%s,%d,%d,%d,%d,%.2f,%.2f\n", impl.c_str(),
           (int)reader_actives.size(), (int)writer_actives.size(),
           (int)stats.Reader.count, (int)stats.Writer.count, task_balance,
           phase_balance);
  } else {
    if (!writer_actives.empty()) {
      AGO_LOG(
          "[% 9s] [Writer] threads %.6d; acquire %.6d times, min %.9lu us, max "
          "%.9lu us, avg %.9lu "
          "us; balance: min %.6d, max %.6d, avg %.6d",
          impl.c_str(), (int)writer_actives.size(), (int)stats.Writer.count,
          stats.Writer.min_us.load(), stats.Writer.max_us.load(),
          stats.Writer.count ? stats.Writer.total_us / stats.Writer.count : 0,
          balancer.Writer.min, balancer.Writer.max, balancer.Writer.average);
    }

    if (!reader_actives.empty()) {
      AGO_LOG(
          "[% 9s] [Reader] threads %.6d; acquire %.6d times, min %.9lu us, max "
          "%.9lu us, avg %.9lu "
          "us; balance: min %.6d, max %.6d, avg %.6d",
          impl.c_str(), (int)reader_actives.size(), (int)stats.Reader.count,
          stats.Reader.min_us.load(), stats.Reader.max_us.load(),
          stats.Reader.count ? stats.Reader.total_us / stats.Reader.count : 0,
          balancer.Reader.min, balancer.Reader.max, balancer.Reader.average);
    }
  }

  ten_event_destroy(start_event);

  ten_rwlock_destroy(lut);
}

static void RunRWLockImplTest(int test_ms, int write_threads, int read_threads,
                              bool perf = false) {
  RunRWLockTest(ten_rwlock_create(TEN_RW_PHASE_FAIR), "PhaseFair", test_ms,
                write_threads, read_threads, perf);
  RunRWLockTest(ten_rwlock_create(TEN_RW_NATIVE), "Native", test_ms,
                write_threads, read_threads, perf);
}

TEST(RWLockTest, no_writer_no_reader) { RunRWLockImplTest(100, 0, 0); }

TEST(RWLockTest, no_writer_single_reader) { RunRWLockImplTest(100, 0, 1); }

TEST(RWLockTest, no_writer_multi_readers) {
  RunRWLockImplTest(100, 0, TEST_THREAD_MAX);
}

TEST(RWLockTest, single_writer_no_reader) { RunRWLockImplTest(100, 1, 0); }

TEST(RWLockTest, multi_writers_no_reader) {
  RunRWLockImplTest(100, TEST_THREAD_MAX, 0);
}

TEST(RWLockTest, single_writer_single_reader) { RunRWLockImplTest(100, 1, 1); }

TEST(RWLockTest, single_writer_multi_readers) {
  RunRWLockImplTest(100, 1, TEST_THREAD_MAX);
}

TEST(RWLockTest, multi_writers_single_reader) {
  RunRWLockImplTest(100, TEST_THREAD_MAX, 1);
}

TEST(RWLockTest, multi_writers_multi_readers) {
  RunRWLockImplTest(100, TEST_THREAD_MAX, TEST_THREAD_MAX);
}
