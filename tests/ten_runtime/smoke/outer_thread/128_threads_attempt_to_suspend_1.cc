//
// Copyright © 2025 Agora
// This file is part of TEN Framework, an open source project.
// Licensed under the Apache License, Version 2.0, with certain conditions.
// Refer to the "LICENSE" file in the root directory for more information.
//
#include <condition_variable>
#include <cstdint>
#include <memory>
#include <nlohmann/json.hpp>
#include <string>
#include <thread>

#include "gtest/gtest.h"
#include "include_internal/ten_runtime/binding/cpp/ten.h"
#include "ten_utils/lang/cpp/lib/value.h"
#include "ten_utils/lib/alloc.h"
#include "ten_utils/lib/thread.h"
#include "ten_utils/lib/time.h"
#include "ten_utils/macro/check.h"
#include "tests/common/client/cpp/msgpack_tcp.h"
#include "tests/ten_runtime/smoke/util/binding/cpp/check.h"

#if defined(__i386__) || defined(__arm__)
#define OUTER_THREAD_FOR_LOOP_CNT 100
#define FROM_EXTENSION_2_CMD_CNT 500
#define OUTER_THREAD_CNT 16
#else
#define OUTER_THREAD_FOR_LOOP_CNT 100
#define FROM_EXTENSION_2_CMD_CNT 500
#define OUTER_THREAD_CNT 128
#endif

namespace {

class Holder {  // NOLINT
 public:
  explicit Holder(void *ptr) : ptr_(ptr) {}
  ~Holder() { TEN_FREE(ptr_); }

 private:
  void *ptr_{nullptr};
};

class test_extension_1 : public ten::extension_t {
 public:
  explicit test_extension_1(const char *name) : ten::extension_t(name) {}

  void on_configure(ten::ten_env_t &ten_env) override {
    // We have increased the path timeout to 20 minutes because, under limited
    // computing resources, it is easy to exceed the path timeout without
    // completing the data transmission. This can lead to the path being
    // discarded, causing the test case to hang indefinitely. Therefore, we have
    // extended the path timeout to avoid this situation.

    // clang-format off
    bool rc = ten_env.init_property_from_json( R"({
      "ten": {
        "path_timeout": 1200000000
      }
    })");
    // clang-format on
    ASSERT_EQ(rc, true);

    ten_env.on_configure_done();
  }

#define OUTER_THREAD_MAIN(X)                                                 \
  void outer_thread##X##_main(                                               \
      ten::ten_env_proxy_t *ten_env_proxy) { /* NOLINT */                    \
    auto *test_data = static_cast<int64_t *>(ten_malloc(sizeof(int64_t)));   \
    TEN_ASSERT(test_data, "Failed to allocate memory.");                     \
                                                                             \
    Holder _(test_data);                                                     \
                                                                             \
    *test_data = ((X) << 16);                                                \
                                                                             \
    std::unique_lock<std::mutex> lock(outer_thread_##X##_cv_lock);           \
                                                                             \
    for (int i = 0; i < OUTER_THREAD_FOR_LOOP_CNT; ++i) {                    \
      if (!outer_thread_##X##_towards_to_close) {                            \
        (*test_data)++;                                                      \
                                                                             \
        ten_random_sleep_range_ms(0, 3);                                     \
                                                                             \
        bool rc = ten_env_proxy->acquire_lock_mode();                        \
        TEN_ASSERT(rc, "Failed to acquire lock mode.");                      \
                                                                             \
        ten_random_sleep_range_ms(0, 3);                                     \
                                                                             \
        ten_env_proxy->notify(send_data_from_outer_thread, test_data, true); \
                                                                             \
        ten_random_sleep_range_ms(0, 3);                                     \
                                                                             \
        rc = ten_env_proxy->release_lock_mode();                             \
        TEN_ASSERT(rc, "Failed to release lock mode.");                      \
      }                                                                      \
    }                                                                        \
    delete ten_env_proxy;                                                    \
    while (!outer_thread_##X##_towards_to_close) {                           \
      outer_thread_##X##_cv.wait(lock);                                      \
    }                                                                        \
  }

  OUTER_THREAD_MAIN(1)
  OUTER_THREAD_MAIN(2)
  OUTER_THREAD_MAIN(3)
  OUTER_THREAD_MAIN(4)
  OUTER_THREAD_MAIN(5)
  OUTER_THREAD_MAIN(6)
  OUTER_THREAD_MAIN(7)
  OUTER_THREAD_MAIN(8)
  OUTER_THREAD_MAIN(9)
  OUTER_THREAD_MAIN(10)
  OUTER_THREAD_MAIN(11)
  OUTER_THREAD_MAIN(12)
  OUTER_THREAD_MAIN(13)
  OUTER_THREAD_MAIN(14)
  OUTER_THREAD_MAIN(15)
  OUTER_THREAD_MAIN(16)

#if !defined(__i386__) && !defined(__arm__)
  OUTER_THREAD_MAIN(17)
  OUTER_THREAD_MAIN(18)
  OUTER_THREAD_MAIN(19)
  OUTER_THREAD_MAIN(20)
  OUTER_THREAD_MAIN(21)
  OUTER_THREAD_MAIN(22)
  OUTER_THREAD_MAIN(23)
  OUTER_THREAD_MAIN(24)
  OUTER_THREAD_MAIN(25)
  OUTER_THREAD_MAIN(26)
  OUTER_THREAD_MAIN(27)
  OUTER_THREAD_MAIN(28)
  OUTER_THREAD_MAIN(29)
  OUTER_THREAD_MAIN(30)
  OUTER_THREAD_MAIN(31)
  OUTER_THREAD_MAIN(32)
  OUTER_THREAD_MAIN(33)
  OUTER_THREAD_MAIN(34)
  OUTER_THREAD_MAIN(35)
  OUTER_THREAD_MAIN(36)
  OUTER_THREAD_MAIN(37)
  OUTER_THREAD_MAIN(38)
  OUTER_THREAD_MAIN(39)
  OUTER_THREAD_MAIN(40)
  OUTER_THREAD_MAIN(41)
  OUTER_THREAD_MAIN(42)
  OUTER_THREAD_MAIN(43)
  OUTER_THREAD_MAIN(44)
  OUTER_THREAD_MAIN(45)
  OUTER_THREAD_MAIN(46)
  OUTER_THREAD_MAIN(47)
  OUTER_THREAD_MAIN(48)
  OUTER_THREAD_MAIN(49)
  OUTER_THREAD_MAIN(50)
  OUTER_THREAD_MAIN(51)
  OUTER_THREAD_MAIN(52)
  OUTER_THREAD_MAIN(53)
  OUTER_THREAD_MAIN(54)
  OUTER_THREAD_MAIN(55)
  OUTER_THREAD_MAIN(56)
  OUTER_THREAD_MAIN(57)
  OUTER_THREAD_MAIN(58)
  OUTER_THREAD_MAIN(59)
  OUTER_THREAD_MAIN(60)
  OUTER_THREAD_MAIN(61)
  OUTER_THREAD_MAIN(62)
  OUTER_THREAD_MAIN(63)
  OUTER_THREAD_MAIN(64)
  OUTER_THREAD_MAIN(65)
  OUTER_THREAD_MAIN(66)
  OUTER_THREAD_MAIN(67)
  OUTER_THREAD_MAIN(68)
  OUTER_THREAD_MAIN(69)
  OUTER_THREAD_MAIN(70)
  OUTER_THREAD_MAIN(71)
  OUTER_THREAD_MAIN(72)
  OUTER_THREAD_MAIN(73)
  OUTER_THREAD_MAIN(74)
  OUTER_THREAD_MAIN(75)
  OUTER_THREAD_MAIN(76)
  OUTER_THREAD_MAIN(77)
  OUTER_THREAD_MAIN(78)
  OUTER_THREAD_MAIN(79)
  OUTER_THREAD_MAIN(80)
  OUTER_THREAD_MAIN(81)
  OUTER_THREAD_MAIN(82)
  OUTER_THREAD_MAIN(83)
  OUTER_THREAD_MAIN(84)
  OUTER_THREAD_MAIN(85)
  OUTER_THREAD_MAIN(86)
  OUTER_THREAD_MAIN(87)
  OUTER_THREAD_MAIN(88)
  OUTER_THREAD_MAIN(89)
  OUTER_THREAD_MAIN(90)
  OUTER_THREAD_MAIN(91)
  OUTER_THREAD_MAIN(92)
  OUTER_THREAD_MAIN(93)
  OUTER_THREAD_MAIN(94)
  OUTER_THREAD_MAIN(95)
  OUTER_THREAD_MAIN(96)
  OUTER_THREAD_MAIN(97)
  OUTER_THREAD_MAIN(98)
  OUTER_THREAD_MAIN(99)
  OUTER_THREAD_MAIN(100)
  OUTER_THREAD_MAIN(101)
  OUTER_THREAD_MAIN(102)
  OUTER_THREAD_MAIN(103)
  OUTER_THREAD_MAIN(104)
  OUTER_THREAD_MAIN(105)
  OUTER_THREAD_MAIN(106)
  OUTER_THREAD_MAIN(107)
  OUTER_THREAD_MAIN(108)
  OUTER_THREAD_MAIN(109)
  OUTER_THREAD_MAIN(110)
  OUTER_THREAD_MAIN(111)
  OUTER_THREAD_MAIN(112)
  OUTER_THREAD_MAIN(113)
  OUTER_THREAD_MAIN(114)
  OUTER_THREAD_MAIN(115)
  OUTER_THREAD_MAIN(116)
  OUTER_THREAD_MAIN(117)
  OUTER_THREAD_MAIN(118)
  OUTER_THREAD_MAIN(119)
  OUTER_THREAD_MAIN(120)
  OUTER_THREAD_MAIN(121)
  OUTER_THREAD_MAIN(122)
  OUTER_THREAD_MAIN(123)
  OUTER_THREAD_MAIN(124)
  OUTER_THREAD_MAIN(125)
  OUTER_THREAD_MAIN(126)
  OUTER_THREAD_MAIN(127)
  OUTER_THREAD_MAIN(128)
#endif

  void on_start(ten::ten_env_t &ten_env) override {
    auto start_to_send_cmd = ten::cmd_t::create("start_to_send");
    ten_env.send_cmd(std::move(start_to_send_cmd),
                     [this](ten::ten_env_t &ten_env,
                            std::unique_ptr<ten::cmd_result_t> cmd_result,
                            ten::error_t *err) {
                       TEN_ASSERT(
                           cmd_result->get_status_code() == TEN_STATUS_CODE_OK,
                           "Failed to send 'start_to_send' command.");

#define CREATE_OUTER_THREAD(X)                                           \
  do {                                                                   \
    auto *ten_env_proxy = ten::ten_env_proxy_t::create(ten_env);         \
    outer_thread##X = new std::thread(                                   \
        &test_extension_1::outer_thread##X##_main, this, ten_env_proxy); \
  } while (0)

                       CREATE_OUTER_THREAD(1);
                       CREATE_OUTER_THREAD(2);
                       CREATE_OUTER_THREAD(3);
                       CREATE_OUTER_THREAD(4);
                       CREATE_OUTER_THREAD(5);
                       CREATE_OUTER_THREAD(6);
                       CREATE_OUTER_THREAD(7);
                       CREATE_OUTER_THREAD(8);
                       CREATE_OUTER_THREAD(9);
                       CREATE_OUTER_THREAD(10);
                       CREATE_OUTER_THREAD(11);
                       CREATE_OUTER_THREAD(12);
                       CREATE_OUTER_THREAD(13);
                       CREATE_OUTER_THREAD(14);
                       CREATE_OUTER_THREAD(15);
                       CREATE_OUTER_THREAD(16);

#if !defined(__i386__) && !defined(__arm__)
                       CREATE_OUTER_THREAD(17);
                       CREATE_OUTER_THREAD(18);
                       CREATE_OUTER_THREAD(19);
                       CREATE_OUTER_THREAD(20);
                       CREATE_OUTER_THREAD(21);
                       CREATE_OUTER_THREAD(22);
                       CREATE_OUTER_THREAD(23);
                       CREATE_OUTER_THREAD(24);
                       CREATE_OUTER_THREAD(25);
                       CREATE_OUTER_THREAD(26);
                       CREATE_OUTER_THREAD(27);
                       CREATE_OUTER_THREAD(28);
                       CREATE_OUTER_THREAD(29);
                       CREATE_OUTER_THREAD(30);
                       CREATE_OUTER_THREAD(31);
                       CREATE_OUTER_THREAD(32);
                       CREATE_OUTER_THREAD(33);
                       CREATE_OUTER_THREAD(34);
                       CREATE_OUTER_THREAD(35);
                       CREATE_OUTER_THREAD(36);
                       CREATE_OUTER_THREAD(37);
                       CREATE_OUTER_THREAD(38);
                       CREATE_OUTER_THREAD(39);
                       CREATE_OUTER_THREAD(40);
                       CREATE_OUTER_THREAD(41);
                       CREATE_OUTER_THREAD(42);
                       CREATE_OUTER_THREAD(43);
                       CREATE_OUTER_THREAD(44);
                       CREATE_OUTER_THREAD(45);
                       CREATE_OUTER_THREAD(46);
                       CREATE_OUTER_THREAD(47);
                       CREATE_OUTER_THREAD(48);
                       CREATE_OUTER_THREAD(49);
                       CREATE_OUTER_THREAD(50);
                       CREATE_OUTER_THREAD(51);
                       CREATE_OUTER_THREAD(52);
                       CREATE_OUTER_THREAD(53);
                       CREATE_OUTER_THREAD(54);
                       CREATE_OUTER_THREAD(55);
                       CREATE_OUTER_THREAD(56);
                       CREATE_OUTER_THREAD(57);
                       CREATE_OUTER_THREAD(58);
                       CREATE_OUTER_THREAD(59);
                       CREATE_OUTER_THREAD(60);
                       CREATE_OUTER_THREAD(61);
                       CREATE_OUTER_THREAD(62);
                       CREATE_OUTER_THREAD(63);
                       CREATE_OUTER_THREAD(64);
                       CREATE_OUTER_THREAD(65);
                       CREATE_OUTER_THREAD(66);
                       CREATE_OUTER_THREAD(67);
                       CREATE_OUTER_THREAD(68);
                       CREATE_OUTER_THREAD(69);
                       CREATE_OUTER_THREAD(70);
                       CREATE_OUTER_THREAD(71);
                       CREATE_OUTER_THREAD(72);
                       CREATE_OUTER_THREAD(73);
                       CREATE_OUTER_THREAD(74);
                       CREATE_OUTER_THREAD(75);
                       CREATE_OUTER_THREAD(76);
                       CREATE_OUTER_THREAD(77);
                       CREATE_OUTER_THREAD(78);
                       CREATE_OUTER_THREAD(79);
                       CREATE_OUTER_THREAD(80);
                       CREATE_OUTER_THREAD(81);
                       CREATE_OUTER_THREAD(82);
                       CREATE_OUTER_THREAD(83);
                       CREATE_OUTER_THREAD(84);
                       CREATE_OUTER_THREAD(85);
                       CREATE_OUTER_THREAD(86);
                       CREATE_OUTER_THREAD(87);
                       CREATE_OUTER_THREAD(88);
                       CREATE_OUTER_THREAD(89);
                       CREATE_OUTER_THREAD(90);
                       CREATE_OUTER_THREAD(91);
                       CREATE_OUTER_THREAD(92);
                       CREATE_OUTER_THREAD(93);
                       CREATE_OUTER_THREAD(94);
                       CREATE_OUTER_THREAD(95);
                       CREATE_OUTER_THREAD(96);
                       CREATE_OUTER_THREAD(97);
                       CREATE_OUTER_THREAD(98);
                       CREATE_OUTER_THREAD(99);
                       CREATE_OUTER_THREAD(100);
                       CREATE_OUTER_THREAD(101);
                       CREATE_OUTER_THREAD(102);
                       CREATE_OUTER_THREAD(103);
                       CREATE_OUTER_THREAD(104);
                       CREATE_OUTER_THREAD(105);
                       CREATE_OUTER_THREAD(106);
                       CREATE_OUTER_THREAD(107);
                       CREATE_OUTER_THREAD(108);
                       CREATE_OUTER_THREAD(109);
                       CREATE_OUTER_THREAD(110);
                       CREATE_OUTER_THREAD(111);
                       CREATE_OUTER_THREAD(112);
                       CREATE_OUTER_THREAD(113);
                       CREATE_OUTER_THREAD(114);
                       CREATE_OUTER_THREAD(115);
                       CREATE_OUTER_THREAD(116);
                       CREATE_OUTER_THREAD(117);
                       CREATE_OUTER_THREAD(118);
                       CREATE_OUTER_THREAD(119);
                       CREATE_OUTER_THREAD(120);
                       CREATE_OUTER_THREAD(121);
                       CREATE_OUTER_THREAD(122);
                       CREATE_OUTER_THREAD(123);
                       CREATE_OUTER_THREAD(124);
                       CREATE_OUTER_THREAD(125);
                       CREATE_OUTER_THREAD(126);
                       CREATE_OUTER_THREAD(127);
                       CREATE_OUTER_THREAD(128);
#endif
                     });

    ten_env.on_start_done();
  }

  void on_stop(ten::ten_env_t &ten_env) override {
#define NOTIFY_OUTER_THREAD_TO_STOP(X)                                         \
  do {                                                                         \
    {                                                                          \
      (ten_env).log(TEN_LOG_LEVEL_INFO, __func__, __FILE__, __LINE__,          \
                    (std::string("notify outer thread ") + std::to_string(X) + \
                     " to stop")                                               \
                        .c_str());                                             \
      std::unique_lock<std::mutex> lock(outer_thread_##X##_cv_lock);           \
      outer_thread_##X##_towards_to_close = true;                              \
    }                                                                          \
    outer_thread_##X##_cv.notify_one();                                        \
  } while (0)

    NOTIFY_OUTER_THREAD_TO_STOP(1);
    NOTIFY_OUTER_THREAD_TO_STOP(2);
    NOTIFY_OUTER_THREAD_TO_STOP(3);
    NOTIFY_OUTER_THREAD_TO_STOP(4);
    NOTIFY_OUTER_THREAD_TO_STOP(5);
    NOTIFY_OUTER_THREAD_TO_STOP(6);
    NOTIFY_OUTER_THREAD_TO_STOP(7);
    NOTIFY_OUTER_THREAD_TO_STOP(8);
    NOTIFY_OUTER_THREAD_TO_STOP(9);
    NOTIFY_OUTER_THREAD_TO_STOP(10);
    NOTIFY_OUTER_THREAD_TO_STOP(11);
    NOTIFY_OUTER_THREAD_TO_STOP(12);
    NOTIFY_OUTER_THREAD_TO_STOP(13);
    NOTIFY_OUTER_THREAD_TO_STOP(14);
    NOTIFY_OUTER_THREAD_TO_STOP(15);
    NOTIFY_OUTER_THREAD_TO_STOP(16);

#if !defined(__i386__) && !defined(__arm__)
    NOTIFY_OUTER_THREAD_TO_STOP(17);
    NOTIFY_OUTER_THREAD_TO_STOP(18);
    NOTIFY_OUTER_THREAD_TO_STOP(19);
    NOTIFY_OUTER_THREAD_TO_STOP(20);
    NOTIFY_OUTER_THREAD_TO_STOP(21);
    NOTIFY_OUTER_THREAD_TO_STOP(22);
    NOTIFY_OUTER_THREAD_TO_STOP(23);
    NOTIFY_OUTER_THREAD_TO_STOP(24);
    NOTIFY_OUTER_THREAD_TO_STOP(25);
    NOTIFY_OUTER_THREAD_TO_STOP(26);
    NOTIFY_OUTER_THREAD_TO_STOP(27);
    NOTIFY_OUTER_THREAD_TO_STOP(28);
    NOTIFY_OUTER_THREAD_TO_STOP(29);
    NOTIFY_OUTER_THREAD_TO_STOP(30);
    NOTIFY_OUTER_THREAD_TO_STOP(31);
    NOTIFY_OUTER_THREAD_TO_STOP(32);
    NOTIFY_OUTER_THREAD_TO_STOP(33);
    NOTIFY_OUTER_THREAD_TO_STOP(34);
    NOTIFY_OUTER_THREAD_TO_STOP(35);
    NOTIFY_OUTER_THREAD_TO_STOP(36);
    NOTIFY_OUTER_THREAD_TO_STOP(37);
    NOTIFY_OUTER_THREAD_TO_STOP(38);
    NOTIFY_OUTER_THREAD_TO_STOP(39);
    NOTIFY_OUTER_THREAD_TO_STOP(40);
    NOTIFY_OUTER_THREAD_TO_STOP(41);
    NOTIFY_OUTER_THREAD_TO_STOP(42);
    NOTIFY_OUTER_THREAD_TO_STOP(43);
    NOTIFY_OUTER_THREAD_TO_STOP(44);
    NOTIFY_OUTER_THREAD_TO_STOP(45);
    NOTIFY_OUTER_THREAD_TO_STOP(46);
    NOTIFY_OUTER_THREAD_TO_STOP(47);
    NOTIFY_OUTER_THREAD_TO_STOP(48);
    NOTIFY_OUTER_THREAD_TO_STOP(49);
    NOTIFY_OUTER_THREAD_TO_STOP(50);
    NOTIFY_OUTER_THREAD_TO_STOP(51);
    NOTIFY_OUTER_THREAD_TO_STOP(52);
    NOTIFY_OUTER_THREAD_TO_STOP(53);
    NOTIFY_OUTER_THREAD_TO_STOP(54);
    NOTIFY_OUTER_THREAD_TO_STOP(55);
    NOTIFY_OUTER_THREAD_TO_STOP(56);
    NOTIFY_OUTER_THREAD_TO_STOP(57);
    NOTIFY_OUTER_THREAD_TO_STOP(58);
    NOTIFY_OUTER_THREAD_TO_STOP(59);
    NOTIFY_OUTER_THREAD_TO_STOP(60);
    NOTIFY_OUTER_THREAD_TO_STOP(61);
    NOTIFY_OUTER_THREAD_TO_STOP(62);
    NOTIFY_OUTER_THREAD_TO_STOP(63);
    NOTIFY_OUTER_THREAD_TO_STOP(64);
    NOTIFY_OUTER_THREAD_TO_STOP(65);
    NOTIFY_OUTER_THREAD_TO_STOP(66);
    NOTIFY_OUTER_THREAD_TO_STOP(67);
    NOTIFY_OUTER_THREAD_TO_STOP(68);
    NOTIFY_OUTER_THREAD_TO_STOP(69);
    NOTIFY_OUTER_THREAD_TO_STOP(70);
    NOTIFY_OUTER_THREAD_TO_STOP(71);
    NOTIFY_OUTER_THREAD_TO_STOP(72);
    NOTIFY_OUTER_THREAD_TO_STOP(73);
    NOTIFY_OUTER_THREAD_TO_STOP(74);
    NOTIFY_OUTER_THREAD_TO_STOP(75);
    NOTIFY_OUTER_THREAD_TO_STOP(76);
    NOTIFY_OUTER_THREAD_TO_STOP(77);
    NOTIFY_OUTER_THREAD_TO_STOP(78);
    NOTIFY_OUTER_THREAD_TO_STOP(79);
    NOTIFY_OUTER_THREAD_TO_STOP(80);
    NOTIFY_OUTER_THREAD_TO_STOP(81);
    NOTIFY_OUTER_THREAD_TO_STOP(82);
    NOTIFY_OUTER_THREAD_TO_STOP(83);
    NOTIFY_OUTER_THREAD_TO_STOP(84);
    NOTIFY_OUTER_THREAD_TO_STOP(85);
    NOTIFY_OUTER_THREAD_TO_STOP(86);
    NOTIFY_OUTER_THREAD_TO_STOP(87);
    NOTIFY_OUTER_THREAD_TO_STOP(88);
    NOTIFY_OUTER_THREAD_TO_STOP(89);
    NOTIFY_OUTER_THREAD_TO_STOP(90);
    NOTIFY_OUTER_THREAD_TO_STOP(91);
    NOTIFY_OUTER_THREAD_TO_STOP(92);
    NOTIFY_OUTER_THREAD_TO_STOP(93);
    NOTIFY_OUTER_THREAD_TO_STOP(94);
    NOTIFY_OUTER_THREAD_TO_STOP(95);
    NOTIFY_OUTER_THREAD_TO_STOP(96);
    NOTIFY_OUTER_THREAD_TO_STOP(97);
    NOTIFY_OUTER_THREAD_TO_STOP(98);
    NOTIFY_OUTER_THREAD_TO_STOP(99);
    NOTIFY_OUTER_THREAD_TO_STOP(100);
    NOTIFY_OUTER_THREAD_TO_STOP(101);
    NOTIFY_OUTER_THREAD_TO_STOP(102);
    NOTIFY_OUTER_THREAD_TO_STOP(103);
    NOTIFY_OUTER_THREAD_TO_STOP(104);
    NOTIFY_OUTER_THREAD_TO_STOP(105);
    NOTIFY_OUTER_THREAD_TO_STOP(106);
    NOTIFY_OUTER_THREAD_TO_STOP(107);
    NOTIFY_OUTER_THREAD_TO_STOP(108);
    NOTIFY_OUTER_THREAD_TO_STOP(109);
    NOTIFY_OUTER_THREAD_TO_STOP(110);
    NOTIFY_OUTER_THREAD_TO_STOP(111);
    NOTIFY_OUTER_THREAD_TO_STOP(112);
    NOTIFY_OUTER_THREAD_TO_STOP(113);
    NOTIFY_OUTER_THREAD_TO_STOP(114);
    NOTIFY_OUTER_THREAD_TO_STOP(115);
    NOTIFY_OUTER_THREAD_TO_STOP(116);
    NOTIFY_OUTER_THREAD_TO_STOP(117);
    NOTIFY_OUTER_THREAD_TO_STOP(118);
    NOTIFY_OUTER_THREAD_TO_STOP(119);
    NOTIFY_OUTER_THREAD_TO_STOP(120);
    NOTIFY_OUTER_THREAD_TO_STOP(121);
    NOTIFY_OUTER_THREAD_TO_STOP(122);
    NOTIFY_OUTER_THREAD_TO_STOP(123);
    NOTIFY_OUTER_THREAD_TO_STOP(124);
    NOTIFY_OUTER_THREAD_TO_STOP(125);
    NOTIFY_OUTER_THREAD_TO_STOP(126);
    NOTIFY_OUTER_THREAD_TO_STOP(127);
    NOTIFY_OUTER_THREAD_TO_STOP(128);
#endif

#define RECLAIM_OUTER_THREAD(X)                                              \
  do {                                                                       \
    (ten_env).log(                                                           \
        TEN_LOG_LEVEL_INFO, __func__, __FILE__, __LINE__,                    \
        (std::string("Reclaim outer thread ") + std::to_string(X)).c_str()); \
    outer_thread##X->join();                                                 \
    delete outer_thread##X;                                                  \
  } while (0)

    RECLAIM_OUTER_THREAD(1);
    RECLAIM_OUTER_THREAD(2);
    RECLAIM_OUTER_THREAD(3);
    RECLAIM_OUTER_THREAD(4);
    RECLAIM_OUTER_THREAD(5);
    RECLAIM_OUTER_THREAD(6);
    RECLAIM_OUTER_THREAD(7);
    RECLAIM_OUTER_THREAD(8);
    RECLAIM_OUTER_THREAD(9);
    RECLAIM_OUTER_THREAD(10);
    RECLAIM_OUTER_THREAD(11);
    RECLAIM_OUTER_THREAD(12);
    RECLAIM_OUTER_THREAD(13);
    RECLAIM_OUTER_THREAD(14);
    RECLAIM_OUTER_THREAD(15);
    RECLAIM_OUTER_THREAD(16);

#if !defined(__i386__) && !defined(__arm__)
    RECLAIM_OUTER_THREAD(17);
    RECLAIM_OUTER_THREAD(18);
    RECLAIM_OUTER_THREAD(19);
    RECLAIM_OUTER_THREAD(20);
    RECLAIM_OUTER_THREAD(21);
    RECLAIM_OUTER_THREAD(22);
    RECLAIM_OUTER_THREAD(23);
    RECLAIM_OUTER_THREAD(24);
    RECLAIM_OUTER_THREAD(25);
    RECLAIM_OUTER_THREAD(26);
    RECLAIM_OUTER_THREAD(27);
    RECLAIM_OUTER_THREAD(28);
    RECLAIM_OUTER_THREAD(29);
    RECLAIM_OUTER_THREAD(30);
    RECLAIM_OUTER_THREAD(31);
    RECLAIM_OUTER_THREAD(32);
    RECLAIM_OUTER_THREAD(33);
    RECLAIM_OUTER_THREAD(34);
    RECLAIM_OUTER_THREAD(35);
    RECLAIM_OUTER_THREAD(36);
    RECLAIM_OUTER_THREAD(37);
    RECLAIM_OUTER_THREAD(38);
    RECLAIM_OUTER_THREAD(39);
    RECLAIM_OUTER_THREAD(40);
    RECLAIM_OUTER_THREAD(41);
    RECLAIM_OUTER_THREAD(42);
    RECLAIM_OUTER_THREAD(43);
    RECLAIM_OUTER_THREAD(44);
    RECLAIM_OUTER_THREAD(45);
    RECLAIM_OUTER_THREAD(46);
    RECLAIM_OUTER_THREAD(47);
    RECLAIM_OUTER_THREAD(48);
    RECLAIM_OUTER_THREAD(49);
    RECLAIM_OUTER_THREAD(50);
    RECLAIM_OUTER_THREAD(51);
    RECLAIM_OUTER_THREAD(52);
    RECLAIM_OUTER_THREAD(53);
    RECLAIM_OUTER_THREAD(54);
    RECLAIM_OUTER_THREAD(55);
    RECLAIM_OUTER_THREAD(56);
    RECLAIM_OUTER_THREAD(57);
    RECLAIM_OUTER_THREAD(58);
    RECLAIM_OUTER_THREAD(59);
    RECLAIM_OUTER_THREAD(60);
    RECLAIM_OUTER_THREAD(61);
    RECLAIM_OUTER_THREAD(62);
    RECLAIM_OUTER_THREAD(63);
    RECLAIM_OUTER_THREAD(64);
    RECLAIM_OUTER_THREAD(65);
    RECLAIM_OUTER_THREAD(66);
    RECLAIM_OUTER_THREAD(67);
    RECLAIM_OUTER_THREAD(68);
    RECLAIM_OUTER_THREAD(69);
    RECLAIM_OUTER_THREAD(70);
    RECLAIM_OUTER_THREAD(71);
    RECLAIM_OUTER_THREAD(72);
    RECLAIM_OUTER_THREAD(73);
    RECLAIM_OUTER_THREAD(74);
    RECLAIM_OUTER_THREAD(75);
    RECLAIM_OUTER_THREAD(76);
    RECLAIM_OUTER_THREAD(77);
    RECLAIM_OUTER_THREAD(78);
    RECLAIM_OUTER_THREAD(79);
    RECLAIM_OUTER_THREAD(80);
    RECLAIM_OUTER_THREAD(81);
    RECLAIM_OUTER_THREAD(82);
    RECLAIM_OUTER_THREAD(83);
    RECLAIM_OUTER_THREAD(84);
    RECLAIM_OUTER_THREAD(85);
    RECLAIM_OUTER_THREAD(86);
    RECLAIM_OUTER_THREAD(87);
    RECLAIM_OUTER_THREAD(88);
    RECLAIM_OUTER_THREAD(89);
    RECLAIM_OUTER_THREAD(90);
    RECLAIM_OUTER_THREAD(91);
    RECLAIM_OUTER_THREAD(92);
    RECLAIM_OUTER_THREAD(93);
    RECLAIM_OUTER_THREAD(94);
    RECLAIM_OUTER_THREAD(95);
    RECLAIM_OUTER_THREAD(96);
    RECLAIM_OUTER_THREAD(97);
    RECLAIM_OUTER_THREAD(98);
    RECLAIM_OUTER_THREAD(99);
    RECLAIM_OUTER_THREAD(100);
    RECLAIM_OUTER_THREAD(101);
    RECLAIM_OUTER_THREAD(102);
    RECLAIM_OUTER_THREAD(103);
    RECLAIM_OUTER_THREAD(104);
    RECLAIM_OUTER_THREAD(105);
    RECLAIM_OUTER_THREAD(106);
    RECLAIM_OUTER_THREAD(107);
    RECLAIM_OUTER_THREAD(108);
    RECLAIM_OUTER_THREAD(109);
    RECLAIM_OUTER_THREAD(110);
    RECLAIM_OUTER_THREAD(111);
    RECLAIM_OUTER_THREAD(112);
    RECLAIM_OUTER_THREAD(113);
    RECLAIM_OUTER_THREAD(114);
    RECLAIM_OUTER_THREAD(115);
    RECLAIM_OUTER_THREAD(116);
    RECLAIM_OUTER_THREAD(117);
    RECLAIM_OUTER_THREAD(118);
    RECLAIM_OUTER_THREAD(119);
    RECLAIM_OUTER_THREAD(120);
    RECLAIM_OUTER_THREAD(121);
    RECLAIM_OUTER_THREAD(122);
    RECLAIM_OUTER_THREAD(123);
    RECLAIM_OUTER_THREAD(124);
    RECLAIM_OUTER_THREAD(125);
    RECLAIM_OUTER_THREAD(126);
    RECLAIM_OUTER_THREAD(127);
    RECLAIM_OUTER_THREAD(128);
#endif

    ten_env.on_stop_done();
  }

  void on_cmd(ten::ten_env_t &ten_env,
              std::unique_ptr<ten::cmd_t> cmd) override {
    const std::string command = cmd->get_name();

    if (command == "from_extension_2") {
      auto cmd_result = ten::cmd_result_t::create(TEN_STATUS_CODE_OK, *cmd);
      cmd_result->set_property("detail", "success");
      ten_env.return_result(std::move(cmd_result));
    }
  }

 private:
#define OTHER_THREAD_CB_PACK(X)                  \
  std::thread *outer_thread##X{nullptr};         \
  std::mutex outer_thread_##X##_cv_lock;         \
  std::condition_variable outer_thread_##X##_cv; \
  bool outer_thread_##X##_towards_to_close{false};

  OTHER_THREAD_CB_PACK(1)
  OTHER_THREAD_CB_PACK(2)
  OTHER_THREAD_CB_PACK(3)
  OTHER_THREAD_CB_PACK(4)
  OTHER_THREAD_CB_PACK(5)
  OTHER_THREAD_CB_PACK(6)
  OTHER_THREAD_CB_PACK(7)
  OTHER_THREAD_CB_PACK(8)
  OTHER_THREAD_CB_PACK(9)
  OTHER_THREAD_CB_PACK(10)
  OTHER_THREAD_CB_PACK(11)
  OTHER_THREAD_CB_PACK(12)
  OTHER_THREAD_CB_PACK(13)
  OTHER_THREAD_CB_PACK(14)
  OTHER_THREAD_CB_PACK(15)
  OTHER_THREAD_CB_PACK(16)

#if !defined(__i386__) && !defined(__arm__)
  OTHER_THREAD_CB_PACK(17)
  OTHER_THREAD_CB_PACK(18)
  OTHER_THREAD_CB_PACK(19)
  OTHER_THREAD_CB_PACK(20)
  OTHER_THREAD_CB_PACK(21)
  OTHER_THREAD_CB_PACK(22)
  OTHER_THREAD_CB_PACK(23)
  OTHER_THREAD_CB_PACK(24)
  OTHER_THREAD_CB_PACK(25)
  OTHER_THREAD_CB_PACK(26)
  OTHER_THREAD_CB_PACK(27)
  OTHER_THREAD_CB_PACK(28)
  OTHER_THREAD_CB_PACK(29)
  OTHER_THREAD_CB_PACK(30)
  OTHER_THREAD_CB_PACK(31)
  OTHER_THREAD_CB_PACK(32)
  OTHER_THREAD_CB_PACK(33)
  OTHER_THREAD_CB_PACK(34)
  OTHER_THREAD_CB_PACK(35)
  OTHER_THREAD_CB_PACK(36)
  OTHER_THREAD_CB_PACK(37)
  OTHER_THREAD_CB_PACK(38)
  OTHER_THREAD_CB_PACK(39)
  OTHER_THREAD_CB_PACK(40)
  OTHER_THREAD_CB_PACK(41)
  OTHER_THREAD_CB_PACK(42)
  OTHER_THREAD_CB_PACK(43)
  OTHER_THREAD_CB_PACK(44)
  OTHER_THREAD_CB_PACK(45)
  OTHER_THREAD_CB_PACK(46)
  OTHER_THREAD_CB_PACK(47)
  OTHER_THREAD_CB_PACK(48)
  OTHER_THREAD_CB_PACK(49)
  OTHER_THREAD_CB_PACK(50)
  OTHER_THREAD_CB_PACK(51)
  OTHER_THREAD_CB_PACK(52)
  OTHER_THREAD_CB_PACK(53)
  OTHER_THREAD_CB_PACK(54)
  OTHER_THREAD_CB_PACK(55)
  OTHER_THREAD_CB_PACK(56)
  OTHER_THREAD_CB_PACK(57)
  OTHER_THREAD_CB_PACK(58)
  OTHER_THREAD_CB_PACK(59)
  OTHER_THREAD_CB_PACK(60)
  OTHER_THREAD_CB_PACK(61)
  OTHER_THREAD_CB_PACK(62)
  OTHER_THREAD_CB_PACK(63)
  OTHER_THREAD_CB_PACK(64)
  OTHER_THREAD_CB_PACK(65)
  OTHER_THREAD_CB_PACK(66)
  OTHER_THREAD_CB_PACK(67)
  OTHER_THREAD_CB_PACK(68)
  OTHER_THREAD_CB_PACK(69)
  OTHER_THREAD_CB_PACK(70)
  OTHER_THREAD_CB_PACK(71)
  OTHER_THREAD_CB_PACK(72)
  OTHER_THREAD_CB_PACK(73)
  OTHER_THREAD_CB_PACK(74)
  OTHER_THREAD_CB_PACK(75)
  OTHER_THREAD_CB_PACK(76)
  OTHER_THREAD_CB_PACK(77)
  OTHER_THREAD_CB_PACK(78)
  OTHER_THREAD_CB_PACK(79)
  OTHER_THREAD_CB_PACK(80)
  OTHER_THREAD_CB_PACK(81)
  OTHER_THREAD_CB_PACK(82)
  OTHER_THREAD_CB_PACK(83)
  OTHER_THREAD_CB_PACK(84)
  OTHER_THREAD_CB_PACK(85)
  OTHER_THREAD_CB_PACK(86)
  OTHER_THREAD_CB_PACK(87)
  OTHER_THREAD_CB_PACK(88)
  OTHER_THREAD_CB_PACK(89)
  OTHER_THREAD_CB_PACK(90)
  OTHER_THREAD_CB_PACK(91)
  OTHER_THREAD_CB_PACK(92)
  OTHER_THREAD_CB_PACK(93)
  OTHER_THREAD_CB_PACK(94)
  OTHER_THREAD_CB_PACK(95)
  OTHER_THREAD_CB_PACK(96)
  OTHER_THREAD_CB_PACK(97)
  OTHER_THREAD_CB_PACK(98)
  OTHER_THREAD_CB_PACK(99)
  OTHER_THREAD_CB_PACK(100)
  OTHER_THREAD_CB_PACK(101)
  OTHER_THREAD_CB_PACK(102)
  OTHER_THREAD_CB_PACK(103)
  OTHER_THREAD_CB_PACK(104)
  OTHER_THREAD_CB_PACK(105)
  OTHER_THREAD_CB_PACK(106)
  OTHER_THREAD_CB_PACK(107)
  OTHER_THREAD_CB_PACK(108)
  OTHER_THREAD_CB_PACK(109)
  OTHER_THREAD_CB_PACK(110)
  OTHER_THREAD_CB_PACK(111)
  OTHER_THREAD_CB_PACK(112)
  OTHER_THREAD_CB_PACK(113)
  OTHER_THREAD_CB_PACK(114)
  OTHER_THREAD_CB_PACK(115)
  OTHER_THREAD_CB_PACK(116)
  OTHER_THREAD_CB_PACK(117)
  OTHER_THREAD_CB_PACK(118)
  OTHER_THREAD_CB_PACK(119)
  OTHER_THREAD_CB_PACK(120)
  OTHER_THREAD_CB_PACK(121)
  OTHER_THREAD_CB_PACK(122)
  OTHER_THREAD_CB_PACK(123)
  OTHER_THREAD_CB_PACK(124)
  OTHER_THREAD_CB_PACK(125)
  OTHER_THREAD_CB_PACK(126)
  OTHER_THREAD_CB_PACK(127)
  OTHER_THREAD_CB_PACK(128)
#endif

  static void send_data_from_outer_thread(ten::ten_env_t &ten_env,
                                          void *user_data) {
    // Create a 'ten::data_t' with the same important data.
    auto ten_data = ten::data_t::create("data");
    ten_data->set_property("test_data", user_data);
    ten_env.send_data(std::move(ten_data));
  }
};

class test_extension_2 : public ten::extension_t {
 public:
  explicit test_extension_2(const char *name)
      : ten::extension_t(name),
        data_received_count(OUTER_THREAD_CNT, 1),
        expected_data_received_count(OUTER_THREAD_CNT,
                                     OUTER_THREAD_FOR_LOOP_CNT + 1) {}

  void on_configure(ten::ten_env_t &ten_env) override {
    // We have increased the path timeout to 20 minutes because, under limited
    // computing resources, it is easy to exceed the path timeout without
    // completing the data transmission. This can lead to the path being
    // discarded, causing the test case to hang indefinitely. Therefore, we have
    // extended the path timeout to avoid this situation.

    // clang-format off
    bool rc = ten_env.init_property_from_json(R"({
      "ten": {
        "path_timeout": 1200000000
      }
    })");
    // clang-format on
    ASSERT_EQ(rc, true);

    ten_env.on_configure_done();
  }

  void on_cmd(ten::ten_env_t &ten_env,
              std::unique_ptr<ten::cmd_t> cmd) override {
    if (cmd->get_name() == std::string("start_to_send")) {
      auto cmd_result = ten::cmd_result_t::create(TEN_STATUS_CODE_OK, *cmd);
      cmd_result->set_property("detail", "ok");
      ten_env.return_result(std::move(cmd_result));

      auto timer_cmd = ten::cmd_timer_t::create();
      timer_cmd->set_dest("localhost", nullptr, nullptr, nullptr);
      timer_cmd->set_timer_id(55);
      timer_cmd->set_timeout_us(1000);
      timer_cmd->set_times(1);

      ten_env.send_cmd(std::move(timer_cmd));
    } else if (ten::msg_internal_accessor_t::get_type(cmd.get()) ==
                   TEN_MSG_TYPE_CMD_TIMEOUT &&
               static_cast<ten::cmd_timeout_t *>(cmd.get())->get_timer_id() ==
                   55) {
      TEN_ASSERT(timeout_thread == nullptr, "Should not happen.");

      auto *ten_env_proxy = ten::ten_env_proxy_t::create(ten_env);

      timeout_thread = new std::thread(
          [this](ten::ten_env_proxy_t *ten_env_proxy) {
            for (int i = 0; i < FROM_EXTENSION_2_CMD_CNT; ++i) {
              ten_random_sleep_range_ms(0, 3);

              ten_env_proxy->notify([this](ten::ten_env_t &ten_env) {
                auto from_extension_2_cmd =
                    ten::cmd_t::create("from_extension_2");

                ten_random_sleep_range_ms(0, 3);

                ten_env.send_cmd(
                    std::move(from_extension_2_cmd),
                    [this](ten::ten_env_t &ten_env,
                           std::unique_ptr<ten::cmd_result_t> cmd_result,
                           ten::error_t *err) {
                      TEN_ASSERT(
                          cmd_result->get_status_code() == TEN_STATUS_CODE_OK,
                          "Failed to send 'from_extension_2' command.");

                      received_from_extension_2_cmd_result++;
                      TEN_ENV_LOG_INFO(
                          ten_env,
                          (std::string("extension_2 got a result for "
                                       "from_extension_2 cmd: ") +
                           std::to_string(received_from_extension_2_cmd_result))
                              .c_str());

                      if ((hello_cmd != nullptr) && is_received_all_data() &&
                          (received_from_extension_2_cmd_result ==
                           expected_received_from_extension_2_cmd_result)) {
                        TEN_LOGI(
                            "extension_2 received all data, return result.");

                        auto cmd_result = ten::cmd_result_t::create(
                            TEN_STATUS_CODE_OK, *hello_cmd);
                        cmd_result->set_property("detail", "ok");
                        ten_env.return_result(std::move(cmd_result));
                      }
                    });
              });
            }

            ten_random_sleep_range_ms(0, 3);

            delete ten_env_proxy;
          },
          ten_env_proxy);
    } else if (cmd->get_name() == std::string("hello_world")) {
      if (is_received_all_data() &&
          (received_from_extension_2_cmd_result ==
           expected_received_from_extension_2_cmd_result)) {
        TEN_LOGI("extension_2 received all data, return result.");

        auto cmd_result = ten::cmd_result_t::create(TEN_STATUS_CODE_OK, *cmd);
        cmd_result->set_property("detail", "ok");
        ten_env.return_result(std::move(cmd_result));
      } else {
        hello_cmd = std::move(cmd);
      }
    }
  }

  void on_data(TEN_UNUSED ten::ten_env_t &ten_env,
               std::unique_ptr<ten::data_t> data) override {
    auto *test_data =
        static_cast<int64_t *>(data->get_property_ptr("test_data"));

    int outer_thread_num = ((*test_data) >> 16) - 1;
    int received_value = (*test_data) & 0xFFFF;
    TEN_ASSERT(data_received_count[outer_thread_num] == received_value,
               "should be %d, but get %d",
               data_received_count[outer_thread_num], received_value);

    data_received_count[outer_thread_num]++;

    if (data_received_count[outer_thread_num] % 50 == 0) {
      TEN_LOGD("extension_2 received %d data from outer thread %d",
               data_received_count[outer_thread_num], outer_thread_num);
    }

    if ((hello_cmd != nullptr) && is_received_all_data() &&
        (received_from_extension_2_cmd_result ==
         expected_received_from_extension_2_cmd_result)) {
      TEN_LOGI("extension_2 received all data, return result.");

      auto cmd_result =
          ten::cmd_result_t::create(TEN_STATUS_CODE_OK, *hello_cmd);
      cmd_result->set_property("detail", "ok");
      ten_env.return_result(std::move(cmd_result));
    }
  }

  void on_stop(ten::ten_env_t &ten_env) override {
    TEN_ASSERT(timeout_thread, "Should not happen.");

    TEN_ENV_LOG_INFO(ten_env, "extension_2 join timeout thread.");

    timeout_thread->join();
    delete timeout_thread;

    ten_env.on_stop_done();
  }

 private:
  std::unique_ptr<ten::cmd_t> hello_cmd{nullptr};
  std::thread *timeout_thread{nullptr};

  std::vector<int> data_received_count;
  std::vector<int> expected_data_received_count;

  int received_from_extension_2_cmd_result{0};
  int expected_received_from_extension_2_cmd_result{FROM_EXTENSION_2_CMD_CNT};

  bool is_received_all_data() {
    for (int i = 0; i < OUTER_THREAD_CNT; ++i) {
      if (data_received_count[i] != expected_data_received_count[i]) {
        return false;
      }
    }
    return true;
  }
};

class test_app : public ten::app_t {
 public:
  void on_configure(ten::ten_env_t &ten_env) override {
    bool rc = ten_env.init_property_from_json(
        // clang-format off
        R"({
             "ten": {
               "uri": "msgpack://127.0.0.1:8001/",
               "log": {
                 "level": 2
               }
             }
           })",
        // clang-format on
        nullptr);
    ASSERT_EQ(rc, true);

    ten_env.on_configure_done();
  }
};

void *test_app_thread_main(TEN_UNUSED void *args) {
  auto *app = new test_app();
  app->run();
  delete app;

  return nullptr;
}

TEN_CPP_REGISTER_ADDON_AS_EXTENSION(
    one_hundred_and_twenty_eight_threads_attempt_to_suspend_1__test_extension_1,
    test_extension_1);
TEN_CPP_REGISTER_ADDON_AS_EXTENSION(
    one_hundred_and_twenty_eight_threads_attempt_to_suspend_1__test_extension_2,
    test_extension_2);

}  // namespace

TEST(OuterThreadTest,
     OneHundredAndTwentyEightThreadsAttemptToSuspend1) {  // NOLINT
  // Start app.
  auto *app_thread =
      ten_thread_create("app thread", test_app_thread_main, nullptr);

  // Create a client and connect to the app.
  auto *client = new ten::msgpack_tcp_client_t("msgpack://127.0.0.1:8001/");

  // Send graph.
  auto start_graph_cmd = ten::cmd_start_graph_t::create();
  start_graph_cmd->set_graph_from_json(R"({
           "nodes": [{
               "type": "extension",
               "name": "test_extension_1",
               "addon": "one_hundred_and_twenty_eight_threads_attempt_to_suspend_1__test_extension_1",
               "app": "msgpack://127.0.0.1:8001/",
               "extension_group": "basic_extension_group"
             },{
               "type": "extension",
               "name": "test_extension_2",
               "addon": "one_hundred_and_twenty_eight_threads_attempt_to_suspend_1__test_extension_2",
               "app": "msgpack://127.0.0.1:8001/",
               "extension_group": "basic_extension_group"
             }],
             "connections": [{
               "app": "msgpack://127.0.0.1:8001/",
               "extension": "test_extension_1",
               "cmd": [{
                 "name": "start_to_send",
                 "dest": [{
                   "app": "msgpack://127.0.0.1:8001/",
                   "extension": "test_extension_2"
                 }]
               }],
               "data": [{
                 "name": "data",
                 "dest": [{
                   "app": "msgpack://127.0.0.1:8001/",
                   "extension": "test_extension_2"
                 }]
               }]
             },{
               "app": "msgpack://127.0.0.1:8001/",
               "extension": "test_extension_2",
               "cmd": [{
                 "name": "from_extension_2",
                 "dest": [{
                   "app": "msgpack://127.0.0.1:8001/",
                   "extension": "test_extension_1"
                 }]
               }]
             }]
           })");
  auto cmd_result =
      client->send_cmd_and_recv_result(std::move(start_graph_cmd));
  ten_test::check_status_code(cmd_result, TEN_STATUS_CODE_OK);

  // Send a user-defined 'hello world' command.
  auto hello_world_cmd = ten::cmd_t::create("hello_world");
  hello_world_cmd->set_dest("msgpack://127.0.0.1:8001/", nullptr,
                            "basic_extension_group", "test_extension_2");
  cmd_result = client->send_cmd_and_recv_result(std::move(hello_world_cmd));

  TEN_LOGI("clinet receives cmd_result");

  ten_test::check_status_code(cmd_result, TEN_STATUS_CODE_OK);
  ten_test::check_detail_with_string(cmd_result, "ok");

  TEN_LOGI("delete client");
  delete client;

  TEN_LOGI("join app_thread");
  ten_thread_join(app_thread, -1);

  TEN_LOGI("end");
}
