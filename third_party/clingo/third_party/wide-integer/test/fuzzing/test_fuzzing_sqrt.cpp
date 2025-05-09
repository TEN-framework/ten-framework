///////////////////////////////////////////////////////////////////////////////
//  Copyright Christopher Kormanyos 2024.
//  Distributed under the Boost Software License,
//  Version 1.0. (See accompanying file LICENSE_1_0.txt
//  or copy at http://www.boost.org/LICENSE_1_0.txt)
//

// cd /mnt/c/Users/ckorm/Documents/Ks/PC_Software/NumericalPrograms/ExtendedNumberTypes/wide_integer
// clang++ -std=c++20 -g -O2 -Wall -Wextra -fsanitize=fuzzer -I. -I/mnt/c/boost/boost_1_85_0 test/fuzzing/test_fuzzing_sqrt.cpp -o test_fuzzing_sqrt
// ./test_fuzzing_sqrt -max_total_time=300

#include <math/wide_integer/uintwide_t.h>

#include <boost/multiprecision/cpp_int.hpp>

#include <algorithm>
#include <cassert>
#include <cstddef>
#include <cstdint>
#include <random>
#include <sstream>
#include <vector>

namespace fuzzing
{
  using boost_uint_backend_type =
    boost::multiprecision::cpp_int_backend<static_cast<unsigned>(UINT32_C(256)),
                                           static_cast<unsigned>(UINT32_C(256)),
                                           boost::multiprecision::unsigned_magnitude>;

  using boost_uint_type = boost::multiprecision::number<boost_uint_backend_type,
                                                        boost::multiprecision::et_off>;

  using local_uint_type = ::math::wide_integer::uint256_t;

  auto eval_op(const std::uint8_t* data, std::size_t size) -> bool;
}

auto fuzzing::eval_op(const std::uint8_t* data, std::size_t size) -> bool
{
  const std::size_t
    max_size
    {
      static_cast<std::size_t>
      (
        std::numeric_limits<fuzzing::local_uint_type>::digits / 8
      )
    };

  bool result_is_ok { true };

  if(size <= max_size)
  {
    local_uint_type a_local { 0U };

    // Import data into the uintwide_t values.
    import_bits
    (
      a_local,
      data,
      data + size,
      8U
    );

    std::stringstream strm_a_local { };

    strm_a_local << a_local;

    boost_uint_type a_boost { strm_a_local.str() };

    local_uint_type result_local { sqrt(a_local) };
    boost_uint_type result_boost { sqrt(a_boost) };

    std::vector<std::uint8_t> result_data_local(max_size, UINT8_C(0));
    std::vector<std::uint8_t> result_data_boost(max_size, UINT8_C(0));

    export_bits(result_local, result_data_local.data(), 8U);
    export_bits(result_boost, result_data_boost.data(), 8U);

    // Verify that both uintwide_t as well as boost obtain the same result.
    const bool result_op_is_ok =
      std::equal
      (
        result_data_local.cbegin(),
        result_data_local.cend(),
        result_data_boost.cbegin(),
        result_data_boost.cend()
      );

    result_is_ok = (result_op_is_ok && result_is_ok);
  }

  // Assert the correct result.
  assert(result_is_ok);

  return result_is_ok;
}

// The fuzzing entry point.
extern "C"
int LLVMFuzzerTestOneInput(const uint8_t* data, size_t size)
{
  const bool result_one_div_is_ok { fuzzing::eval_op(data, size) };

  return (result_one_div_is_ok ? 0 : -1);
}
