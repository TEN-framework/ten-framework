///////////////////////////////////////////////////////////////////////////////
//  Copyright Christopher Kormanyos 2019 - 2022.
//  Distributed under the Boost Software License,
//  Version 1.0. (See accompanying file LICENSE_1_0.txt
//  or copy at http://www.boost.org/LICENSE_1_0.txt)
//

#ifndef TEST_UINTWIDE_T_N_BASE_2019_12_29_H // NOLINT(llvm-header-guard)
  #define TEST_UINTWIDE_T_N_BASE_2019_12_29_H

  #include <atomic>
  #include <random>
  #include <sstream>

  #include <boost/version.hpp>

  #if !defined(BOOST_VERSION)
  #error BOOST_VERSION is not defined. Ensure that <boost/version.hpp> is properly included.
  #endif

  #if ((BOOST_VERSION >= 107900) && !defined(BOOST_MP_STANDALONE))
  #define BOOST_MP_STANDALONE
  #endif

  #if ((BOOST_VERSION >= 108000) && !defined(BOOST_NO_EXCEPTIONS))
  #define BOOST_NO_EXCEPTIONS
  #endif

  #if (BOOST_VERSION < 108000)
  #if defined(__GNUC__)
  #pragma GCC diagnostic push
  #pragma GCC diagnostic ignored "-Wconversion"
  #pragma GCC diagnostic push
  #pragma GCC diagnostic ignored "-Wsign-conversion"
  #pragma GCC diagnostic push
  #pragma GCC diagnostic ignored "-Wunused-parameter"
  #endif
  #endif

  #if (defined(__GNUC__) && !defined(__clang__) && (__GNUC__ >= 12))
  #pragma GCC diagnostic push
  #pragma GCC diagnostic ignored "-Wrestrict"
  #endif

  #if (BOOST_VERSION < 108000)
  #if (defined(__clang__) && (__clang_major__ > 9)) && !defined(__APPLE__)
  #pragma GCC diagnostic push
  #pragma GCC diagnostic ignored "-Wdeprecated-copy"
  #endif
  #endif

  #include <boost/multiprecision/cpp_int.hpp>

  #include <math/wide_integer/uintwide_t.h>
  #include <test/parallel_for.h>

  class test_uintwide_t_n_base
  {
  public:
    virtual ~test_uintwide_t_n_base() = default;

    #if defined(WIDE_INTEGER_NAMESPACE)
    WIDE_INTEGER_NODISCARD
    virtual auto get_digits2() const -> WIDE_INTEGER_NAMESPACE::math::wide_integer::size_t = 0;
    #else
    WIDE_INTEGER_NODISCARD
    virtual auto get_digits2() const -> ::math::wide_integer::size_t = 0;
    #endif

    WIDE_INTEGER_NODISCARD
    auto size() const -> std::size_t { return number_of_cases; }

    virtual auto initialize() -> void = 0;

    test_uintwide_t_n_base() = delete;

    test_uintwide_t_n_base(const test_uintwide_t_n_base&)  = delete;
    test_uintwide_t_n_base(      test_uintwide_t_n_base&&) = delete;

    auto operator=(const test_uintwide_t_n_base&)  -> test_uintwide_t_n_base& = delete;
    auto operator=(      test_uintwide_t_n_base&&) -> test_uintwide_t_n_base& = delete;

  protected:
    using random_engine_type =
      std::linear_congruential_engine<std::uint32_t, 48271, 0, 2147483647>; // NOLINT(cppcoreguidelines-avoid-magic-numbers,readability-magic-numbers)

    static auto my_random_generator() -> random_engine_type&;

    explicit test_uintwide_t_n_base(const std::size_t count)
      : number_of_cases(count) { }

    template<typename UnsignedIntegralType>
    static auto hexlexical_cast(const UnsignedIntegralType& u) -> std::string
    {
      std::stringstream ss;

      ss << std::hex << u;

      return ss.str();
    }

    template<typename IntegralType>
    static auto declexical_cast(const IntegralType& n) -> std::string
    {
      std::stringstream ss;

      ss << std::dec << n;

      return ss.str();
    }

    template<typename OtherLocalUintType,
             typename OtherBoostUintType,
             typename AllocatorType = void>
    static auto get_equal_random_test_values_boost_and_local_n(      OtherLocalUintType* u_local,
                                                                     OtherBoostUintType* u_boost,
                                                               const std::size_t         count) -> void
    {
      using other_local_uint_type = OtherLocalUintType;
      using other_boost_uint_type = OtherBoostUintType;

      my_random_generator().seed(static_cast<typename random_engine_type::result_type>(std::clock()));

      #if defined(WIDE_INTEGER_NAMESPACE)
      using distribution_type =
        WIDE_INTEGER_NAMESPACE::math::wide_integer::uniform_int_distribution<other_local_uint_type::my_width2, typename other_local_uint_type::limb_type, AllocatorType>;
      #else
      using distribution_type =
        ::math::wide_integer::uniform_int_distribution<other_local_uint_type::my_width2, typename other_local_uint_type::limb_type, AllocatorType>;
      #endif

      distribution_type distribution;

      std::atomic_flag rnd_lock = ATOMIC_FLAG_INIT;

      my_concurrency::parallel_for
      (
        static_cast<std::size_t>(0U),
        count,
        [&u_local, &u_boost, &distribution, &rnd_lock](std::size_t i)
        {
          while(rnd_lock.test_and_set()) { ; }
          const other_local_uint_type a = distribution(my_random_generator());
          rnd_lock.clear();

          u_local[i] = a;                                                // NOLINT(cppcoreguidelines-pro-bounds-pointer-arithmetic)
          u_boost[i] = other_boost_uint_type("0x" + hexlexical_cast(a)); // NOLINT(cppcoreguidelines-pro-bounds-pointer-arithmetic)
        }
      );
    }

  private:
    const std::size_t number_of_cases;
  };

  #if (BOOST_VERSION < 108000)
  #if (defined(__clang__) && (__clang_major__ > 9)) && !defined(__APPLE__)
  #pragma GCC diagnostic pop
  #endif
  #endif

  #if (defined(__GNUC__) && !defined(__clang__) && (__GNUC__ >= 12))
  #pragma GCC diagnostic pop
  #endif

  #if (BOOST_VERSION < 108000)
  #if defined(__GNUC__)
  #pragma GCC diagnostic pop
  #pragma GCC diagnostic pop
  #pragma GCC diagnostic pop
  #endif
  #endif

#endif // TEST_UINTWIDE_T_N_BASE_2019_12_29_H
