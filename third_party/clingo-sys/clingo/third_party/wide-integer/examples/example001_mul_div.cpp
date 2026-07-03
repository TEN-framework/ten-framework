///////////////////////////////////////////////////////////////////
//  Copyright Christopher Kormanyos 2018 - 2024.                 //
//  Distributed under the Boost Software License,                //
//  Version 1.0. (See accompanying file LICENSE_1_0.txt          //
//  or copy at http://www.boost.org/LICENSE_1_0.txt)             //
///////////////////////////////////////////////////////////////////

#include <examples/example_uintwide_t.h>
#include <math/wide_integer/uintwide_t.h>

#if (defined(__GNUC__) && defined(__AVR__) && (__GNUC__ < 10))
#define WIDE_INTEGER_EXAMPLE001_CONSTEXPR_OR_CONST const             // NOLINT(cppcoreguidelines-macro-usage)
#define WIDE_INTEGER_EXAMPLE001_CONSTEXPR_IS_COMPILE_TIME_CONSTANT 0 // NOLINT(cppcoreguidelines-macro-usage)
#else
#define WIDE_INTEGER_EXAMPLE001_CONSTEXPR_OR_CONST constexpr         // NOLINT(cppcoreguidelines-macro-usage)
#define WIDE_INTEGER_EXAMPLE001_CONSTEXPR_IS_COMPILE_TIME_CONSTANT 1 // NOLINT(cppcoreguidelines-macro-usage)
#endif

#if defined(WIDE_INTEGER_NAMESPACE)
auto WIDE_INTEGER_NAMESPACE::math::wide_integer::example001_mul_div() -> bool
#else
auto ::math::wide_integer::example001_mul_div() -> bool
#endif
{
  #if defined(WIDE_INTEGER_NAMESPACE)
  using WIDE_INTEGER_NAMESPACE::math::wide_integer::uint256_t;
  using WIDE_INTEGER_NAMESPACE::math::wide_integer::uint512_t;
  #else
  using ::math::wide_integer::uint256_t;
  using ::math::wide_integer::uint512_t;
  #endif

  static_assert((   std::numeric_limits<uint256_t>::digits == static_cast<int>(INT16_C(256))
                 && std::numeric_limits<uint512_t>::digits == static_cast<int>(INT16_C(512))),
                "Error: Incorrect digit count for this example");

  WIDE_INTEGER_EXAMPLE001_CONSTEXPR_OR_CONST uint256_t a("0xF4DF741DE58BCB2F37F18372026EF9CBCFC456CB80AF54D53BDEED78410065DE");
  WIDE_INTEGER_EXAMPLE001_CONSTEXPR_OR_CONST uint256_t b("0x166D63E0202B3D90ECCEAA046341AB504658F55B974A7FD63733ECF89DD0DF75");

  WIDE_INTEGER_EXAMPLE001_CONSTEXPR_OR_CONST auto c = uint512_t(a) * uint512_t(b);
  WIDE_INTEGER_EXAMPLE001_CONSTEXPR_OR_CONST auto d = (a / b);

  WIDE_INTEGER_EXAMPLE001_CONSTEXPR_OR_CONST auto result_is_ok =
  (
        (c == "0x1573D6A7CEA734D99865C4F428184983CDB018B80E9CC44B83C773FBE11993E7E491A360C57EB4306C61F9A04F7F7D99BE3676AAD2D71C5592D5AE70F84AF076")
     && (static_cast<std::uint_fast8_t>(d) == static_cast<std::uint_fast8_t>(UINT8_C(10)))
  );

  #if (defined(WIDE_INTEGER_EXAMPLE001_CONSTEXPR_IS_COMPILE_TIME_CONSTANT) \
           && (WIDE_INTEGER_EXAMPLE001_CONSTEXPR_IS_COMPILE_TIME_CONSTANT == 1))
  static_assert(result_is_ok, "Error: example001_mul_div not OK!");
  #endif

  return result_is_ok;
}

// Enable this if you would like to activate this main() as a standalone example.
#if defined(WIDE_INTEGER_STANDALONE_EXAMPLE001_MUL_DIV)

#if !defined(WIDE_INTEGER_DISABLE_IOSTREAM)
#include <iomanip>
#include <iostream>
#endif

constexpr auto example_standalone_foodcafe = static_cast<std::uint32_t>(UINT32_C(0xF00DCAFE));

extern "C"
{
  extern volatile std::uint32_t example_standalone_result;

  auto example_run_standalone       () -> bool;
  auto example_get_standalone_result() noexcept -> bool;

  auto example_run_standalone() -> bool
  {
    bool result_is_ok = true;

    for(unsigned i = 0U; i < 64U; ++i)
    {
      #if defined(WIDE_INTEGER_NAMESPACE)
      result_is_ok &= WIDE_INTEGER_NAMESPACE::math::wide_integer::example001_mul_div();
      #else
      result_is_ok &= ::math::wide_integer::example001_mul_div();
      #endif
    }

    example_standalone_result =
      static_cast<std::uint32_t>
      (
        result_is_ok ? example_standalone_foodcafe : static_cast<std::uint32_t>(UINT32_C(0xFFFFFFFF))
      );

    #if !defined(WIDE_INTEGER_DISABLE_IOSTREAM)
    std::cout << "result_is_ok: " << std::boolalpha << result_is_ok << std::endl;
    #endif

    return result_is_ok;
  }

  auto example_get_standalone_result() noexcept -> bool
  {
    return { example_standalone_result == static_cast<std::uint32_t>(UINT32_C(0xF00DCAFE)) };
  }
}

auto main() -> int
{
  auto result_is_ok = true;

  result_is_ok = (::example_run_standalone       () && result_is_ok);
  result_is_ok = (::example_get_standalone_result() && result_is_ok);

  return (result_is_ok ? 0 : -1);
}

extern "C"
{
  volatile std::uint32_t example_standalone_result { };
}

#endif
