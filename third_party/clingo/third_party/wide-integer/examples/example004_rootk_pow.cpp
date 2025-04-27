///////////////////////////////////////////////////////////////////
//  Copyright Christopher Kormanyos 2018 - 2022.                 //
//  Distributed under the Boost Software License,                //
//  Version 1.0. (See accompanying file LICENSE_1_0.txt          //
//  or copy at http://www.boost.org/LICENSE_1_0.txt)             //
///////////////////////////////////////////////////////////////////

#include <examples/example_uintwide_t.h>
#include <math/wide_integer/uintwide_t.h>

#if defined(WIDE_INTEGER_NAMESPACE)
auto WIDE_INTEGER_NAMESPACE::math::wide_integer::example004_rootk_pow() -> bool
#else
auto ::math::wide_integer::example004_rootk_pow() -> bool
#endif
{
  bool result_is_ok = true;

  {
    #if defined(WIDE_INTEGER_NAMESPACE)
    using WIDE_INTEGER_NAMESPACE::math::wide_integer::uint256_t;
    #else
    using ::math::wide_integer::uint256_t;
    #endif

    WIDE_INTEGER_CONSTEXPR uint256_t x("0x95E0E51079E1D11737D3FD01429AA745582FEB4381D61FA56948C1A949E43C32");
    WIDE_INTEGER_CONSTEXPR uint256_t r = rootk(x, 7U); // NOLINT(cppcoreguidelines-avoid-magic-numbers,readability-magic-numbers)

    WIDE_INTEGER_CONSTEXPR bool result_is_ok_root = (r == UINT64_C(0x16067D1894));

    result_is_ok = (result_is_ok_root && result_is_ok);

    #if defined(WIDE_INTEGER_CONSTEXPR_IS_COMPILE_TIME_CONST) && (WIDE_INTEGER_CONSTEXPR_IS_COMPILE_TIME_CONST != 0)
    static_assert(result_is_ok_root, "Error: example004_rootk_pow not OK!");
    #endif
  }

  {
    #if defined(WIDE_INTEGER_NAMESPACE)
    using WIDE_INTEGER_NAMESPACE::math::wide_integer::uint256_t;
    #else
    using ::math::wide_integer::uint256_t;
    #endif

    WIDE_INTEGER_CONSTEXPR uint256_t r { static_cast<std::uint64_t>(UINT64_C(0x16067D1894)) };
    WIDE_INTEGER_CONSTEXPR uint256_t p = pow(r, static_cast<unsigned>(UINT8_C(7)));

    WIDE_INTEGER_CONSTEXPR bool result_is_ok_pow = (p == "0x95E0E5104B2F636571834936C982E40EFA25682E7370CD1C248051E1CDC34000");

    result_is_ok = (result_is_ok_pow && result_is_ok);

    #if defined(WIDE_INTEGER_CONSTEXPR_IS_COMPILE_TIME_CONST) && (WIDE_INTEGER_CONSTEXPR_IS_COMPILE_TIME_CONST != 0)
    static_assert(result_is_ok_pow, "Error: example004_rootk_pow not OK!");
    #endif
  }

  {
    #if defined(WIDE_INTEGER_NAMESPACE)
    using WIDE_INTEGER_NAMESPACE::math::wide_integer::int256_t;
    #else
    using ::math::wide_integer::int256_t;
    #endif

    WIDE_INTEGER_CONSTEXPR int256_t x("-17791969125525294590007745776736486317864490689865550963808715359713140948018");
    WIDE_INTEGER_CONSTEXPR int256_t r = cbrt(x);

    WIDE_INTEGER_CONSTEXPR bool result_is_ok_root = (r == int256_t("-26106060416733621800766427"));

    result_is_ok = (result_is_ok_root && result_is_ok);

    #if defined(WIDE_INTEGER_CONSTEXPR_IS_COMPILE_TIME_CONST) && (WIDE_INTEGER_CONSTEXPR_IS_COMPILE_TIME_CONST != 0)
    static_assert(result_is_ok_root, "Error: example004_rootk_pow not OK!");
    #endif
  }

  {
    #if defined(WIDE_INTEGER_NAMESPACE)
    using WIDE_INTEGER_NAMESPACE::math::wide_integer::int256_t;
    #else
    using ::math::wide_integer::int256_t;
    #endif

    WIDE_INTEGER_CONSTEXPR int256_t x("-17791969125525294590007745776736486317864490689865550963808715359713140948018");
    WIDE_INTEGER_CONSTEXPR int256_t r = rootk(x, 3);

    WIDE_INTEGER_CONSTEXPR bool result_is_ok_root = (r == int256_t("-26106060416733621800766427"));

    result_is_ok = (result_is_ok_root && result_is_ok);

    #if defined(WIDE_INTEGER_CONSTEXPR_IS_COMPILE_TIME_CONST) && (WIDE_INTEGER_CONSTEXPR_IS_COMPILE_TIME_CONST != 0)
    static_assert(result_is_ok_root, "Error: example004_rootk_pow not OK!");
    #endif
  }

  return result_is_ok;
}

// Enable this if you would like to activate this main() as a standalone example.
#if defined(WIDE_INTEGER_STANDALONE_EXAMPLE004_ROOTK_POW)

#include <iomanip>
#include <iostream>

auto main() -> int
{
  #if defined(WIDE_INTEGER_NAMESPACE)
  const auto result_is_ok = WIDE_INTEGER_NAMESPACE::math::wide_integer::example004_rootk_pow();
  #else
  const auto result_is_ok = ::math::wide_integer::example004_rootk_pow();
  #endif

  std::cout << "result_is_ok: " << std::boolalpha << result_is_ok << std::endl;

  return (result_is_ok ? 0 : -1);
}

#endif
