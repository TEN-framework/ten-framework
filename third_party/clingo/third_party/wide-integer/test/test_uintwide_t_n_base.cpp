///////////////////////////////////////////////////////////////////////////////
//  Copyright Christopher Kormanyos 2021 - 2022.
//  Distributed under the Boost Software License,
//  Version 1.0. (See accompanying file LICENSE_1_0.txt
//  or copy at http://www.boost.org/LICENSE_1_0.txt)
//

#include <test/test_uintwide_t_n_base.h>

auto test_uintwide_t_n_base::my_random_generator() -> test_uintwide_t_n_base::random_engine_type&
{
  static random_engine_type my_generator; // NOLINT(cert-msc32-c,cert-msc51-cpp)

  return my_generator;
}
