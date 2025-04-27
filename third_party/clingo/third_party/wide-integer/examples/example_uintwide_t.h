///////////////////////////////////////////////////////////////////
//  Copyright Christopher Kormanyos 2021 - 2023.                 //
//  Distributed under the Boost Software License,                //
//  Version 1.0. (See accompanying file LICENSE_1_0.txt          //
//  or copy at http://www.boost.org/LICENSE_1_0.txt)             //
///////////////////////////////////////////////////////////////////

#ifndef EXAMPLE_UINTWIDE_T_2021_04_29_H // NOLINT(llvm-header-guard)
  #define EXAMPLE_UINTWIDE_T_2021_04_29_H

  // This file contains function prototypes of the uintwide_t examples.

  #include <math/wide_integer/uintwide_t.h>

  WIDE_INTEGER_NAMESPACE_BEGIN

  #if(__cplusplus >= 201703L)
  namespace math::wide_integer {
  #else
  namespace math { namespace wide_integer { // NOLINT(modernize-concat-nested-namespaces)
  #endif

  auto example000_numeric_limits     () -> bool;
  auto example000a_builtin_convert   () -> bool;
  auto example001_mul_div            () -> bool;
  auto example001a_div_mod           () -> bool;
  auto example002_shl_shr            () -> bool;
  auto example003_sqrt               () -> bool;
  auto example003a_cbrt              () -> bool;
  auto example004_rootk_pow          () -> bool;
  auto example005_powm               () -> bool;
  auto example005a_pow_factors_of_p99() -> bool;
  auto example006_gcd                () -> bool;
  auto example007_random_generator   () -> bool;
  auto example008_miller_rabin_prime () -> bool;
  auto example008a_miller_rabin_prime() -> bool;
  auto example009_timed_mul          () -> bool;
  auto example009a_timed_mul_4_by_4  () -> bool;
  auto example009b_timed_mul_8_by_8  () -> bool;
  auto example010_uint48_t           () -> bool;
  auto example011_uint24_t           () -> bool;
  auto example012_rsa_crypto         () -> bool;
  auto example013_ecdsa_sign_verify  () -> bool;
  auto example014_pi_spigot_wide     () -> bool;

  #if(__cplusplus >= 201703L)
  } // namespace math::wide_integer
  #else
  } // namespace wide_integer
  } // namespace math
  #endif

  WIDE_INTEGER_NAMESPACE_END

#endif // EXAMPLE_UINTWIDE_T_2021_04_29_H
