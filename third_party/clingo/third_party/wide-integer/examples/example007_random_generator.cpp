///////////////////////////////////////////////////////////////////
//  Copyright Christopher Kormanyos 2018 - 2022.                 //
//  Distributed under the Boost Software License,                //
//  Version 1.0. (See accompanying file LICENSE_1_0.txt          //
//  or copy at http://www.boost.org/LICENSE_1_0.txt)             //
///////////////////////////////////////////////////////////////////

#include <cstdint>
#include <random>
#include <string>

#include <examples/example_uintwide_t.h>
#include <math/wide_integer/uintwide_t.h>

namespace local_random
{
  template<typename LimbType,
           typename AllocatorType = void>
  auto generate() -> bool
  {
    using random_engine_type = std::mersenne_twister_engine<std::uint64_t,
                                                            static_cast<std::size_t>(UINT32_C( 64)),
                                                            static_cast<std::size_t>(UINT32_C(312)),
                                                            static_cast<std::size_t>(UINT32_C(156)),
                                                            static_cast<std::size_t>(UINT32_C( 31)),
                                                            UINT64_C(0xB5026F5AA96619E9),
                                                            static_cast<std::size_t>(UINT32_C( 29)),
                                                            UINT64_C(0x5555555555555555),
                                                            static_cast<std::size_t>(UINT32_C( 17)),
                                                            UINT64_C(0x71D67FFFEDA60000),
                                                            static_cast<std::size_t>(UINT32_C( 37)),
                                                            UINT64_C(0xFFF7EEE000000000),
                                                            static_cast<std::size_t>(UINT32_C( 43)),
                                                            UINT64_C(6364136223846793005)>;

    #if defined(WIDE_INTEGER_NAMESPACE)
    using wide_integer_type  = WIDE_INTEGER_NAMESPACE::math::wide_integer::uintwide_t<static_cast<WIDE_INTEGER_NAMESPACE::math::wide_integer::size_t>(UINT32_C(256)), std::uint32_t, AllocatorType>;
    using distribution_type  = WIDE_INTEGER_NAMESPACE::math::wide_integer::uniform_int_distribution<wide_integer_type::my_width2, typename wide_integer_type::limb_type>;
    #else
    using wide_integer_type  = ::math::wide_integer::uintwide_t<static_cast<math::wide_integer::size_t>(UINT32_C(256)), std::uint32_t, AllocatorType>;
    using distribution_type  = ::math::wide_integer::uniform_int_distribution<wide_integer_type::my_width2, typename wide_integer_type::limb_type>;
    #endif

    // Generate a random number with wide_integer_type having limbs of type LimbType.
    // Purosely use the default seed.
    random_engine_type generator; // NOLINT(cert-msc32-c,cert-msc51-cpp)

    distribution_type distribution;

    const wide_integer_type n = distribution(generator);

    const auto result_is_ok =
      (n == wide_integer_type("0xF258D22D4DB91392B5EE8CB6ABE457F8401F7AC78BC80F1CC96D191CF6F6AEA6"));

    return result_is_ok;
  }
} // namespace local_random

#if defined(WIDE_INTEGER_NAMESPACE)
auto WIDE_INTEGER_NAMESPACE::math::wide_integer::example007_random_generator() -> bool
#else
auto ::math::wide_integer::example007_random_generator() -> bool
#endif
{
  const bool result_08_is_ok = local_random::generate<std::uint8_t> ();
  const bool result_16_is_ok = local_random::generate<std::uint16_t>();
  const bool result_32_is_ok = local_random::generate<std::uint32_t>();

  const auto result_is_ok = (   result_08_is_ok
                             && result_16_is_ok
                             && result_32_is_ok);

  return result_is_ok;
}

// Enable this if you would like to activate this main() as a standalone example.
#if defined(WIDE_INTEGER_STANDALONE_EXAMPLE007_RANDOM_GENERATOR)

#include <iomanip>
#include <iostream>

auto main() -> int
{
  #if defined(WIDE_INTEGER_NAMESPACE)
  const auto result_is_ok = WIDE_INTEGER_NAMESPACE::math::wide_integer::example007_random_generator();
  #else
  const auto result_is_ok = ::math::wide_integer::example007_random_generator();
  #endif

  std::cout << "result_is_ok: " << std::boolalpha << result_is_ok << std::endl;

  return (result_is_ok ? 0 : -1);
}

#endif
