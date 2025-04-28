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

#if defined(__clang__)
  #if defined __has_feature && __has_feature(thread_sanitizer)
  #define UINTWIDE_T_REDUCE_TEST_DEPTH
  #endif
#elif defined(__GNUC__)
  #if defined(__SANITIZE_THREAD__) || defined(WIDE_INTEGER_HAS_COVERAGE)
  #define UINTWIDE_T_REDUCE_TEST_DEPTH
  #endif
#elif defined(_MSC_VER)
  #if defined(_DEBUG)
  #define UINTWIDE_T_REDUCE_TEST_DEPTH
  #endif
#endif

namespace local_float_convert
{
  auto engine_man() -> std::mt19937&                                                         { static std::mt19937                                                         my_engine_man; return my_engine_man; } // NOLINT(cert-msc32-c,cert-msc51-cpp)
  auto engine_sgn() -> std::ranlux24_base&                                                   { static std::ranlux24_base                                                   my_engine_sgn; return my_engine_sgn; } // NOLINT(cert-msc32-c,cert-msc51-cpp)
  auto engine_e10() -> std::linear_congruential_engine<std::uint32_t, 48271, 0, 2147483647>& { static std::linear_congruential_engine<std::uint32_t, 48271, 0, 2147483647> my_engine_e10; return my_engine_e10; } // NOLINT(cert-msc32-c,cert-msc51-cpp,cppcoreguidelines-avoid-magic-numbers,readability-magic-numbers)

  template<typename UnsignedIntegralType>
  auto unsigned_dist_maker(const UnsignedIntegralType lo,
                           const UnsignedIntegralType hi) -> std::uniform_int_distribution<UnsignedIntegralType>
  {
    return std::uniform_int_distribution<UnsignedIntegralType>(lo, hi);
  }

  template<typename FloatingPointType,
           const std::int32_t LoExp10,
           const std::int32_t HiExp10>
  auto get_random_float() -> FloatingPointType
  {
    using local_builtin_float_type = FloatingPointType;

    static std::uniform_real_distribution<local_builtin_float_type>
    dist_man
    (
      static_cast<local_builtin_float_type>(0.0F),
      static_cast<local_builtin_float_type>(1.0F)
    );

    static auto dist_sgn = unsigned_dist_maker(static_cast<unsigned>(UINT8_C(0)),
                                               static_cast<unsigned>(UINT8_C(1)));

    static auto dist_e10 = unsigned_dist_maker(LoExp10, HiExp10);

    using std::pow;

    const auto p10 = dist_e10(engine_e10());

    const local_builtin_float_type e10 = pow(static_cast<local_builtin_float_type>(10.0F),
                                             static_cast<local_builtin_float_type>(p10));

    const local_builtin_float_type a = dist_man(engine_man()) * e10;

    const bool is_neg = (dist_sgn(engine_sgn()) != 0);

    const local_builtin_float_type f = ((!is_neg) ? a : -a);

    return f;
  }

  template<const std::size_t MaxDigitsToGet,
           const std::size_t MinDigitsToGet = 2U>
  auto get_random_digit_string(std::string& str) -> void // NOLINT(google-runtime-references)
  {
    static_assert(MinDigitsToGet >=  2U, "Error: The minimum number of digits to get must be  2 or more");

    static auto dist_sgn = unsigned_dist_maker(static_cast<unsigned>(UINT8_C(0)),
                                               static_cast<unsigned>(UINT8_C(1)));

    static auto dist_len = unsigned_dist_maker(MinDigitsToGet, MaxDigitsToGet);

    static auto dist_first = unsigned_dist_maker(static_cast<unsigned>(UINT8_C(1)),
                                                 static_cast<unsigned>(UINT8_C(9)));

    static auto dist_following = unsigned_dist_maker(static_cast<unsigned>(UINT8_C(0)),
                                                     static_cast<unsigned>(UINT8_C(9)));

    const bool is_neg = (dist_sgn(engine_sgn()) != 0);

    const auto len = dist_len(engine_e10());

    auto pos = static_cast<std::string::size_type>(0U);

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
          dist_first(engine_man())
        + static_cast<std::uniform_int_distribution<unsigned>::result_type>(UINT32_C(0x30))
      );

    ++pos;

    while(pos < str.length())
    {
      str.at(pos) =
        static_cast<char>
        (
            dist_following(engine_man())
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
} // namespace local_float_convert

#if defined(WIDE_INTEGER_NAMESPACE)
auto WIDE_INTEGER_NAMESPACE::math::wide_integer::test_uintwide_t_float_convert() -> bool
#else
auto ::math::wide_integer::test_uintwide_t_float_convert() -> bool
#endif
{
  constexpr auto digits2 = static_cast<unsigned>(256U);

  using boost_uint_backend_type =
    boost::multiprecision::cpp_int_backend<digits2,
                                           digits2,
                                           boost::multiprecision::unsigned_magnitude>;
  using boost_sint_backend_type =
    boost::multiprecision::cpp_int_backend<digits2,
                                           digits2,
                                           boost::multiprecision::signed_magnitude>;

  using boost_uint_type = boost::multiprecision::number<boost_uint_backend_type, boost::multiprecision::et_on>;
  using boost_sint_type = boost::multiprecision::number<boost_sint_backend_type, boost::multiprecision::et_on>;

  #if defined(WIDE_INTEGER_HAS_LIMB_TYPE_UINT64)
  using local_limb_type = std::uint64_t;
  #else
  using local_limb_type = std::uint32_t;
  #endif

  #if defined(WIDE_INTEGER_NAMESPACE)
  using local_uint_type = WIDE_INTEGER_NAMESPACE::math::wide_integer::uintwide_t<digits2, local_limb_type, void>;
  using local_sint_type = WIDE_INTEGER_NAMESPACE::math::wide_integer::uintwide_t<digits2, local_limb_type, void, true>;
  #else
  using local_uint_type = ::math::wide_integer::uintwide_t<digits2, local_limb_type, void>;
  using local_sint_type = ::math::wide_integer::uintwide_t<digits2, local_limb_type, void, true>;
  #endif

  local_float_convert::engine_man().seed(static_cast<typename std::mt19937::result_type>                                                        (std::clock()));
  local_float_convert::engine_sgn().seed(static_cast<typename std::ranlux24_base::result_type>                                                  (std::clock()));
  local_float_convert::engine_e10().seed(static_cast<typename std::linear_congruential_engine<std::uint32_t, 48271, 0, 2147483647>::result_type>(std::clock())); // NOLINT(cppcoreguidelines-avoid-magic-numbers,readability-magic-numbers)

  bool result_is_ok = true;

  #if !defined(UINTWIDE_T_REDUCE_TEST_DEPTH)
  for(auto i = static_cast<std::size_t>(0U); i < static_cast<std::size_t>(UINT32_C(0x80000)); ++i)
  #else
  for(auto i = static_cast<std::size_t>(0U); i < static_cast<std::size_t>(UINT32_C(0x10000)); ++i)
  #endif
  {
    const auto f = local_float_convert::get_random_float<float, -1, 27>();

    auto n_boost = static_cast<boost_sint_type>(f);
    auto n_local = static_cast<local_sint_type>(f);

    const std::string str_boost_signed = local_float_convert::hexlexical_cast(static_cast<boost_uint_type>(n_boost));
    const std::string str_local_signed = local_float_convert::hexlexical_cast(static_cast<local_uint_type>(n_local));

    result_is_ok = ((str_boost_signed == str_local_signed) && result_is_ok);
  }

  #if !defined(UINTWIDE_T_REDUCE_TEST_DEPTH)
  for(auto i = static_cast<std::size_t>(0U); i < static_cast<std::size_t>(UINT32_C(0x80000)); ++i)
  #else
  for(auto i = static_cast<std::size_t>(0U); i < static_cast<std::size_t>(UINT32_C(0x10000)); ++i)
  #endif
  {
    const auto d = local_float_convert::get_random_float<double, -1, 75>();

    auto n_boost = static_cast<boost_sint_type>(d);
    auto n_local = static_cast<local_sint_type>(d);

    const std::string str_boost_signed = local_float_convert::hexlexical_cast(static_cast<boost_uint_type>(n_boost));
    const std::string str_local_signed = local_float_convert::hexlexical_cast(static_cast<local_uint_type>(n_local));

    result_is_ok = ((str_boost_signed == str_local_signed) && result_is_ok);
  }

  local_float_convert::engine_man().seed(static_cast<typename std::mt19937::result_type>                                                        (std::clock()));
  local_float_convert::engine_sgn().seed(static_cast<typename std::ranlux24_base::result_type>                                                  (std::clock()));
  local_float_convert::engine_e10().seed(static_cast<typename std::linear_congruential_engine<std::uint32_t, 48271, 0, 2147483647>::result_type>(std::clock())); // NOLINT(cppcoreguidelines-avoid-magic-numbers,readability-magic-numbers)

  #if !defined(UINTWIDE_T_REDUCE_TEST_DEPTH)
  for(auto i = static_cast<std::size_t>(0U); i < static_cast<std::size_t>(UINT32_C(0x100000)); ++i)
  #else
  for(auto i = static_cast<std::size_t>(0U); i < static_cast<std::size_t>(UINT32_C(0x20000)); ++i)
  #endif
  {
    std::string str_digits;

    local_float_convert::get_random_digit_string<31U>(str_digits); // NOLINT(cppcoreguidelines-avoid-magic-numbers,readability-magic-numbers)

    const auto n_boost = boost_sint_type(str_digits.c_str());
    const auto n_local = local_sint_type(str_digits.c_str());

    const auto f_boost = static_cast<float>(n_boost);
    const auto f_local = static_cast<float>(n_local);

    using std::fabs;

    constexpr auto cast_tol_float = static_cast<float>(std::numeric_limits<float>::epsilon() * 2.0F);

    const float closeness      = fabs(1.0F - fabs(f_boost / f_local));
    const bool  result_f_is_ok = (closeness < cast_tol_float);

    result_is_ok = (result_f_is_ok && result_is_ok);
  }

  #if !defined(UINTWIDE_T_REDUCE_TEST_DEPTH)
  for(auto i = static_cast<std::size_t>(0U); i < static_cast<std::size_t>(UINT32_C(0x40000)); ++i)
  #else
  for(auto i = static_cast<std::size_t>(0U); i < static_cast<std::size_t>(UINT32_C(0x08000)); ++i)
  #endif
  {
    std::string str_digits;

    local_float_convert::get_random_digit_string<71U>(str_digits); // NOLINT(cppcoreguidelines-avoid-magic-numbers,readability-magic-numbers)

    const auto n_boost = boost_sint_type(str_digits.c_str());
    const auto n_local = local_sint_type(str_digits.c_str());

    const auto d_boost = static_cast<double>(n_boost);
    const auto d_local = static_cast<double>(n_local);

    using std::fabs;

    constexpr auto cast_tol_double = static_cast<double>(std::numeric_limits<double>::epsilon() * 2.0);

    const double closeness      = fabs(1.0 - fabs(d_boost / d_local));
    const bool   result_f_is_ok = (closeness < cast_tol_double);

    result_is_ok = (result_f_is_ok && result_is_ok);
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
