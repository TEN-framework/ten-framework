///////////////////////////////////////////////////////////////////////////////
//  Copyright Christopher Kormanyos 2019 - 2022.
//  Distributed under the Boost Software License,
//  Version 1.0. (See accompanying file LICENSE_1_0.txt
//  or copy at http://www.boost.org/LICENSE_1_0.txt)
//

#ifndef TEST_UINTWIDE_T_N_BINARY_OPS_BASE_2019_12_19_H // NOLINT(llvm-header-guard)
  #define TEST_UINTWIDE_T_N_BINARY_OPS_BASE_2019_12_19_H

  #include <cstdint>
  #include <iomanip>
  #include <iostream>
  #include <random>

  #include <test/test_uintwide_t_n_base.h>

  class test_uintwide_t_n_binary_ops_base : public test_uintwide_t_n_base
  {
  public:
    explicit test_uintwide_t_n_binary_ops_base(const std::size_t count)
      : test_uintwide_t_n_base(count) { }

    test_uintwide_t_n_binary_ops_base() = delete;

    test_uintwide_t_n_binary_ops_base(const test_uintwide_t_n_binary_ops_base&)  = delete;
    test_uintwide_t_n_binary_ops_base(      test_uintwide_t_n_binary_ops_base&&) = delete;

    auto operator=(const test_uintwide_t_n_binary_ops_base&)  -> test_uintwide_t_n_binary_ops_base& = delete;
    auto operator=(      test_uintwide_t_n_binary_ops_base&&) -> test_uintwide_t_n_binary_ops_base& = delete;

    ~test_uintwide_t_n_binary_ops_base() override = default;

    virtual auto do_test(std::size_t rounds) -> bool = 0;

  protected:
    using random_generator_type = std::mersenne_twister_engine<std::uint32_t,
                                                               static_cast<std::size_t>(UINT32_C( 32)),
                                                               static_cast<std::size_t>(UINT32_C(624)),
                                                               static_cast<std::size_t>(UINT32_C(397)),
                                                               static_cast<std::size_t>(UINT32_C( 31)),
                                                               UINT32_C(0x9908B0DF),
                                                               static_cast<std::size_t>(UINT32_C( 11)),
                                                               UINT32_C(0xFFFFFFFF),
                                                               static_cast<std::size_t>(UINT32_C(  7)),
                                                               UINT32_C(0x9D2C5680),
                                                               static_cast<std::size_t>(UINT32_C( 15)),
                                                               UINT32_C(0xEFC60000),
                                                               static_cast<std::size_t>(UINT32_C( 18)),
                                                               UINT32_C(1812433253)>;

    static auto my_rnd() -> std::random_device&;
    static auto my_gen() -> random_generator_type&;
  };

#endif // TEST_UINTWIDE_T_N_BINARY_OPS_BASE_2019_12_19_H
