///////////////////////////////////////////////////////////////////
//  Copyright Christopher Kormanyos 2018 - 2023.                 //
//  Distributed under the Boost Software License,                //
//  Version 1.0. (See accompanying file LICENSE_1_0.txt          //
//  or copy at http://www.boost.org/LICENSE_1_0.txt)             //
///////////////////////////////////////////////////////////////////

#include <examples/example_uintwide_t.h>
#include <math/wide_integer/uintwide_t.h>

#if defined(WIDE_INTEGER_NAMESPACE)
auto WIDE_INTEGER_NAMESPACE::math::wide_integer::example006_gcd() -> bool
#else
auto ::math::wide_integer::example006_gcd() -> bool
#endif
{
  #if defined(WIDE_INTEGER_NAMESPACE)
  using WIDE_INTEGER_NAMESPACE::math::wide_integer::uint256_t;
  #else
  using ::math::wide_integer::uint256_t;
  #endif

  auto result_is_ok = true;

  {
    WIDE_INTEGER_CONSTEXPR auto a = uint256_t("0x1035452A5197CF882B5B5EB64C8CCFEE4D772F9F7B66A239649A43093464EFF5");
    WIDE_INTEGER_CONSTEXPR auto b = uint256_t("0xB4F6151D727361113083D9A0DEB91B0B62A250F65DA6543823703D0140C873AD");

    WIDE_INTEGER_CONSTEXPR auto c = gcd(a, b);

    WIDE_INTEGER_CONSTEXPR auto result_gcd_is_ok = (static_cast<std::uint32_t>(c) == static_cast<std::uint32_t>(UINT32_C(11759761)));

    #if defined(WIDE_INTEGER_CONSTEXPR_IS_COMPILE_TIME_CONST) && (WIDE_INTEGER_CONSTEXPR_IS_COMPILE_TIME_CONST != 0)
    static_assert(result_gcd_is_ok, "Error: example006_gcd not OK!");
    #endif

    result_is_ok = (result_gcd_is_ok && result_is_ok);
  }

  {
    WIDE_INTEGER_CONSTEXPR auto a = uint256_t("0xD2690CD26CD57A3C443993851A70D3B62F841573668DF7B229508371A0AEDE7F");
    WIDE_INTEGER_CONSTEXPR auto b = uint256_t("0xFE719235CD0B1A314D4CA6940AEDC38BDF8E9484E68CE814EDAA17D87B0B4CC8");

    WIDE_INTEGER_CONSTEXPR auto c = gcd(a, b);

    WIDE_INTEGER_CONSTEXPR auto result_gcd_is_ok = (static_cast<std::uint32_t>(c) == static_cast<std::uint32_t>(UINT32_C(12170749)));

    #if defined(WIDE_INTEGER_CONSTEXPR_IS_COMPILE_TIME_CONST) && (WIDE_INTEGER_CONSTEXPR_IS_COMPILE_TIME_CONST != 0)
    static_assert(result_gcd_is_ok, "Error: example006_gcd not OK!");
    #endif

    result_is_ok = (result_gcd_is_ok && result_is_ok);
  }

  {
    WIDE_INTEGER_CONSTEXPR auto a = uint256_t("0x7495AFF66DCB1085DC4CC294ECCBB1B455F65765DD4E9735564FDD80A05168A");
    WIDE_INTEGER_CONSTEXPR auto b = uint256_t("0x7A0543EF0705942D09962172ED5038814AE6EDF8EED2FC6C52CF317D253BC81F");

    WIDE_INTEGER_CONSTEXPR auto c = gcd(a, b);

    WIDE_INTEGER_CONSTEXPR auto result_gcd_is_ok = (static_cast<std::uint32_t>(c) == static_cast<std::uint32_t>(UINT32_C(13520497)));

    #if defined(WIDE_INTEGER_CONSTEXPR_IS_COMPILE_TIME_CONST) && (WIDE_INTEGER_CONSTEXPR_IS_COMPILE_TIME_CONST != 0)
    static_assert(result_gcd_is_ok, "Error: example006_gcd not OK!");
    #endif

    result_is_ok = (result_gcd_is_ok && result_is_ok);
  }

  return result_is_ok;
}

// Enable this if you would like to activate this main() as a standalone example.
#if defined(WIDE_INTEGER_STANDALONE_EXAMPLE006_GCD)

#include <iomanip>
#include <iostream>

auto main() -> int
{
  #if defined(WIDE_INTEGER_NAMESPACE)
  const auto result_is_ok = WIDE_INTEGER_NAMESPACE::math::wide_integer::example006_gcd();
  #else
  const auto result_is_ok = ::math::wide_integer::example006_gcd();
  #endif

  std::cout << "result_is_ok: " << std::boolalpha << result_is_ok << std::endl;

  return (result_is_ok ? 0 : -1);
}

#endif
