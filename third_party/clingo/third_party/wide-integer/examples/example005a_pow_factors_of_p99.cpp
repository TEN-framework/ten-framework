///////////////////////////////////////////////////////////////////
//  Copyright Christopher Kormanyos 2018 - 2022.                 //
//  Distributed under the Boost Software License,                //
//  Version 1.0. (See accompanying file LICENSE_1_0.txt          //
//  or copy at http://www.boost.org/LICENSE_1_0.txt)             //
///////////////////////////////////////////////////////////////////

#include <examples/example_uintwide_t.h>
#include <math/wide_integer/uintwide_t.h>

#if defined(WIDE_INTEGER_NAMESPACE)
auto WIDE_INTEGER_NAMESPACE::math::wide_integer::example005a_pow_factors_of_p99() -> bool
#else
auto ::math::wide_integer::example005a_pow_factors_of_p99() -> bool
#endif
{
  #if defined(WIDE_INTEGER_NAMESPACE)
  using uint384_t = WIDE_INTEGER_NAMESPACE::math::wide_integer::uintwide_t<static_cast<WIDE_INTEGER_NAMESPACE::math::wide_integer::size_t>(UINT32_C(384))>;
  #else
  using uint384_t = ::math::wide_integer::uintwide_t<static_cast<math::wide_integer::size_t>(UINT32_C(384))>;
  #endif

  const uint384_t c = (pow(uint384_t(10U), 99) - 1) / 9;

  // Consider Table 9, page 410 of the classic work:
  // H. Riesel, "Prime Numbers and Computer Methods of Factorization",
  // Second Edition (Birkhaeuser, 1994). In that table, we find the
  // prime factorization of P99 = (10^99 - 1) / 9. This example
  // verifies the tabulated result.

  // FactorInteger[(10^33 - 1)/9]
  const uint384_t control_p33
  {
      uint384_t(3U)
    * uint384_t(37U)
    * uint384_t(67U)
    * uint384_t(21649U)
    * uint384_t(513239U)
    * uint384_t("1344628210313298373")
  };

  // FactorInteger[(10^99 - 1)/9]
  const uint384_t control_p99
  {
      control_p33
    * uint384_t(3U)
    * uint384_t(199U)
    * uint384_t(397U)
    * uint384_t(34849U)
    * uint384_t(333667U)
    * uint384_t("362853724342990469324766235474268869786311886053883")
  };

  const auto result_is_ok = (c == control_p99);

  return result_is_ok;
}

// Enable this if you would like to activate this main() as a standalone example.
#if defined(WIDE_INTEGER_STANDALONE_EXAMPLE005A_POW_FACTORS_OF_P99)

#include <iomanip>
#include <iostream>

auto main() -> int
{
  #if defined(WIDE_INTEGER_NAMESPACE)
  const auto result_is_ok = WIDE_INTEGER_NAMESPACE::math::wide_integer::example005a_pow_factors_of_p99();
  #else
  const auto result_is_ok = ::math::wide_integer::example005a_pow_factors_of_p99();
  #endif

  std::cout << "result_is_ok: " << std::boolalpha << result_is_ok << std::endl;

  return (result_is_ok ? 0 : -1);
}

#endif
