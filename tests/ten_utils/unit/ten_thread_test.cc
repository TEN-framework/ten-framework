//
// Copyright © 2025 Agora
// This file is part of TEN Framework, an open source project.
// Licensed under the Apache License, Version 2.0, with certain conditions.
// Refer to the "LICENSE" file in the root directory for more information.
//
#include <thread>

#include "gtest/gtest.h"
#include "ten_utils/lib/event.h"
#include "ten_utils/lib/thread.h"
#include "ten_utils/lib/thread_local.h"

static void *dummy_routine(void *) { return nullptr; }

TEST(ThreadTest, negative) {
  // no routine, not a valid thread
  auto t = ten_thread_create(nullptr, nullptr, nullptr);
  EXPECT_EQ(t, nullptr);

  t = ten_thread_create(nullptr, dummy_routine, nullptr);
  EXPECT_NE(t, nullptr);

  EXPECT_EQ(ten_thread_join(t, -1), 0);

  EXPECT_NE(ten_thread_join(nullptr, 0), 0);
}

struct simple_routine_data {
  ten_thread_t *thread;
  ten_event_t *go;
  ten_tid_t tid;
};

static void *simple_routine(void *args) {
  simple_routine_data *t = (simple_routine_data *)args;
  ten_event_wait(t->go, -1);

  EXPECT_NE(args, nullptr);
  EXPECT_NE(ten_thread_self(), nullptr);
  EXPECT_EQ(ten_thread_self(), t->thread);
  EXPECT_NE(ten_thread_get_id(nullptr), 0);
  EXPECT_EQ(ten_thread_get_id(nullptr), t->tid);
  EXPECT_EQ(ten_thread_get_id(ten_thread_self()), t->tid);
  return nullptr;
}

TEST(ThreadTest, positive) {
  EXPECT_EQ(ten_thread_self(), nullptr);

  auto create_thread_task = [] {
    simple_routine_data data = {0};
    data.go = ten_event_create(0, 0);
    ten_thread_t *t = ten_thread_create(nullptr, simple_routine, &data);
    EXPECT_NE(t, nullptr);
    EXPECT_NE(ten_thread_get_id(t), 0);
    data.thread = t;
    data.tid = ten_thread_get_id(t);
    ten_event_set(data.go);
    ten_thread_join(t, -1);
    ten_event_destroy(data.go);
  };

  std::thread threads[10];
  for (auto i = 0; i < 10; i++) {
    threads[i] = std::thread(create_thread_task);
  }

  for (auto i = 0; i < 10; i++) {
    threads[i].join();
  }
}

TEST(ThreadLocalTest, negative) {
  EXPECT_EQ(ten_thread_self(), nullptr);
  EXPECT_EQ(ten_thread_get_key(kInvalidTlsKey), nullptr);
  EXPECT_EQ(ten_thread_set_key(kInvalidTlsKey, nullptr), -1);
}

TEST(ThreadLocalTest, positive) {
  auto key = ten_thread_key_create();
  EXPECT_NE(key, kInvalidTlsKey);
  EXPECT_EQ(ten_thread_get_key(key), nullptr);
  EXPECT_EQ(ten_thread_set_key(key, (void *)0xdeadbeef), 0);
  EXPECT_EQ(ten_thread_get_key(key), (void *)0xdeadbeef);
  ten_thread_key_destroy(key);
  EXPECT_EQ(ten_thread_get_key(key), nullptr);

  ten_event_t *t1_ready, *t2_ready;
  ten_event_t *go;
  ten_event_t *t1_done, *t2_done;
  key = kInvalidTlsKey;

  t1_ready = ten_event_create(0, 0);
  t2_ready = ten_event_create(0, 0);
  go = ten_event_create(0, 0);
  t1_done = ten_event_create(0, 0);
  t2_done = ten_event_create(0, 0);

  auto task1 = [t1_ready, go, t1_done, t2_done, &key] {
    ten_event_set(t1_ready);
    ten_event_wait(go, -1);
    key = ten_thread_key_create();
    EXPECT_EQ(ten_thread_set_key(key, (void *)0xdeadbee1), 0);
    ten_event_set(t1_done);
    ten_event_wait(t2_done, -1);
    EXPECT_EQ(ten_thread_get_key(key), (void *)0xdeadbee1);
  };

  auto task2 = [t2_ready, go, t1_done, t2_done, &key] {
    ten_event_set(t2_ready);
    ten_event_wait(go, -1);
    ten_event_wait(t1_done, -1);
    EXPECT_EQ(ten_thread_get_key(key), nullptr);
    EXPECT_EQ(ten_thread_set_key(key, (void *)0xdeadbee2), 0);
    ten_event_set(t2_done);
    EXPECT_EQ(ten_thread_get_key(key), (void *)0xdeadbee2);
  };

  auto t1 = std::thread(task1);
  auto t2 = std::thread(task2);

  ten_event_wait(t1_ready, -1);
  ten_event_wait(t2_ready, -1);

  ten_event_set(go);

  t1.join();
  t2.join();

  ten_event_destroy(t1_ready);
  ten_event_destroy(t2_ready);
  ten_event_destroy(go);
  ten_event_destroy(t1_done);
  ten_event_destroy(t2_done);
}
