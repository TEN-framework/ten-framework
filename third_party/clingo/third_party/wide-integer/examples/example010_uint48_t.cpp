///////////////////////////////////////////////////////////////////
//  Copyright Christopher Kormanyos 2018 - 2022.                 //
//  Distributed under the Boost Software License,                //
//  Version 1.0. (See accompanying file LICENSE_1_0.txt          //
//  or copy at http://www.boost.org/LICENSE_1_0.txt)             //
///////////////////////////////////////////////////////////////////

#include <random>

#include <examples/example_uintwide_t.h>
#include <math/wide_integer/uintwide_t.h>

#if defined(WIDE_INTEGER_NAMESPACE)
auto WIDE_INTEGER_NAMESPACE::math::wide_integer::example010_uint48_t() -> bool
#else
auto ::math::wide_integer::example010_uint48_t() -> bool
#endif
{
  #if defined(WIDE_INTEGER_NAMESPACE)
  using uint48_t = WIDE_INTEGER_NAMESPACE::math::wide_integer::uintwide_t<static_cast<WIDE_INTEGER_NAMESPACE::math::wide_integer::size_t>(UINT32_C(48)), std::uint8_t>;
  #else
  using uint48_t = ::math::wide_integer::uintwide_t<static_cast<math::wide_integer::size_t>(UINT32_C(48)), std::uint8_t>;
  #endif

  #if defined(WIDE_INTEGER_NAMESPACE)
  using distribution_type  = WIDE_INTEGER_NAMESPACE::math::wide_integer::uniform_int_distribution<static_cast<WIDE_INTEGER_NAMESPACE::math::wide_integer::size_t>(UINT32_C(48)), typename uint48_t::limb_type>;
  #else
  using distribution_type  = ::math::wide_integer::uniform_int_distribution<static_cast<math::wide_integer::size_t>(UINT32_C(48)), typename uint48_t::limb_type>;
  #endif

  using random_engine_type = std::linear_congruential_engine<std::uint32_t, UINT32_C(48271), UINT32_C(0), UINT32_C(2147483647)>;

  random_engine_type generator(static_cast<std::uint32_t>(UINT32_C(0xF00DCAFE))); // NOLINT(cert-msc32-c,cert-msc51-cpp,cppcoreguidelines-avoid-magic-numbers,readability-magic-numbers)

  distribution_type distribution;

  const auto a64 = static_cast<std::uint64_t>(distribution(generator));
  const auto b64 = static_cast<std::uint64_t>(distribution(generator));

  const uint48_t a(a64);
  const uint48_t b(b64);

  const uint48_t c_add = (a + b);
  const uint48_t c_sub = (a - b);
  const uint48_t c_mul = (a * b);
  const uint48_t c_div = (a / b);

  const auto result_is_ok = (   (   (c_add == static_cast<std::uint64_t>((a64 + b64) & static_cast<std::uint64_t>(UINT64_C(0x0000FFFFFFFFFFFF))))
                                 && (c_sub == static_cast<std::uint64_t>((a64 - b64) & static_cast<std::uint64_t>(UINT64_C(0x0000FFFFFFFFFFFF))))
                                 && (c_mul == static_cast<std::uint64_t>((a64 * b64) & static_cast<std::uint64_t>(UINT64_C(0x0000FFFFFFFFFFFF))))
                                 && (c_div == static_cast<std::uint64_t>((a64 / b64) & static_cast<std::uint64_t>(UINT64_C(0x0000FFFFFFFFFFFF)))))
                             &&
                                (   (static_cast<std::uint64_t>(c_add) == static_cast<std::uint64_t>((a64 + b64) & static_cast<std::uint64_t>(UINT64_C(0x0000FFFFFFFFFFFF))))
                                 && (static_cast<std::uint64_t>(c_sub) == static_cast<std::uint64_t>((a64 - b64) & static_cast<std::uint64_t>(UINT64_C(0x0000FFFFFFFFFFFF))))
                                 && (static_cast<std::uint64_t>(c_mul) == static_cast<std::uint64_t>((a64 * b64) & static_cast<std::uint64_t>(UINT64_C(0x0000FFFFFFFFFFFF))))
                                 && (static_cast<std::uint64_t>(c_div) == static_cast<std::uint64_t>((a64 / b64) & static_cast<std::uint64_t>(UINT64_C(0x0000FFFFFFFFFFFF))))));

  return result_is_ok;
}

// Enable this if you would like to activate this main() as a standalone example.
#if defined(WIDE_INTEGER_STANDALONE_EXAMPLE010_UINT48_T)

#include <iomanip>
#include <iostream>

auto main() -> int
{
  #if defined(WIDE_INTEGER_NAMESPACE)
  const auto result_is_ok = WIDE_INTEGER_NAMESPACE::math::wide_integer::example010_uint48_t();
  #else
  const auto result_is_ok = ::math::wide_integer::example010_uint48_t();
  #endif

  std::cout << "result_is_ok: " << std::boolalpha << result_is_ok << std::endl;

  return (result_is_ok ? 0 : -1);
}

#endif
