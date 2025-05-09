///////////////////////////////////////////////////////////////////////////////
//  Copyright Christopher Kormanyos 2024.
//  Distributed under the Boost Software License,
//  Version 1.0. (See accompanying file LICENSE_1_0.txt
//  or copy at http://www.boost.org/LICENSE_1_0.txt)
//

// cd /mnt/c/Users/ckorm/Documents/Ks/PC_Software/NumericalPrograms/ExtendedNumberTypes/wide_integer
// clang++ -std=c++20 -g -O2 -Wall -Wextra -fsanitize=fuzzer -I. -I/mnt/c/boost/boost_1_85_0 test/fuzzing/test_fuzzing_sdiv.cpp -o test_fuzzing_sdiv
// ./test_fuzzing_sdiv -max_total_time=300

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
  auto pseudo_random_sign_bit() -> int
  {
    static unsigned seed_prescaler { };

    static std::mt19937 engine { };

    if((seed_prescaler++ % 0x10000U) == 0U)
    {
      std::random_device rd { };

      engine.seed(rd());
    }

    // Create a uniform distribution for the bit position (0 to 1).
    static std::uniform_int_distribution<int> bit_dist(0, 1);

    // Generate a pseudo-random sign bit.
    return bit_dist(engine);
  }

  using boost_uint_backend_type =
    boost::multiprecision::cpp_int_backend<static_cast<unsigned>(UINT32_C(256)),
                                           static_cast<unsigned>(UINT32_C(256)),
                                           boost::multiprecision::unsigned_magnitude>;

  using boost_uint_type = boost::multiprecision::number<boost_uint_backend_type,
                                                        boost::multiprecision::et_off>;

  using local_uint_type = ::math::wide_integer::uint256_t;

  using boost_sint_backend_type =
    boost::multiprecision::cpp_int_backend<static_cast<unsigned>(UINT32_C(256)),
                                           static_cast<unsigned>(UINT32_C(256)),
                                           boost::multiprecision::signed_magnitude>;

  using boost_sint_type = boost::multiprecision::number<boost_sint_backend_type,
                                                        boost::multiprecision::et_off>;

  using local_sint_type = ::math::wide_integer::int256_t;

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

  if((size > std::size_t { UINT8_C(1) }) && (size <= std::size_t { max_size * 2U }))
  {
    local_uint_type a_local { 0U };
    local_uint_type b_local { 0U };

    // Import data into the uintwide_t values.
    import_bits
    (
      a_local,
      data,
      data + std::size_t { size / 2U },
      8U
    );

    import_bits
    (
      b_local,
      data + std::size_t { size / 2U },
      data + size,
      8U
    );

    if(a_local + 256U < b_local)
    {
      std::swap(a_local, b_local);
    }

    if(b_local != 0U)
    {
      local_sint_type a_signed_local { a_local };
      local_sint_type b_signed_local { b_local };

      const int
        sign_mixer
        {
          (pseudo_random_sign_bit() << 1U) | pseudo_random_sign_bit()
        };

      if     (sign_mixer == 0) { }
      else if(sign_mixer == 1) { a_signed_local = -a_signed_local; }
      else if(sign_mixer == 2) { b_signed_local = -b_signed_local; }
      else                     { a_signed_local = -a_signed_local; b_signed_local = -b_signed_local; }

      std::stringstream strm_a_local { };
      std::stringstream strm_b_local { };

      strm_a_local << a_signed_local;
      strm_b_local << b_signed_local;

      boost_sint_type a_signed_boost { strm_a_local.str() };
      boost_sint_type b_signed_boost { strm_b_local.str() };

      local_sint_type result_signed_local { a_signed_local / b_signed_local };
      boost_sint_type result_signed_boost { a_signed_boost / b_signed_boost };

      std::vector<std::uint8_t> result_signed_data_local(max_size, UINT8_C(0));
      std::vector<std::uint8_t> result_signed_data_boost(result_signed_data_local.size(), UINT8_C(0));

      export_bits(result_signed_local, result_signed_data_local.data(), 8U);
      export_bits(result_signed_boost, result_signed_data_boost.data(), 8U);

      // Verify that both uintwide_t as well as boost obtain the same result.
      const bool result_op_is_ok =
        std::equal
        (
          result_signed_data_local.cbegin(),
          result_signed_data_local.cend(),
          result_signed_data_boost.cbegin(),
          result_signed_data_boost.cend()
        );

      result_is_ok = (result_op_is_ok && result_is_ok);
    }
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
