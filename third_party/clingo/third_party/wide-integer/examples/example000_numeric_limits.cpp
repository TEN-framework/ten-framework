///////////////////////////////////////////////////////////////////
//  Copyright Christopher Kormanyos 2021 - 2024.                 //
//  Distributed under the Boost Software License,                //
//  Version 1.0. (See accompanying file LICENSE_1_0.txt          //
//  or copy at http://www.boost.org/LICENSE_1_0.txt)             //
///////////////////////////////////////////////////////////////////

#include <examples/example_uintwide_t.h>
#include <math/wide_integer/uintwide_t.h>

namespace local
{
  #if defined(WIDE_INTEGER_NAMESPACE)
  using WIDE_INTEGER_NAMESPACE::math::wide_integer::uint64_t;
  using WIDE_INTEGER_NAMESPACE::math::wide_integer::int64_t;
  #else
  using ::math::wide_integer::uint64_t;
  using ::math::wide_integer::int64_t;
  #endif

  #if defined(WIDE_INTEGER_NAMESPACE)
  using uint32_t = WIDE_INTEGER_NAMESPACE::math::wide_integer::uintwide_t<static_cast<WIDE_INTEGER_NAMESPACE::math::wide_integer::size_t>(UINT32_C(32)), std::uint8_t, void, false>;
  using  int32_t = WIDE_INTEGER_NAMESPACE::math::wide_integer::uintwide_t<static_cast<WIDE_INTEGER_NAMESPACE::math::wide_integer::size_t>(UINT32_C(32)), std::uint8_t, void, true>;
  #else
  using uint32_t = ::math::wide_integer::uintwide_t<static_cast<math::wide_integer::size_t>(UINT32_C(32)), std::uint8_t, void, false>;
  using  int32_t = ::math::wide_integer::uintwide_t<static_cast<math::wide_integer::size_t>(UINT32_C(32)), std::uint8_t, void, true>;
  #endif
} // namespace local

#if defined(WIDE_INTEGER_NAMESPACE)
auto WIDE_INTEGER_NAMESPACE::math::wide_integer::example000_numeric_limits() -> bool
#else
auto ::math::wide_integer::example000_numeric_limits() -> bool
#endif
{
  bool result_is_ok = true;

  {
    #if defined(WIDE_INTEGER_NAMESPACE)
    using WIDE_INTEGER_NAMESPACE::math::wide_integer::uint256_t;
    #else
    using ::math::wide_integer::uint256_t;
    #endif

    constexpr uint256_t my_max("0xFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFF");
    constexpr uint256_t my_min(0U);

    constexpr bool result_uint256_t_is_ok =
         ((std::numeric_limits<uint256_t>::max)  () == my_max)
      && ((std::numeric_limits<uint256_t>::min)  () == my_min)
      && ( std::numeric_limits<uint256_t>::lowest() == uint256_t(std::numeric_limits<unsigned>::lowest()))
      && ( std::numeric_limits<uint256_t>::digits   == static_cast<int>(INT32_C(256)))
      && ( std::numeric_limits<uint256_t>::digits10 == static_cast<int>(INT32_C(77)))
      ;

    static_assert(result_uint256_t_is_ok, "Error: example000_numeric_limits unsigned not OK!");

    result_is_ok = (result_uint256_t_is_ok && result_is_ok);
  }

  {
    #if defined(WIDE_INTEGER_NAMESPACE)
    using WIDE_INTEGER_NAMESPACE::math::wide_integer::int256_t;
    #else
    using ::math::wide_integer::int256_t;
    #endif

    constexpr int256_t my_max   ("0x7FFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFF");
    constexpr int256_t my_min   ("0x8000000000000000000000000000000000000000000000000000000000000000");

    #if (defined(_MSC_VER) && (_MSC_VER < 1920))
    #pragma warning(push)
    #pragma warning(disable : 4307)
    #endif
    constexpr int256_t my_min2  ("-57896044618658097711785492504343953926634992332820282019728792003956564819968");
    #if (defined(_MSC_VER) && (_MSC_VER < 1920))
    #pragma warning(pop)
    #endif

    constexpr int256_t my_lowest = my_min2;

    static_assert((std::numeric_limits<int256_t>::max)  () == my_max, "Error: example000_numeric_limits signed not OK!");
    static_assert((std::numeric_limits<int256_t>::min)  () == my_min, "Error: example000_numeric_limits signed not OK!");
    static_assert((std::numeric_limits<int256_t>::min)  () == my_min2, "Error: example000_numeric_limits signed not OK!");
    static_assert( std::numeric_limits<int256_t>::lowest() == my_lowest, "Error: example000_numeric_limits signed not OK!");
    static_assert( std::numeric_limits<int256_t>::digits   == static_cast<int>(INT32_C(255)), "Error: example000_numeric_limits signed not OK!");
    static_assert( std::numeric_limits<int256_t>::digits10 == static_cast<int>(INT32_C(76)), "Error: example000_numeric_limits signed not OK!");

    constexpr bool result_int256_t_is_ok =
         ((std::numeric_limits<int256_t>::max)  () == my_max)
      && ((std::numeric_limits<int256_t>::min)  () == my_min)
      && ((std::numeric_limits<int256_t>::min)  () == my_min2)
      && ( std::numeric_limits<int256_t>::lowest() == my_lowest)
      && ( std::numeric_limits<int256_t>::digits   == static_cast<int>(INT32_C(255)))
      && ( std::numeric_limits<int256_t>::digits10 == static_cast<int>(INT32_C(76)))
      ;

    static_assert(result_int256_t_is_ok, "Error: example000_numeric_limits signed not OK!");

    result_is_ok = (result_int256_t_is_ok && result_is_ok);
  }

  {
    constexpr bool result_uint64_t_is_ok =
         ((std::numeric_limits<local::uint64_t>::max)  () ==       (std::numeric_limits<std::uint64_t>::max)  ())
      && ((std::numeric_limits<local::uint64_t>::min)  () ==       (std::numeric_limits<std::uint64_t>::min)  ())
      && ( std::numeric_limits<local::uint64_t>::lowest() ==        std::numeric_limits<std::uint64_t>::lowest())
      && ( std::numeric_limits<local::uint64_t>::lowest() ==  -1 - (std::numeric_limits<local::uint64_t>::max)())
      && ( std::numeric_limits<local::uint64_t>::digits   ==        std::numeric_limits<std::uint64_t>::digits  )
      && ( std::numeric_limits<local::uint64_t>::digits10 ==        std::numeric_limits<std::uint64_t>::digits10)
      ;

    static_assert(result_uint64_t_is_ok, "Error: example000_numeric_limits unsigned not OK!");

    result_is_ok = (result_uint64_t_is_ok && result_is_ok);
  }

  {
    constexpr bool result_int64_t_is_ok =
         ((std::numeric_limits<local::int64_t>::max)  () ==       (std::numeric_limits<std::int64_t>::max)  ())
      && ((std::numeric_limits<local::int64_t>::min)  () ==       (std::numeric_limits<std::int64_t>::min)  ())
      && ( std::numeric_limits<local::int64_t>::lowest() ==        std::numeric_limits<std::int64_t>::lowest())
      && ( std::numeric_limits<local::int64_t>::lowest() ==  -1 - (std::numeric_limits<local::int64_t>::max)())
      && ( std::numeric_limits<local::int64_t>::digits   ==        std::numeric_limits<std::int64_t>::digits  )
      && ( std::numeric_limits<local::int64_t>::digits10 ==        std::numeric_limits<std::int64_t>::digits10)
      ;

    static_assert(result_int64_t_is_ok, "Error: example000_numeric_limits unsigned not OK!");

    result_is_ok = (result_int64_t_is_ok && result_is_ok);
  }

  {
    constexpr bool result_uint32_t_is_ok =
         ((std::numeric_limits<local::uint32_t>::max)  () ==       (std::numeric_limits<std::uint32_t>::max)  ())
      && ((std::numeric_limits<local::uint32_t>::min)  () ==       (std::numeric_limits<std::uint32_t>::min)  ())
      && ( std::numeric_limits<local::uint32_t>::lowest() ==        std::numeric_limits<std::uint32_t>::lowest())
      && ( std::numeric_limits<local::uint32_t>::lowest() ==  -1 - (std::numeric_limits<local::uint32_t>::max)())
      && ( std::numeric_limits<local::uint32_t>::digits   ==        std::numeric_limits<std::uint32_t>::digits  )
      && ( std::numeric_limits<local::uint32_t>::digits10 ==        std::numeric_limits<std::uint32_t>::digits10)
      ;

    static_assert(result_uint32_t_is_ok, "Error: example000_numeric_limits unsigned not OK!");

    result_is_ok = (result_uint32_t_is_ok && result_is_ok);
  }

  {
    constexpr bool result_int32_t_is_ok =
         ((std::numeric_limits<local::int32_t>::max)  () ==       (std::numeric_limits<std::int32_t>::max)  ())
      && ((std::numeric_limits<local::int32_t>::min)  () ==       (std::numeric_limits<std::int32_t>::min)  ())
      && ( std::numeric_limits<local::int32_t>::lowest() ==        std::numeric_limits<std::int32_t>::lowest())
      && ( std::numeric_limits<local::int32_t>::lowest() ==  -1 - (std::numeric_limits<local::int32_t>::max)())
      && ( std::numeric_limits<local::int32_t>::digits   ==        std::numeric_limits<std::int32_t>::digits  )
      && ( std::numeric_limits<local::int32_t>::digits10 ==        std::numeric_limits<std::int32_t>::digits10)
      ;

    static_assert(result_int32_t_is_ok, "Error: example000_numeric_limits unsigned not OK!");

    result_is_ok = (result_int32_t_is_ok && result_is_ok);
  }

  return result_is_ok;
}

// Enable this if you would like to activate this main() as a standalone example.
#if defined(WIDE_INTEGER_STANDALONE_EXAMPLE000_NUMERIC_LIMITS)

#include <iomanip>
#include <iostream>

auto main() -> int
{
  #if defined(WIDE_INTEGER_NAMESPACE)
  const auto result_is_ok = WIDE_INTEGER_NAMESPACE::math::wide_integer::example000_numeric_limits();
  #else
  const auto result_is_ok = ::math::wide_integer::example000_numeric_limits();
  #endif

  std::cout << "result_is_ok: " << std::boolalpha << result_is_ok << std::endl;

  return (result_is_ok ? 0 : -1);
}

#endif
