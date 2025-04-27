///////////////////////////////////////////////////////////////////////////////
//  Copyright Christopher Kormanyos 2021 - 2022.
//  Distributed under the Boost Software License,
//  Version 1.0. (See accompanying file LICENSE_1_0.txt
//  or copy at http://www.boost.org/LICENSE_1_0.txt)
//

#include <cstdint>
#include <ctime>
#include <random>
#include <sstream>
#include <string>

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

#if (((BOOST_VERSION == 108000) || (BOOST_VERSION == 108100)) && defined(BOOST_NO_EXCEPTIONS))
#if defined(__clang__)
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wsometimes-uninitialized"
#endif
#if defined(_MSC_VER)
#pragma warning(push)
#pragma warning(disable : 4701)
#endif
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

#include <boost/multiprecision/cpp_int.hpp>

#include <math/wide_integer/uintwide_t.h>
#include <test/test_uintwide_t.h>

namespace local_int_convert
{
  auto engine_val() -> std::mt19937&                                                         { static std::mt19937                                                         my_engine_val; return my_engine_val; } // NOLINT(cert-msc32-c,cert-msc51-cpp)
  auto engine_sgn() -> std::ranlux24_base&                                                   { static std::ranlux24_base                                                   my_engine_sgn; return my_engine_sgn; } // NOLINT(cert-msc32-c,cert-msc51-cpp)
  auto engine_len() -> std::linear_congruential_engine<std::uint32_t, 48271, 0, 2147483647>& { static std::linear_congruential_engine<std::uint32_t, 48271, 0, 2147483647> my_engine_len; return my_engine_len; } // NOLINT(cert-msc32-c,cert-msc51-cpp,cppcoreguidelines-avoid-magic-numbers,readability-magic-numbers)

  template<const std::size_t MaxDigitsToGet,
           const std::size_t MinDigitsToGet = 2U>
  auto get_random_digit_string(std::string& str) -> void // NOLINT(google-runtime-references)
  {
    static_assert(MinDigitsToGet >=  2U, "Error: The minimum number of digits to get must be  2 or more");

    static std::uniform_int_distribution<unsigned>
    dist_sgn
    (
      static_cast<unsigned>(UINT8_C(0)),
      static_cast<unsigned>(UINT8_C(1))
    );

    static std::uniform_int_distribution<unsigned>
    dist_len
    (
      MinDigitsToGet,
      MaxDigitsToGet
    );

    static std::uniform_int_distribution<unsigned>
    dist_first
    (
      static_cast<unsigned>(UINT8_C(1)),
      static_cast<unsigned>(UINT8_C(9))
    );

    static std::uniform_int_distribution<unsigned>
    dist_following
    (
      static_cast<unsigned>(UINT8_C(0)),
      static_cast<unsigned>(UINT8_C(9))
    );

    const bool is_neg = (dist_sgn(engine_sgn()) != 0);

    const auto len = static_cast<std::string::size_type>(dist_len(engine_len()));

    std::string::size_type pos = 0U;

    if(is_neg)
    {
      str.resize(len + 1U);

      str.at(pos) = '-';

      ++pos;
    }
    else
    {
      str.resize(len);
    }

    str.at(pos) =
      static_cast<char>
      (
          dist_first(engine_val())
        + static_cast<std::uniform_int_distribution<unsigned>::result_type>(UINT32_C(0x30))
      );

    ++pos;

    while(pos < str.length())
    {
      str.at(pos) =
        static_cast<char>
        (
            dist_following(engine_val())
          + static_cast<std::uniform_int_distribution<unsigned>::result_type>(UINT32_C(0x30))
        );

      ++pos;
    }
  }

  template<typename UnsignedIntegralType>
  static auto hexlexical_cast(const UnsignedIntegralType& u) -> std::string
  {
    std::stringstream ss;

    ss << std::hex << u;

    return ss.str();
  }
} // namespace local_int_convert

#if defined(WIDE_INTEGER_NAMESPACE)
auto WIDE_INTEGER_NAMESPACE::math::wide_integer::test_uintwide_t_int_convert() -> bool
#else
auto ::math::wide_integer::test_uintwide_t_int_convert() -> bool
#endif
{
  constexpr auto digits2 = static_cast<unsigned>(UINT32_C(256));

  using boost_sint_backend_type =
    boost::multiprecision::cpp_int_backend<digits2,
                                           digits2,
                                           boost::multiprecision::signed_magnitude>;

  using boost_sint_type = boost::multiprecision::number<boost_sint_backend_type, boost::multiprecision::et_on>;

  #if defined(WIDE_INTEGER_HAS_LIMB_TYPE_UINT64)
  using local_limb_type = std::uint64_t;
  #else
  using local_limb_type = std::uint32_t;
  #endif

  #if defined(WIDE_INTEGER_NAMESPACE)
  using local_sint_type = WIDE_INTEGER_NAMESPACE::math::wide_integer::uintwide_t<digits2, local_limb_type, void, true>;
  #else
  using local_sint_type = ::math::wide_integer::uintwide_t<digits2, local_limb_type, void, true>;
  #endif

  local_int_convert::engine_val().seed(static_cast<typename std::mt19937::result_type>                                                        (std::clock()));
  local_int_convert::engine_sgn().seed(static_cast<typename std::ranlux24_base::result_type>                                                  (std::clock()));
  local_int_convert::engine_len().seed(static_cast<typename std::linear_congruential_engine<std::uint32_t, 48271, 0, 2147483647>::result_type>(std::clock())); // NOLINT(cppcoreguidelines-avoid-magic-numbers,readability-magic-numbers)

  bool result_is_ok = true;

  for(auto   i = static_cast<std::size_t>(UINT32_C(0));
             i < static_cast<std::size_t>(UINT32_C(0x100000));
           ++i)
  {
    std::string str_digits;

    local_int_convert::get_random_digit_string<static_cast<std::size_t>(UINT32_C(18)), static_cast<std::size_t>(UINT32_C(2))>(str_digits);

    const auto n_boost = boost_sint_type(str_digits.c_str());
    const auto n_local = local_sint_type(str_digits.c_str());

    const auto n_ctrl_boost = static_cast<std::int64_t>(n_boost);
    const auto n_ctrl_local = static_cast<std::int64_t>(n_local);

    const bool result_n_is_ok = (n_ctrl_boost == n_ctrl_local);

    result_is_ok = (result_n_is_ok && result_is_ok);
  }

  return result_is_ok;
}

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

#if (((BOOST_VERSION == 108000) || (BOOST_VERSION == 108100)) && defined(BOOST_NO_EXCEPTIONS))
#if defined(__clang__)
#pragma GCC diagnostic pop
#endif
#if defined(_MSC_VER)
#pragma warning(pop)
#endif
#endif
