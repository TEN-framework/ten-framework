///////////////////////////////////////////////////////////////////
//  Copyright Christopher Kormanyos 2018 - 2024.                 //
//  Distributed under the Boost Software License,                //
//  Version 1.0. (See accompanying file LICENSE_1_0.txt          //
//  or copy at http://www.boost.org/LICENSE_1_0.txt)             //
///////////////////////////////////////////////////////////////////

#include <examples/example_uintwide_t.h>
#include <math/wide_integer/uintwide_t.h>
#include <test/stopwatch.h>

#include <util/utility/util_pseudorandom_time_point_seed.h>

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <iomanip>
#include <iostream>
#include <limits>
#include <random>
#include <vector>

namespace local_timed_mul_4_by_4
{
  template<typename UnsignedIntegralIteratorType,
           typename RandomEngineType>
  auto get_random_big_uint(RandomEngineType& rng, UnsignedIntegralIteratorType it_out) -> void
  {
    using local_uint_type = typename std::iterator_traits<UnsignedIntegralIteratorType>::value_type;

    #if defined(WIDE_INTEGER_NAMESPACE)
    using distribution_type =
      WIDE_INTEGER_NAMESPACE::math::wide_integer::uniform_int_distribution<std::numeric_limits<local_uint_type>::digits, typename local_uint_type::limb_type>;
    #else
    using distribution_type =
      ::math::wide_integer::uniform_int_distribution<std::numeric_limits<local_uint_type>::digits, typename local_uint_type::limb_type>;
    #endif

    distribution_type distribution;

    *it_out = distribution(rng);
  }

  #if defined(WIDE_INTEGER_NAMESPACE)
  using big_uint_type = WIDE_INTEGER_NAMESPACE::math::wide_integer::uintwide_t<static_cast<WIDE_INTEGER_NAMESPACE::math::wide_integer::size_t>(UINT32_C(128))>;
  #else
  using big_uint_type = ::math::wide_integer::uintwide_t<static_cast<math::wide_integer::size_t>(UINT32_C(128))>;
  #endif

  auto local_a() -> std::vector<big_uint_type>&
  {
    static std::vector<big_uint_type>
      my_local_a
      (
        static_cast<typename std::vector<big_uint_type>::size_type>(UINT32_C(1024))
      );

    return my_local_a;
  }

  auto local_b() -> std::vector<big_uint_type>&
  {
    static std::vector<big_uint_type> my_local_b(local_a().size());

    return my_local_b;
  }
} // namespace local_timed_mul_4_by_4

#if defined(WIDE_INTEGER_NAMESPACE)
auto WIDE_INTEGER_NAMESPACE::math::wide_integer::example009a_timed_mul_4_by_4() -> bool
#else
auto ::math::wide_integer::example009a_timed_mul_4_by_4() -> bool
#endif
{
  using random_engine_type =
    std::linear_congruential_engine<std::uint32_t, UINT32_C(48271), UINT32_C(0), UINT32_C(2147483647)>;

  random_engine_type rng; // NOLINT(cert-msc32-c,cert-msc51-cpp)

  rng.seed(::util::util_pseudorandom_time_point_seed::value<typename random_engine_type::result_type>());

  for(auto i = static_cast<typename std::vector<local_timed_mul_4_by_4::big_uint_type>::size_type>(0U); i < local_timed_mul_4_by_4::local_a().size(); ++i)
  {
    local_timed_mul_4_by_4::get_random_big_uint(rng, local_timed_mul_4_by_4::local_a().begin() + static_cast<typename std::vector<local_timed_mul_4_by_4::big_uint_type>::difference_type>(i));
    local_timed_mul_4_by_4::get_random_big_uint(rng, local_timed_mul_4_by_4::local_b().begin() + static_cast<typename std::vector<local_timed_mul_4_by_4::big_uint_type>::difference_type>(i));
  }

  std::uint64_t count = 0U;
  std::size_t   index = 0U;

  using stopwatch_type = concurrency::stopwatch;

  stopwatch_type my_stopwatch { };

  while(stopwatch_type::elapsed_time<float>(my_stopwatch) < static_cast<float>(6.0L)) // NOLINT(cppcoreguidelines-avoid-magic-numbers,readability-magic-numbers)
  {
    local_timed_mul_4_by_4::local_a().at(index + 0U) * local_timed_mul_4_by_4::local_b().at(index + 0U);
    local_timed_mul_4_by_4::local_a().at(index + 1U) * local_timed_mul_4_by_4::local_b().at(index + 1U);
    local_timed_mul_4_by_4::local_a().at(index + 2U) * local_timed_mul_4_by_4::local_b().at(index + 2U);
    local_timed_mul_4_by_4::local_a().at(index + 3U) * local_timed_mul_4_by_4::local_b().at(index + 3U);

    count += 4U;
    index += 4U;

    if(index >= local_timed_mul_4_by_4::local_a().size())
    {
      index = 0U;
    }
  }

  const float kops_per_sec = static_cast<float>(count) / static_cast<float>(static_cast<float>(stopwatch_type::elapsed_time<float>(my_stopwatch) * 1000.0F));

  {
    const auto flg = std::cout.flags();

    std::cout << "bits: "
              << std::numeric_limits<local_timed_mul_4_by_4::big_uint_type>::digits
              << ", kops_per_sec: "
              << std::fixed
              << std::setprecision(2)
              << kops_per_sec
              << ", count: "
              << count
              << std::endl;

    std::cout.flags(flg);
  }

  const auto result_is_ok = (kops_per_sec > (std::numeric_limits<float>::min)());

  return result_is_ok;
}

// Enable this if you would like to activate this main() as a standalone example.
#if defined(WIDE_INTEGER_STANDALONE_EXAMPLE009A_TIMED_MUL_4_BY_4)

auto main() -> int
{
  #if defined(WIDE_INTEGER_NAMESPACE)
  const auto result_is_ok = WIDE_INTEGER_NAMESPACE::math::wide_integer::example009a_timed_mul_4_by_4();
  #else
  const auto result_is_ok = ::math::wide_integer::example009a_timed_mul_4_by_4();
  #endif

  std::cout << "result_is_ok: " << std::boolalpha << result_is_ok << std::endl;

  return (result_is_ok ? 0 : -1);
}

#endif
