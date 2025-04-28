///////////////////////////////////////////////////////////////////////////////
//  Copyright Christopher Kormanyos 2022.
//  Distributed under the Boost Software License,
//  Version 1.0. (See accompanying file LICENSE_1_0.txt
//  or copy at http://www.boost.org/LICENSE_1_0.txt)
//

#include <iostream>
#include <sstream>
#include <string>

#if !defined(WIDE_INTEGER_DISABLE_PRIVATE_CLASS_DATA_MEMBERS)
#define WIDE_INTEGER_DISABLE_PRIVATE_CLASS_DATA_MEMBERS
#endif

// cd /mnt/c/Users/User/Documents/Ks/PC_Software/NumericalPrograms/ExtendedNumberTypes/wide_integer
// g++-10 -finline-functions -m64 -O3 -Werror -Wall -Wextra -Wconversion -Wsign-conversion -Wshadow -Wundef -Wunused-parameter -Wuninitialized -Wunreachable-code -Winit-self -Wzero-as-null-pointer-constant -std=c++20 -DWIDE_INTEGER_DISABLE_PRIVATE_CLASS_DATA_MEMBERS -DWIDE_INTEGER_NAMESPACE=ckormanyos -I. test/test_uintwide_t_xtra_from_issue_335.cpp -o test_uintwide_t_xtra_from_issue_335.exe

#include <math/wide_integer/uintwide_t.h>

namespace from_issue_335
{
  // See also: https://github.com/ckormanyos/wide-integer/issues/335

  template<typename UInt,
           uint32_t BitCount = static_cast<uint32_t>(8U * sizeof(UInt)),
           UInt     MaxValue = -UInt(1)>
  auto f(uint32_t n) -> std::string
  {
    std::stringstream strm;

    strm << BitCount << "-bit ; MaxValue = " << MaxValue << ", n = " << UInt(n);

    return strm.str();
  }

  auto test_uintwide_t_xtra_from_issue_335() -> bool
  {
    #if defined(WIDE_INTEGER_NAMESPACE)
    using local_wide_integer_type = WIDE_INTEGER_NAMESPACE::math::wide_integer::uint256_t;
    #else
    using local_wide_integer_type = ::math::wide_integer::uint256_t;
    #endif

    // String result:
    //   "256-bit ; MaxValue = 115792089237316195423570985008687907853269984665640564039457584007913129639935, n = 42"
    const auto str_result = f<local_wide_integer_type>(42);

    std::cout << str_result << std::endl;

    const auto result_is_ok =
    (
      str_result == "256-bit ; MaxValue = 115792089237316195423570985008687907853269984665640564039457584007913129639935, n = 42"
    );

    return result_is_ok;
  }
}

auto main() -> int
{
  const auto result_is_ok = from_issue_335::test_uintwide_t_xtra_from_issue_335();

  std::cout << "result_is_ok: " << std::boolalpha << result_is_ok << std::endl;

  return (result_is_ok ? 0 : -1);
}
