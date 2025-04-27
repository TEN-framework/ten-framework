///////////////////////////////////////////////////////////////////
//  Copyright Christopher Kormanyos 2019 - 2023.                 //
//  Distributed under the Boost Software License,                //
//  Version 1.0. (See accompanying file LICENSE_1_0.txt          //
//  or copy at http://www.boost.org/LICENSE_1_0.txt)             //
///////////////////////////////////////////////////////////////////

#ifndef TEST_UINTWIDE_T_2019_12_15_H // NOLINT(llvm-header-guard)
  #define TEST_UINTWIDE_T_2019_12_15_H

  #include <math/wide_integer/uintwide_t.h>

  WIDE_INTEGER_NAMESPACE_BEGIN

  #if(__cplusplus >= 201703L)
  namespace math::wide_integer {
  #else
  namespace math { namespace wide_integer { // NOLINT(modernize-concat-nested-namespaces)
  #endif

  auto test_uintwide_t_boost_backend() -> bool;
  auto test_uintwide_t_examples     () -> bool;
  auto test_uintwide_t_edge_cases   () -> bool;
  auto test_uintwide_t_float_convert() -> bool;
  auto test_uintwide_t_int_convert  () -> bool;
  auto test_uintwide_t_spot_values  () -> bool;

  #if(__cplusplus >= 201703L)
  } // namespace math::wide_integer
  #else
  } // namespace wide_integer
  } // namespace math
  #endif

  WIDE_INTEGER_NAMESPACE_END

#endif // TEST_UINTWIDE_T_2019_12_15_H
