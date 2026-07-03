//
// Copyright © 2025 Agora
// This file is part of TEN Framework, an open source project.
// Licensed under the Apache License, Version 2.0, with certain conditions.
// Refer to the "LICENSE" file in the root directory for more information.
//
#include <chrono>
#include <cstdlib>
#include <thread>

#include "gtest/gtest.h"
#include "ten_utils/lib/atomic.h"
#include "ten_utils/lib/event.h"
#include "ten_utils/lib/shared_event.h"
#include "ten_utils/lib/spinlock.h"
#include "ten_utils/lib/time.h"

struct SharedEventCheckpoint {
  uint32_t sig;
  ten_atomic_t lock;
  ten_shared_event_t *event;
  ten_atomic_t value;
};

TEST(SharedEventTest, positive) {
  SharedEventCheckpoint t1_checkpoints[2];
  SharedEventCheckpoint t2_checkpoints[2];

  for (int index = 0; index < 2; index++) {
    SharedEventCheckpoint *p1 = t1_checkpoints + index;
    SharedEventCheckpoint *p2 = t2_checkpoints + index;
    p1->event = ten_shared_event_create(&p1->sig, &p1->lock, 0, 0);
    ten_atomic_store(&p1->value, 0);
    p2->event = ten_shared_event_create(&p2->sig, &p2->lock, 0, 0);
    ten_atomic_store(&p2->value, 0);
  }

  std::thread t1 = std::thread([p1 = t1_checkpoints, p2 = t2_checkpoints] {
    ten_shared_event_wait(p1[0].event, -1);
    ten_shared_event_wait(p2[1].event, -1);
    EXPECT_EQ(ten_atomic_load(&p2[0].value), 1);
    ten_atomic_store(&p1[0].value, 1);
    ten_shared_event_set(p1[1].event);
  });

  std::thread t2 = std::thread([p1 = t1_checkpoints, p2 = t2_checkpoints] {
    ten_shared_event_wait(p2[0].event, -1);
    EXPECT_EQ(ten_atomic_load(&p1[0].value), 0);
    ten_atomic_store(&p2[0].value, 1);
    ten_shared_event_set(p2[1].event);
    ten_shared_event_wait(p1[1].event, -1);
    EXPECT_EQ(ten_atomic_load(&p1[0].value), 1);
  });

  ten_shared_event_set(t1_checkpoints[0].event);
  ten_shared_event_set(t2_checkpoints[0].event);

  t1.join();
  t2.join();

  for (int index = 0; index < 2; index++) {
    SharedEventCheckpoint *p1 = t1_checkpoints + index;
    SharedEventCheckpoint *p2 = t2_checkpoints + index;
    ten_shared_event_destroy(p1->event);
    ten_shared_event_destroy(p2->event);
  }
}

struct SpinlockAcquireInfo {
  int64_t begin_time = 0;
  int64_t acquired_time = 0;
  int64_t released_time = 0;
};

TEST(SpinLockTest, test) {
  srand(time(nullptr));

  ten_event_t *start_event = ten_event_create(0, 0);

  ten_atomic_t addr = 0;
  ten_spinlock_t *lock = ten_spinlock_from_addr(&addr);

  ten_atomic_t lock_cnt = {0};

  SpinlockAcquireInfo thread_info1;
  SpinlockAcquireInfo thread_info2;
  int val = 0;

  auto thrd_1_op = [start_event, &lock, &lock_cnt, &thread_info1, &val] {
    ten_event_wait(start_event, -1);

    thread_info1.begin_time = ten_current_time_ms();
    ten_spinlock_lock(lock);
    val++;
    EXPECT_EQ(val, 1);
    thread_info1.acquired_time = ten_current_time_ms();
    printf("[   LOG    ][thrd_1] acquire spin lock spent %d ms\n",
           (int)(thread_info1.acquired_time - thread_info1.begin_time));

    auto ran = rand() % 20 + 30;
    thread_info1.released_time = thread_info1.acquired_time + ran;

    if (ten_atomic_fetch_add(&lock_cnt, 1) == 0) {
      printf("[   LOG    ][thrd_1] wait %d ms before release spin lock\n",
             (int)ran);
      std::this_thread::sleep_for(std::chrono::milliseconds(ran));
    }
    EXPECT_EQ(val, 1);
    val--;
    ten_spinlock_unlock(lock);

    printf("[   LOG    ][thrd_1] unlocked successfully\n");
  };

  auto thrd_2_op = [&start_event, &lock, &lock_cnt, &thread_info2, &val] {
    ten_event_wait(start_event, -1);

    thread_info2.begin_time = ten_current_time_ms();
    ten_spinlock_lock(lock);
    val++;
    EXPECT_EQ(val, 1);
    thread_info2.acquired_time = ten_current_time_ms();
    printf("[   LOG    ][thrd_2] acquire spin lock spent %d ms\n",
           (int)(thread_info2.acquired_time - thread_info2.begin_time));

    auto ran = rand() % 20 + 30;
    thread_info2.released_time = thread_info2.acquired_time + ran;
    if (ten_atomic_fetch_add(&lock_cnt, 1) == 0) {
      printf("[   LOG    ][thrd_2] wait %d ms before release spin lock\n",
             (int)ran);
      std::this_thread::sleep_for(std::chrono::milliseconds(ran));
    }
    EXPECT_EQ(val, 1);
    val--;
    ten_spinlock_unlock(lock);

    printf("[   LOG    ][thrd_2] unlocked successfully\n");
  };

  auto thrd_1 = std::thread(thrd_1_op);
  auto thrd_2 = std::thread(thrd_2_op);

  ten_event_set(start_event);

  if (thrd_1.joinable()) {
    thrd_1.join();
  }

  if (thrd_2.joinable()) {
    thrd_2.join();
  }

  EXPECT_EQ(lock_cnt, 2);
  ten_event_destroy(start_event);

  if (thread_info1.acquired_time > thread_info2.acquired_time) {
    EXPECT_GE(thread_info1.acquired_time, thread_info2.released_time);
  } else {
    EXPECT_LE(thread_info1.acquired_time, thread_info2.released_time);
  }
}
