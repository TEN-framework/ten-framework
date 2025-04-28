///////////////////////////////////////////////////////////////////////////////
//  Copyright Christopher Kormanyos 2019 - 2022.
//  Distributed under the Boost Software License,
//  Version 1.0. (See accompanying file LICENSE_1_0.txt
//  or copy at http://www.boost.org/LICENSE_1_0.txt)
//

#include <chrono>
#include <random>
#include <string>
#include <vector>

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

#if defined(__GNUC__)
#if (BOOST_VERSION < 108000)
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wconversion"
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wsign-conversion"
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wunused-parameter"
#else
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wconversion"
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wsign-conversion"
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
#include <boost/multiprecision/uintwide_t_backend.hpp>

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

namespace test_uintwide_t_edge {

namespace local_edge_cases {

  #if !defined(UINTWIDE_T_REDUCE_TEST_DEPTH)
  constexpr auto local_digits2       = static_cast<std::size_t>(UINT16_C(16384));
  #endif
  constexpr auto local_digits2_small = static_cast<std::size_t>(UINT16_C(256));

} // namespace local_edge_cases

#if !defined(UINTWIDE_T_REDUCE_TEST_DEPTH)
constexpr auto loop_count_lo = static_cast<std::uint32_t>(UINT16_C(64));
constexpr auto loop_count_hi = static_cast<std::uint32_t>(UINT16_C(256));
#else
constexpr auto loop_count_lo = static_cast<std::uint32_t>(UINT16_C(4));
constexpr auto loop_count_hi = static_cast<std::uint32_t>(UINT16_C(8));
#endif

// Forward declaration
template<typename IntegralTimePointType,
         typename ClockType = std::chrono::high_resolution_clock>
auto time_point() -> IntegralTimePointType;

#if defined(WIDE_INTEGER_NAMESPACE)
using local_uintwide_t_small_unsigned_type =
  WIDE_INTEGER_NAMESPACE::math::wide_integer::uintwide_t<local_edge_cases::local_digits2_small, std::uint16_t, void, false>;
#else
using local_uintwide_t_small_unsigned_type =
  ::math::wide_integer::uintwide_t<local_edge_cases::local_digits2_small, std::uint16_t, void, false>;
#endif

#if defined(WIDE_INTEGER_NAMESPACE)
using local_uintwide_t_small_signed_type =
  WIDE_INTEGER_NAMESPACE::math::wide_integer::uintwide_t<local_edge_cases::local_digits2_small, std::uint16_t, void, true>;
#else
using local_uintwide_t_small_signed_type =
  ::math::wide_integer::uintwide_t<local_edge_cases::local_digits2_small, std::uint16_t, void, true>;
#endif

#if !defined(UINTWIDE_T_REDUCE_TEST_DEPTH)
using local_uint_backend_type =
  boost::multiprecision::uintwide_t_backend<local_edge_cases::local_digits2,
                                            std::uint32_t,
                                            std::allocator<std::uint32_t>>;

using boost_uint_backend_allocator_type = void;

using boost_uint_backend_type =
  boost::multiprecision::cpp_int_backend<local_edge_cases::local_digits2,
                                         local_edge_cases::local_digits2,
                                         boost::multiprecision::unsigned_magnitude,
                                         boost::multiprecision::unchecked,
                                         boost_uint_backend_allocator_type>;

using local_uint_type =
  boost::multiprecision::number<local_uint_backend_type,
                                boost::multiprecision::et_off>;

using boost_uint_type =
  boost::multiprecision::number<boost_uint_backend_type,
                                boost::multiprecision::et_off>;
#endif

enum class local_base
{
  dec,
  hex,
  oct
};

using eng_sgn_type = std::ranlux24;
using eng_dig_type = std::ranlux48;

std::uniform_int_distribution<std::uint32_t> dist_sgn    (UINT32_C(0), UINT32_C(1));  // NOLINT(cert-err58-cpp,cppcoreguidelines-avoid-non-const-global-variables)
std::uniform_int_distribution<std::uint32_t> dist_dig_dec(UINT32_C(1), UINT32_C(9));  // NOLINT(cert-err58-cpp,cppcoreguidelines-avoid-non-const-global-variables)
std::uniform_int_distribution<std::uint32_t> dist_dig_hex(UINT32_C(1), UINT32_C(15)); // NOLINT(cert-err58-cpp,cppcoreguidelines-avoid-non-const-global-variables)
std::uniform_int_distribution<std::uint32_t> dist_dig_oct(UINT32_C(1), UINT32_C(7));  // NOLINT(cert-err58-cpp,cppcoreguidelines-avoid-non-const-global-variables)

eng_sgn_type eng_sgn; // NOLINT(cert-msc32-c,cert-msc51-cpp,cert-err58-cpp,cppcoreguidelines-avoid-non-const-global-variables)
eng_dig_type eng_dig; // NOLINT(cert-msc32-c,cert-msc51-cpp,cert-err58-cpp,cppcoreguidelines-avoid-non-const-global-variables)

auto zero_as_limb               () -> const typename local_uintwide_t_small_unsigned_type::limb_type&;
auto zero_as_small_unsigned_type() -> const local_uintwide_t_small_unsigned_type&;
auto one_as_small_unsigned_type () -> const local_uintwide_t_small_unsigned_type&;
auto m_one_as_small_signed_type () -> const local_uintwide_t_small_signed_type&;

template<typename IntegralTimePointType,
         typename ClockType>
auto time_point() -> IntegralTimePointType
{
  using local_integral_time_point_type = IntegralTimePointType;
  using local_clock_type               = ClockType;

  const auto current_now =
    static_cast<std::uintmax_t>
    (
      std::chrono::duration_cast<std::chrono::nanoseconds>
      (
        local_clock_type::now().time_since_epoch()
      ).count()
    );

  return static_cast<local_integral_time_point_type>(current_now);
}

template<typename IntegralTypeWithStringConstruction>
auto generate_wide_integer_value(bool       is_positive           = true,
                                 local_base base_to_get           = local_base::dec,
                                 int        digits_in_base_to_get = std::numeric_limits<IntegralTypeWithStringConstruction>::digits10) -> IntegralTypeWithStringConstruction
{
  using local_integral_type = IntegralTypeWithStringConstruction;

  static_assert(   (  std::numeric_limits<local_integral_type>::is_signed  && std::numeric_limits<local_integral_type>::digits > static_cast<int>(INT8_C(63)))
                || ((!std::numeric_limits<local_integral_type>::is_signed) && std::numeric_limits<local_integral_type>::digits > static_cast<int>(INT8_C(64))),
                "Error: Integral type destination does not have enough digits10");

  std::string str_x(static_cast<std::size_t>(digits_in_base_to_get), '0');

  std::generate(str_x.begin(),
                str_x.end(),
                [&base_to_get]() // NOLINT(modernize-use-trailing-return-type,-warnings-as-errors)
                {
                  char c { };

                  if(base_to_get == local_base::oct)
                  {
                    c = static_cast<char>(dist_dig_oct(eng_dig));
                    c = static_cast<char>(c + static_cast<char>(INT8_C(0x30)));
                  }
                  else if(base_to_get == local_base::hex)
                  {
                    c = static_cast<char>(dist_dig_hex(eng_dig));

                    if(c < static_cast<char>(INT8_C(0xA)))
                    {
                      c = static_cast<char>(c + static_cast<char>(INT8_C(0x30)));
                    }
                    else
                    {
                      c = static_cast<char>(c + static_cast<char>(INT8_C(0x41)));
                    }
                  }
                  else
                  {
                    c = static_cast<char>(dist_dig_dec(eng_dig));
                    c = static_cast<char>(c + static_cast<char>(INT8_C(0x30)));
                  }

                  return c;
                });

  if(base_to_get == local_base::oct)
  {
    str_x.insert(str_x.begin(), static_cast<std::size_t>(UINT8_C(1)), '0');
  }
  else if(base_to_get == local_base::hex)
  {
    str_x.insert(str_x.begin(), static_cast<std::size_t>(UINT8_C(1)), 'x');
    str_x.insert(str_x.begin(), static_cast<std::size_t>(UINT8_C(1)), '0');
  }

  // Insert either a positive sign or a negative sign
  // (always one or the other) depending on the sign of x.
  const auto sign_char_to_insert =
    static_cast<char>
    (
      is_positive
        ? '+'
        : static_cast<char>((dist_sgn(eng_sgn) != static_cast<std::uint32_t>(UINT32_C(0))) ? '+' : '-')
    );

  str_x.insert(str_x.begin(), static_cast<std::size_t>(UINT8_C(1)), sign_char_to_insert);

  return local_integral_type(str_x.c_str());
}

#if !defined(UINTWIDE_T_REDUCE_TEST_DEPTH)
auto test_various_edge_operations() -> bool
{
  const auto u_max_local = (std::numeric_limits<local_uint_type>::max)();
  const auto u_max_boost = (std::numeric_limits<boost_uint_type>::max)();

  local_uint_type result_local;
  boost_uint_type result_boost;

  result_local = u_max_local * u_max_local;
  result_boost = u_max_boost * u_max_boost;

  const auto result01_is_ok = ((result_local == local_uint_type(static_cast<unsigned>(UINT8_C(1)))) && (result_boost == boost_uint_type(static_cast<unsigned>(UINT8_C(1)))));

  result_local = (u_max_local - 1U) * u_max_local;
  result_boost = (u_max_boost - 1U) * u_max_boost;

  const auto result02_is_ok = ((result_local == local_uint_type(static_cast<unsigned>(UINT8_C(2)))) && (result_boost == boost_uint_type(static_cast<unsigned>(UINT8_C(2)))));

  const std::string str_seven_and_effs =
    "0x7" + std::string(static_cast<std::string::size_type>((local_edge_cases::local_digits2 / 4) - static_cast<unsigned>(UINT8_C(1))), 'F');

  const local_uint_type u_seven_and_effs_local(str_seven_and_effs.c_str());
  const boost_uint_type u_seven_and_effs_boost(str_seven_and_effs.c_str());

  result_local = u_seven_and_effs_local * u_seven_and_effs_local;
  result_boost = u_seven_and_effs_boost * u_seven_and_effs_boost;

  const auto result03_is_ok = (result_local.convert_to<std::string>() == result_boost.convert_to<std::string>());

  const std::string str_three_quarter_effs_and_zeros =
      "0x"
    + std::string(static_cast<std::string::size_type>((local_edge_cases::local_digits2 / 4) * static_cast<unsigned>(UINT8_C(3))), 'F')
    + std::string(static_cast<std::string::size_type>((local_edge_cases::local_digits2 / 4) * static_cast<unsigned>(UINT8_C(1))), '0')
    ;

  const local_uint_type u_three_quarter_effs_and_zeros_local(str_three_quarter_effs_and_zeros.c_str());
  const boost_uint_type u_three_quarter_effs_and_zeros_boost(str_three_quarter_effs_and_zeros.c_str());

  result_local = u_three_quarter_effs_and_zeros_local * u_three_quarter_effs_and_zeros_local;
  result_boost = u_three_quarter_effs_and_zeros_boost * u_three_quarter_effs_and_zeros_boost;

  const auto result04_is_ok = (result_local.convert_to<std::string>() == result_boost.convert_to<std::string>());

  const std::string str_one_quarter_effs_and_zeros =
      "0x"
    + std::string(static_cast<std::string::size_type>((local_edge_cases::local_digits2 / 4) * static_cast<unsigned>(UINT8_C(1))), 'F')
    + std::string(static_cast<std::string::size_type>((local_edge_cases::local_digits2 / 4) * static_cast<unsigned>(UINT8_C(3))), '0')
    ;

  const local_uint_type u_one_quarter_effs_and_zeros_local(str_one_quarter_effs_and_zeros.c_str());
  const boost_uint_type u_one_quarter_effs_and_zeros_boost(str_one_quarter_effs_and_zeros.c_str());

  result_local = u_one_quarter_effs_and_zeros_local * u_one_quarter_effs_and_zeros_local;
  result_boost = u_one_quarter_effs_and_zeros_boost * u_one_quarter_effs_and_zeros_boost;

  const bool result05_is_ok = (result_local.convert_to<std::string>() == result_boost.convert_to<std::string>());

  const local_uint_type one_limb_effs_prior_to_half_and_zeros_local(local_uint_type(UINT32_C(0xFFFFFFFF)) << ((std::numeric_limits<local_uint_type>::digits / 2) - 32));
  const boost_uint_type one_limb_effs_prior_to_half_and_zeros_boost(boost_uint_type(UINT32_C(0xFFFFFFFF)) << ((std::numeric_limits<boost_uint_type>::digits / 2) - 32));

  result_local = one_limb_effs_prior_to_half_and_zeros_local * one_limb_effs_prior_to_half_and_zeros_local;
  result_boost = one_limb_effs_prior_to_half_and_zeros_boost * one_limb_effs_prior_to_half_and_zeros_boost;

  const auto result06_is_ok = (result_local.convert_to<std::string>() == result_boost.convert_to<std::string>());

  const local_uint_type u_mid_local = u_three_quarter_effs_and_zeros_local / static_cast<typename local_uint_type::backend_type::representation_type::limb_type>(UINT8_C(2));
  const boost_uint_type u_mid_boost = u_three_quarter_effs_and_zeros_boost / static_cast<typename std::iterator_traits<boost_uint_type::backend_type::limb_pointer>::value_type>(UINT8_C(2));

  constexpr auto signed_shift_amount =
    static_cast<int>
    (
      -(std::numeric_limits<typename local_uint_type::backend_type::representation_type::limb_type>::digits + 7)
    );

  result_local = u_mid_local;
  result_local.backend().representation() >>= signed_shift_amount;
  result_boost = u_mid_boost * (boost_uint_type(1U) << (-signed_shift_amount));

  const auto result07_is_ok = (result_local.convert_to<std::string>() == result_boost.convert_to<std::string>());

  result_local = u_mid_local;
  result_local.backend().representation() <<= signed_shift_amount;
  result_boost = u_mid_boost / (boost_uint_type(1U) << (-signed_shift_amount));

  const auto result08_is_ok = (result_local.convert_to<std::string>() == result_boost.convert_to<std::string>());

  auto result_is_ok = (   result01_is_ok
                       && result02_is_ok
                       && result03_is_ok
                       && result04_is_ok
                       && result05_is_ok
                       && result06_is_ok
                       && result07_is_ok
                       && result08_is_ok);

  {
    using local_derived_uint_type = typename local_uint_type::backend_type::representation_type;
    using local_limb_type         = local_derived_uint_type::limb_type;

    local_derived_uint_type dt(static_cast<local_limb_type>(INT8_C(-3)));

    std::fill(dt.representation().begin(), dt.representation().end(), static_cast<local_limb_type>(UINT8_C(0)));

    const auto result_fill_with_zero_is_ok = (dt == 0U);

    result_is_ok = (result_fill_with_zero_is_ok && result_is_ok);

    std::fill(dt.representation().begin(), dt.representation().end(), (std::numeric_limits<local_limb_type>::max)());

    const auto result_fill_with_effs_is_ok = (dt == (std::numeric_limits<local_derived_uint_type>::max)());

    result_is_ok = (result_fill_with_effs_is_ok && result_is_ok);
  }

  return result_is_ok;
}
#endif

auto test_various_ostream_ops() -> bool
{
  auto result_is_ok = true;

  eng_sgn.seed(time_point<typename eng_sgn_type::result_type>());
  eng_dig.seed(time_point<typename eng_dig_type::result_type>());

  {
    const auto u = local_uintwide_t_small_unsigned_type(static_cast<std::uint32_t>(UINT32_C(29363)));

    std::stringstream strm;

    strm << std::dec << std::showbase << std::setw(static_cast<std::streamsize>(INT8_C(100))) << std::setfill('#') << u;

    std::string str_ctrl(static_cast<std::size_t>(UINT8_C(100)), '#');

    str_ctrl.at(static_cast<std::size_t>(static_cast<std::size_t>(str_ctrl.size() - static_cast<std::size_t>(UINT8_C(1))) - static_cast<std::size_t>(UINT8_C(4)))) = '2';
    str_ctrl.at(static_cast<std::size_t>(static_cast<std::size_t>(str_ctrl.size() - static_cast<std::size_t>(UINT8_C(1))) - static_cast<std::size_t>(UINT8_C(3)))) = '9';
    str_ctrl.at(static_cast<std::size_t>(static_cast<std::size_t>(str_ctrl.size() - static_cast<std::size_t>(UINT8_C(1))) - static_cast<std::size_t>(UINT8_C(2)))) = '3';
    str_ctrl.at(static_cast<std::size_t>(static_cast<std::size_t>(str_ctrl.size() - static_cast<std::size_t>(UINT8_C(1))) - static_cast<std::size_t>(UINT8_C(1)))) = '6';
    str_ctrl.at(static_cast<std::size_t>(static_cast<std::size_t>(str_ctrl.size() - static_cast<std::size_t>(UINT8_C(1))) - static_cast<std::size_t>(UINT8_C(0)))) = '3';

    const auto result_u_fill_is_ok = (strm.str() == str_ctrl);

    result_is_ok = (result_u_fill_is_ok && result_is_ok);
  }

  {
    const auto u = local_uintwide_t_small_unsigned_type(static_cast<std::uint32_t>(UINT32_C(41719)));

    std::stringstream strm;

    strm << std::hex << std::uppercase << std::showbase << std::setw(static_cast<std::streamsize>(INT8_C(100))) << std::setfill('#') << u;

    std::string str_ctrl(static_cast<std::size_t>(UINT8_C(100)), '#');

    str_ctrl.at(static_cast<std::size_t>(static_cast<std::size_t>(str_ctrl.size() - static_cast<std::size_t>(UINT8_C(1))) - static_cast<std::size_t>(UINT8_C(5)))) = '0';
    str_ctrl.at(static_cast<std::size_t>(static_cast<std::size_t>(str_ctrl.size() - static_cast<std::size_t>(UINT8_C(1))) - static_cast<std::size_t>(UINT8_C(4)))) = 'X';
    str_ctrl.at(static_cast<std::size_t>(static_cast<std::size_t>(str_ctrl.size() - static_cast<std::size_t>(UINT8_C(1))) - static_cast<std::size_t>(UINT8_C(3)))) = 'A';
    str_ctrl.at(static_cast<std::size_t>(static_cast<std::size_t>(str_ctrl.size() - static_cast<std::size_t>(UINT8_C(1))) - static_cast<std::size_t>(UINT8_C(2)))) = '2';
    str_ctrl.at(static_cast<std::size_t>(static_cast<std::size_t>(str_ctrl.size() - static_cast<std::size_t>(UINT8_C(1))) - static_cast<std::size_t>(UINT8_C(1)))) = 'F';
    str_ctrl.at(static_cast<std::size_t>(static_cast<std::size_t>(str_ctrl.size() - static_cast<std::size_t>(UINT8_C(1))) - static_cast<std::size_t>(UINT8_C(0)))) = '7';

    const auto result_u_fill_is_ok = (strm.str() == str_ctrl);

    result_is_ok = (result_u_fill_is_ok && result_is_ok);
  }

  {
    const auto u = local_uintwide_t_small_unsigned_type(static_cast<std::uint32_t>(UINT32_C(29363)));

    std::stringstream strm;

    strm << std::oct << std::uppercase << std::showbase << std::setw(static_cast<std::streamsize>(INT8_C(100))) << std::setfill('#') << u;

    std::string str_ctrl(static_cast<std::size_t>(UINT8_C(100)), '#');

    str_ctrl.at(static_cast<std::size_t>(static_cast<std::size_t>(str_ctrl.size() - static_cast<std::size_t>(UINT8_C(1))) - static_cast<std::size_t>(UINT8_C(5)))) = '0';
    str_ctrl.at(static_cast<std::size_t>(static_cast<std::size_t>(str_ctrl.size() - static_cast<std::size_t>(UINT8_C(1))) - static_cast<std::size_t>(UINT8_C(4)))) = '7';
    str_ctrl.at(static_cast<std::size_t>(static_cast<std::size_t>(str_ctrl.size() - static_cast<std::size_t>(UINT8_C(1))) - static_cast<std::size_t>(UINT8_C(3)))) = '1';
    str_ctrl.at(static_cast<std::size_t>(static_cast<std::size_t>(str_ctrl.size() - static_cast<std::size_t>(UINT8_C(1))) - static_cast<std::size_t>(UINT8_C(2)))) = '2';
    str_ctrl.at(static_cast<std::size_t>(static_cast<std::size_t>(str_ctrl.size() - static_cast<std::size_t>(UINT8_C(1))) - static_cast<std::size_t>(UINT8_C(1)))) = '6';
    str_ctrl.at(static_cast<std::size_t>(static_cast<std::size_t>(str_ctrl.size() - static_cast<std::size_t>(UINT8_C(1))) - static_cast<std::size_t>(UINT8_C(0)))) = '3';

    const auto result_u_fill_is_ok = (strm.str() == str_ctrl);

    result_is_ok = (result_u_fill_is_ok && result_is_ok);
  }

  {
    const auto z = local_uintwide_t_small_unsigned_type(static_cast<std::uint32_t>(UINT8_C(0)));

    {
      std::stringstream strm;

      strm << std::oct << z;

      const auto result_zero_print_as_oct_is_ok = (strm.str() == "0");

      result_is_ok = (result_zero_print_as_oct_is_ok && result_is_ok);
    }

    {
      std::stringstream strm;

      strm << std::dec << z;

      const auto result_zero_print_as_dec_is_ok = (strm.str() == "0");

      result_is_ok = (result_zero_print_as_dec_is_ok && result_is_ok);
    }

    {
      std::stringstream strm;

      strm << std::hex << z;

      const auto result_zero_print_as_hex_is_ok = (strm.str() == "0");

      result_is_ok = (result_zero_print_as_hex_is_ok && result_is_ok);
    }
  }

  for(auto   i = static_cast<unsigned>(UINT8_C(0));
             i < static_cast<unsigned>(UINT32_C(1024));
           ++i)
  {
    const auto u =
      generate_wide_integer_value<local_uintwide_t_small_unsigned_type>
      (
        true,
        local_base::dec,
        std::numeric_limits<local_uintwide_t_small_unsigned_type>::digits10
      );

    std::stringstream strm;

    strm << std::dec << u;

    const local_uintwide_t_small_unsigned_type u_strm(strm.str().c_str());

    const auto result_u_is_ok = (u == u_strm);

    result_is_ok = (result_u_is_ok && result_is_ok);
  }

  for(auto   i = static_cast<unsigned>(UINT8_C(0));
             i < static_cast<unsigned>(UINT32_C(1024));
           ++i)
  {
    const auto u =
      generate_wide_integer_value<local_uintwide_t_small_unsigned_type>
      (
        true,
        local_base::hex,
        std::numeric_limits<local_uintwide_t_small_unsigned_type>::digits / 4
      );

    std::stringstream strm;

    strm << std::hex << std::showbase << u;

    const local_uintwide_t_small_unsigned_type u_strm(strm.str().c_str());

    const auto result_u_is_ok = (u == u_strm);

    result_is_ok = (result_u_is_ok && result_is_ok);
  }

  for(auto   i = static_cast<unsigned>(UINT8_C(0));
             i < static_cast<unsigned>(UINT32_C(1024));
           ++i)
  {
    const auto u =
      generate_wide_integer_value<local_uintwide_t_small_unsigned_type>
      (
        true,
        local_base::oct,
        std::numeric_limits<local_uintwide_t_small_unsigned_type>::digits / 3
      );

    std::stringstream strm;

    strm << std::oct << std::showbase << u;

    const local_uintwide_t_small_unsigned_type u_strm(strm.str().c_str());

    const auto result_u_is_ok = (u == u_strm);

    result_is_ok = (result_u_is_ok && result_is_ok);
  }

  for(auto   i = static_cast<unsigned>(UINT8_C(0));
             i < static_cast<unsigned>(UINT32_C(1024));
           ++i)
  {
    const auto n =
      generate_wide_integer_value<local_uintwide_t_small_signed_type>
      (
        false,
        local_base::dec,
        std::numeric_limits<local_uintwide_t_small_signed_type>::digits10
      );

    std::stringstream strm;

    strm << std::dec << std::showpos << n;

    const local_uintwide_t_small_signed_type n_strm(strm.str().c_str());

    const auto result_n_is_ok = (n == n_strm);

    result_is_ok = (result_n_is_ok && result_is_ok);
  }

  for(auto   i = static_cast<unsigned>(UINT8_C(0));
             i < static_cast<unsigned>(UINT32_C(1024));
           ++i)
  {
    const auto n =
      generate_wide_integer_value<local_uintwide_t_small_signed_type>
      (
        false,
        local_base::hex,
        (std::numeric_limits<local_uintwide_t_small_signed_type>::digits + 1) / 4
      );

    std::stringstream strm;

    strm << std::hex << std::showbase << std::showpos << n;

    const local_uintwide_t_small_signed_type n_strm(strm.str().c_str());

    const auto result_n_is_ok = (n == n_strm);

    result_is_ok = (result_n_is_ok && result_is_ok);
  }

  for(auto   i = static_cast<unsigned>(UINT8_C(0));
             i < static_cast<unsigned>(UINT32_C(1024));
           ++i)
  {
    const auto n =
      generate_wide_integer_value<local_uintwide_t_small_signed_type>
      (
        false,
        local_base::oct,
        (std::numeric_limits<local_uintwide_t_small_signed_type>::digits + 1) / 3
      );

    std::stringstream strm;

    strm << std::oct << std::showbase << std::showpos << n;

    const local_uintwide_t_small_signed_type n_strm(strm.str().c_str());

    const auto result_n_is_ok = (n == n_strm);

    result_is_ok = (result_n_is_ok && result_is_ok);
  }

  {
    const local_uintwide_t_small_unsigned_type m1("-0x1");

    std::stringstream strm;

    strm << std::hex << m1;

    const local_uintwide_t_small_unsigned_type m1_from_strm(strm.str().c_str());

    const auto result_read_and_round_trip_neg_hex_str_is_ok =
    (
      m1_from_strm == (std::numeric_limits<local_uintwide_t_small_unsigned_type>::max)()
    );

    result_is_ok = (result_read_and_round_trip_neg_hex_str_is_ok && result_is_ok);
  }

  return result_is_ok;
}

auto test_various_roots_and_pow_etc() -> bool
{
  auto result_is_ok = true;

  const auto ten_pow_forty = local_uintwide_t_small_unsigned_type("10000000000000000000000000000000000000000");

  {
    const auto u_root = rootk(ten_pow_forty, static_cast<std::uint_fast8_t>(UINT8_C(1)));

    const auto result_u_root_is_ok = (u_root == ten_pow_forty);

    result_is_ok = (result_u_root_is_ok && result_is_ok);
  }

  {
    const auto u_root = rootk(ten_pow_forty, static_cast<std::uint_fast8_t>(UINT8_C(2)));

    const auto ten_pow_twenty = local_uintwide_t_small_unsigned_type("100000000000000000000");

    const auto result_u_root_is_ok = (   (u_root == ten_pow_twenty)
                                      && (u_root == sqrt(ten_pow_forty)));

    result_is_ok = (result_u_root_is_ok && result_is_ok);
  }

  {
    const auto& u      = zero_as_small_unsigned_type();
    const auto  u_root = sqrt(u);

    const auto result_sqrt_zero_is_ok = (u_root == 0U);

    result_is_ok = (result_sqrt_zero_is_ok && result_is_ok);
  }

  {
    const auto& u      = zero_as_small_unsigned_type();
    const auto  u_root = cbrt(u);

    const auto result_cbrt_zero_is_ok = (u_root == zero_as_small_unsigned_type());

    result_is_ok = (result_cbrt_zero_is_ok && result_is_ok);
  }

  {
    const auto& u      = zero_as_small_unsigned_type();
    const auto  u_root = rootk(u, 7U);

    const auto result_rootk_zero_is_ok = (u_root == zero_as_small_unsigned_type());

    result_is_ok = (result_rootk_zero_is_ok && result_is_ok);
  }

  for(auto   i = static_cast<unsigned>(UINT8_C(0));
             i < static_cast<unsigned>(loop_count_hi);
           ++i)
  {
    auto b_gen = generate_wide_integer_value<local_uintwide_t_small_unsigned_type>(); // NOLINT
    auto m_gen = generate_wide_integer_value<local_uintwide_t_small_unsigned_type>();

    while(!(b_gen > m_gen)) // NOLINT(altera-id-dependent-backward-branch)
    {
      b_gen = generate_wide_integer_value<local_uintwide_t_small_unsigned_type>();
      m_gen = generate_wide_integer_value<local_uintwide_t_small_unsigned_type>();
    }

    const auto powm_zero_result = powm(b_gen, 0U, m_gen);
    const auto powm_one_result  = powm(b_gen, 1U, m_gen);

    const auto result_powm_checks_are_ok = (   (powm_zero_result == one_as_small_unsigned_type())
                                            && (powm_one_result  == (b_gen % m_gen)));

    result_is_ok = (result_powm_checks_are_ok && result_is_ok);
  }

  {
    using cbrt_data_array_type =
      std::array<local_uintwide_t_small_unsigned_type, static_cast<std::size_t>(UINT8_C(3))>;

    const cbrt_data_array_type cbrt_data =
    {
      local_uintwide_t_small_unsigned_type("67828177552242475987719934121374621432592443433392865437894564885546642548776"),
      local_uintwide_t_small_unsigned_type("114688795833607759436755318000801811092546159899778523634298604873518327575437"),
      local_uintwide_t_small_unsigned_type("97147734462982474181332761989635722126392718381938387792632943975979168194114"),
    };

    const cbrt_data_array_type cbrt_ctrl =
    {
      local_uintwide_t_small_unsigned_type("40782143592716585610825381"),
      local_uintwide_t_small_unsigned_type("48585535929752371864326912"),
      local_uintwide_t_small_unsigned_type("45970323401457076345923126"),
    };

    auto i = static_cast<typename cbrt_data_array_type::size_type>(UINT8_C(0));

    for(const auto& u : cbrt_data)
    {
      const auto cbrt_u = cbrt(u);

      const auto result_cbrt_is_ok = (cbrt_u == cbrt_ctrl[i++]);

      result_is_ok = (result_cbrt_is_ok && result_is_ok);
    }
  }

  for(auto   i = static_cast<unsigned>(UINT8_C(0));
             i < static_cast<unsigned>(loop_count_hi);
           ++i)
  {
    const auto high_bit =
      static_cast<local_uintwide_t_small_unsigned_type>
      (
           local_uintwide_t_small_unsigned_type(1)
        << (std::numeric_limits<local_uintwide_t_small_unsigned_type>::digits - 1)
      );

    const auto u =
      static_cast<local_uintwide_t_small_unsigned_type>
      (
        static_cast<local_uintwide_t_small_unsigned_type>
        (
            generate_wide_integer_value<local_uintwide_t_small_unsigned_type>()
          | high_bit
        )
        >> 3U
      );

    const auto sqrt_sqrt_u    = sqrt(sqrt(u));
    const auto quartic_root_u = rootk(u, 4U);

    const auto result_quartic_root_is_ok = (sqrt_sqrt_u == quartic_root_u);

    result_is_ok = (result_quartic_root_is_ok && result_is_ok);
  }

  for(auto   i = static_cast<unsigned>(UINT8_C(0));
             i < static_cast<unsigned>(loop_count_hi);
           ++i)
  {
    const auto b_gen = generate_wide_integer_value<local_uintwide_t_small_unsigned_type>();

    const auto powm_zero_one_result = powm(b_gen, 0U, one_as_small_unsigned_type());

    const auto result_powm_zero_one_is_ok = (powm_zero_one_result == zero_as_small_unsigned_type());

    result_is_ok = (result_powm_zero_one_is_ok && result_is_ok);
  }

  for(auto   i = static_cast<unsigned>(UINT8_C(0));
             i < static_cast<unsigned>(loop_count_hi);
           ++i)
  {
    constexpr auto digits10_to_get_b =
      static_cast<int>
      (
          static_cast<float>(std::numeric_limits<local_uintwide_t_small_unsigned_type>::digits10)
        * 0.45F
      );

    constexpr auto digits10_to_get_m =
      static_cast<int>
      (
          static_cast<float>(std::numeric_limits<local_uintwide_t_small_unsigned_type>::digits10)
        * 0.55F
      );

    const auto b_gen = generate_wide_integer_value<local_uintwide_t_small_unsigned_type>(true, local_base::dec, digits10_to_get_b);
    const auto m_gen = generate_wide_integer_value<local_uintwide_t_small_unsigned_type>(true, local_base::dec, digits10_to_get_m);

    const auto powm_two_result  = powm(b_gen, 2U, m_gen);
    const auto powm_two_control = static_cast<local_uintwide_t_small_unsigned_type>((b_gen * b_gen) % m_gen);

    const auto result_powm_two_is_ok = (powm_two_result == powm_two_control);

    result_is_ok = (result_powm_two_is_ok && result_is_ok);
  }

  return result_is_ok;
}

namespace local_edge_cases
{
  using small_integers_array_type = std::array<int, static_cast<std::size_t>(UINT8_C(50))>;

  constexpr auto small_integers =
    small_integers_array_type
    {
        1,
        2,
        3,   5,   7,  11,  13,  17,  19,  23,
        29,  31,  37,  41,  43,  47,  53,  59,
        61,  67,  71,  73,  79,  83,  89,  97,
      101, 103, 107, 109, 113, 127, 131, 137,
      139, 149, 151, 157, 163, 167, 173, 179,
      181, 191, 193, 197, 199, 211, 223, 227
    };
} // namespace local_edge_cases

auto test_small_prime_and_non_prime() -> bool
{
  constexpr auto local_my_width2 = local_uintwide_t_small_unsigned_type::my_width2;

  using local_limb_type = typename local_uintwide_t_small_unsigned_type::limb_type;

  #if defined(WIDE_INTEGER_NAMESPACE)
  using local_distribution_type = WIDE_INTEGER_NAMESPACE::math::wide_integer::uniform_int_distribution<local_my_width2, local_limb_type, void>;
  #else
  using local_distribution_type = ::math::wide_integer::uniform_int_distribution<local_my_width2, local_limb_type, void>;
  #endif

  using random_engine_type = std::minstd_rand;

  local_distribution_type distribution;

  using local_random_engine_result_type = typename random_engine_type::result_type;

  auto generator = random_engine_type(time_point<local_random_engine_result_type>());

  random_engine_type local_generator(generator);

  auto result_is_ok = true;

  auto result_p_is_prime_is_ok = true;

  for(auto ip = static_cast<std::size_t>(UINT8_C(1)); ip < local_edge_cases::small_integers.size(); ++ip)
  {
    const auto p_is_prime =
      miller_rabin
      (
        static_cast<local_uintwide_t_small_unsigned_type>(local_edge_cases::small_integers[ip]), // NOLINT(cppcoreguidelines-pro-bounds-constant-array-index)
        25U,
        distribution,
        local_generator
      );

    result_p_is_prime_is_ok = (p_is_prime && result_p_is_prime_is_ok);
  }

  const auto result_one_is_prime =
    miller_rabin
    (
      static_cast<local_uintwide_t_small_unsigned_type>(local_edge_cases::small_integers.front()),
      25U,
      distribution,
      local_generator
    );

  const auto result_one_is_not_prime_is_ok = (!result_one_is_prime);

  result_is_ok = (result_one_is_not_prime_is_ok && result_is_ok);

  const auto not_prime_checker =
    [&distribution, &local_generator](const std::size_t first, const std::size_t last_inclusive)
    {
      auto result_small_n_is_not_prime_is_ok = true;

      local_uintwide_t_small_unsigned_type prime_candidate = local_edge_cases::small_integers[first]; // NOLINT(cppcoreguidelines-pro-bounds-constant-array-index)

      for(auto ip = static_cast<std::size_t>(first + static_cast<std::size_t>(UINT8_C(1))); ip <= last_inclusive; ++ip) // NOLINT(altera-id-dependent-backward-branch)
      {
        prime_candidate *= local_edge_cases::small_integers[ip]; // NOLINT(cppcoreguidelines-pro-bounds-constant-array-index)

        const auto result_small_n_is_prime = miller_rabin(prime_candidate, 25U, distribution, local_generator);

        result_small_n_is_not_prime_is_ok = ((!result_small_n_is_prime) && result_small_n_is_not_prime_is_ok);
      }

      return result_small_n_is_not_prime_is_ok;
    };

  {
    // Exclude small prime factors from { 3 ...  53 }.
    // Product[Prime[i], {i, 2, 16}] = 16294579238595022365
    const auto result_not_prime_checker_is_ok =
      not_prime_checker
      (
        static_cast<std::size_t>(UINT8_C(2)),
        static_cast<std::size_t>(UINT8_C(16))
      );

    result_is_ok = (result_not_prime_checker_is_ok && result_is_ok);
  }
  {
    // Exclude small prime factors from { 59 ... 101 }.
    // Product[Prime[i], {i, 17, 26}] = 7145393598349078859
    const auto result_not_prime_checker_is_ok =
      not_prime_checker
      (
        static_cast<std::size_t>(UINT8_C(17)),
        static_cast<std::size_t>(UINT8_C(26))
      );

    result_is_ok = (result_not_prime_checker_is_ok && result_is_ok);
  }
  {
    // Exclude small prime factors from { 103 ... 149 }.
    // Product[Prime[i], {i, 27, 35}] = 6408001374760705163
    const auto result_not_prime_checker_is_ok =
      not_prime_checker
      (
        static_cast<std::size_t>(UINT8_C(27)),
        static_cast<std::size_t>(UINT8_C(35))
      );

    result_is_ok = (result_not_prime_checker_is_ok && result_is_ok);
  }
  {
    // Exclude small prime factors from { 151 ... 191 }.
    // Product[Prime[i], {i, 36, 43}] = 690862709424854779
    const auto result_not_prime_checker_is_ok =
      not_prime_checker
      (
        static_cast<std::size_t>(UINT8_C(36)),
        static_cast<std::size_t>(UINT8_C(43))
      );

    result_is_ok = (result_not_prime_checker_is_ok && result_is_ok);
  }
  {
    // Exclude small prime factors from { 193 ... 227 }.
    // Product[Prime[i], {i, 44, 49}] = 80814592450549
    const auto result_not_prime_checker_is_ok =
      not_prime_checker
      (
        static_cast<std::size_t>(UINT8_C(44)),
        static_cast<std::size_t>(UINT8_C(49))
      );

    result_is_ok = (result_not_prime_checker_is_ok && result_is_ok);
  }

  return result_is_ok;
}

auto test_some_gcd_and_equal_left_right() -> bool
{
  auto result_is_ok = true;

  for(auto   i = static_cast<unsigned>(UINT8_C(0));
             i < static_cast<unsigned>(UINT32_C(64));
           ++i)
  {
    auto result_gcd_is_ok = true;

    const auto left =
      generate_wide_integer_value<local_uintwide_t_small_unsigned_type>
      (
        true,
        local_base::dec,
        static_cast<int>(std::numeric_limits<local_uintwide_t_small_unsigned_type>::digits10 - static_cast<int>(INT8_C(1)))
      );

    const auto right = left;

    {
      const auto result_gcd_left_equal_right_is_ok = ((left == right) && (gcd(left, right) == left));

      result_gcd_is_ok = (result_gcd_left_equal_right_is_ok && result_gcd_is_ok);
    }

    {
      const auto u_left  = left + static_cast<unsigned>(UINT8_C(1));
      const auto v_right = right;

      const auto result_gcd_left_unequal_right_is_ok = ((u_left != v_right) && (gcd(u_left, v_right) != u_left));

      result_gcd_is_ok = (result_gcd_left_unequal_right_is_ok && result_gcd_is_ok);
    }

    result_is_ok = (result_gcd_is_ok && result_is_ok);
  }

  const auto gcd64_equal_checker =
    [](const std::size_t first, const std::size_t last_inclusive, const std::uint64_t right) // NOLINT(bugprone-easily-swappable-parameters)
    {
      #if defined(WIDE_INTEGER_NAMESPACE)
      using WIDE_INTEGER_NAMESPACE::math::wide_integer::gcd;
      #else
      using ::math::wide_integer::gcd;
      #endif

      auto left = static_cast<std::uint64_t>(local_edge_cases::small_integers[first]); // NOLINT(cppcoreguidelines-pro-bounds-constant-array-index)

      for(auto ig = static_cast<std::size_t>(first + static_cast<std::size_t>(UINT8_C(1))); ig <= last_inclusive; ++ig) // NOLINT(altera-id-dependent-backward-branch)
      {
        left *= static_cast<std::uint64_t>(local_edge_cases::small_integers[ig]); // NOLINT(cppcoreguidelines-pro-bounds-constant-array-index)
      }

      const auto result_gcd_left_equal_right_is_ok = ((left == right) && (gcd(left, right) == left));

      return result_gcd_left_equal_right_is_ok;
    };

  {
    // Consider small prime factors from { 3 ...  53 }.
    // Product[Prime[i], {i, 2, 16}] = 16294579238595022365
    const auto result_gcd64_equal_checker_is_ok =
      gcd64_equal_checker
      (
        static_cast<std::size_t>(UINT8_C(2)),
        static_cast<std::size_t>(UINT8_C(16)),
        static_cast<std::uint64_t>(UINT64_C(16294579238595022365))
      );

    result_is_ok = (result_gcd64_equal_checker_is_ok && result_is_ok);
  }
  {
    // Consider small prime factors from { 59 ... 101 }.
    // Product[Prime[i], {i, 17, 26}] = 7145393598349078859
    const auto result_gcd64_equal_checker_is_ok =
      gcd64_equal_checker
      (
        static_cast<std::size_t>(UINT8_C(17)),
        static_cast<std::size_t>(UINT8_C(26)),
        static_cast<std::uint64_t>(UINT64_C(7145393598349078859))
      );

    result_is_ok = (result_gcd64_equal_checker_is_ok && result_is_ok);
  }
  {
    // Consider small prime factors from { 103 ... 149 }.
    // Product[Prime[i], {i, 27, 35}] = 6408001374760705163
    const auto result_gcd64_equal_checker_is_ok =
      gcd64_equal_checker
      (
        static_cast<std::size_t>(UINT8_C(27)),
        static_cast<std::size_t>(UINT8_C(35)),
        static_cast<std::uint64_t>(UINT64_C(6408001374760705163))
      );

    result_is_ok = (result_gcd64_equal_checker_is_ok && result_is_ok);
  }
  {
    // Consider small prime factors from { 151 ... 191 }.
    // Product[Prime[i], {i, 36, 43}] = 690862709424854779
    const auto result_gcd64_equal_checker_is_ok =
      gcd64_equal_checker
      (
        static_cast<std::size_t>(UINT8_C(36)),
        static_cast<std::size_t>(UINT8_C(43)),
        static_cast<std::uint64_t>(UINT64_C(690862709424854779))
      );

    result_is_ok = (result_gcd64_equal_checker_is_ok && result_is_ok);
  }
  {
    // Consider small prime factors from { 193 ... 227 }.
    // Product[Prime[i], {i, 44, 49}] = 80814592450549
    const auto result_gcd64_equal_checker_is_ok =
      gcd64_equal_checker
      (
        static_cast<std::size_t>(UINT8_C(44)),
        static_cast<std::size_t>(UINT8_C(49)),
        static_cast<std::uint64_t>(UINT64_C(80814592450549))
      );

    result_is_ok = (result_gcd64_equal_checker_is_ok && result_is_ok);
  }

  {
    #if defined(WIDE_INTEGER_NAMESPACE)
    using WIDE_INTEGER_NAMESPACE::math::wide_integer::gcd;
    #else
    using ::math::wide_integer::gcd;
    #endif

    // GCD[6170895419598858564, 1073014744210933590]
    // 594
    const auto gcd64 = gcd(static_cast<std::uint64_t>(UINT64_C(6170895419598858564)),
                           static_cast<std::uint64_t>(UINT64_C(1073014744210933590)));

    const auto result_gcd64_is_ok = (gcd64 == static_cast<std::uint64_t>(UINT64_C(594)));

    result_is_ok = (result_gcd64_is_ok && result_is_ok);
  }

  {
    // GCD[20769612331917304, 11556552886528217295]
    // 6673

    #if defined(WIDE_INTEGER_NAMESPACE)
    using WIDE_INTEGER_NAMESPACE::math::wide_integer::gcd;
    #else
    using ::math::wide_integer::gcd;
    #endif

    const auto gcd64 = gcd(static_cast<std::uint64_t>(UINT64_C(20769612331917304)),
                           static_cast<std::uint64_t>(UINT64_C(11556552886528217295)));

    const auto result_gcd64_is_ok = (gcd64 == static_cast<std::uint64_t>(UINT64_C(6673)));

    result_is_ok = (result_gcd64_is_ok && result_is_ok);
  }

  {
    // GCD[3263830144632800334, 9189394046487653520]
    // 56598

    #if defined(WIDE_INTEGER_NAMESPACE)
    using WIDE_INTEGER_NAMESPACE::math::wide_integer::gcd;
    #else
    using ::math::wide_integer::gcd;
    #endif

    const auto gcd64 = gcd(static_cast<std::uint64_t>(UINT64_C(3263830144632800334)),
                           static_cast<std::uint64_t>(UINT64_C(9189394046487653520)));

    const auto result_gcd64_is_ok = (gcd64 == static_cast<std::uint64_t>(UINT64_C(56598)));

    result_is_ok = (result_gcd64_is_ok && result_is_ok);
  }

  {
    // GCD[7515843862511910988, 11558893357905095758]
    // 420278

    #if defined(WIDE_INTEGER_NAMESPACE)
    using WIDE_INTEGER_NAMESPACE::math::wide_integer::gcd;
    #else
    using ::math::wide_integer::gcd;
    #endif

    const auto gcd64 = gcd(static_cast<std::uint64_t>(UINT64_C(7515843862511910988)),
                           static_cast<std::uint64_t>(UINT64_C(11558893357905095758)));

    const auto result_gcd64_is_ok = (gcd64 == static_cast<std::uint64_t>(UINT64_C(420278)));

    result_is_ok = (result_gcd64_is_ok && result_is_ok);
  }

  {
    #if defined(WIDE_INTEGER_NAMESPACE)
    using WIDE_INTEGER_NAMESPACE::math::wide_integer::gcd;
    #else
    using ::math::wide_integer::gcd;
    #endif

    const auto result_gcd_left_zero = gcd(static_cast<std::uint64_t>(UINT64_C(7515843862511910988)),
                                          static_cast<std::uint64_t>(UINT64_C(0)));

    const auto result_gcd_left_zero_is_ok = (result_gcd_left_zero == static_cast<std::uint64_t>(UINT64_C(7515843862511910988)));

    result_is_ok = (result_gcd_left_zero_is_ok && result_is_ok);
  }

  {
    #if defined(WIDE_INTEGER_NAMESPACE)
    using WIDE_INTEGER_NAMESPACE::math::wide_integer::gcd;
    #else
    using ::math::wide_integer::gcd;
    #endif

    const auto result_gcd_right_zero = gcd(static_cast<std::uint64_t>(UINT64_C(0)),
                                           static_cast<std::uint64_t>(UINT64_C(7515843862511910988)));

    const auto result_gcd_right_zero_is_ok = (result_gcd_right_zero == static_cast<std::uint64_t>(UINT64_C(7515843862511910988)));

    result_is_ok = (result_gcd_right_zero_is_ok && result_is_ok);
  }

  return result_is_ok;
}

auto test_various_isolated_edge_cases() -> bool // NOLINT(readability-function-cognitive-complexity)
{
  auto result_is_ok = true;

  {
    using local_rep_type   = typename local_uintwide_t_small_unsigned_type::representation_type;
    using local_value_type = typename local_rep_type::value_type;

    local_rep_type
      rep
      (
        local_uintwide_t_small_unsigned_type::number_of_limbs,
        (std::numeric_limits<local_value_type>::max)(),
        typename local_rep_type::allocator_type()
      );

    const auto rep_as_max_is_ok =
      (local_uintwide_t_small_unsigned_type(rep) == (std::numeric_limits<local_uintwide_t_small_unsigned_type>::max)());

    result_is_ok = (rep_as_max_is_ok && result_is_ok);

    std::fill(rep.begin(), rep.end(), static_cast<local_value_type>(UINT8_C(0)));

    const auto rep_as_zero_is_ok = (local_uintwide_t_small_unsigned_type(rep) == 0);

    result_is_ok = (rep_as_zero_is_ok && result_is_ok);

    std::fill(rep.begin(), rep.end(), (std::numeric_limits<local_value_type>::max)());

    const auto rep_as_max2_is_ok =
      (local_uintwide_t_small_unsigned_type(rep) == (std::numeric_limits<local_uintwide_t_small_unsigned_type>::max)());

    result_is_ok = (rep_as_max2_is_ok && result_is_ok);

    rep =
      local_rep_type
      (
        static_cast<typename local_rep_type::size_type>(rep.size()),
        (std::numeric_limits<local_value_type>::max)(),
        typename local_rep_type::allocator_type()
      );

    const auto rep_as_max3_is_ok =
    (
      local_uintwide_t_small_unsigned_type(rep) == (std::numeric_limits<local_uintwide_t_small_unsigned_type>::max)()
    );

    result_is_ok = (rep_as_max3_is_ok && result_is_ok);
  }

  {
    WIDE_INTEGER_CONSTEXPR auto inf_f  = std::numeric_limits<float>::infinity();
    WIDE_INTEGER_CONSTEXPR auto inf_d  = std::numeric_limits<double>::infinity();
    WIDE_INTEGER_CONSTEXPR auto inf_ld = std::numeric_limits<long double>::infinity();

    local_uintwide_t_small_unsigned_type u_inf_f (inf_f);
    local_uintwide_t_small_unsigned_type u_inf_d (inf_d);
    local_uintwide_t_small_unsigned_type u_inf_ld(inf_ld);

    const auto result_infinities_is_ok = (   (u_inf_f  == 0)
                                          && (u_inf_d  == 0)
                                          && (u_inf_ld == 0));

    result_is_ok = (result_infinities_is_ok && result_is_ok);
  }

  for(auto   i = static_cast<unsigned>(UINT8_C(0));
             i < static_cast<unsigned>(loop_count_hi);
           ++i)
  {
    // Verify shift of an unsigned wide-integer by a signed amount.

    const auto u_gen = generate_wide_integer_value<local_uintwide_t_small_unsigned_type>();

    const auto ur_neg  = u_gen << static_cast<int>(INT8_C(-4));
    const auto ur_ctrl = u_gen >> static_cast<unsigned>(UINT8_C(4));

    const auto ul_neg  = u_gen >> static_cast<int>(INT8_C(-4));
    const auto ul_ctrl = u_gen << static_cast<unsigned>(UINT8_C(4));

    const auto result_left_is_ok  = (ul_neg == ul_ctrl);
    const auto result_right_is_ok = (ur_neg == ur_ctrl);

    result_is_ok = (result_left_is_ok && result_right_is_ok && result_is_ok);
  }

  for(auto   i = static_cast<unsigned>(UINT8_C(0));
             i < static_cast<unsigned>(loop_count_hi);
           ++i)
  {
    // Verify shift of a signed wide-integer by a signed amount.

    const auto s_gen = generate_wide_integer_value<local_uintwide_t_small_signed_type>(false);

    const auto sr_neg  = s_gen << static_cast<int>(INT8_C(-4));
    const auto sr_ctrl = s_gen >> static_cast<unsigned>(UINT8_C(4));

    const auto sl_neg  = s_gen >> static_cast<int>(INT8_C(-4));
    const auto sl_ctrl = s_gen << static_cast<unsigned>(UINT8_C(4));

    const auto result_left_is_ok  = (sl_neg == sl_ctrl);
    const auto result_right_is_ok = (sr_neg == sr_ctrl);

    result_is_ok = (result_left_is_ok && result_right_is_ok && result_is_ok);
  }

  {
    local_uintwide_t_small_unsigned_type u1(static_cast<unsigned>(UINT8_C(1)));

    u1 /= zero_as_limb();

    const auto result_overflow_is_ok = (u1 == (std::numeric_limits<local_uintwide_t_small_unsigned_type>::max)());

    result_is_ok = (result_overflow_is_ok && result_is_ok);
  }

  {
    local_uintwide_t_small_unsigned_type u1(static_cast<unsigned>(UINT8_C(1)));

    u1 /= local_uintwide_t_small_unsigned_type(zero_as_limb());

    const auto result_overflow_is_ok = (u1 == (std::numeric_limits<local_uintwide_t_small_unsigned_type>::max)());

    result_is_ok = (result_overflow_is_ok && result_is_ok);
  }

  for(auto   i = static_cast<unsigned>(UINT8_C(0));
             i < static_cast<unsigned>(loop_count_hi);
           ++i)
  {
    // Verify division of finite, unsigned numerator by zero which returns the maximum of the type.

    auto u_gen = generate_wide_integer_value<local_uintwide_t_small_unsigned_type>();

    u_gen /= zero_as_small_unsigned_type();

    const auto result_unsigned_div_by_zero_is_ok = (u_gen == (std::numeric_limits<local_uintwide_t_small_unsigned_type>::max)());

    result_is_ok = (result_unsigned_div_by_zero_is_ok && result_is_ok);
  }

  for(auto   i = static_cast<unsigned>(UINT8_C(0));
             i < static_cast<unsigned>(loop_count_hi);
           ++i)
  {
    // Verify division of finite, signed numerator by zero which returns the maximum of the type.

    const auto u_gen = generate_wide_integer_value<local_uintwide_t_small_unsigned_type>();
    auto n_gen = local_uintwide_t_small_signed_type(u_gen);

    n_gen /= local_uintwide_t_small_signed_type(zero_as_small_unsigned_type());

    const auto result_signed_div_by_zero_is_ok = (n_gen == (std::numeric_limits<local_uintwide_t_small_signed_type>::max)());

    result_is_ok = (result_signed_div_by_zero_is_ok && result_is_ok);
  }

  {
    // Verify division of zero by zero which returns the maximum of the type.

    auto z = zero_as_small_unsigned_type();

    z /= zero_as_small_unsigned_type();

    const auto result_zero_div_by_zero_is_ok = (z == (std::numeric_limits<local_uintwide_t_small_unsigned_type>::max)());

    result_is_ok = (result_zero_div_by_zero_is_ok && result_is_ok);
  }

  for(auto   i = static_cast<unsigned>(UINT8_C(0));
             i < static_cast<unsigned>(loop_count_hi);
           ++i)
  {
    // Verify modulus of zero with a finite denominator which returns zero modulus.

    auto u_gen = generate_wide_integer_value<local_uintwide_t_small_unsigned_type>();

    const auto mod = zero_as_small_unsigned_type() % u_gen;

    const auto result_zero_mod_with_finite_is_ok = (mod == zero_as_small_unsigned_type());

    result_is_ok = (result_zero_mod_with_finite_is_ok && result_is_ok);
  }

  {
    const auto ten_pow_forty = local_uintwide_t_small_unsigned_type("10000000000000000000000000000000000000000");

          auto a(ten_pow_forty);
    const auto b(local_uintwide_t_small_unsigned_type("10000000000000000000000000000000000000000"));

    const auto& c(a %= b);

    #if (defined(__clang__) && (defined(__clang_major__) && (__clang_major__ > 6)))
    #pragma GCC diagnostic push
    #pragma GCC diagnostic ignored "-Wself-assign-overloaded"
    #endif

    const auto& d(a %= a); // NOLINT(clang-diagnostic-self-assign-overloaded)

    #if (defined(__clang__) && (defined(__clang_major__) && (__clang_major__ > 6)))
    #pragma GCC diagnostic pop
    #endif

    const auto result_self_mod_is_ok = ((c == 0) && (d == 0));

    result_is_ok = (result_self_mod_is_ok && result_is_ok);
  }


  for(auto   i = static_cast<unsigned>(UINT8_C(0));
             i < static_cast<unsigned>(loop_count_hi);
           ++i)
  {
    auto shift_amount =
      static_cast<unsigned>
      (
          (
                (std::numeric_limits<local_uintwide_t_small_unsigned_type>::digits / 100)                // NOLINT(cppcoreguidelines-avoid-magic-numbers,readability-magic-numbers)
            + (((std::numeric_limits<local_uintwide_t_small_unsigned_type>::digits % 100) != 0) ? 1 : 0) // NOLINT(cppcoreguidelines-avoid-magic-numbers,readability-magic-numbers)
          )
        *
          100 // NOLINT(cppcoreguidelines-avoid-magic-numbers,readability-magic-numbers)
      );

    for( ; shift_amount  < static_cast<unsigned>(UINT32_C(2000)); // NOLINT(altera-id-dependent-backward-branch)
           shift_amount += static_cast<unsigned>(UINT32_C(100)))
    {
      const auto u_gen = generate_wide_integer_value<local_uintwide_t_small_unsigned_type>();

      auto result_overshift_is_ok = true;

      const auto u_left_n  = local_uintwide_t_small_unsigned_type(u_gen) << static_cast<std::int32_t> (shift_amount);
      const auto u_left_u  = local_uintwide_t_small_unsigned_type(u_gen) << static_cast<std::uint32_t>(shift_amount);
      const auto u_right_n = local_uintwide_t_small_unsigned_type(u_gen) >> static_cast<std::int32_t> (shift_amount);
      const auto u_right_u = local_uintwide_t_small_unsigned_type(u_gen) >> static_cast<std::uint32_t>(shift_amount);

      result_overshift_is_ok = ((u_left_n  == zero_as_small_unsigned_type()) && result_overshift_is_ok);
      result_overshift_is_ok = ((u_left_u  == zero_as_small_unsigned_type()) && result_overshift_is_ok);
      result_overshift_is_ok = ((u_right_n == zero_as_small_unsigned_type()) && result_overshift_is_ok);
      result_overshift_is_ok = ((u_right_u == zero_as_small_unsigned_type()) && result_overshift_is_ok);

      result_is_ok = (result_overshift_is_ok && result_is_ok);
    }
  }

  for(auto   i = static_cast<unsigned>(UINT8_C(0));
             i < static_cast<unsigned>(loop_count_hi);
           ++i)
  {
    auto shift_amount =
      static_cast<unsigned>
      (
          (
                (std::numeric_limits<local_uintwide_t_small_signed_type>::digits / 100)                // NOLINT(cppcoreguidelines-avoid-magic-numbers,readability-magic-numbers)
            + (((std::numeric_limits<local_uintwide_t_small_signed_type>::digits % 100) != 0) ? 1 : 0) // NOLINT(cppcoreguidelines-avoid-magic-numbers,readability-magic-numbers)
          )
        *
          100 // NOLINT(cppcoreguidelines-avoid-magic-numbers,readability-magic-numbers)
      );

    for( ; shift_amount  < static_cast<unsigned>(UINT32_C(2000)); // NOLINT(altera-id-dependent-backward-branch)
           shift_amount += static_cast<unsigned>(UINT32_C(100)))
    {
      const auto n_gen = generate_wide_integer_value<local_uintwide_t_small_signed_type>(false);

      const auto n_is_neg = (n_gen < 0);

      const auto n_left_n  = local_uintwide_t_small_signed_type(n_gen) << static_cast<std::int32_t> (shift_amount);
      const auto n_left_u  = local_uintwide_t_small_signed_type(n_gen) << static_cast<std::uint32_t>(shift_amount);
      const auto n_right_n = local_uintwide_t_small_signed_type(n_gen) >> static_cast<std::int32_t> (shift_amount);
      const auto n_right_u = local_uintwide_t_small_signed_type(n_gen) >> static_cast<std::uint32_t>(shift_amount);

      auto result_overshift_is_ok = true;

      result_overshift_is_ok = ((n_left_n  == local_uintwide_t_small_signed_type(zero_as_small_unsigned_type())) && result_overshift_is_ok);
      result_overshift_is_ok = ((n_left_u  == local_uintwide_t_small_signed_type(zero_as_small_unsigned_type())) && result_overshift_is_ok);
      result_overshift_is_ok = ((n_right_n == ((!n_is_neg) ? local_uintwide_t_small_signed_type(zero_as_small_unsigned_type()) : m_one_as_small_signed_type())) && result_overshift_is_ok);
      result_overshift_is_ok = ((n_right_u == ((!n_is_neg) ? local_uintwide_t_small_signed_type(zero_as_small_unsigned_type()) : m_one_as_small_signed_type())) && result_overshift_is_ok);

      result_is_ok = (result_overshift_is_ok && result_is_ok);
    }
  }

  return result_is_ok;
}

auto test_to_chars_and_to_string() -> bool // NOLINT(readability-function-cognitive-complexity)
{
  eng_sgn.seed(time_point<typename eng_sgn_type::result_type>());
  eng_dig.seed(time_point<typename eng_dig_type::result_type>());

  auto result_is_ok = true;

  #if defined(__cpp_lib_to_chars)
  for(auto   i = static_cast<unsigned>(UINT8_C(0));
             i < static_cast<unsigned>(loop_count_hi);
           ++i)
  {
    // Verify write to_chars() and read back from string of unsigned uintwide_t.
    // Use all three bases octal, decimal, and hexadecimal.

    using to_chars_storage_array_oct_type =
      std::array<char, static_cast<std::size_t>(local_uintwide_t_small_unsigned_type::wr_string_max_buffer_size_oct())>;

    using to_chars_storage_array_dec_type =
      std::array<char, static_cast<std::size_t>(local_uintwide_t_small_unsigned_type::wr_string_max_buffer_size_dec())>;

    using to_chars_storage_array_hex_type =
      std::array<char, static_cast<std::size_t>(local_uintwide_t_small_unsigned_type::wr_string_max_buffer_size_hex())>;

    constexpr auto char_fill = '\0';

    to_chars_storage_array_oct_type arr_oct { }; arr_oct.fill(char_fill);
    to_chars_storage_array_dec_type arr_dec { }; arr_dec.fill(char_fill);
    to_chars_storage_array_hex_type arr_hex { }; arr_hex.fill(char_fill);

    auto u_gen = generate_wide_integer_value<local_uintwide_t_small_unsigned_type>();

    using std::to_chars;

    const auto result_oct_as_chars = to_chars(arr_oct.data(), arr_oct.data() + arr_oct.size(), u_gen,  8);
    const auto result_dec_as_chars = to_chars(arr_dec.data(), arr_dec.data() + arr_dec.size(), u_gen, 10);
    const auto result_hex_as_chars = to_chars(arr_hex.data(), arr_hex.data() + arr_hex.size(), u_gen, 16);

    auto result_oct_as_str = std::string(arr_oct.data());
         result_oct_as_str.insert(result_oct_as_str.begin(), static_cast<std::string::size_type>(UINT8_C(1)), '0');

    const auto result_dec_as_str = std::string(arr_dec.data());

    auto result_hex_as_str = std::string(arr_hex.data());
    result_hex_as_str.insert(result_hex_as_str.begin(), static_cast<std::string::size_type>(UINT8_C(1)), 'x');
    result_hex_as_str.insert(result_hex_as_str.begin(), static_cast<std::string::size_type>(UINT8_C(1)), '0');

    const local_uintwide_t_small_unsigned_type u_from_string_oct(result_oct_as_str.c_str());
    const local_uintwide_t_small_unsigned_type u_from_string_dec(result_dec_as_str.c_str());
    const local_uintwide_t_small_unsigned_type u_from_string_hex(result_hex_as_str.c_str());

    const auto result_u_to_from_string_oct_is_ok = ((u_gen == u_from_string_oct) && (result_oct_as_chars.ec == std::errc()));
    const auto result_u_to_from_string_dec_is_ok = ((u_gen == u_from_string_dec) && (result_dec_as_chars.ec == std::errc()));
    const auto result_u_to_from_string_hex_is_ok = ((u_gen == u_from_string_hex) && (result_hex_as_chars.ec == std::errc()));

    result_is_ok = (result_u_to_from_string_oct_is_ok && result_is_ok);
    result_is_ok = (result_u_to_from_string_dec_is_ok && result_is_ok);
    result_is_ok = (result_u_to_from_string_hex_is_ok && result_is_ok);
  }

  for(auto   i = static_cast<unsigned>(UINT8_C(0));
             i < static_cast<unsigned>(loop_count_hi);
           ++i)
  {
    // Verify write to_chars() and read back from string of signed uintwide_t.
    // Use only base decimal.

    using to_chars_storage_array_dec_type =
      std::array<char, static_cast<std::size_t>(local_uintwide_t_small_signed_type::wr_string_max_buffer_size_dec())>;

    constexpr auto char_fill = '\0';

    to_chars_storage_array_dec_type arr_dec { }; arr_dec.fill(char_fill);

    auto n_gen = generate_wide_integer_value<local_uintwide_t_small_signed_type>(false);

    using std::to_chars;

    const auto result_dec_as_chars = to_chars(arr_dec.data(), arr_dec.data() + arr_dec.size(), n_gen, 10);

    static_cast<void>(result_dec_as_chars);

    const auto result_dec_as_str = std::string(arr_dec.data());

    const local_uintwide_t_small_signed_type n_from_string_dec(result_dec_as_str.c_str());

    const auto result_n_to_from_string_dec_is_ok = (n_gen == n_from_string_dec);

    result_is_ok = (result_n_to_from_string_dec_is_ok && result_is_ok);
  }
  #endif // __cpp_lib_to_chars

  #if !defined(WIDE_INTEGER_DISABLE_TO_STRING)
  for(auto   i = static_cast<unsigned>(UINT8_C(0));
             i < static_cast<unsigned>(loop_count_hi);
           ++i)
  {
    // Verify write to_string() and read back from string of unsigned uintwide_t.

    auto u_gen = generate_wide_integer_value<local_uintwide_t_small_unsigned_type>();

    using std::to_string;

    const auto str_u = to_string(u_gen);

    const local_uintwide_t_small_unsigned_type u_from_string(str_u.c_str());

    const auto result_u_to_from_string_is_ok = (u_gen == u_from_string);

    result_is_ok = (result_u_to_from_string_is_ok && result_is_ok);
  }

  for(auto   i = static_cast<unsigned>(UINT8_C(0));
             i < static_cast<unsigned>(loop_count_hi);
           ++i)
  {
    // Verify write to_string() and read back from string of signed uintwide_t.

    auto n_gen = generate_wide_integer_value<local_uintwide_t_small_signed_type>(false);

    using std::to_string;

    const auto str_n = to_string(n_gen);

    const local_uintwide_t_small_signed_type n_from_string(str_n.c_str());

    const auto result_n_to_from_string_is_ok = (n_gen == n_from_string);

    result_is_ok = (result_n_to_from_string_is_ok && result_is_ok);
  }

  {
    // Ensure that uintwide_t's function to_string (in namespace
    // math::wide_integer) does *not* conflict with the standard library's
    // std::to_string function name. Also ensure that ADL works properly
    // for uintwide_t's namespace-specific to_string function.

    using std::to_string;

    const auto u_gen = generate_wide_integer_value<local_uintwide_t_small_unsigned_type>();
    const auto n_gen = generate_wide_integer_value<local_uintwide_t_small_signed_type>(false);

    const auto str_u = to_string(u_gen);
    const auto str_n = to_string(n_gen);

    const auto u64 = static_cast<std::uint64_t>(UINT64_C(0xFFFFFFFF55555555));
    const auto ni  = static_cast<int>(INT8_C(42));

    const auto str_u64 = to_string(u64);
    const auto str_ni  = to_string(ni);

    const auto str2_u64 = std::to_string(u64);
    const auto str2_ni  = std::to_string(ni);

    const auto result_to_strings_are_ok = (   (!str_u.empty())
                                           && (!str_n.empty())
                                           && (!str_u64.empty())
                                           && (!str_ni.empty())
                                           && (!str2_u64.empty())
                                           && (!str2_ni.empty()));

    result_is_ok = (result_to_strings_are_ok && result_is_ok);
  }

  #if !defined(UINTWIDE_T_REDUCE_TEST_DEPTH)
  for(auto   i = static_cast<unsigned>(UINT8_C(0));
             i < static_cast<unsigned>(UINT32_C(32));
           ++i)
  {
    // Verify write to_string() and read back from string of
    // this test file's wide unsigned uintwide_t type.
    // Thereby we ensure that the to_string() function
    // will use dynamic allocation instead of stack allocation
    // in this particular test.

    using local_derived_uint_type = typename local_uint_backend_type::representation_type;

    using std::to_string;

    const auto u_gen = generate_wide_integer_value<local_derived_uint_type>();

    const auto str_u = to_string(u_gen);

    const local_derived_uint_type u_from_string(str_u.c_str());

    const auto result_u_to_from_string_is_ok = (u_gen == u_from_string);

    result_is_ok = (result_u_to_from_string_is_ok && result_is_ok);
  }
  #endif
  #endif // !WIDE_INTEGER_DISABLE_TO_STRING

  return result_is_ok;
}

auto test_import_bits() -> bool // NOLINT(readability-function-cognitive-complexity)
{
  eng_sgn.seed(time_point<typename eng_sgn_type::result_type>());
  eng_dig.seed(time_point<typename eng_dig_type::result_type>());

  using local_boost_small_uint_backend_type =
    boost::multiprecision::cpp_int_backend<local_edge_cases::local_digits2_small,
                                           local_edge_cases::local_digits2_small,
                                           boost::multiprecision::unsigned_magnitude,
                                           boost::multiprecision::unchecked,
                                           void>;

  using local_boost_small_uint_type =
    boost::multiprecision::number<local_boost_small_uint_backend_type,
                                  boost::multiprecision::et_off>;

  auto result_is_ok = true;

  static const std::array<bool, static_cast<std::size_t>(UINT8_C(2))> msv_options = { true, false };

  for(const auto& msv_first : msv_options) // NOLINT
  {
    for(auto   i = static_cast<unsigned>(UINT8_C(0));
               i < static_cast<unsigned>(loop_count_lo);
             ++i)
    {
      // Verify import_bits() and compare with Boost control value(s).
      // The input and output ranges have elements having the same widths.
      // Use the full bit width and representation length of uintwide_t.

      using local_representation_type = typename local_uintwide_t_small_unsigned_type::representation_type;
      using local_input_value_type    = typename local_representation_type::value_type;

      auto u_gen = generate_wide_integer_value<local_uintwide_t_small_unsigned_type>();

      local_representation_type bits(u_gen.crepresentation());

      if(msv_first)
      {
        std::reverse(bits.begin(), bits.end());
      }

      local_uintwide_t_small_unsigned_type val_uintwide_t { };
      local_boost_small_uint_type          val_boost      { };

      const auto oscillated_chunk_size =
        static_cast<unsigned>
        (
          static_cast<unsigned>(i % 2U) == 0U
            ? static_cast<unsigned>(std::numeric_limits<local_input_value_type>::digits)
            : 0U
        );

      static_cast<void>(import_bits(val_uintwide_t, bits.cbegin(), bits.cend(), oscillated_chunk_size, msv_first));
      static_cast<void>(import_bits(val_boost,      bits.cbegin(), bits.cend(), oscillated_chunk_size, msv_first)); // NOLINT

      using std::to_string;

      const auto str_uintwide_t = to_string(val_uintwide_t);
      const auto str_boost      = val_boost.str();

      const auto result_import_bits_is_ok = ((str_uintwide_t == str_boost) && (u_gen == val_uintwide_t));

      result_is_ok = (result_import_bits_is_ok && result_is_ok);
    }
  }

  for(const auto& msv_first : msv_options) // NOLINT
  {
    static const std::array<unsigned, static_cast<std::size_t>(UINT8_C(3))> bits_for_chunks = { 7U, 9U, 15U };

    for(const auto& chunk_size : bits_for_chunks)
    {
      for(auto   i = static_cast<unsigned>(UINT8_C(0));
                 i < static_cast<unsigned>(loop_count_lo);
               ++i)
      {
        // Verify import_bits() and compare with Boost control value(s).
        // The input and output ranges have elements having the same widths.
        // Use various input bit counts less than the result limb's width.
        // Use the full size of elements in the wide integer for the
        // distance of the input range.

        using local_representation_type = typename local_uintwide_t_small_unsigned_type::representation_type;

        auto u_gen = generate_wide_integer_value<local_uintwide_t_small_unsigned_type>();

        local_representation_type bits(u_gen.crepresentation());

        if(msv_first)
        {
          std::reverse(bits.begin(), bits.end());
        }

        local_uintwide_t_small_unsigned_type val_uintwide_t { };
        local_boost_small_uint_type          val_boost      { };

        using std::to_string;

        static_cast<void>(import_bits(val_uintwide_t, bits.cbegin(), bits.cend(), chunk_size, msv_first));
        static_cast<void>(import_bits(val_boost,      bits.cbegin(), bits.cend(), chunk_size, msv_first)); // NOLINT

        const auto str_uintwide_t = to_string(val_uintwide_t);
        const auto str_boost      =           val_boost.str();

        const auto result_import_bits_is_ok = (str_uintwide_t == str_boost);

        result_is_ok = (result_import_bits_is_ok && result_is_ok);
      }
    }
  }

  for(const auto& msv_first : msv_options) // NOLINT
  {
    for(auto   i = static_cast<unsigned>(UINT8_C(0));
               i < static_cast<unsigned>(loop_count_lo);
             ++i)
    {
      // Verify import_bits() and compare with Boost control value(s).
      // The input and output ranges have elements having different widths.
      // Use various input bit counts exceeding the result limb's width.
      // Use the full size of elements in the wide integer for the
      // distance of the input range.

      using local_representation_type = typename local_uintwide_t_small_unsigned_type::representation_type;

      using local_input_value_type = typename local_representation_type::value_type;

      auto u_gen = generate_wide_integer_value<local_uintwide_t_small_unsigned_type>();

      local_representation_type bits(u_gen.crepresentation());

      #if defined(WIDE_INTEGER_NAMESPACE)
      using local_input_double_width_value_type =
        typename WIDE_INTEGER_NAMESPACE::math::wide_integer::detail::uint_type_helper<static_cast<std::size_t>(std::numeric_limits<local_input_value_type>::digits * 2)>::exact_unsigned_type;
      #else
      using local_input_double_width_value_type =
        typename ::math::wide_integer::detail::uint_type_helper<static_cast<std::size_t>(std::numeric_limits<local_input_value_type>::digits * 2)>::exact_unsigned_type;
      #endif

      using local_double_width_input_array_type =
        std::array<local_input_double_width_value_type, local_uintwide_t_small_unsigned_type::number_of_limbs / 2U>;

      local_double_width_input_array_type bits_double_width;

      {
        using local_size_type = typename local_representation_type::size_type;

        auto index = static_cast<local_size_type>(UINT8_C(0));

        for(auto& elem : bits_double_width)
        {
          #if defined(WIDE_INTEGER_NAMESPACE)
          using WIDE_INTEGER_NAMESPACE::math::wide_integer::detail::make_large;
          #else
          using ::math::wide_integer::detail::make_large;
          #endif

          const auto index_plus_one =
            static_cast<local_size_type>
            (
              index + static_cast<local_size_type>(UINT8_C(1))
            );

          #if defined(WIDE_INTEGER_NAMESPACE)
          elem = make_large(*WIDE_INTEGER_NAMESPACE::math::wide_integer::detail::advance_and_point(bits.cbegin(), index),
                            *WIDE_INTEGER_NAMESPACE::math::wide_integer::detail::advance_and_point(bits.cbegin(), index_plus_one));
          #else
          elem = make_large(*::math::wide_integer::detail::advance_and_point(bits.cbegin(), index),
                            *::math::wide_integer::detail::advance_and_point(bits.cbegin(), index_plus_one));
          #endif

          index = static_cast<local_size_type>(index + static_cast<local_size_type>(UINT8_C(2)));
        }
      }

      local_uintwide_t_small_unsigned_type val_uintwide_t { };
      local_boost_small_uint_type          val_boost      { };

      using std::to_string;

      static_cast<void>(import_bits(val_uintwide_t, bits_double_width.cbegin(), bits_double_width.cend(), static_cast<unsigned>(std::numeric_limits<local_input_double_width_value_type>::digits), msv_first));
      static_cast<void>(import_bits(val_boost,      bits_double_width.cbegin(), bits_double_width.cend(), static_cast<unsigned>(std::numeric_limits<local_input_double_width_value_type>::digits), msv_first)); // NOLINT

      const auto str_uintwide_t = to_string(val_uintwide_t);
      const auto str_boost      =           val_boost.str();

      const auto result_import_bits_is_ok = (str_uintwide_t == str_boost);

      result_is_ok = (result_import_bits_is_ok && result_is_ok);
    }
  }

  for(const auto& msv_first : msv_options) // NOLINT
  {
    for(auto   i = static_cast<unsigned>(UINT8_C(0));
               i < static_cast<unsigned>(loop_count_lo);
             ++i)
    {
      // Verify import_bits() and compare with Boost control value(s).
      // Use various input bit counts exceeding the result limb's width.
      // Use only part of the size of elements in the wide integer for the
      // distance of the input range.

      using local_representation_type = typename local_uintwide_t_small_unsigned_type::representation_type;

      using local_input_value_type = typename local_representation_type::value_type;

      auto u_gen = generate_wide_integer_value<local_uintwide_t_small_unsigned_type>();

      using local_representation_less_wide_type =
        std::array<local_input_value_type, static_cast<std::size_t>(static_cast<std::size_t>(local_uintwide_t_small_unsigned_type::number_of_limbs) - 2U)>;

      local_representation_less_wide_type bits { };

      std::copy(u_gen.crepresentation().cbegin(),
                #if defined(WIDE_INTEGER_NAMESPACE)
                WIDE_INTEGER_NAMESPACE::math::wide_integer::detail::advance_and_point(u_gen.crepresentation().cbegin(), std::tuple_size<local_representation_less_wide_type>::value),
                #else
                ::math::wide_integer::detail::advance_and_point(u_gen.crepresentation().cbegin(), std::tuple_size<local_representation_less_wide_type>::value),
                #endif
                bits.begin());

      #if defined(WIDE_INTEGER_NAMESPACE)
      using local_input_double_width_value_type =
        typename WIDE_INTEGER_NAMESPACE::math::wide_integer::detail::uint_type_helper<static_cast<std::size_t>(std::numeric_limits<local_input_value_type>::digits * 2)>::exact_unsigned_type;
      #else
      using local_input_double_width_value_type =
        typename ::math::wide_integer::detail::uint_type_helper<static_cast<std::size_t>(std::numeric_limits<local_input_value_type>::digits * 2)>::exact_unsigned_type;
      #endif

      using local_double_width_less_wide_input_array_type =
        std::array<local_input_double_width_value_type, std::tuple_size<local_representation_less_wide_type>::value / 2U>;

      static_assert(std::tuple_size<local_double_width_less_wide_input_array_type>::value == static_cast<std::size_t>(static_cast<std::size_t>(static_cast<std::size_t>(local_uintwide_t_small_unsigned_type::number_of_limbs) / 2U) - 1U),
                    "Error: Type definition widths are not OK");

      local_double_width_less_wide_input_array_type bits_double_width;

      {
        using local_size_type = typename local_representation_type::size_type;

        auto index = static_cast<local_size_type>(UINT8_C(0));

        for(auto& elem : bits_double_width)
        {
          #if defined(WIDE_INTEGER_NAMESPACE)
          using WIDE_INTEGER_NAMESPACE::math::wide_integer::detail::make_large;
          #else
          using ::math::wide_integer::detail::make_large;
          #endif

          const auto index_plus_one =
            static_cast<local_size_type>
            (
              index + static_cast<local_size_type>(UINT8_C(1))
            );

          elem = make_large(bits[index], bits[index_plus_one]);

          index = static_cast<local_size_type>(index + static_cast<local_size_type>(UINT8_C(2)));
        }
      }

      local_uintwide_t_small_unsigned_type val_uintwide_t { };
      local_boost_small_uint_type          val_boost      { };

      using std::to_string;

      static_cast<void>(import_bits(val_uintwide_t, bits_double_width.cbegin(), bits_double_width.cend(), static_cast<unsigned>(std::numeric_limits<local_input_double_width_value_type>::digits), msv_first));
      static_cast<void>(import_bits(val_boost,      bits_double_width.cbegin(), bits_double_width.cend(), static_cast<unsigned>(std::numeric_limits<local_input_double_width_value_type>::digits), msv_first)); // NOLINT

      const auto str_uintwide_t = to_string(val_uintwide_t);
      const auto str_boost      =           val_boost.str();

      const auto result_import_bits_is_ok = (str_uintwide_t == str_boost);

      result_is_ok = (result_import_bits_is_ok && result_is_ok);
    }
  }

  {
    // Additional verification of import_bits().

    const std::array<std::uint32_t, 1U> bits_in = { static_cast<std::uint32_t>(UINT32_C(0x5555AAAA)) };

    using chunk_sizes_array_type = std::array<unsigned, static_cast<std::size_t>(UINT8_C(4))>;

    static const chunk_sizes_array_type various_chunk_sizes =
    {
      0U, 32U, 24U, 2U
    };

    static const std::array<std::uint32_t, std::tuple_size<chunk_sizes_array_type>::value> various_import_results =
    {
      static_cast<std::uint32_t>(UINT32_C(0x5555AAAA)),
      static_cast<std::uint32_t>(UINT32_C(0x5555AAAA)),
      static_cast<std::uint32_t>(UINT32_C(0x0055AAAA)),
      static_cast<std::uint32_t>(UINT32_C(2))
    };

    for(auto i = static_cast<std::size_t>(UINT8_C(0)); i < various_chunk_sizes.size(); ++i)
    {
      auto u = local_uintwide_t_small_unsigned_type { };

      static_cast<void>
      (
        import_bits(u, bits_in.cbegin(), bits_in.cend(), various_chunk_sizes[i]) // NOLINT(cppcoreguidelines-pro-bounds-constant-array-index)
      );

      const auto result_import_is_ok = (u == various_import_results[i]); // NOLINT(cppcoreguidelines-pro-bounds-constant-array-index)

      result_is_ok = (result_import_is_ok && result_is_ok);
    }
  }

  return result_is_ok;
}

auto test_export_bits() -> bool // NOLINT(readability-function-cognitive-complexity)
{
  eng_sgn.seed(time_point<typename eng_sgn_type::result_type>());
  eng_dig.seed(time_point<typename eng_dig_type::result_type>());

  using local_boost_small_uint_backend_type =
    boost::multiprecision::cpp_int_backend<local_edge_cases::local_digits2_small,
                                           local_edge_cases::local_digits2_small,
                                           boost::multiprecision::unsigned_magnitude,
                                           boost::multiprecision::unchecked,
                                           void>;

  using local_boost_small_uint_type =
    boost::multiprecision::number<local_boost_small_uint_backend_type,
                                  boost::multiprecision::et_off>;

  auto result_is_ok = true;

  static const std::array<bool, static_cast<std::size_t>(UINT8_C(2))> msv_options = { true, false };

  for(const auto& msv_first : msv_options) // NOLINT
  {
    for(auto   i = static_cast<unsigned>(UINT8_C(0));
               i < static_cast<unsigned>(loop_count_lo);
             ++i)
    {
      // Verify export_bits() and compare with Boost control value(s).
      // The input and output ranges have elements having the same widths.
      // Use the full bit width and representation length of uintwide_t.

      using local_representation_type = typename local_uintwide_t_small_unsigned_type::representation_type;

      using local_input_value_type = typename local_representation_type::value_type;

      using std::to_string;

            auto val_uintwide_t = generate_wide_integer_value<local_uintwide_t_small_unsigned_type>();
      const auto val_boost      = local_boost_small_uint_type(to_string(val_uintwide_t));

      using local_output_array_type =
        std::array<local_input_value_type, local_uintwide_t_small_unsigned_type::number_of_limbs>;

      using local_result_value_type = typename local_output_array_type::value_type;

      local_output_array_type bits_result_from_uintwide_t { };
      local_output_array_type bits_result_from_boost      { };

      static_cast<void>(export_bits(val_uintwide_t, bits_result_from_uintwide_t.begin(), static_cast<unsigned>(std::numeric_limits<local_result_value_type>::digits), msv_first));
      static_cast<void>(export_bits(val_boost,      bits_result_from_boost.begin(),      static_cast<unsigned>(std::numeric_limits<local_result_value_type>::digits), msv_first)); // NOLINT

      const auto result_export_bits_is_ok = std::equal(bits_result_from_uintwide_t.cbegin(),
                                                       bits_result_from_uintwide_t.cend(),
                                                       bits_result_from_boost.cbegin());

      result_is_ok = (result_export_bits_is_ok && result_is_ok);
    }
  }

  for(const auto& msv_first : msv_options) // NOLINT
  {
    static const std::array<unsigned, static_cast<std::size_t>(UINT8_C(3))> bits_for_chunks = { 7U, 9U, 15U };

    for(const auto& chunk_size : bits_for_chunks)
    {
      for(auto   i = static_cast<unsigned>(UINT8_C(0));
                 i < static_cast<unsigned>(loop_count_lo);
               ++i)
      {
        // Verify export_bits() and compare with Boost control value(s).
        // The input and output ranges have elements having the same widths.
        // Use various input bit counts less than the result limb's width.
        // Use the full size of elements in the wide integer for the
        // distance of the input range.

        using local_representation_type = typename local_uintwide_t_small_unsigned_type::representation_type;

        using local_input_value_type = typename local_representation_type::value_type;

        using std::to_string;

              auto val_uintwide_t = generate_wide_integer_value<local_uintwide_t_small_unsigned_type>();
        const auto val_boost      = local_boost_small_uint_type(to_string(val_uintwide_t));

        using local_output_vector_type = std::vector<local_input_value_type>;

        using local_result_value_type = typename local_output_vector_type::value_type;

        const auto output_distance_chunk_size_has_mod =
        (
          static_cast<int>
          (
            std::numeric_limits<local_uintwide_t_small_unsigned_type>::digits % static_cast<int>(chunk_size)
          ) != 0
        );

        const auto output_distance =
          static_cast<std::size_t>
          (
              static_cast<std::size_t>(std::numeric_limits<local_uintwide_t_small_unsigned_type>::digits / static_cast<int>(chunk_size))
            + static_cast<std::size_t>
              (
                output_distance_chunk_size_has_mod ? static_cast<std::size_t>(UINT8_C(1))
                                                   : static_cast<std::size_t>(UINT8_C(0))
              )
          );

        local_output_vector_type bits_result_from_uintwide_t(output_distance, static_cast<local_result_value_type>(UINT8_C(0)));
        local_output_vector_type bits_result_from_boost     (output_distance, static_cast<local_result_value_type>(UINT8_C(0)));

        static_cast<void>(export_bits(val_uintwide_t, bits_result_from_uintwide_t.begin(), chunk_size, msv_first));
        static_cast<void>(export_bits(val_boost,      bits_result_from_boost.begin(),      chunk_size, msv_first)); // NOLINT

        const auto result_export_bits_is_ok = std::equal(bits_result_from_uintwide_t.cbegin(),
                                                         bits_result_from_uintwide_t.cend(),
                                                         bits_result_from_boost.cbegin());

        result_is_ok = (result_export_bits_is_ok && result_is_ok);
      }
    }
  }

  for(const auto& msv_first : msv_options) // NOLINT
  {
    for(auto   i = static_cast<unsigned>(UINT8_C(0));
               i < static_cast<unsigned>(loop_count_lo);
             ++i)
    {
      // Verify export_bits() and compare with Boost control value(s).
      // The input and output ranges have elements having different widths.
      // Use various input bit counts exceeding the result limb's width.
      // Use the full size of elements in the wide integer for the
      // distance of the input range.

      using local_representation_type = typename local_uintwide_t_small_unsigned_type::representation_type;

      using local_input_value_type = typename local_representation_type::value_type;

      using std::to_string;

            auto val_uintwide_t = generate_wide_integer_value<local_uintwide_t_small_unsigned_type>();
      const auto val_boost      = local_boost_small_uint_type(to_string(val_uintwide_t));

      #if defined(WIDE_INTEGER_NAMESPACE)
      using local_result_double_width_value_type =
        typename WIDE_INTEGER_NAMESPACE::math::wide_integer::detail::uint_type_helper<static_cast<std::size_t>(std::numeric_limits<local_input_value_type>::digits * 2)>::exact_unsigned_type;
      #else
      using local_result_double_width_value_type =
        typename ::math::wide_integer::detail::uint_type_helper<static_cast<std::size_t>(std::numeric_limits<local_input_value_type>::digits * 2)>::exact_unsigned_type;
      #endif

      using local_double_width_output_array_type =
        std::array<local_result_double_width_value_type, local_uintwide_t_small_unsigned_type::number_of_limbs / 2U>;

      local_double_width_output_array_type bits_result_double_width_from_uintwide_t { };
      local_double_width_output_array_type bits_result_double_width_from_boost      { };

      static_cast<void>(export_bits(val_uintwide_t, bits_result_double_width_from_uintwide_t.begin(), static_cast<unsigned>(std::numeric_limits<local_result_double_width_value_type>::digits), msv_first));
      static_cast<void>(export_bits(val_boost,      bits_result_double_width_from_boost.begin(),      static_cast<unsigned>(std::numeric_limits<local_result_double_width_value_type>::digits), msv_first)); // NOLINT

      const auto result_export_bits_is_ok = std::equal(bits_result_double_width_from_uintwide_t.cbegin(),
                                                       bits_result_double_width_from_uintwide_t.cend(),
                                                       bits_result_double_width_from_boost.cbegin());

      result_is_ok = (result_export_bits_is_ok && result_is_ok);
    }
  }

  {
    // Note that export_bits uses the absolute value.
    // So test this feature by using negative one here.
    const auto val_uintwide_t = local_uintwide_t_small_signed_type(-1);
    const auto val_boost      = local_boost_small_uint_type(1);

    using local_result_type = std::uint32_t;

    auto result_one_uintwide_t = local_result_type { };
    auto result_one_boost      = local_result_type { };

    static_cast<void>(export_bits(val_uintwide_t, &result_one_uintwide_t, static_cast<unsigned>(std::numeric_limits<local_result_type>::digits)));
    static_cast<void>(export_bits(val_boost,      &result_one_boost,      static_cast<unsigned>(std::numeric_limits<local_result_type>::digits)));

    const auto result_is_one_and_compare_one_is_ok = (   (result_one_uintwide_t == result_one_boost)
                                                      && (result_one_uintwide_t == 1));

    result_is_ok = (result_is_one_and_compare_one_is_ok && result_is_ok);
  }

  return result_is_ok;
}

} // namespace test_uintwide_t_edge

// LCOV_EXCL_START
#if defined(WIDE_INTEGER_NAMESPACE)
auto WIDE_INTEGER_NAMESPACE::math::wide_integer::test_uintwide_t_edge_cases() -> bool
#else
auto ::math::wide_integer::test_uintwide_t_edge_cases() -> bool
#endif
{
  test_uintwide_t_edge::eng_sgn.seed(test_uintwide_t_edge::time_point<typename test_uintwide_t_edge::eng_sgn_type::result_type>());
  test_uintwide_t_edge::eng_dig.seed(test_uintwide_t_edge::time_point<typename test_uintwide_t_edge::eng_dig_type::result_type>());

  auto result_is_ok = true;

  #if !defined(UINTWIDE_T_REDUCE_TEST_DEPTH)
  result_is_ok = (test_uintwide_t_edge::test_various_edge_operations    () && result_is_ok);
  #endif
  // LCOV_EXCL_STOP
  result_is_ok = (test_uintwide_t_edge::test_various_ostream_ops            () && result_is_ok);
  result_is_ok = (test_uintwide_t_edge::test_various_roots_and_pow_etc      () && result_is_ok);
  result_is_ok = (test_uintwide_t_edge::test_small_prime_and_non_prime      () && result_is_ok);
  result_is_ok = (test_uintwide_t_edge::test_some_gcd_and_equal_left_right  () && result_is_ok);
  result_is_ok = (test_uintwide_t_edge::test_various_isolated_edge_cases    () && result_is_ok);
  result_is_ok = (test_uintwide_t_edge::test_to_chars_and_to_string         () && result_is_ok);
  result_is_ok = (test_uintwide_t_edge::test_import_bits                    () && result_is_ok);
  result_is_ok = (test_uintwide_t_edge::test_export_bits                    () && result_is_ok);

  return result_is_ok;
}

// LCOV_EXCL_START
auto test_uintwide_t_edge::zero_as_limb() -> const typename test_uintwide_t_edge::local_uintwide_t_small_unsigned_type::limb_type&
{
  using local_limb_type = typename local_uintwide_t_small_unsigned_type::limb_type;

  static const auto local_zero_limb = static_cast<local_limb_type>(UINT8_C(0));

  return local_zero_limb;
}
// LCOV_EXCL_STOP

auto test_uintwide_t_edge::zero_as_small_unsigned_type() -> const test_uintwide_t_edge::local_uintwide_t_small_unsigned_type& // LCOV_EXCL_LINE
{
  using local_limb_type = typename local_uintwide_t_small_unsigned_type::limb_type;

  static const auto local_zero_as_small_unsigned_type =
    local_uintwide_t_small_unsigned_type
    (
      static_cast<local_limb_type>(UINT8_C(0))
    );

  return local_zero_as_small_unsigned_type;
}

auto test_uintwide_t_edge::one_as_small_unsigned_type() -> const test_uintwide_t_edge::local_uintwide_t_small_unsigned_type&
{
  using local_limb_type = typename local_uintwide_t_small_unsigned_type::limb_type;

  static const auto local_one_as_small_signed_type =
    local_uintwide_t_small_unsigned_type
    (
      static_cast<std::make_signed_t<local_limb_type>>(UINT8_C(1))
    );

  return local_one_as_small_signed_type;
}

auto test_uintwide_t_edge::m_one_as_small_signed_type() -> const test_uintwide_t_edge::local_uintwide_t_small_signed_type&
{
  using local_limb_type = typename local_uintwide_t_small_signed_type::limb_type;

  static const auto local_m_one_as_small_signed_type =
    local_uintwide_t_small_signed_type
    (
      static_cast<std::make_signed_t<local_limb_type>>(INT8_C(-1))
    );

  return local_m_one_as_small_signed_type;
}

#if (BOOST_VERSION < 108000)
#if (defined(__clang__) && (__clang_major__ > 9)) && !defined(__APPLE__)
#pragma GCC diagnostic pop
#endif
#endif

#if (defined(__GNUC__) && !defined(__clang__) && (__GNUC__ >= 12))
#pragma GCC diagnostic pop
#endif

#if defined(__GNUC__)
#if (BOOST_VERSION < 108000)
#pragma GCC diagnostic pop
#pragma GCC diagnostic pop
#pragma GCC diagnostic pop
#else
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
