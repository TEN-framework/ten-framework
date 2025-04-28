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
auto WIDE_INTEGER_NAMESPACE::math::wide_integer::example011_uint24_t() -> bool
#else
auto ::math::wide_integer::example011_uint24_t() -> bool
#endif
{
  #if defined(WIDE_INTEGER_NAMESPACE)
  using uint24_t = WIDE_INTEGER_NAMESPACE::math::wide_integer::uintwide_t<static_cast<WIDE_INTEGER_NAMESPACE::math::wide_integer::size_t>(UINT32_C(24)), std::uint8_t>;
  #else
  using uint24_t = ::math::wide_integer::uintwide_t<static_cast<math::wide_integer::size_t>(UINT32_C(24)), std::uint8_t>;
  #endif

  #if defined(WIDE_INTEGER_NAMESPACE)
  using distribution_type  = WIDE_INTEGER_NAMESPACE::math::wide_integer::uniform_int_distribution<static_cast<WIDE_INTEGER_NAMESPACE::math::wide_integer::size_t>(UINT32_C(24)), typename uint24_t::limb_type>;
  #else
  using distribution_type  = ::math::wide_integer::uniform_int_distribution<static_cast<math::wide_integer::size_t>(UINT32_C(24)), typename uint24_t::limb_type>;
  #endif

  using random_engine_type = std::linear_congruential_engine<std::uint32_t, UINT32_C(48271), UINT32_C(0), UINT32_C(2147483647)>;

  random_engine_type generator(UINT32_C(0xDEADBEEF)); // NOLINT(cert-msc32-c,cert-msc51-cpp)

  distribution_type distribution;

  const auto a32 = static_cast<std::uint32_t>(distribution(generator));
  const auto b32 = static_cast<std::uint32_t>(distribution(generator));

  const uint24_t a(a32);
  const uint24_t b(b32);

  const uint24_t c_add = (a + b);
  const uint24_t c_sub = (a - b);
  const uint24_t c_mul = (a * b);
  const uint24_t c_div = (a / b);

  const auto result_is_ok = (   (   (c_add == static_cast<std::uint32_t>(static_cast<std::uint32_t>(a32 + b32) & UINT32_C(0x00FFFFFF)))
                                 && (c_sub == static_cast<std::uint32_t>(static_cast<std::uint32_t>(a32 - b32) & UINT32_C(0x00FFFFFF)))
                                 && (c_mul == static_cast<std::uint32_t>(static_cast<std::uint32_t>(a32 * b32) & UINT32_C(0x00FFFFFF)))
                                 && (c_div == static_cast<std::uint32_t>(static_cast<std::uint32_t>(a32 / b32) & UINT32_C(0x00FFFFFF))))
                             &&
                                (   (static_cast<std::uint64_t>(c_add) == static_cast<std::uint64_t>(static_cast<std::uint32_t>(a32 + b32) & UINT32_C(0x00FFFFFF)))
                                 && (static_cast<std::uint64_t>(c_sub) == static_cast<std::uint64_t>(static_cast<std::uint32_t>(a32 - b32) & UINT32_C(0x00FFFFFF)))
                                 && (static_cast<std::uint64_t>(c_mul) == static_cast<std::uint64_t>(static_cast<std::uint32_t>(a32 * b32) & UINT32_C(0x00FFFFFF)))
                                 && (static_cast<std::uint64_t>(c_div) == static_cast<std::uint64_t>(static_cast<std::uint32_t>(a32 / b32) & UINT32_C(0x00FFFFFF)))));

  return result_is_ok;
}

// Enable this if you would like to activate this main() as a standalone example.
#if defined(WIDE_INTEGER_STANDALONE_EXAMPLE011_UINT24_T)

#include <iomanip>
#include <iostream>

auto main() -> int
{
  #if defined(WIDE_INTEGER_NAMESPACE)
  const auto result_is_ok = WIDE_INTEGER_NAMESPACE::math::wide_integer::example011_uint24_t();
  #else
  const auto result_is_ok = ::math::wide_integer::example011_uint24_t();
  #endif

  std::cout << "result_is_ok: " << std::boolalpha << result_is_ok << std::endl;

  return (result_is_ok ? 0 : -1);
}

#endif
