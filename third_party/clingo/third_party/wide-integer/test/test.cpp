///////////////////////////////////////////////////////////////////
//  Copyright Christopher Kormanyos 2018 - 2023.                 //
//  Distributed under the Boost Software License,                //
//  Version 1.0. (See accompanying file LICENSE_1_0.txt          //
//  or copy at http://www.boost.org/LICENSE_1_0.txt)             //
///////////////////////////////////////////////////////////////////

// On Windows subsystem for LINUX
// cd /mnt/c/Users/User/Documents/Ks/PC_Software/NumericalPrograms/ExtendedNumberTypes/wide_integer

// When using -std=c++20 and g++-12
// g++-12 -finline-functions -march=native -mtune=native -O3 -Wall -Wextra -Wpedantic -Wconversion -Wsign-conversion -std=c++20 -DWIDE_INTEGER_HAS_LIMB_TYPE_UINT64 -DWIDE_INTEGER_HAS_MUL_8_BY_8_UNROLL -I. -I/mnt/c/boost/boost_1_83_0 -pthread -lpthread test/test.cpp test/test_uintwide_t_boost_backend.cpp test/test_uintwide_t_edge_cases.cpp test/test_uintwide_t_examples.cpp test/test_uintwide_t_float_convert.cpp test/test_uintwide_t_int_convert.cpp test/test_uintwide_t_n_base.cpp test/test_uintwide_t_n_binary_ops_base.cpp examples/example000a_builtin_convert.cpp test/test_uintwide_t_spot_values.cpp examples/example000_numeric_limits.cpp examples/example001_mul_div.cpp examples/example001a_div_mod.cpp examples/example002_shl_shr.cpp examples/example003_sqrt.cpp examples/example003a_cbrt.cpp examples/example004_rootk_pow.cpp examples/example005_powm.cpp examples/example005a_pow_factors_of_p99.cpp examples/example006_gcd.cpp examples/example007_random_generator.cpp examples/example008_miller_rabin_prime.cpp examples/example008a_miller_rabin_prime.cpp examples/example009_timed_mul.cpp examples/example009a_timed_mul_4_by_4.cpp examples/example009b_timed_mul_8_by_8.cpp examples/example010_uint48_t.cpp examples/example011_uint24_t.cpp examples/example012_rsa_crypto.cpp examples/example013_ecdsa_sign_verify.cpp examples/example014_pi_spigot_wide.cpp -o wide_integer.exe

// When using local Boost-develop branch, use specific include paths.
// -I/mnt/c/boost/modular_boost/boost/libs/config/include -I/mnt/c/boost/modular_boost/boost/libs/multiprecision/include

// When using -std=c++14 and g++
// g++ -finline-functions -march=native -mtune=native -O3 -Wall -Wextra -Wpedantic -Wconversion -Wsign-conversion -std=c++14 -DWIDE_INTEGER_HAS_LIMB_TYPE_UINT64 -DWIDE_INTEGER_HAS_MUL_8_BY_8_UNROLL -I. -I/mnt/c/boost/boost_1_83_0 -pthread -lpthread test/test.cpp test/test_uintwide_t_boost_backend.cpp test/test_uintwide_t_edge_cases.cpp test/test_uintwide_t_examples.cpp test/test_uintwide_t_float_convert.cpp test/test_uintwide_t_int_convert.cpp test/test_uintwide_t_n_base.cpp test/test_uintwide_t_n_binary_ops_base.cpp examples/example000a_builtin_convert.cpp test/test_uintwide_t_spot_values.cpp examples/example000_numeric_limits.cpp examples/example001_mul_div.cpp examples/example001a_div_mod.cpp examples/example002_shl_shr.cpp examples/example003_sqrt.cpp examples/example003a_cbrt.cpp examples/example004_rootk_pow.cpp examples/example005_powm.cpp examples/example005a_pow_factors_of_p99.cpp examples/example006_gcd.cpp examples/example007_random_generator.cpp examples/example008_miller_rabin_prime.cpp examples/example008a_miller_rabin_prime.cpp examples/example009_timed_mul.cpp examples/example009a_timed_mul_4_by_4.cpp examples/example009b_timed_mul_8_by_8.cpp examples/example010_uint48_t.cpp examples/example011_uint24_t.cpp examples/example012_rsa_crypto.cpp examples/example013_ecdsa_sign_verify.cpp examples/example014_pi_spigot_wide.cpp -o wide_integer.exe

// cd .tidy/make
// make prepare -f make_tidy_01_generic.gmk MY_BOOST_ROOT=/mnt/c/boost/boost_1_83_0
// make tidy -f make_tidy_01_generic.gmk --jobs=8 MY_BOOST_ROOT=/mnt/c/boost/boost_1_83_0

// cd .gcov/make
// make prepare -f make_gcov_01_generic.gmk MY_ALL_COV=0 MY_BOOST_ROOT=/mnt/c/boost/boost_1_83_0 MY_CC=g++
// make gcov -f make_gcov_01_generic.gmk --jobs=8 MY_ALL_COV=0 MY_BOOST_ROOT=/mnt/c/boost/boost_1_83_0 MY_CC=g++

// cd /mnt/c/Users/User/Documents/Ks/PC_Software/NumericalPrograms/ExtendedNumberTypes/wide_integer
// PATH=/home/chris/local/coverity/cov-analysis-linux64-2022.6.0/bin:$PATH
// cov-build --dir cov-int g++ -finline-functions -march=native -mtune=native -O3 -Wall -Wextra -Wpedantic -Wconversion -Wsign-conversion -std=c++14 -DWIDE_INTEGER_HAS_LIMB_TYPE_UINT64 -DWIDE_INTEGER_HAS_MUL_8_BY_8_UNROLL -I. -I/mnt/c/boost/boost_1_83_0 -pthread -lpthread test/test.cpp test/test_uintwide_t_boost_backend.cpp test/test_uintwide_t_edge_cases.cpp test/test_uintwide_t_examples.cpp test/test_uintwide_t_float_convert.cpp test/test_uintwide_t_int_convert.cpp test/test_uintwide_t_n_base.cpp test/test_uintwide_t_n_binary_ops_base.cpp examples/example000a_builtin_convert.cpp test/test_uintwide_t_spot_values.cpp examples/example000_numeric_limits.cpp examples/example001_mul_div.cpp examples/example001a_div_mod.cpp examples/example002_shl_shr.cpp examples/example003_sqrt.cpp examples/example003a_cbrt.cpp examples/example004_rootk_pow.cpp examples/example005_powm.cpp examples/example005a_pow_factors_of_p99.cpp examples/example006_gcd.cpp examples/example007_random_generator.cpp examples/example008_miller_rabin_prime.cpp examples/example008a_miller_rabin_prime.cpp examples/example009_timed_mul.cpp examples/example009a_timed_mul_4_by_4.cpp examples/example009b_timed_mul_8_by_8.cpp examples/example010_uint48_t.cpp examples/example011_uint24_t.cpp examples/example012_rsa_crypto.cpp examples/example013_ecdsa_sign_verify.cpp examples/example014_pi_spigot_wide.cpp -o wide_integer.exe
// tar caf wide-integer.bz2 cov-int

#include <chrono>
#include <iomanip>
#include <iostream>

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

#if (((BOOST_VERSION == 108000) || (BOOST_VERSION == 108100)) && defined(BOOST_NO_EXCEPTIONS))
#if defined(__clang__)
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wsometimes-uninitialized"
#endif
#if defined(_MSC_VER)
#pragma warning(push)
#pragma warning(disable : 4701)
#endif
#endif

#if (BOOST_VERSION < 107900)
#if defined(__GNUC__)
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wconversion"
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wsign-conversion"
#endif
#endif

#if (defined(__GNUC__) && !defined(__clang__) && (__GNUC__ >= 12))
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wrestrict"
#endif

#if (BOOST_VERSION < 107900)
#include <boost/lexical_cast.hpp>
#endif

#include <test/test_uintwide_t.h>
#include <test/test_uintwide_t_n_binary_ops_mul_div_4_by_4_template.h>
#include <test/test_uintwide_t_n_binary_ops_mul_n_by_m_template.h>
#include <test/test_uintwide_t_n_binary_ops_template.h>
#include <test/test_uintwide_t_n_binary_ops_template_signed.h>

#if defined(__clang__)
  #if defined __has_feature && __has_feature(thread_sanitizer)
  #define UINTWIDE_T_REDUCE_TEST_DEPTH
  #endif
#elif defined(__GNUC__)
  #if defined(__SANITIZE_THREAD__) || defined(WIDE_INTEGER_HAS_COVERAGE)
  #define UINTWIDE_T_REDUCE_TEST_DEPTH
  #endif
#elif defined(_MSC_VER)
  #if defined(_DEBUG)
  #define UINTWIDE_T_REDUCE_TEST_DEPTH
  #endif
#endif

namespace local {

using clock_type = std::chrono::high_resolution_clock;

const auto wide_decimal_time_start = clock_type::now();

#if !defined(UINTWIDE_T_REDUCE_TEST_DEPTH)
constexpr auto test_uintwide_t_n_binary_ops_rounds = static_cast<std::size_t>(4U); // NOLINT(cppcoreguidelines-avoid-magic-numbers,readability-magic-numbers)
#else
constexpr auto test_uintwide_t_n_binary_ops_rounds = static_cast<std::size_t>(1U); // NOLINT(cppcoreguidelines-avoid-magic-numbers,readability-magic-numbers)
#endif

#if !defined(UINTWIDE_T_REDUCE_TEST_DEPTH)
constexpr std::size_t test_uintwide_t_n_binary_ops_4_by_4_cases = std::uint32_t(1UL << 15U); // NOLINT(cppcoreguidelines-avoid-magic-numbers,readability-magic-numbers)
#else
constexpr std::size_t test_uintwide_t_n_binary_ops_4_by_4_cases = std::uint32_t(1UL << 9U); // NOLINT(cppcoreguidelines-avoid-magic-numbers,readability-magic-numbers)
#endif

auto test_uintwide_t_small_bits() -> bool
{
  std::cout << "running: test_uintwide_t_small_bits" << std::endl;

  bool result_is_ok = true;

  {
    #if defined(WIDE_INTEGER_NAMESPACE)
    using local_uint16_t = WIDE_INTEGER_NAMESPACE::math::wide_integer::uintwide_t<static_cast<WIDE_INTEGER_NAMESPACE::math::wide_integer::size_t>(UINT32_C(16)), std::uint8_t>;
    #else
    using local_uint16_t = ::math::wide_integer::uintwide_t<static_cast<math::wide_integer::size_t>(UINT32_C(16)), std::uint8_t>;
    #endif

    local_uint16_t a = UINT16_C(0x5522); // NOLINT(cppcoreguidelines-avoid-magic-numbers,readability-magic-numbers)
    local_uint16_t b = UINT16_C(0xFFEE); // NOLINT(cppcoreguidelines-avoid-magic-numbers,readability-magic-numbers)

    local_uint16_t c = a * b;

    result_is_ok = ((c == UINT32_C(0x039C)) && result_is_ok); // NOLINT(cppcoreguidelines-avoid-magic-numbers,readability-magic-numbers)
  }

  {
    #if defined(WIDE_INTEGER_NAMESPACE)
    using local_uint24_t = WIDE_INTEGER_NAMESPACE::math::wide_integer::uintwide_t<static_cast<WIDE_INTEGER_NAMESPACE::math::wide_integer::size_t>(UINT32_C(24)), std::uint8_t>;
    #else
    using local_uint24_t = ::math::wide_integer::uintwide_t<static_cast<math::wide_integer::size_t>(UINT32_C(24)), std::uint8_t>;
    #endif

    local_uint24_t a = UINT32_C(0x11FF5522); // NOLINT(cppcoreguidelines-avoid-magic-numbers,readability-magic-numbers)
    local_uint24_t b = UINT32_C(0xABCDFFEE); // NOLINT(cppcoreguidelines-avoid-magic-numbers,readability-magic-numbers)

    local_uint24_t c = a * b;

    result_is_ok = ((c == UINT32_C(0x0068039C)) && result_is_ok); // NOLINT(cppcoreguidelines-avoid-magic-numbers,readability-magic-numbers)
  }

  {
    #if defined(WIDE_INTEGER_NAMESPACE)
    using local_uint32_t = WIDE_INTEGER_NAMESPACE::math::wide_integer::uintwide_t<static_cast<WIDE_INTEGER_NAMESPACE::math::wide_integer::size_t>(UINT32_C(32)), std::uint16_t>;
    #else
    using local_uint32_t = ::math::wide_integer::uintwide_t<static_cast<math::wide_integer::size_t>(UINT32_C(32)), std::uint16_t>;
    #endif

    local_uint32_t a = UINT32_C(0x11FF5522); // NOLINT(cppcoreguidelines-avoid-magic-numbers,readability-magic-numbers)
    local_uint32_t b = UINT32_C(0xABCDFFEE); // NOLINT(cppcoreguidelines-avoid-magic-numbers,readability-magic-numbers)

    local_uint32_t c = a * b;

    result_is_ok = ((c == UINT32_C(0xF368039C)) && result_is_ok); // NOLINT(cppcoreguidelines-avoid-magic-numbers,readability-magic-numbers)
  }

  return result_is_ok;
}

auto test_uintwide_t_boost_backend() -> bool
{
  std::cout << "running: test_uintwide_t_boost_backend" << std::endl;
  #if defined(WIDE_INTEGER_NAMESPACE)
  const bool result_test_uintwide_t_boost_backend_is_ok = WIDE_INTEGER_NAMESPACE::math::wide_integer::test_uintwide_t_boost_backend();
  #else
  const bool result_test_uintwide_t_boost_backend_is_ok = ::math::wide_integer::test_uintwide_t_boost_backend();
  #endif
  return result_test_uintwide_t_boost_backend_is_ok;
}

auto test_uintwide_t_examples() -> bool
{
  std::cout << "running: test_uintwide_t_examples" << std::endl;
  #if defined(WIDE_INTEGER_NAMESPACE)
  const bool result_test_uintwide_t_examples_is_ok = WIDE_INTEGER_NAMESPACE::math::wide_integer::test_uintwide_t_examples();
  #else
  const bool result_test_uintwide_t_examples_is_ok = ::math::wide_integer::test_uintwide_t_examples();
  #endif
  return result_test_uintwide_t_examples_is_ok;
}

auto test_uintwide_t_edge_cases() -> bool
{
  std::cout << "running: test_uintwide_t_edge_cases" << std::endl;
  #if defined(WIDE_INTEGER_NAMESPACE)
  const bool result_test_uintwide_t_edge_cases_is_ok = WIDE_INTEGER_NAMESPACE::math::wide_integer::test_uintwide_t_edge_cases();
  #else
  const bool result_test_uintwide_t_edge_cases_is_ok = ::math::wide_integer::test_uintwide_t_edge_cases();
  #endif
  return result_test_uintwide_t_edge_cases_is_ok;
}

auto test_uintwide_t_float_convert() -> bool
{
  std::cout << "running: test_uintwide_t_float_convert" << std::endl;
  #if defined(WIDE_INTEGER_NAMESPACE)
  const bool result_test_uintwide_t_float_convert_is_ok = WIDE_INTEGER_NAMESPACE::math::wide_integer::test_uintwide_t_float_convert();
  #else
  const bool result_test_uintwide_t_float_convert_is_ok = ::math::wide_integer::test_uintwide_t_float_convert();
  #endif
  return result_test_uintwide_t_float_convert_is_ok;
}

auto test_uintwide_t_int_convert() -> bool
{
  std::cout << "running: test_uintwide_t_int_convert" << std::endl;
  #if defined(WIDE_INTEGER_NAMESPACE)
  const bool result_test_uintwide_t_int_convert_is_ok = WIDE_INTEGER_NAMESPACE::math::wide_integer::test_uintwide_t_int_convert();
  #else
  const bool result_test_uintwide_t_int_convert_is_ok = ::math::wide_integer::test_uintwide_t_int_convert();
  #endif
  return result_test_uintwide_t_int_convert_is_ok;
}

auto test_uintwide_t_spot_values() -> bool
{
  std::cout << "running: test_uintwide_t_spot_values" << std::endl;
  #if defined(WIDE_INTEGER_NAMESPACE)
  const bool result_test_uintwide_t_spot_values_is_ok = WIDE_INTEGER_NAMESPACE::math::wide_integer::test_uintwide_t_spot_values();
  #else
  const bool result_test_uintwide_t_spot_values_is_ok = ::math::wide_integer::test_uintwide_t_spot_values();
  #endif
  return result_test_uintwide_t_spot_values_is_ok;
}

auto test_uintwide_t_0000024() -> bool
{
  #if !defined(UINTWIDE_T_REDUCE_TEST_DEPTH)
  constexpr auto count = static_cast<std::size_t>(1UL << 13U);
  #else
  constexpr auto count = static_cast<std::size_t>(1UL << 10U);
  #endif

  std::cout << "running: test_uintwide_t_0000024" << std::endl;
  test_uintwide_t_n_binary_ops_template<24U, std::uint8_t> test_uintwide_t_n_binary_ops_template_instance(count); // NOLINT(cppcoreguidelines-avoid-magic-numbers,readability-magic-numbers)
  const auto result_is_ok =
    test_uintwide_t_n_binary_ops_template_instance.do_test(test_uintwide_t_n_binary_ops_rounds);
  return result_is_ok;
}

auto test_uintwide_t_0000048() -> bool
{
  #if !defined(UINTWIDE_T_REDUCE_TEST_DEPTH)
  constexpr auto count = static_cast<std::size_t>(1UL << 13U);
  #else
  constexpr auto count = static_cast<std::size_t>(1UL << 10U);
  #endif

  std::cout << "running: test_uintwide_t_0000048" << std::endl;
  test_uintwide_t_n_binary_ops_template<48U, std::uint16_t> test_uintwide_t_n_binary_ops_template_instance(count); // NOLINT(cppcoreguidelines-avoid-magic-numbers,readability-magic-numbers)
  const auto result_is_ok =
    test_uintwide_t_n_binary_ops_template_instance.do_test(test_uintwide_t_n_binary_ops_rounds);
  return result_is_ok;
}

auto test_uintwide_t_0000064() -> bool
{
  #if !defined(UINTWIDE_T_REDUCE_TEST_DEPTH)
  constexpr auto count = static_cast<std::size_t>(1UL << 13U);
  #else
  constexpr auto count = static_cast<std::size_t>(1UL << 10U);
  #endif

  std::cout << "running: test_uintwide_t_0000064" << std::endl;
  test_uintwide_t_n_binary_ops_template<64U, std::uint32_t> test_uintwide_t_n_binary_ops_template_instance(count); // NOLINT(cppcoreguidelines-avoid-magic-numbers,readability-magic-numbers)
  const auto result_is_ok =
    test_uintwide_t_n_binary_ops_template_instance.do_test(test_uintwide_t_n_binary_ops_rounds);
  return result_is_ok;
}

auto test_uintwide_t_0000064_signed() -> bool
{
  #if !defined(UINTWIDE_T_REDUCE_TEST_DEPTH)
  constexpr auto count = static_cast<std::size_t>(1UL << 13U);
  #else
  constexpr auto count = static_cast<std::size_t>(1UL << 10U);
  #endif

  std::cout << "running: test_uintwide_t_0000064_signed" << std::endl;
  test_uintwide_t_n_binary_ops_template_signed<64U, std::uint16_t> test_uintwide_t_n_binary_ops_template_instance(count); // NOLINT(cppcoreguidelines-avoid-magic-numbers,readability-magic-numbers)
  const auto result_is_ok =
    test_uintwide_t_n_binary_ops_template_instance.do_test(test_uintwide_t_n_binary_ops_rounds);
  return result_is_ok;
}

auto test_uintwide_t_0000096() -> bool
{
  #if !defined(UINTWIDE_T_REDUCE_TEST_DEPTH)
  constexpr auto count = static_cast<std::size_t>(1UL << 13U);
  #else
  constexpr auto count = static_cast<std::size_t>(1UL << 10U);
  #endif

  std::cout << "running: test_uintwide_t_0000096" << std::endl;
  test_uintwide_t_n_binary_ops_template<96U, std::uint16_t> test_uintwide_t_n_binary_ops_template_instance(count); // NOLINT(cppcoreguidelines-avoid-magic-numbers,readability-magic-numbers)
  const auto result_is_ok =
    test_uintwide_t_n_binary_ops_template_instance.do_test(test_uintwide_t_n_binary_ops_rounds);
  return result_is_ok;
}

auto test_uintwide_t_0000128() -> bool
{
  #if !defined(UINTWIDE_T_REDUCE_TEST_DEPTH)
  constexpr auto count = static_cast<std::size_t>(1UL << 13U);
  #else
  constexpr auto count = static_cast<std::size_t>(1UL << 10U);
  #endif

  std::cout << "running: test_uintwide_t_0000128" << std::endl;
  test_uintwide_t_n_binary_ops_template<128U> test_uintwide_t_n_binary_ops_template_instance(count); // NOLINT(cppcoreguidelines-avoid-magic-numbers,readability-magic-numbers)
  const auto result_is_ok =
    test_uintwide_t_n_binary_ops_template_instance.do_test(test_uintwide_t_n_binary_ops_rounds);
  return result_is_ok;
}

auto test_uintwide_t_0000256() -> bool
{
  #if !defined(UINTWIDE_T_REDUCE_TEST_DEPTH)
  constexpr auto count = static_cast<std::size_t>(1UL << 13U);
  #else
  constexpr auto count = static_cast<std::size_t>(1UL << 10U);
  #endif

  std::cout << "running: test_uintwide_t_0000256" << std::endl;
  test_uintwide_t_n_binary_ops_template<256U> test_uintwide_t_n_binary_ops_template_instance(count); // NOLINT(cppcoreguidelines-avoid-magic-numbers,readability-magic-numbers)
  const auto result_is_ok =
    test_uintwide_t_n_binary_ops_template_instance.do_test(test_uintwide_t_n_binary_ops_rounds);
  return result_is_ok;
}

#if defined(WIDE_INTEGER_HAS_LIMB_TYPE_UINT64)
auto test_uintwide_t_0000256_limb_type_uint64_t() -> bool
{
  #if !defined(UINTWIDE_T_REDUCE_TEST_DEPTH)
  constexpr auto count = static_cast<std::size_t>(1UL << 13U);
  #else
  constexpr auto count = static_cast<std::size_t>(1UL << 10U);
  #endif

  std::cout << "running: test_uintwide_t_0000256_limb_type_uint64_t" << std::endl;
  test_uintwide_t_n_binary_ops_template<256U, std::uint64_t> test_uintwide_t_n_binary_ops_template_instance(count); // NOLINT(cppcoreguidelines-avoid-magic-numbers,readability-magic-numbers)
  const auto result_is_ok =
    test_uintwide_t_n_binary_ops_template_instance.do_test(test_uintwide_t_n_binary_ops_rounds);
  return result_is_ok;
}
#endif

auto test_uintwide_t_0000512() -> bool
{
  #if !defined(UINTWIDE_T_REDUCE_TEST_DEPTH)
  constexpr auto count = static_cast<std::size_t>(1UL << 13U);
  #else
  constexpr auto count = static_cast<std::size_t>(1UL << 10U);
  #endif

  std::cout << "running: test_uintwide_t_0000512" << std::endl;
  test_uintwide_t_n_binary_ops_template<512U> test_uintwide_t_n_binary_ops_template_instance(count); // NOLINT(cppcoreguidelines-avoid-magic-numbers,readability-magic-numbers)
  const auto result_is_ok =
    test_uintwide_t_n_binary_ops_template_instance.do_test(test_uintwide_t_n_binary_ops_rounds);
  return result_is_ok;
}

auto test_uintwide_t_0000512_signed() -> bool
{
  #if !defined(UINTWIDE_T_REDUCE_TEST_DEPTH)
  constexpr auto count = static_cast<std::size_t>(1UL << 13U);
  #else
  constexpr auto count = static_cast<std::size_t>(1UL << 10U);
  #endif

  std::cout << "running: test_uintwide_t_0000512_signed" << std::endl;
  test_uintwide_t_n_binary_ops_template_signed<512U> test_uintwide_t_n_binary_ops_template_signed_instance(count); // NOLINT(cppcoreguidelines-avoid-magic-numbers,readability-magic-numbers)
  const auto result_is_ok =
    test_uintwide_t_n_binary_ops_template_signed_instance.do_test(test_uintwide_t_n_binary_ops_rounds);
  return result_is_ok;
}

auto test_uintwide_t_0001024() -> bool
{
  #if !defined(UINTWIDE_T_REDUCE_TEST_DEPTH)
  constexpr auto count = static_cast<std::size_t>(1UL << 12U);
  #else
  constexpr auto count = static_cast<std::size_t>(1UL << 9U);
  #endif

  std::cout << "running: test_uintwide_t_0001024" << std::endl;
  test_uintwide_t_n_binary_ops_template<1024U> test_uintwide_t_n_binary_ops_template_instance(count); // NOLINT(cppcoreguidelines-avoid-magic-numbers,readability-magic-numbers)
  const auto result_is_ok =
    test_uintwide_t_n_binary_ops_template_instance.do_test(test_uintwide_t_n_binary_ops_rounds);
  return result_is_ok;
}

auto test_uintwide_t_0002048() -> bool
{
  #if !defined(UINTWIDE_T_REDUCE_TEST_DEPTH)
  constexpr auto count = static_cast<std::size_t>(1UL << 11U);
  #else
  constexpr auto count = static_cast<std::size_t>(1UL << 8U);
  #endif

  std::cout << "running: test_uintwide_t_0002048" << std::endl;
  test_uintwide_t_n_binary_ops_template<2048U> test_uintwide_t_n_binary_ops_template_instance(count); // NOLINT(cppcoreguidelines-avoid-magic-numbers,readability-magic-numbers)
  const auto result_is_ok =
    test_uintwide_t_n_binary_ops_template_instance.do_test(test_uintwide_t_n_binary_ops_rounds);
  return result_is_ok;
}

auto test_uintwide_t_0008192() -> bool
{
  #if !defined(UINTWIDE_T_REDUCE_TEST_DEPTH)
  constexpr auto count = static_cast<std::size_t>(1UL << 8U);
  #else
  constexpr auto count = static_cast<std::size_t>(1UL << 5U);
  #endif

  std::cout << "running: test_uintwide_t_0008192" << std::endl;
  test_uintwide_t_n_binary_ops_template<8192U> test_uintwide_t_n_binary_ops_template_instance(count); // NOLINT(cppcoreguidelines-avoid-magic-numbers,readability-magic-numbers)
  const auto result_is_ok =
    test_uintwide_t_n_binary_ops_template_instance.do_test(test_uintwide_t_n_binary_ops_rounds);
  return result_is_ok;
}

#if defined(WIDE_INTEGER_HAS_LIMB_TYPE_UINT64)
auto test_uintwide_t_0008192_limb_type_uint64_t() -> bool
{
  #if !defined(UINTWIDE_T_REDUCE_TEST_DEPTH)
  constexpr auto count = static_cast<std::size_t>(1UL << 8U);
  #else
  constexpr auto count = static_cast<std::size_t>(1UL << 5U);
  #endif

  std::cout << "running: test_uintwide_t_0008192_limb_type_uint64_t" << std::endl;
  test_uintwide_t_n_binary_ops_template<8192U, std::uint64_t> test_uintwide_t_n_binary_ops_template_instance(count); // NOLINT(cppcoreguidelines-avoid-magic-numbers,readability-magic-numbers)
  const auto result_is_ok =
    test_uintwide_t_n_binary_ops_template_instance.do_test(test_uintwide_t_n_binary_ops_rounds);
  return result_is_ok;
}
#endif

auto test_uintwide_t_0012288() -> bool
{
  #if !defined(UINTWIDE_T_REDUCE_TEST_DEPTH)
  constexpr auto count = static_cast<std::size_t>(1UL << 7U);
  #else
  constexpr auto count = static_cast<std::size_t>(1UL << 4U);
  #endif

  std::cout << "running: test_uintwide_t_0012288" << std::endl;
  test_uintwide_t_n_binary_ops_template<12288U> test_uintwide_t_n_binary_ops_template_instance(count); // NOLINT(cppcoreguidelines-avoid-magic-numbers,readability-magic-numbers)
  const auto result_is_ok =
    test_uintwide_t_n_binary_ops_template_instance.do_test(test_uintwide_t_n_binary_ops_rounds);
  return result_is_ok;
}

auto test_uintwide_t_0032768() -> bool
{
  #if !defined(UINTWIDE_T_REDUCE_TEST_DEPTH)
  constexpr auto count = static_cast<std::size_t>(1UL << 7U); // NOLINT(cppcoreguidelines-avoid-magic-numbers,readability-magic-numbers)
  #else
  constexpr auto count = static_cast<std::size_t>(1UL << 4U); // NOLINT(cppcoreguidelines-avoid-magic-numbers,readability-magic-numbers)
  #endif

  std::cout << "running: test_uintwide_t_0032768" << std::endl;
  test_uintwide_t_n_binary_ops_template<32768U> test_uintwide_t_n_binary_ops_template_instance(count); // NOLINT(cppcoreguidelines-avoid-magic-numbers,readability-magic-numbers)
  const auto result_is_ok =
    test_uintwide_t_n_binary_ops_template_instance.do_test(test_uintwide_t_n_binary_ops_rounds);
  return result_is_ok;
}

auto test_uintwide_t_0065536_alloc() -> bool
{
  #if !defined(UINTWIDE_T_REDUCE_TEST_DEPTH)
  constexpr auto count = static_cast<std::size_t>(1UL << 5U); // NOLINT(cppcoreguidelines-avoid-magic-numbers,readability-magic-numbers)
  #else
  constexpr auto count = static_cast<std::size_t>(1UL << 2U); // NOLINT(cppcoreguidelines-avoid-magic-numbers,readability-magic-numbers)
  #endif

  std::cout << "running: test_uintwide_t_0065536_alloc" << std::endl;
  test_uintwide_t_n_binary_ops_template<65536U, std::uint32_t, std::allocator<std::uint32_t>> test_uintwide_t_n_binary_ops_template_instance(count); // NOLINT(cppcoreguidelines-avoid-magic-numbers,readability-magic-numbers)
  const auto result_is_ok =
    test_uintwide_t_n_binary_ops_template_instance.do_test(test_uintwide_t_n_binary_ops_rounds);
  return result_is_ok;
}

auto test_uintwide_t_0008192_by_0012288() -> bool
{
  #if !defined(UINTWIDE_T_REDUCE_TEST_DEPTH)
  constexpr auto count = static_cast<std::size_t>(1UL << 7U); // NOLINT(cppcoreguidelines-avoid-magic-numbers,readability-magic-numbers)
  #else
  constexpr auto count = static_cast<std::size_t>(1UL << 4U); // NOLINT(cppcoreguidelines-avoid-magic-numbers,readability-magic-numbers)
  #endif

  std::cout << "running: test_uintwide_t_0008192_by_0012288" << std::endl;
  test_uintwide_t_n_binary_ops_mul_n_by_m_template<8192U, 12288U> test_uintwide_t_n_binary_ops_template_instance(count); // NOLINT(cppcoreguidelines-avoid-magic-numbers,readability-magic-numbers)
  const auto result_is_ok =
    test_uintwide_t_n_binary_ops_template_instance.do_test(test_uintwide_t_n_binary_ops_rounds);
  return result_is_ok;
}

auto test_uintwide_t_0012288_by_0008192() -> bool
{
  #if !defined(UINTWIDE_T_REDUCE_TEST_DEPTH)
  constexpr auto count = static_cast<std::size_t>(1UL << 7U); // NOLINT(cppcoreguidelines-avoid-magic-numbers,readability-magic-numbers)
  #else
  constexpr auto count = static_cast<std::size_t>(1UL << 4U); // NOLINT(cppcoreguidelines-avoid-magic-numbers,readability-magic-numbers)
  #endif

  std::cout << "running: test_uintwide_t_0012288_by_0008192" << std::endl;
  test_uintwide_t_n_binary_ops_mul_n_by_m_template<12288U, 8192U> test_uintwide_t_n_binary_ops_template_instance(count); // NOLINT(cppcoreguidelines-avoid-magic-numbers,readability-magic-numbers)
  const auto result_is_ok =
    test_uintwide_t_n_binary_ops_template_instance.do_test(test_uintwide_t_n_binary_ops_rounds);
  return result_is_ok;
}

auto test_uintwide_t_0000032_by_0000032_4_by_4() -> bool
{
  std::cout << "running: test_uintwide_t_0000032_by_0000032_4_by_4" << std::endl;
  test_uintwide_t_n_binary_ops_mul_div_4_by_4_template<32U, std::uint8_t> test_uintwide_t_n_binary_ops_template_instance(test_uintwide_t_n_binary_ops_4_by_4_cases); // NOLINT(cppcoreguidelines-avoid-magic-numbers,readability-magic-numbers)
  const auto result_is_ok =
    test_uintwide_t_n_binary_ops_template_instance.do_test(test_uintwide_t_n_binary_ops_rounds);
  return result_is_ok;
}

auto test_uintwide_t_0000064_by_0000064_4_by_4() -> bool
{
  std::cout << "running: test_uintwide_t_0000064_by_0000064_4_by_4" << std::endl;
  test_uintwide_t_n_binary_ops_mul_div_4_by_4_template<64U, std::uint16_t> test_uintwide_t_n_binary_ops_template_instance(test_uintwide_t_n_binary_ops_4_by_4_cases); // NOLINT(cppcoreguidelines-avoid-magic-numbers,readability-magic-numbers)
  const auto result_is_ok =
    test_uintwide_t_n_binary_ops_template_instance.do_test(test_uintwide_t_n_binary_ops_rounds);
  return result_is_ok;
}

auto run() -> bool // NOLINT(readability-function-cognitive-complexity)
{
  #if (BOOST_VERSION < 107900)
  using boost_wrapexcept_lexical_type = ::boost::wrapexcept<::boost::bad_lexical_cast>;
  using boost_wrapexcept_runtime_type = ::boost::wrapexcept<std::runtime_error>;
  #endif

  const auto start = clock_type::now();

  bool result_is_ok = true;

  #if ((BOOST_VERSION < 107900) || ((BOOST_VERSION >= 108000) && !defined(BOOST_NO_EXCEPTIONS)))
  try
  {
  #endif

  result_is_ok = (test_uintwide_t_small_bits()                 && result_is_ok);   std::cout << "result_is_ok: " << std::boolalpha << result_is_ok << std::endl;
  result_is_ok = (test_uintwide_t_boost_backend()              && result_is_ok);   std::cout << "result_is_ok: " << std::boolalpha << result_is_ok << std::endl;
  result_is_ok = (test_uintwide_t_examples()                   && result_is_ok);   std::cout << "result_is_ok: " << std::boolalpha << result_is_ok << std::endl;
  result_is_ok = (test_uintwide_t_edge_cases()                 && result_is_ok);   std::cout << "result_is_ok: " << std::boolalpha << result_is_ok << std::endl;
  result_is_ok = (test_uintwide_t_float_convert()              && result_is_ok);   std::cout << "result_is_ok: " << std::boolalpha << result_is_ok << std::endl;
  result_is_ok = (test_uintwide_t_int_convert()                && result_is_ok);   std::cout << "result_is_ok: " << std::boolalpha << result_is_ok << std::endl;
  result_is_ok = (test_uintwide_t_spot_values()                && result_is_ok);   std::cout << "result_is_ok: " << std::boolalpha << result_is_ok << std::endl;
  result_is_ok = (test_uintwide_t_0000024()                    && result_is_ok);   std::cout << "result_is_ok: " << std::boolalpha << result_is_ok << std::endl;
  result_is_ok = (test_uintwide_t_0000048()                    && result_is_ok);   std::cout << "result_is_ok: " << std::boolalpha << result_is_ok << std::endl;
  result_is_ok = (test_uintwide_t_0000064()                    && result_is_ok);   std::cout << "result_is_ok: " << std::boolalpha << result_is_ok << std::endl;
  result_is_ok = (test_uintwide_t_0000064_signed()             && result_is_ok);   std::cout << "result_is_ok: " << std::boolalpha << result_is_ok << std::endl;
  result_is_ok = (test_uintwide_t_0000096()                    && result_is_ok);   std::cout << "result_is_ok: " << std::boolalpha << result_is_ok << std::endl;
  result_is_ok = (test_uintwide_t_0000128()                    && result_is_ok);   std::cout << "result_is_ok: " << std::boolalpha << result_is_ok << std::endl;
  result_is_ok = (test_uintwide_t_0000256()                    && result_is_ok);   std::cout << "result_is_ok: " << std::boolalpha << result_is_ok << std::endl;
  #if defined(WIDE_INTEGER_HAS_LIMB_TYPE_UINT64)
  result_is_ok = (test_uintwide_t_0000256_limb_type_uint64_t() && result_is_ok);   std::cout << "result_is_ok: " << std::boolalpha << result_is_ok << std::endl;
  #endif
  result_is_ok = (test_uintwide_t_0000512()                    && result_is_ok);   std::cout << "result_is_ok: " << std::boolalpha << result_is_ok << std::endl;
  result_is_ok = (test_uintwide_t_0000512_signed()             && result_is_ok);   std::cout << "result_is_ok: " << std::boolalpha << result_is_ok << std::endl;
  result_is_ok = (test_uintwide_t_0001024()                    && result_is_ok);   std::cout << "result_is_ok: " << std::boolalpha << result_is_ok << std::endl;
  result_is_ok = (test_uintwide_t_0002048()                    && result_is_ok);   std::cout << "result_is_ok: " << std::boolalpha << result_is_ok << std::endl;
  result_is_ok = (test_uintwide_t_0008192()                    && result_is_ok);   std::cout << "result_is_ok: " << std::boolalpha << result_is_ok << std::endl;
  #if defined(WIDE_INTEGER_HAS_LIMB_TYPE_UINT64)
  result_is_ok = (test_uintwide_t_0008192_limb_type_uint64_t() && result_is_ok);   std::cout << "result_is_ok: " << std::boolalpha << result_is_ok << std::endl;
  #endif
  result_is_ok = (test_uintwide_t_0012288()                    && result_is_ok);   std::cout << "result_is_ok: " << std::boolalpha << result_is_ok << std::endl;
  result_is_ok = (test_uintwide_t_0032768()                    && result_is_ok);   std::cout << "result_is_ok: " << std::boolalpha << result_is_ok << std::endl;
  result_is_ok = (test_uintwide_t_0065536_alloc()              && result_is_ok);   std::cout << "result_is_ok: " << std::boolalpha << result_is_ok << std::endl;
  result_is_ok = (test_uintwide_t_0008192_by_0012288()         && result_is_ok);   std::cout << "result_is_ok: " << std::boolalpha << result_is_ok << std::endl;
  result_is_ok = (test_uintwide_t_0012288_by_0008192()         && result_is_ok);   std::cout << "result_is_ok: " << std::boolalpha << result_is_ok << std::endl;
  result_is_ok = (test_uintwide_t_0000032_by_0000032_4_by_4()  && result_is_ok);   std::cout << "result_is_ok: " << std::boolalpha << result_is_ok << std::endl;
  result_is_ok = (test_uintwide_t_0000064_by_0000064_4_by_4()  && result_is_ok);   std::cout << "result_is_ok: " << std::boolalpha << result_is_ok << std::endl;

  #if ((BOOST_VERSION < 107900) || ((BOOST_VERSION >= 108000) && !defined(BOOST_NO_EXCEPTIONS)))
  }
  catch(boost_wrapexcept_lexical_type& e)
  {
    result_is_ok = false;

    std::cout << "Exception: boost_wrapexcept_lexical_type: " << e.what() << std::endl;
  }
  catch(boost_wrapexcept_runtime_type& e)
  {
    result_is_ok = false;

    std::cout << "Exception: boost_wrapexcept_runtime_type: " << e.what() << std::endl;
  }
  #endif

  const auto stop = clock_type::now();

  {
    constexpr auto one_thousand_milliseconds = 1000.0F;

    const auto flg = std::cout.flags();

    std::cout << "result_is_ok: "
              << std::boolalpha
              << result_is_ok
              << ", time: "
              << std::fixed
              << std::setprecision(1)
              << (static_cast<float>(std::chrono::duration_cast<std::chrono::milliseconds>(stop - start).count())) / one_thousand_milliseconds
              << "s"
              << std::endl;

    std::cout.flags(flg);
  }

  return result_is_ok;
}

} // namespace local

auto main() -> int // NOLINT(bugprone-exception-escape)
{
  const auto result_is_ok = local::run();

  const auto result_of_main = (result_is_ok ? 0 : -1);

  std::cout << "result_of_main: " << result_of_main << std::endl;

  return result_of_main;
}

#if (defined(__GNUC__) && !defined(__clang__) && (__GNUC__ >= 12))
#pragma GCC diagnostic pop
#endif

#if (BOOST_VERSION < 107900)
#if defined(__GNUC__)
#pragma GCC diagnostic pop
#pragma GCC diagnostic pop
#endif
#endif

#if (((BOOST_VERSION == 108000) || (BOOST_VERSION == 108100)) && defined(BOOST_NO_EXCEPTIONS))
#if defined(__clang__)
#pragma GCC diagnostic pop
#endif
#if defined(_MSC_VER)
#pragma warning(pop)
#endif
#endif
