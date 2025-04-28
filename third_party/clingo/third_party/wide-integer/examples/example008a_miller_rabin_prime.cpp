///////////////////////////////////////////////////////////////////
//  Copyright Christopher Kormanyos 2018 - 2022.                 //
//  Distributed under the Boost Software License,                //
//  Version 1.0. (See accompanying file LICENSE_1_0.txt          //
//  or copy at http://www.boost.org/LICENSE_1_0.txt)             //
///////////////////////////////////////////////////////////////////

// This Miller-Rabin primality test is loosely based on
// an adaptation of some code from Boost.Multiprecision.
// The Boost.Multiprecision code can be found here:
// https://www.boost.org/doc/libs/1_78_0/libs/multiprecision/doc/html/boost_multiprecision/tut/primetest.html

#include <ctime>
#include <random>
#include <sstream>
#include <string>

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
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Warray-bounds"
#endif

#if (BOOST_VERSION < 108000)
#if (defined(__clang__) && (__clang_major__ > 9)) && !defined(__APPLE__)
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wdeprecated-copy"
#endif
#endif

#include <boost/multiprecision/miller_rabin.hpp>
#include <boost/multiprecision/uintwide_t_backend.hpp>

#include <examples/example_uintwide_t.h>

namespace local_miller_rabin {

template<typename UnsignedIntegralType>
auto lexical_cast(const UnsignedIntegralType& u) -> std::string
{
  std::stringstream ss;

  ss << u;

  return ss.str();
}

} // namespace local_miller_rabin

#if defined(WIDE_INTEGER_NAMESPACE)
auto WIDE_INTEGER_NAMESPACE::math::wide_integer::example008a_miller_rabin_prime() -> bool
#else
auto ::math::wide_integer::example008a_miller_rabin_prime() -> bool
#endif
{
  #if defined(WIDE_INTEGER_NAMESPACE)
  using boost_wide_integer_type =
    boost::multiprecision::number<boost::multiprecision::uintwide_t_backend<static_cast<WIDE_INTEGER_NAMESPACE::math::wide_integer::size_t>(UINT32_C(512))>,
                                  boost::multiprecision::et_off>;
  #else
  using boost_wide_integer_type =
    boost::multiprecision::number<boost::multiprecision::uintwide_t_backend<static_cast<math::wide_integer::size_t>(UINT32_C(512))>,
                                  boost::multiprecision::et_off>;
  #endif

  // This example uses wide_integer's uniform_int_distribution to select
  // prime candidates. These prime candidates are subsequently converted
  // (via string-streaming) to Boost.Multiprecision integers.

  #if defined(WIDE_INTEGER_NAMESPACE)
  using local_wide_integer_type = WIDE_INTEGER_NAMESPACE::math::wide_integer::uintwide_t              <static_cast<WIDE_INTEGER_NAMESPACE::math::wide_integer::size_t>(std::numeric_limits<boost_wide_integer_type>::digits)>;
  using local_distribution_type = WIDE_INTEGER_NAMESPACE::math::wide_integer::uniform_int_distribution<static_cast<WIDE_INTEGER_NAMESPACE::math::wide_integer::size_t>(std::numeric_limits<boost_wide_integer_type>::digits)>;
  #else
  using local_wide_integer_type = ::math::wide_integer::uintwide_t              <static_cast<math::wide_integer::size_t>(std::numeric_limits<boost_wide_integer_type>::digits)>;
  using local_distribution_type = ::math::wide_integer::uniform_int_distribution<static_cast<math::wide_integer::size_t>(std::numeric_limits<boost_wide_integer_type>::digits)>;
  #endif

  using random_engine1_type = std::mt19937;
  using random_engine2_type = std::linear_congruential_engine<std::uint32_t, UINT32_C(48271), UINT32_C(0), UINT32_C(2147483647)>; // NOLINT(cppcoreguidelines-avoid-magic-numbers,readability-magic-numbers)

  const auto seed_start = std::clock();

  random_engine1_type gen1(static_cast<typename random_engine1_type::result_type>(seed_start));
  random_engine2_type gen2(static_cast<typename random_engine2_type::result_type>(seed_start));

  // Select prime candidates from a range of 10^150 ... max(uint512_t)-1.
  WIDE_INTEGER_CONSTEXPR local_wide_integer_type
    dist_min
    (
      "1"
      "00000000000000000000000000000000000000000000000000"
      "00000000000000000000000000000000000000000000000000"
      "00000000000000000000000000000000000000000000000000"
    );

  WIDE_INTEGER_CONSTEXPR auto dist_max =
    local_wide_integer_type
    {
        (std::numeric_limits<local_wide_integer_type>::max)()
      - static_cast<int>(INT8_C(1))
    };

  local_distribution_type
    dist
    {
      dist_min,
      dist_max
    };

  boost_wide_integer_type p0;
  boost_wide_integer_type p1;

  auto dist_func =
    [&dist, &gen1]() // NOLINT(modernize-use-trailing-return-type)
    {
      const auto n = dist(gen1);

      return boost_wide_integer_type(local_miller_rabin::lexical_cast(n));
    };

  for(;;)
  {
    p0 = dist_func();

    const auto p0_is_probably_prime = boost::multiprecision::miller_rabin_test(p0, 25U, gen2);

    if(p0_is_probably_prime)
    {
      break;
    }
  }

  const auto seed_next = std::clock();

  gen1.seed(static_cast<typename random_engine1_type::result_type>(seed_next));

  for(;;)
  {
    p1 = dist_func();

    const auto p1_is_probably_prime = boost::multiprecision::miller_rabin_test(p1, 25U, gen2);

    if(p1_is_probably_prime)
    {
      break;
    }
  }

  const boost_wide_integer_type gd = gcd(p0, p1);

  const auto result_is_ok = (   (p0  > boost_wide_integer_type(local_miller_rabin::lexical_cast(dist_min)))
                             && (p1  > boost_wide_integer_type(local_miller_rabin::lexical_cast(dist_min)))
                             && (p0 != 0U)
                             && (p1 != 0U)
                             && (p0 != p1)
                             && (gd == 1U));

  return result_is_ok;
}

// Enable this if you would like to activate this main() as a standalone example.
#if defined(WIDE_INTEGER_STANDALONE_EXAMPLE008A_MILLER_RABIN_PRIME)

#include <iomanip>
#include <iostream>

auto main() -> int // NOLINT(bugprone-exception-escape)
{
  #if defined(WIDE_INTEGER_NAMESPACE)
  const auto result_is_ok = WIDE_INTEGER_NAMESPACE::math::wide_integer::example008a_miller_rabin_prime();
  #else
  const auto result_is_ok = ::math::wide_integer::example008a_miller_rabin_prime();
  #endif

  std::cout << "result_is_ok: " << std::boolalpha << result_is_ok << std::endl;

  return (result_is_ok ? 0 : -1);
}

#endif

#if (BOOST_VERSION < 108000)
#if (defined(__clang__) && (__clang_major__ > 9)) && !defined(__APPLE__)
#pragma GCC diagnostic pop
#endif
#endif

#if (defined(__GNUC__) && !defined(__clang__) && (__GNUC__ >= 12))
#pragma GCC diagnostic pop
#pragma GCC diagnostic pop
#endif

#if (BOOST_VERSION < 108000)
#if defined(__GNUC__)
#pragma GCC diagnostic pop
#pragma GCC diagnostic pop
#pragma GCC diagnostic pop
#endif
#endif
