///////////////////////////////////////////////////////////////////
//  Copyright Christopher Kormanyos 2018 - 2022.                 //
//  Distributed under the Boost Software License,                //
//  Version 1.0. (See accompanying file LICENSE_1_0.txt          //
//  or copy at http://www.boost.org/LICENSE_1_0.txt)             //
///////////////////////////////////////////////////////////////////

#include <string>

#include <examples/example_uintwide_t.h>
#include <math/wide_integer/uintwide_t.h>

#if defined(WIDE_INTEGER_NAMESPACE)
auto WIDE_INTEGER_NAMESPACE::math::wide_integer::example003a_cbrt() -> bool
#else
auto ::math::wide_integer::example003a_cbrt() -> bool
#endif
{
  #if defined(WIDE_INTEGER_NAMESPACE)
  using uint11264_t = WIDE_INTEGER_NAMESPACE::math::wide_integer::uintwide_t<static_cast<WIDE_INTEGER_NAMESPACE::math::wide_integer::size_t>(UINT32_C(11264)), std::uint32_t, std::allocator<std::uint32_t>>;
  #else
  using uint11264_t = ::math::wide_integer::uintwide_t<static_cast<math::wide_integer::size_t>(UINT32_C(11264)), std::uint32_t, std::allocator<std::uint32_t>>;
  #endif

  // Create the string '1' + 3,333 times '0', which is
  // equivalent to the decimal integral value 10^3333.

  const std::string str_a = "1" + std::string(3333U, '0');

  const uint11264_t a = str_a.data();

  const uint11264_t s = cbrt(a);

  // Create the string '1' + 1,111 times '0', which is
  // equivalent to the decimal integral value 10^1111.
  // (This is the cube root of 10^3333.)

  const std::string str_control = "1" + std::string(1111U, '0');

  const auto result_is_ok = (s == uint11264_t(str_control.data()));

  return result_is_ok;
}

// Enable this if you would like to activate this main() as a standalone example.
#if defined(WIDE_INTEGER_STANDALONE_EXAMPLE003A_CBRT)

#include <iomanip>
#include <iostream>

auto main() -> int
{
  #if defined(WIDE_INTEGER_NAMESPACE)
  const auto result_is_ok = WIDE_INTEGER_NAMESPACE::math::wide_integer::example003a_cbrt();
  #else
  const auto result_is_ok = ::math::wide_integer::example003a_cbrt();
  #endif

  std::cout << "result_is_ok: " << std::boolalpha << result_is_ok << std::endl;

  return (result_is_ok ? 0 : -1);
}

#endif
