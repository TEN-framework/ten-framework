///////////////////////////////////////////////////////////////
//  Copyright 2022 Christopher Kormanyos.
//  Distributed under the Boost
//  Software License, Version 1.0. (See accompanying file
//  LICENSE_1_0.txt or copy at https://www.boost.org/LICENSE_1_0.txt

#include <boost/version.hpp>

#if !defined(BOOST_VERSION)
#error BOOST_VERSION is not defined. Ensure that <boost/version.hpp> is properly included.
#endif

#if ((BOOST_VERSION >= 107900) && !defined(BOOST_MP_STANDALONE))
#define BOOST_MP_STANDALONE
#endif

#if ((BOOST_VERSION >= 108000) && !defined(BOOST_NO_EXCEPTIONS))
#define BOOST_NO_EXCEPTIONS
#endif

#if (BOOST_VERSION < 108000)
#if defined(__GNUC__)
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wconversion"
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wsign-conversion"
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wunused-parameter"
#endif
#endif

#if (defined(__GNUC__) && !defined(__clang__) && (__GNUC__ >= 12))
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wrestrict"
#endif

#if (BOOST_VERSION < 108000)
#if (defined(__clang__) && (__clang_major__ > 9)) && !defined(__APPLE__)
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wdeprecated-copy"
#endif
#endif

#if defined(__GNUC__)
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wshadow"
#endif

#include <boost/multiprecision/cpp_int.hpp>
#include <boost/multiprecision/uintwide_t_backend.hpp>

#include "test_arithmetic.hpp"

#include <test/test_uintwide_t.h>


// cd /mnt/c/Users/User/Documents/Ks/PC_Software/NumericalPrograms/ExtendedNumberTypes/wide_integer
// g++-10 -finline-functions -march=native -mtune=native -O3 -Wall -std=gnu++20 -DWIDE_INTEGER_HAS_LIMB_TYPE_UINT64 -I. -I/mnt/c/boost/boost_1_78_0 test/test_uintwide_t_boost_backend_via_test_arithmetic.cpp -o test_uintwide_t_boost_backend_via_test_arithmetic.exe

auto main() -> int
{
  using local_big_uint_backend_type =
    boost::multiprecision::uintwide_t_backend<static_cast<math::wide_integer::size_t>(UINT32_C(1024)), std::uint8_t, std::allocator<void>>;

  using local_big_uint_type =
    boost::multiprecision::number<local_big_uint_backend_type,
                                  boost::multiprecision::et_off>;

  test<local_big_uint_type>();

  const int n_errors = boost::report_errors();

  return ((n_errors == 0) ? 0 : -1);
}

#if defined(__GNUC__)
#pragma GCC diagnostic pop
#endif

#if (BOOST_VERSION < 108000)
#if (defined(__clang__) && (__clang_major__ > 9)) && !defined(__APPLE__)
#pragma GCC diagnostic pop
#endif
#endif

#if (defined(__GNUC__) && !defined(__clang__) && (__GNUC__ >= 12))
#pragma GCC diagnostic pop
#endif

#if (BOOST_VERSION < 108000)
#if defined(__GNUC__)
#pragma GCC diagnostic pop
#pragma GCC diagnostic pop
#pragma GCC diagnostic pop
#endif
#endif
