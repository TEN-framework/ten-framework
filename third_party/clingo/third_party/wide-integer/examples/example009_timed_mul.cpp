///////////////////////////////////////////////////////////////////
//  Copyright Christopher Kormanyos 2018 - 2022.                 //
//  Distributed under the Boost Software License,                //
//  Version 1.0. (See accompanying file LICENSE_1_0.txt          //
//  or copy at http://www.boost.org/LICENSE_1_0.txt)             //
///////////////////////////////////////////////////////////////////

#include <algorithm>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <iomanip>
#include <iostream>
#include <iterator>
#include <limits>
#include <random>
#include <vector>

#include <examples/example_uintwide_t.h>
#include <math/wide_integer/uintwide_t.h>

namespace local_timed_mul
{
  constexpr std::uint32_t wide_integer_test9_digits2 = static_cast<std::uint32_t>(1ULL << 15U);

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
  using big_uint_type = WIDE_INTEGER_NAMESPACE::math::wide_integer::uintwide_t<wide_integer_test9_digits2>;
  #else
  using big_uint_type = ::math::wide_integer::uintwide_t<wide_integer_test9_digits2>;
  #endif

  auto local_a() -> std::vector<big_uint_type>&
  {
    static std::vector<big_uint_type>
      my_local_a
      (
        static_cast<typename std::vector<big_uint_type>::size_type>(UINT32_C(128))
      );

    return my_local_a;
  }

  auto local_b() -> std::vector<big_uint_type>&
  {
    static std::vector<big_uint_type> my_local_b(local_a().size());

    return my_local_b;
  }
} // namespace local_timed_mul

#if defined(WIDE_INTEGER_NAMESPACE)
auto WIDE_INTEGER_NAMESPACE::math::wide_integer::example009_timed_mul() -> bool
#else
auto ::math::wide_integer::example009_timed_mul() -> bool
#endif
{
  using random_engine_type =
    std::linear_congruential_engine<std::uint32_t, UINT32_C(48271), UINT32_C(0), UINT32_C(2147483647)>;

  random_engine_type rng; // NOLINT(cert-msc32-c,cert-msc51-cpp)

  rng.seed(static_cast<typename random_engine_type::result_type>(std::clock()));

  for(auto i = static_cast<typename std::vector<local_timed_mul::big_uint_type>::size_type>(0U); i < local_timed_mul::local_a().size(); ++i)
  {
    local_timed_mul::get_random_big_uint(rng, local_timed_mul::local_a().begin() + static_cast<typename std::vector<local_timed_mul::big_uint_type>::difference_type>(i));
    local_timed_mul::get_random_big_uint(rng, local_timed_mul::local_b().begin() + static_cast<typename std::vector<local_timed_mul::big_uint_type>::difference_type>(i));
  }

  std::uint64_t count = 0U;
  std::size_t   index = 0U;

  std::intmax_t total_time { };

  const std::chrono::high_resolution_clock::time_point begin = std::chrono::high_resolution_clock::now();

  for(;;)
  {
    local_timed_mul::local_a().at(index) * local_timed_mul::local_b().at(index);

    const std::chrono::high_resolution_clock::time_point end = std::chrono::high_resolution_clock::now();

    total_time = static_cast<std::intmax_t>(std::chrono::duration_cast<std::chrono::milliseconds>(end - begin).count());

    ++count;
    ++index;

    if(index >= local_timed_mul::local_a().size())
    {
      index = 0U;
    }

    if(total_time > INTMAX_C(5999))
    {
      break;
    }
  }

  const float kops_per_sec = static_cast<float>(count) / static_cast<float>(static_cast<std::uint32_t>(total_time));

  {
    const auto flg = std::cout.flags();

    std::cout << "bits: "
              << std::numeric_limits<local_timed_mul::big_uint_type>::digits
              << ", kops_per_sec: "
              << std::fixed
              << std::setprecision(3)
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
#if defined(WIDE_INTEGER_STANDALONE_EXAMPLE009_TIMED_MUL)

auto main() -> int
{
  #if defined(WIDE_INTEGER_NAMESPACE)
  const auto result_is_ok = WIDE_INTEGER_NAMESPACE::math::wide_integer::example009_timed_mul();
  #else
  const auto result_is_ok = ::math::wide_integer::example009_timed_mul();
  #endif

  std::cout << "result_is_ok: " << std::boolalpha << result_is_ok << std::endl;

  return (result_is_ok ? 0 : -1);
}

#endif
