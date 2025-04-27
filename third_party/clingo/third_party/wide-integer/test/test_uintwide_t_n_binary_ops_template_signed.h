///////////////////////////////////////////////////////////////////////////////
//  Copyright Christopher Kormanyos 2021 - 2022.
//  Distributed under the Boost Software License,
//  Version 1.0. (See accompanying file LICENSE_1_0.txt
//  or copy at http://www.boost.org/LICENSE_1_0.txt)
//

#ifndef TEST_UINTWIDE_T_N_BINARY_OPS_TEMPLATE_SIGNED_2021_06_05_H // NOLINT(llvm-header-guard)
  #define TEST_UINTWIDE_T_N_BINARY_OPS_TEMPLATE_SIGNED_2021_06_05_H

  #include <algorithm>
  #include <atomic>
  #include <cstddef>
  #include <random>
  #include <vector>

  #include <test/test_uintwide_t_n_binary_ops_base.h>

  #if defined(WIDE_INTEGER_NAMESPACE)
  template<const WIDE_INTEGER_NAMESPACE::math::wide_integer::size_t MyDigits2,
           typename MyLimbType = std::uint32_t,
           typename AllocatorType = void>
  #else
  template<const ::math::wide_integer::size_t MyDigits2,
           typename MyLimbType = std::uint32_t,
           typename AllocatorType = void>
  #endif
  class test_uintwide_t_n_binary_ops_template_signed : public test_uintwide_t_n_binary_ops_base // NOLINT(cppcoreguidelines-special-member-functions,hicpp-special-member-functions)
  {
  private:
    #if defined(WIDE_INTEGER_NAMESPACE)
    static constexpr auto digits2 = static_cast<WIDE_INTEGER_NAMESPACE::math::wide_integer::size_t>(MyDigits2);
    #else
    static constexpr auto digits2 = static_cast<math::wide_integer::size_t>(MyDigits2);
    #endif

    using boost_uint_backend_allocator_type = void;

    using boost_uint_backend_type =
      boost::multiprecision::cpp_int_backend<digits2,
                                             digits2,
                                             boost::multiprecision::unsigned_magnitude,
                                             boost::multiprecision::unchecked,
                                             boost_uint_backend_allocator_type>;

    using boost_sint_backend_allocator_type = void;

    using boost_sint_backend_type =
      boost::multiprecision::cpp_int_backend<digits2,
                                             digits2,
                                             boost::multiprecision::signed_magnitude,
                                             boost::multiprecision::unchecked,
                                             boost_sint_backend_allocator_type>;

    using boost_uint_type = boost::multiprecision::number<boost_uint_backend_type, boost::multiprecision::et_on>;
    using boost_sint_type = boost::multiprecision::number<boost_sint_backend_type, boost::multiprecision::et_on>;

    using local_limb_type = MyLimbType;

    #if defined(WIDE_INTEGER_NAMESPACE)
    using local_uint_type = WIDE_INTEGER_NAMESPACE::math::wide_integer::uintwide_t<digits2, local_limb_type, AllocatorType>;
    using local_sint_type = WIDE_INTEGER_NAMESPACE::math::wide_integer::uintwide_t<digits2, local_limb_type, AllocatorType, true>;
    #else
    using local_uint_type = ::math::wide_integer::uintwide_t<digits2, local_limb_type, AllocatorType>;
    using local_sint_type = ::math::wide_integer::uintwide_t<digits2, local_limb_type, AllocatorType, true>;
    #endif

  public:
    explicit test_uintwide_t_n_binary_ops_template_signed(const std::size_t count)
      : test_uintwide_t_n_binary_ops_base(count) { }

    ~test_uintwide_t_n_binary_ops_template_signed() override = default;

    #if defined(WIDE_INTEGER_NAMESPACE)
    WIDE_INTEGER_NODISCARD auto get_digits2() const -> WIDE_INTEGER_NAMESPACE::math::wide_integer::size_t override { return digits2; }
    #else
    WIDE_INTEGER_NODISCARD auto get_digits2() const -> ::math::wide_integer::size_t override { return digits2; }
    #endif

    auto initialize() -> void override
    {
      a_local.clear();
      b_local.clear();
      a_local_signed.clear();
      b_local_signed.clear();

      a_boost.clear();
      b_boost.clear();
      a_boost_signed.clear();
      b_boost_signed.clear();

      a_local.resize(size());
      b_local.resize(size());
      a_local_signed.resize(size());
      b_local_signed.resize(size());

      a_boost.resize(size());
      b_boost.resize(size());
      a_boost_signed.resize(size());
      b_boost_signed.resize(size());

      get_equal_random_test_values_boost_and_local_n<local_uint_type, boost_uint_type, AllocatorType>(a_local.data(), a_boost.data(), size());
      get_equal_random_test_values_boost_and_local_n<local_uint_type, boost_uint_type, AllocatorType>(b_local.data(), b_boost.data(), size());

      std::copy(a_local.cbegin(), a_local.cend(), a_local_signed.begin());
      std::copy(b_local.cbegin(), b_local.cend(), b_local_signed.begin());

      std::copy(a_boost.cbegin(), a_boost.cend(), a_boost_signed.begin());
      std::copy(b_boost.cbegin(), b_boost.cend(), b_boost_signed.begin());
    }

    WIDE_INTEGER_NODISCARD auto test_binary_add() const -> bool
    {
      bool result_is_ok = true;

      std::atomic_flag test_lock = ATOMIC_FLAG_INIT;

      my_concurrency::parallel_for
      (
        static_cast<std::size_t>(0U),
        size(),
        [&test_lock, &result_is_ok, this](std::size_t i)
        {
          const boost_sint_type c_boost_signed = a_boost_signed[i] + b_boost_signed[i];
          const local_sint_type c_local_signed = a_local_signed[i] + b_local_signed[i];

          const std::string str_boost_signed = hexlexical_cast(static_cast<boost_uint_type>(c_boost_signed));
          const std::string str_local_signed = hexlexical_cast(static_cast<local_uint_type>(c_local_signed));

          while(test_lock.test_and_set()) { ; }
          result_is_ok = ((str_boost_signed == str_local_signed) && result_is_ok);
          test_lock.clear();
        }
      );

      return result_is_ok;
    }

    WIDE_INTEGER_NODISCARD auto test_binary_sub() const -> bool
    {
      bool result_is_ok = true;

      std::atomic_flag test_lock = ATOMIC_FLAG_INIT;

      my_concurrency::parallel_for
      (
        static_cast<std::size_t>(0U),
        size(),
        [&test_lock, &result_is_ok, this](std::size_t i)
        {
          const boost_sint_type c_boost_signed = a_boost_signed[i] - b_boost_signed[i];
          const local_sint_type c_local_signed = a_local_signed[i] - b_local_signed[i];

          const std::string str_boost_signed = hexlexical_cast(static_cast<boost_uint_type>(c_boost_signed));
          const std::string str_local_signed = hexlexical_cast(static_cast<local_uint_type>(c_local_signed));

          while(test_lock.test_and_set()) { ; }
          result_is_ok = ((str_boost_signed == str_local_signed) && result_is_ok);
          test_lock.clear();
        }
      );

      return result_is_ok;
    }

    WIDE_INTEGER_NODISCARD auto test_binary_mul() const -> bool
    {
      bool result_is_ok = true;

      std::atomic_flag test_lock = ATOMIC_FLAG_INIT;

      my_concurrency::parallel_for
      (
        static_cast<std::size_t>(0U),
        size(),
        [&test_lock, &result_is_ok, this](std::size_t i)
        {
          const boost_sint_type c_boost_signed = a_boost_signed[i] * b_boost_signed[i];
          const local_sint_type c_local_signed = a_local_signed[i] * b_local_signed[i];

          const std::string str_boost_signed = hexlexical_cast(static_cast<boost_uint_type>(c_boost_signed));
          const std::string str_local_signed = hexlexical_cast(static_cast<local_uint_type>(c_local_signed));

          while(test_lock.test_and_set()) { ; }
          result_is_ok = ((str_boost_signed == str_local_signed) && result_is_ok);
          test_lock.clear();
        }
      );

      return result_is_ok;
    }

    WIDE_INTEGER_NODISCARD auto test_binary_div() const -> bool
    {
      bool result_is_ok = true;

      std::atomic_flag test_lock = ATOMIC_FLAG_INIT;

      my_concurrency::parallel_for
      (
        static_cast<std::size_t>(0U),
        size(),
        [&test_lock, &result_is_ok, this](std::size_t i)
        {
          const boost_sint_type c_boost_signed = a_boost_signed[i] / b_boost_signed[i];
          const local_sint_type c_local_signed = a_local_signed[i] / b_local_signed[i];

          const std::string str_boost_signed = hexlexical_cast(static_cast<boost_uint_type>(c_boost_signed));
          const std::string str_local_signed = hexlexical_cast(static_cast<local_uint_type>(c_local_signed));

          while(test_lock.test_and_set()) { ; }
          result_is_ok = ((str_boost_signed == str_local_signed) && result_is_ok);
          test_lock.clear();
        }
      );

      return result_is_ok;
    }

    WIDE_INTEGER_NODISCARD auto test_binary_mod() const -> bool
    {
      bool result_is_ok = true;

      std::atomic_flag test_lock = ATOMIC_FLAG_INIT;

      my_concurrency::parallel_for
      (
        static_cast<std::size_t>(0U),
        size(),
        [&test_lock, &result_is_ok, this](std::size_t i)
        {
          const boost_sint_type c_boost_signed = a_boost_signed[i] % b_boost_signed[i];
          const local_sint_type c_local_signed = a_local_signed[i] % b_local_signed[i];

          const std::string str_boost_signed = hexlexical_cast(static_cast<boost_uint_type>(c_boost_signed));
          const std::string str_local_signed = hexlexical_cast(static_cast<local_uint_type>(c_local_signed));

          while(test_lock.test_and_set()) { ; }
          result_is_ok = ((str_boost_signed == str_local_signed) && result_is_ok);
          test_lock.clear();
        }
      );

      return result_is_ok;
    }

    auto do_test(std::size_t rounds) -> bool override
    {
      bool result_is_ok = true;

      for(std::size_t i = 0U; i < rounds; ++i)
      {
        std::cout << "initialize()       boost compare with uintwide_t: round " << i << ",  digits2: " << this->get_digits2() << std::endl;
        this->initialize();

        std::cout << "test_binary_add()  boost compare with uintwide_t: round " << i << ",  digits2: " << this->get_digits2() << std::endl;
        result_is_ok = (test_binary_add() && result_is_ok);

        std::cout << "test_binary_sub()  boost compare with uintwide_t: round " << i << ",  digits2: " << this->get_digits2() << std::endl;
        result_is_ok = (test_binary_sub() && result_is_ok);

        std::cout << "test_binary_mul()  boost compare with uintwide_t: round " << i << ",  digits2: " << this->get_digits2() << std::endl;
        result_is_ok = (test_binary_mul() && result_is_ok);

        std::cout << "test_binary_div()  boost compare with uintwide_t: round " << i << ",  digits2: " << this->get_digits2() << std::endl;
        result_is_ok = (test_binary_div() && result_is_ok);

        std::cout << "test_binary_mod()  boost compare with uintwide_t: round " << i << ",  digits2: " << this->get_digits2() << std::endl;
        result_is_ok = (test_binary_mod() && result_is_ok);
      }

      return result_is_ok;
    }

  private:
    std::vector<local_uint_type> a_local { };        // NOLINT(readability-identifier-naming)
    std::vector<local_uint_type> b_local { };        // NOLINT(readability-identifier-naming)
    std::vector<local_sint_type> a_local_signed { }; // NOLINT(readability-identifier-naming)
    std::vector<local_sint_type> b_local_signed { }; // NOLINT(readability-identifier-naming)

    std::vector<boost_uint_type> a_boost { };        // NOLINT(readability-identifier-naming)
    std::vector<boost_uint_type> b_boost { };        // NOLINT(readability-identifier-naming)
    std::vector<boost_sint_type> a_boost_signed { }; // NOLINT(readability-identifier-naming)
    std::vector<boost_sint_type> b_boost_signed { }; // NOLINT(readability-identifier-naming)
  };


  template<typename AllocatorType>
  #if defined(WIDE_INTEGER_NAMESPACE)
  class test_uintwide_t_n_binary_ops_template_signed<static_cast<WIDE_INTEGER_NAMESPACE::math::wide_integer::size_t>(UINT32_C(64)), std::uint16_t, AllocatorType> // NOLINT(cppcoreguidelines-special-member-functions,hicpp-special-member-functions)
  #else
  class test_uintwide_t_n_binary_ops_template_signed<static_cast<math::wide_integer::size_t>(UINT32_C(64)), std::uint16_t, AllocatorType> // NOLINT(cppcoreguidelines-special-member-functions,hicpp-special-member-functions)
  #endif
    : public test_uintwide_t_n_binary_ops_base
  {
  private:
    #if defined(WIDE_INTEGER_NAMESPACE)
    static constexpr auto digits2 = static_cast<WIDE_INTEGER_NAMESPACE::math::wide_integer::size_t>(UINT32_C(64));
    #else
    static constexpr auto digits2 = static_cast<math::wide_integer::size_t>(UINT32_C(64));
    #endif

    using native_uint_type = std::uint64_t;
    using native_sint_type = std::int64_t;

    using local_limb_type = std::uint16_t;

    #if defined(WIDE_INTEGER_NAMESPACE)
    using local_uint_type = WIDE_INTEGER_NAMESPACE::math::wide_integer::uintwide_t<digits2, local_limb_type, AllocatorType>;
    using local_sint_type = WIDE_INTEGER_NAMESPACE::math::wide_integer::uintwide_t<digits2, local_limb_type, AllocatorType, true>;
    #else
    using local_uint_type = ::math::wide_integer::uintwide_t<digits2, local_limb_type, AllocatorType>;
    using local_sint_type = ::math::wide_integer::uintwide_t<digits2, local_limb_type, AllocatorType, true>;
    #endif

  public:
    explicit test_uintwide_t_n_binary_ops_template_signed(const std::size_t count)
      : test_uintwide_t_n_binary_ops_base(count) { }

    ~test_uintwide_t_n_binary_ops_template_signed() override = default;

    #if defined(WIDE_INTEGER_NAMESPACE)
    WIDE_INTEGER_NODISCARD auto get_digits2() const -> WIDE_INTEGER_NAMESPACE::math::wide_integer::size_t override { return digits2; }
    #else
    WIDE_INTEGER_NODISCARD auto get_digits2() const -> ::math::wide_integer::size_t override { return digits2; }
    #endif

    auto initialize() -> void override
    {
      a_local_signed.clear();
      b_local_signed.clear();

      a_native_signed.clear();
      b_native_signed.clear();

      a_local_signed.resize(size());
      b_local_signed.resize(size());

      a_native_signed.resize(size());
      b_native_signed.resize(size());

      std::mt19937_64 eng64(static_cast<typename std::mt19937_64::result_type>(std::clock()));

      std::uniform_int_distribution<std::uint64_t> dst_u64(UINT64_C(1), UINT64_C(0xFFFFFFFFFFFFFFFF));

      for(size_t i = 0U; i < size(); ++i)
      {
        a_native_signed[i] = static_cast<std::int64_t>(dst_u64(eng64));
        b_native_signed[i] = static_cast<std::int64_t>(dst_u64(eng64));

        a_local_signed[i] = static_cast<local_sint_type>(a_native_signed[i]);
        b_local_signed[i] = static_cast<local_sint_type>(b_native_signed[i]);
      }
    }

    WIDE_INTEGER_NODISCARD auto test_binary_add() const -> bool
    {
      bool result_is_ok = true;

      std::atomic_flag test_lock = ATOMIC_FLAG_INIT;

      my_concurrency::parallel_for
      (
        static_cast<std::size_t>(0U),
        size(),
        [&test_lock, &result_is_ok, this](std::size_t i)
        {
          const native_uint_type c_native_unsigned =
            static_cast<native_uint_type>(a_native_signed[i]) + static_cast<native_uint_type>(b_native_signed[i]);

          const native_sint_type c_native_signed   =
            c_native_unsigned < UINT64_C(0x8000000000000000) ? static_cast<native_sint_type>(c_native_unsigned) : -static_cast<native_sint_type>(~c_native_unsigned + 1U);

          const local_sint_type  c_local_signed  = a_local_signed[i] + b_local_signed[i];

          const std::string str_native_signed = declexical_cast(c_native_signed);
          const std::string str_local_signed  = declexical_cast(c_local_signed);

          while(test_lock.test_and_set()) { ; }
          result_is_ok = ((str_native_signed == str_local_signed) && result_is_ok);
          test_lock.clear();
        }
      );

      return result_is_ok;
    }

    WIDE_INTEGER_NODISCARD auto test_binary_sub() const -> bool
    {
      bool result_is_ok = true;

      std::atomic_flag test_lock = ATOMIC_FLAG_INIT;

      my_concurrency::parallel_for
      (
        static_cast<std::size_t>(0U),
        size(),
        [&test_lock, &result_is_ok, this](std::size_t i)
        {
          const native_uint_type c_native_unsigned =
            static_cast<native_uint_type>(a_native_signed[i]) - static_cast<native_uint_type>(b_native_signed[i]);

          const native_sint_type c_native_signed   =
            c_native_unsigned < UINT64_C(0x8000000000000000) ? static_cast<native_sint_type>(c_native_unsigned) : -static_cast<native_sint_type>(~c_native_unsigned + 1U);

          const local_sint_type  c_local_signed    = a_local_signed[i] - b_local_signed[i];

          const std::string str_native_signed = declexical_cast(c_native_signed);
          const std::string str_local_signed  = declexical_cast(c_local_signed);

          while(test_lock.test_and_set()) { ; }
          result_is_ok = ((str_native_signed == str_local_signed) && result_is_ok);
          test_lock.clear();
        }
      );

      return result_is_ok;
    }

    WIDE_INTEGER_NODISCARD auto test_binary_mul() const -> bool
    {
      bool result_is_ok = true;

      std::atomic_flag test_lock = ATOMIC_FLAG_INIT;

      my_concurrency::parallel_for
      (
        static_cast<std::size_t>(0U),
        size(),
        [&test_lock, &result_is_ok, this](std::size_t i)
        {
          const native_uint_type c_native_unsigned =
            static_cast<native_uint_type>(a_native_signed[i]) * static_cast<native_uint_type>(b_native_signed[i]);

          const native_sint_type c_native_signed   =
            c_native_unsigned < UINT64_C(0x8000000000000000) ? static_cast<native_sint_type>(c_native_unsigned) : -static_cast<native_sint_type>(~c_native_unsigned + 1U);

          const local_sint_type  c_local_signed    = a_local_signed[i] * b_local_signed[i];

          const std::string str_native_signed = declexical_cast(c_native_signed);
          const std::string str_local_signed  = declexical_cast(c_local_signed);

          while(test_lock.test_and_set()) { ; }
          result_is_ok = ((str_native_signed == str_local_signed) && result_is_ok);
          test_lock.clear();
        }
      );

      return result_is_ok;
    }

    WIDE_INTEGER_NODISCARD auto test_binary_div() const -> bool
    {
      bool result_is_ok = true;

      std::atomic_flag test_lock = ATOMIC_FLAG_INIT;

      my_concurrency::parallel_for
      (
        static_cast<std::size_t>(0U),
        size(),
        [&test_lock, &result_is_ok, this](std::size_t i)
        {
          const native_sint_type c_native_signed = a_native_signed[i] / b_native_signed[i];
          const local_sint_type  c_local_signed  = a_local_signed [i] / b_local_signed[i];

          const std::string str_native_signed = declexical_cast(c_native_signed);
          const std::string str_local_signed  = declexical_cast(c_local_signed);

          while(test_lock.test_and_set()) { ; }
          result_is_ok = ((str_native_signed == str_local_signed) && result_is_ok);
          test_lock.clear();
        }
      );

      return result_is_ok;
    }

    WIDE_INTEGER_NODISCARD auto test_binary_mod() const -> bool
    {
      bool result_is_ok = true;

      std::atomic_flag test_lock = ATOMIC_FLAG_INIT;

      my_concurrency::parallel_for
      (
        static_cast<std::size_t>(0U),
        size(),
        [&test_lock, &result_is_ok, this](std::size_t i)
        {
          const native_sint_type c_native_signed = a_native_signed[i] % b_native_signed[i];
          const local_sint_type  c_local_signed  = a_local_signed [i] % b_local_signed[i];

          const std::string str_native_signed = declexical_cast(c_native_signed);
          const std::string str_local_signed  = declexical_cast(c_local_signed);

          while(test_lock.test_and_set()) { ; }
          result_is_ok = ((str_native_signed == str_local_signed) && result_is_ok);
          test_lock.clear();
        }
      );

      return result_is_ok;
    }

    WIDE_INTEGER_NODISCARD virtual auto test_binary_mod1() const -> bool
    {
      using local_signed_limb_type = typename local_sint_type::limb_type;

      bool result_is_ok = true;

      std::atomic_flag test_lock = ATOMIC_FLAG_INIT;

      my_concurrency::parallel_for
      (
        static_cast<std::size_t>(0U),
        size(),
        [&test_lock, &result_is_ok, this](std::size_t i)
        {
          while(test_lock.test_and_set()) { ; }
          const auto u = static_cast<local_signed_limb_type>(my_distrib_1_to_0xFFFF(my_eng));
          test_lock.clear();

          const auto c_n = static_cast<local_signed_limb_type>(a_native_signed[i] % u);
          const auto c_l = static_cast<local_signed_limb_type>(a_local_signed [i] % u);

          while(test_lock.test_and_set()) { ; }
          result_is_ok = ((c_n == c_l) && result_is_ok);
          test_lock.clear();
        }
      );

      return result_is_ok;
    }

    WIDE_INTEGER_NODISCARD virtual auto test_binary_shr() const -> bool
    {
      my_gen().seed(static_cast<typename random_generator_type::result_type>(std::clock()));

      bool result_is_ok = true;

      std::atomic_flag test_lock = ATOMIC_FLAG_INIT;

      my_concurrency::parallel_for
      (
        static_cast<std::size_t>(0U),
        size(),
        [&test_lock, &result_is_ok, this](std::size_t i)
        {
          while(test_lock.test_and_set()) { ; }
          const auto u_shr = static_cast<std::uint32_t>(my_distrib_0_to_63(my_eng));
          test_lock.clear();

          const native_sint_type c_native_signed = a_native_signed[i] >> u_shr; // NOLINT(hicpp-signed-bitwise)
          const local_sint_type  c_local_signed  = a_local_signed [i] >> u_shr;

          const bool current_result_is_zero = (c_local_signed == 0U);
          const bool current_result_is_ok   = (current_result_is_zero || (c_native_signed == static_cast<std::int64_t>(c_local_signed)));

          while(test_lock.test_and_set()) { ; }
          result_is_ok = (current_result_is_ok && result_is_ok);
          test_lock.clear();
        }
      );

      return result_is_ok;
    }

    auto do_test(std::size_t rounds) -> bool override
    {
      bool result_is_ok = true;

      for(std::size_t i = 0U; i < rounds; ++i)
      {
        std::cout << "initialize()       boost compare with uintwide_t: round " << i << ",  digits2: " << this->get_digits2() << std::endl;
        this->initialize();

        std::cout << "test_binary_add()  boost compare with uintwide_t: round " << i << ",  digits2: " << this->get_digits2() << std::endl;
        result_is_ok = (test_binary_add() && result_is_ok);

        std::cout << "test_binary_sub()  boost compare with uintwide_t: round " << i << ",  digits2: " << this->get_digits2() << std::endl;
        result_is_ok = (test_binary_sub() && result_is_ok);

        std::cout << "test_binary_mul()  boost compare with uintwide_t: round " << i << ",  digits2: " << this->get_digits2() << std::endl;
        result_is_ok = (test_binary_mul() && result_is_ok);

        std::cout << "test_binary_div()  boost compare with uintwide_t: round " << i << ",  digits2: " << this->get_digits2() << std::endl;
        result_is_ok = (test_binary_div() && result_is_ok);

        std::cout << "test_binary_mod()  boost compare with uintwide_t: round " << i << ",  digits2: " << this->get_digits2() << std::endl;
        result_is_ok = (test_binary_mod() && result_is_ok);

        std::cout << "test_binary_mod1() boost compare with uintwide_t: round " << i << ",  digits2: " << this->get_digits2() << std::endl;
        result_is_ok = (test_binary_mod1() && result_is_ok);

        std::cout << "test_binary_shr()  boost compare with uintwide_t: round " << i << ",  digits2: " << this->get_digits2() << std::endl;
        result_is_ok = (test_binary_shr() && result_is_ok);
      }

      return result_is_ok;
    }

  private:
    static std::minstd_rand                my_eng;                 // NOLINT(cppcoreguidelines-avoid-non-const-global-variables)
    static std::uniform_int_distribution<> my_distrib_0_to_63;     // NOLINT(cppcoreguidelines-avoid-non-const-global-variables)
    static std::uniform_int_distribution<> my_distrib_1_to_0xFFFF; // NOLINT(cppcoreguidelines-avoid-non-const-global-variables)

    std::vector<local_sint_type> a_local_signed { }; // NOLINT(readability-identifier-naming)
    std::vector<local_sint_type> b_local_signed { }; // NOLINT(readability-identifier-naming)

    std::vector<native_sint_type> a_native_signed { }; // NOLINT(readability-identifier-naming)
    std::vector<native_sint_type> b_native_signed { }; // NOLINT(readability-identifier-naming)
  };

  #if defined(WIDE_INTEGER_NAMESPACE)
  template<typename AllocatorType> std::minstd_rand                test_uintwide_t_n_binary_ops_template_signed<static_cast<WIDE_INTEGER_NAMESPACE::math::wide_integer::size_t>(UINT32_C(64)), std::uint16_t, AllocatorType>::my_eng;                                                // NOLINT(cppcoreguidelines-avoid-non-const-global-variables,cert-err58-cpp,cert-msc32-c,cert-msc51-cpp)
  template<typename AllocatorType> std::uniform_int_distribution<> test_uintwide_t_n_binary_ops_template_signed<static_cast<WIDE_INTEGER_NAMESPACE::math::wide_integer::size_t>(UINT32_C(64)), std::uint16_t, AllocatorType>::my_distrib_0_to_63(UINT16_C(0), UINT16_C(63));         // NOLINT(cppcoreguidelines-avoid-non-const-global-variables,cert-err58-cpp)
  template<typename AllocatorType> std::uniform_int_distribution<> test_uintwide_t_n_binary_ops_template_signed<static_cast<WIDE_INTEGER_NAMESPACE::math::wide_integer::size_t>(UINT32_C(64)), std::uint16_t, AllocatorType>::my_distrib_1_to_0xFFFF(UINT16_C(1), UINT16_C(0xFFFF)); // NOLINT(cppcoreguidelines-avoid-non-const-global-variables,cert-err58-cpp)
  #else
  template<typename AllocatorType> std::minstd_rand                test_uintwide_t_n_binary_ops_template_signed<static_cast<math::wide_integer::size_t>(UINT32_C(64)), std::uint16_t, AllocatorType>::my_eng;                                                // NOLINT(cppcoreguidelines-avoid-non-const-global-variables,cert-err58-cpp,cert-msc32-c,cert-msc51-cpp)
  template<typename AllocatorType> std::uniform_int_distribution<> test_uintwide_t_n_binary_ops_template_signed<static_cast<math::wide_integer::size_t>(UINT32_C(64)), std::uint16_t, AllocatorType>::my_distrib_0_to_63(UINT16_C(0), UINT16_C(63));         // NOLINT(cppcoreguidelines-avoid-non-const-global-variables,cert-err58-cpp)
  template<typename AllocatorType> std::uniform_int_distribution<> test_uintwide_t_n_binary_ops_template_signed<static_cast<math::wide_integer::size_t>(UINT32_C(64)), std::uint16_t, AllocatorType>::my_distrib_1_to_0xFFFF(UINT16_C(1), UINT16_C(0xFFFF)); // NOLINT(cppcoreguidelines-avoid-non-const-global-variables,cert-err58-cpp)
  #endif

#endif // TEST_UINTWIDE_T_N_BINARY_OPS_TEMPLATE_SIGNED_2021_06_05_H
