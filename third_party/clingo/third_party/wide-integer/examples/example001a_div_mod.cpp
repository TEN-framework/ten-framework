///////////////////////////////////////////////////////////////////
//  Copyright Christopher Kormanyos 2018 -2022.                  //
//  Distributed under the Boost Software License,                //
//  Version 1.0. (See accompanying file LICENSE_1_0.txt          //
//  or copy at http://www.boost.org/LICENSE_1_0.txt)             //
///////////////////////////////////////////////////////////////////

#include <examples/example_uintwide_t.h>
#include <math/wide_integer/uintwide_t.h>

#if defined(WIDE_INTEGER_NAMESPACE)
auto WIDE_INTEGER_NAMESPACE::math::wide_integer::example001a_div_mod() -> bool
#else
auto ::math::wide_integer::example001a_div_mod() -> bool
#endif
{
  #if defined(WIDE_INTEGER_NAMESPACE)
  using WIDE_INTEGER_NAMESPACE::math::wide_integer::uint512_t;
  #else
  using ::math::wide_integer::uint512_t;
  #endif

  // QuotientRemainder[698937339790347543053797400564366118744312537138445607919548628175822115805812983955794321304304417541511379093392776018867245622409026835324102460829431,100041341335406267530943777943625254875702684549707174207105689918734693139781]
  // {6986485091668619828842978360442127600954041171641881730123945989288792389271,100041341335406267530943777943625254875702684549707174207105689918734693139780}

  WIDE_INTEGER_CONSTEXPR uint512_t a("698937339790347543053797400564366118744312537138445607919548628175822115805812983955794321304304417541511379093392776018867245622409026835324102460829431");
  WIDE_INTEGER_CONSTEXPR uint512_t b("100041341335406267530943777943625254875702684549707174207105689918734693139781");

  WIDE_INTEGER_CONSTEXPR uint512_t c = (a / b);
  WIDE_INTEGER_CONSTEXPR uint512_t d = (a % b);

  WIDE_INTEGER_CONSTEXPR bool result_is_ok = (   (c == "6986485091668619828842978360442127600954041171641881730123945989288792389271")
                                              && (d == "100041341335406267530943777943625254875702684549707174207105689918734693139780"));

  #if defined(WIDE_INTEGER_CONSTEXPR_IS_COMPILE_TIME_CONST) && (WIDE_INTEGER_CONSTEXPR_IS_COMPILE_TIME_CONST != 0)
  static_assert(result_is_ok, "Error: example001a_div_mod not OK!");
  #endif

  return result_is_ok;
}

// Enable this if you would like to activate this main() as a standalone example.
#if defined(WIDE_INTEGER_STANDALONE_EXAMPLE001A_DIV_MOD)

#include <iomanip>
#include <iostream>

auto main() -> int
{
  #if defined(WIDE_INTEGER_NAMESPACE)
  const auto result_is_ok = WIDE_INTEGER_NAMESPACE::math::wide_integer::example001a_div_mod();
  #else
  const auto result_is_ok = ::math::wide_integer::example001a_div_mod();
  #endif

  std::cout << "result_is_ok: " << std::boolalpha << result_is_ok << std::endl;

  return (result_is_ok ? 0 : -1);
}

#endif
