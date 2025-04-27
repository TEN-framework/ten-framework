///////////////////////////////////////////////////////////////////////////////
//  Copyright Christopher Kormanyos 2019 - 2023.
//  Distributed under the Boost Software License,
//  Version 1.0. (See accompanying file LICENSE_1_0.txt
//  or copy at http://www.boost.org/LICENSE_1_0.txt)
//

#include <iostream>

#include <cassert>
#include <sstream>

#include <math/wide_integer/uintwide_t.h>
#include <test/test_uintwide_t.h>

namespace from_issue_362
{
  auto test_uintwide_t_spot_values_from_issue_362() -> bool
  {
    #if defined(WIDE_INTEGER_NAMESPACE)
    using local_uint512_t = WIDE_INTEGER_NAMESPACE::math::wide_integer::uint512_t;
    #else
    using local_uint512_t = ::math::wide_integer::uint512_t;
    #endif

    const auto a = local_uint512_t(static_cast<std::uint32_t>(UINT32_C(11111)));
    const auto b = local_uint512_t(static_cast<std::uint32_t>(UINT32_C(22222)));
    const auto c = a * b;

    auto result_is_ok = (c == local_uint512_t(static_cast<std::uint32_t>(UINT32_C(246908642))));

    return result_is_ok;
  }
} // namespace from_issue_362

namespace from_issue_342
{
  auto test_uintwide_t_spot_values_from_issue_342_pos() -> bool
  {
    // See also: https://github.com/ckormanyos/wide-integer/issues/342

    #if defined(WIDE_INTEGER_NAMESPACE)
    using local_uint128_t = WIDE_INTEGER_NAMESPACE::math::wide_integer::uint128_t;
    #else
    using local_uint128_t = ::math::wide_integer::uint128_t;
    #endif

    auto result_is_ok = true;

    {
      const local_uint128_t a = 11;
      const local_uint128_t b =  3;

      const auto r_pp = divmod(+a, +b);

      const auto result_pp_is_ok = ((r_pp.first == +3) && (r_pp.second == +2));

      result_is_ok = (result_pp_is_ok && result_is_ok);
    }

    {
      const local_uint128_t a = 12;
      const local_uint128_t b =  3;

      const auto r_pp = divmod(+a, +b);

      const auto result_divmod_is_ok = ((r_pp.first == +4) && (r_pp.second == 0));

      result_is_ok = (result_divmod_is_ok && result_is_ok);
    }

    return result_is_ok;
  }

  auto test_uintwide_t_spot_values_from_issue_342_mix() -> bool
  {
    // See also: https://github.com/ckormanyos/wide-integer/issues/342

    #if defined(WIDE_INTEGER_NAMESPACE)
    using local_int128_t = WIDE_INTEGER_NAMESPACE::math::wide_integer::int128_t;
    #else
    using local_int128_t = ::math::wide_integer::int128_t;
    #endif

    auto result_is_ok = true;

    {
      const local_int128_t a = 17;
      const local_int128_t b =  4;

      const auto r_pp = divmod(+a, +b);
      const auto r_pm = divmod(+a, -b);
      const auto r_mp = divmod(-a, +b);
      const auto r_mm = divmod(-a, -b);

      const auto result_pp_is_ok = ((r_pp.first == +4) && (r_pp.second == +1));
      const auto result_pm_is_ok = ((r_pm.first == -5) && (r_pm.second == -3));
      const auto result_mp_is_ok = ((r_mp.first == -5) && (r_mp.second == +3));
      const auto result_mm_is_ok = ((r_mm.first == +4) && (r_mm.second == -1));

      const auto result_divmod_is_ok =
      (
           result_pp_is_ok
        && result_pm_is_ok
        && result_mp_is_ok
        && result_mm_is_ok
      );

      result_is_ok = (result_divmod_is_ok && result_is_ok);
    }

    {
      const local_int128_t a = 12;
      const local_int128_t b =  3;

      const auto r_pp = divmod(+a, +b);
      const auto r_pm = divmod(+a, -b);
      const auto r_mp = divmod(-a, +b);
      const auto r_mm = divmod(-a, -b);

      const auto result_pp_is_ok = ((r_pp.first == +4) && (r_pp.second == 0));
      const auto result_pm_is_ok = ((r_pm.first == -4) && (r_pm.second == 0));
      const auto result_mp_is_ok = ((r_mp.first == -4) && (r_mp.second == 0));
      const auto result_mm_is_ok = ((r_mm.first == +4) && (r_mm.second == 0));

      const auto result_divmod_is_ok =
      (
           result_pp_is_ok
        && result_pm_is_ok
        && result_mp_is_ok
        && result_mm_is_ok
      );

      result_is_ok = (result_divmod_is_ok && result_is_ok);
    }

    {
      const local_int128_t a =  32;
      const local_int128_t b = 115;

      const auto r_pp = divmod(+a, +b);
      const auto r_pm = divmod(+a, -b);
      const auto r_mp = divmod(-a, +b);
      const auto r_mm = divmod(-a, -b);

      const auto result_pp_is_ok = ((r_pp.first == +0) && (r_pp.second == +32));
      const auto result_pm_is_ok = ((r_pm.first == -1) && (r_pm.second == -83));
      const auto result_mp_is_ok = ((r_mp.first == -1) && (r_mp.second == +83));
      const auto result_mm_is_ok = ((r_mm.first == +0) && (r_mm.second == -32));

      const auto result_divmod_is_ok =
      (
           result_pp_is_ok
        && result_pm_is_ok
        && result_mp_is_ok
        && result_mm_is_ok
      );

      result_is_ok = (result_divmod_is_ok && result_is_ok);
    }

    return result_is_ok;
  }
} // namespace from_issue_342

namespace from_issue_339
{
  // See also: https://github.com/ckormanyos/wide-integer/issues/339

  #if defined(WIDE_INTEGER_NAMESPACE)
  using uint2048 = WIDE_INTEGER_NAMESPACE::math::wide_integer::uint2048_t;
  using uint4096 = WIDE_INTEGER_NAMESPACE::math::wide_integer::uint4096_t;
  using sint2048 = WIDE_INTEGER_NAMESPACE::math::wide_integer::uintwide_t<uint2048::my_width2, uint2048::limb_type, void, true>;
  #else
  using uint2048 = ::math::wide_integer::uint2048_t;
  using uint4096 = ::math::wide_integer::uint4096_t;
  using sint2048 = ::math::wide_integer::uintwide_t<uint2048::my_width2, typename uint2048::limb_type, void, true>;
  #endif

  auto modInverse(uint2048 A, uint2048 M) -> uint2048 // NOLINT(readability-identifier-naming)
  {
    uint4096 m0 = M;
    uint4096 y  = 0;
    uint4096 x  = 1;

    if(M == 1)
    {
      return 0; // LCOV_EXCL_LINE
    }

    while (A > 1)
    {
      uint4096 q = A / M;
      uint4096 t = M;

      M = A % M;
      A = t;
      t = y;

      y = x - q * y;
      x = t;
    }

    if (x < 0)
    {
      x += m0; // LCOV_EXCL_LINE
    }

    return x;
  }

  auto test_uintwide_t_spot_values_from_issue_339_underflow_2048_4096() -> bool
  {
    const auto mod_inv_unsigned = modInverse(uint2048(59), uint2048(164));

    const auto result_unsigned_is_ok =
      (
        mod_inv_unsigned == uint2048
                            (
                              "1044388881413152506691752710716624382579964249047383780384233483283953907971557456848826811934997558"
                              "3408901067144392628379875734381857936072632360878513652779459569765437099983403615901343837183144280"
                              "7001185594622637631883939771274567233468434458661749680790870580370407128404874011860911446797778359"
                              "8029006686938976881787785946905630190260940599579453432823469303026696443059025015972399867714215541"
                              "6938355598852914863182379144344967340878118726394964751001890413490084170616750936683338505510329720"
                              "8826955076998361636941193301521379682583718809183365675122131849284636812555022599830041234478486259"
                              "5674492194617023806505913245610825731835380087608622102834270197698202313169017678006675195485079921"
                              "6364193702853751247840149071591354599827905133996115517942711068311340905842728842797915548497829543"
                              "2353451706522326906139490598769300212296339568778287894844061600741294567491982305057164237715481632"
                              "1380631045902916136926708342856440730447899971901781465763473223850267253059899795996090799469201774"
                              "6248177184498674556592501783290704731194331655508075682218465717463732968849128195203174570024409266"
                              "1691087414838507841192980452298185733897764810312608590300130241346718972667321649151113160292078173"
                              "8033436090243804708340403154190311"
                            )
       );

    const auto mod_inv_signed = static_cast<sint2048>(mod_inv_unsigned);

    const auto result_signed_is_ok = (mod_inv_signed == -25);

    const auto result_is_ok = (result_unsigned_is_ok && result_signed_is_ok);

    return result_is_ok;
  }
} // namespace from_issue_339

namespace from_issue_316
{
  // See also: https://github.com/ckormanyos/wide-integer/issues/316

  using import_export_array_type = std::array<std::uint8_t, static_cast<std::size_t>(128U)>; // NOLINT(cppcoreguidelines-avoid-magic-numbers,readability-magic-numbers)

  constexpr import_export_array_type bin_128_source_of_bits_imported =
  {
    142, 215,  17, 233, 75,    7, 202,  91,
    88,   53, 153, 106,  94, 112, 136,  40,
    229,   3, 176, 116,  42, 179,  23, 109,
    103,  70,  57, 154, 157, 110, 148,  87,
     86,  78, 175,  99,   6, 111,  16, 103,
    142,  61, 253, 224,  39,  52, 137, 252,
     56, 116, 147,  71, 168,  16, 155, 245,
    197,  97,  57,  69, 226,  13, 239, 164,
     40, 228, 250, 130, 128, 186, 150,   3,
     64,  81, 241, 165,  43, 136,  99,  79,
    124, 188,  50,  46, 152, 197, 205, 204,
    103, 254,  61, 143, 94,  31,    6,  98,
    165,  16, 223, 175,  30,  87, 156, 176,
    232,  56, 179,  56, 184, 220, 100, 141,
    212, 201,  55, 246, 199, 117,  28, 154,
     51, 140,   5,  95, 102, 187, 133, 248
  };

  auto test_uintwide_t_spot_values_from_issue_316_import_export_original() -> bool
  {
    // See also: https://github.com/ckormanyos/wide-integer/issues/316

    #if defined(WIDE_INTEGER_NAMESPACE)
    using local_uint1024_t = WIDE_INTEGER_NAMESPACE::math::wide_integer::uint1024_t;
    #else
    using local_uint1024_t = ::math::wide_integer::uint1024_t;
    #endif

    auto result_is_ok = true;

    auto big_int = local_uint1024_t { };

    import_bits(big_int,
                bin_128_source_of_bits_imported.cbegin(),
                bin_128_source_of_bits_imported.cend());

    constexpr auto j = static_cast<int>(INT8_C(50));

    const auto big_int_50 = big_int + j;

    import_export_array_type out;

    for(auto i = static_cast<int>(INT8_C(0)); i < j; ++i)
    {
      // Fill the output with erroneous values.
      out.fill(static_cast<uint8_t>(UINT8_C(0x55)));

      ++big_int;

      static_cast<void>
      (
        export_bits
        (
          big_int,
          out.begin(),
          static_cast<unsigned int>
          (
            std::numeric_limits<typename import_export_array_type::value_type>::digits
          )
        )
      );
    }

    const auto result_increment_and_export_is_ok = (big_int == big_int_50);

    import_bits(big_int, out.cbegin(), out.cend());

    const auto result_increment_export_and_import_is_ok = (big_int == big_int_50);

    result_is_ok = (   result_increment_and_export_is_ok
                    && result_increment_export_and_import_is_ok
                    && result_is_ok);

    return result_is_ok;
  }

  auto test_uintwide_t_spot_values_from_issue_316_import_export_extended() -> bool
  {
    // See also: https://github.com/ckormanyos/wide-integer/issues/316

    import_export_array_type bin_128_made_from_bits_exported;

    #if defined(WIDE_INTEGER_NAMESPACE)
    using local_uint1024_t = WIDE_INTEGER_NAMESPACE::math::wide_integer::uint1024_t;
    #else
    using local_uint1024_t = ::math::wide_integer::uint1024_t;
    #endif

    local_uint1024_t val_made_from_bits_imported;

    auto result_is_ok = true;

    import_bits(val_made_from_bits_imported,
                bin_128_source_of_bits_imported.cbegin(),
                bin_128_source_of_bits_imported.cend());

    static_cast<void>
    (
      export_bits
      (
        val_made_from_bits_imported,
        bin_128_made_from_bits_exported.begin(),
        static_cast<unsigned int>
        (
          std::numeric_limits<typename import_export_array_type::value_type>::digits
        )
      )
    );

    const auto result_import_and_export_same_is_ok =
      std::equal(bin_128_source_of_bits_imported.cbegin(),
                 bin_128_source_of_bits_imported.cend(),
                 bin_128_made_from_bits_exported.cbegin());

    result_is_ok = (result_import_and_export_same_is_ok && result_is_ok);

    return result_is_ok;
  }

} // namespace from_issue_316

namespace from_issue_266
{
  auto test_uintwide_t_spot_values_from_issue_266_inc() -> bool
  {
    // See also: https://github.com/ckormanyos/wide-integer/issues/266

    #if defined(WIDE_INTEGER_NAMESPACE)
    using local_uint128_t = WIDE_INTEGER_NAMESPACE::math::wide_integer::uint128_t;
    #else
    using local_uint128_t = ::math::wide_integer::uint128_t;
    #endif

    WIDE_INTEGER_CONSTEXPR local_uint128_t inc_value  ("0x0000000000000001FFFFFFFFFFFFFFFF");
    WIDE_INTEGER_CONSTEXPR local_uint128_t inc_value_p(++(local_uint128_t(inc_value)));

    const auto result_is_ok = (inc_value_p == local_uint128_t("0x00000000000000020000000000000000"));

    #if(WIDE_INTEGER_CONSTEXPR_IS_COMPILE_TIME_CONST == 1)
    static_assert(inc_value_p == local_uint128_t("0x00000000000000020000000000000000"), "Error: Incrementing 128-bit type is not OK");
    #endif

    return result_is_ok;
  }

  auto test_uintwide_t_spot_values_from_issue_266_dec() -> bool
  {
    // See also: https://github.com/ckormanyos/wide-integer/issues/266

    #if defined(WIDE_INTEGER_NAMESPACE)
    using local_uint128_t = WIDE_INTEGER_NAMESPACE::math::wide_integer::uint128_t;
    #else
    using local_uint128_t = ::math::wide_integer::uint128_t;
    #endif

    WIDE_INTEGER_CONSTEXPR local_uint128_t dec_value  ("0x00000000000000020000000000000000");
    WIDE_INTEGER_CONSTEXPR local_uint128_t dec_value_d(--(local_uint128_t(dec_value)));

    const auto result_is_ok = (dec_value_d == local_uint128_t("0x0000000000000001FFFFFFFFFFFFFFFF"));

    #if(WIDE_INTEGER_CONSTEXPR_IS_COMPILE_TIME_CONST == 1)
    static_assert(dec_value_d == local_uint128_t("0x0000000000000001FFFFFFFFFFFFFFFF"), "Error: Decrementing 128-bit type is not OK");
    #endif

    return result_is_ok;
  }
} // namespace from_issue_266

namespace from_issue_234
{
  // See also https://github.com/ckormanyos/wide-integer/issues/234#issuecomment-1052960210
  #if defined(WIDE_INTEGER_NAMESPACE)
  using uint80  = WIDE_INTEGER_NAMESPACE::math::wide_integer::uintwide_t<static_cast<WIDE_INTEGER_NAMESPACE::math::wide_integer::size_t>(UINT32_C( 80)), std::uint16_t>;
  using uint512 = WIDE_INTEGER_NAMESPACE::math::wide_integer::uintwide_t<static_cast<WIDE_INTEGER_NAMESPACE::math::wide_integer::size_t>(UINT32_C(512)), std::uint32_t>;
  #else
  using uint80  = ::math::wide_integer::uintwide_t<static_cast<math::wide_integer::size_t>(UINT32_C( 80)), std::uint16_t>;
  using uint512 = ::math::wide_integer::uintwide_t<static_cast<math::wide_integer::size_t>(UINT32_C(512)), std::uint32_t>;
  #endif

  WIDE_INTEGER_CONSTEXPR auto convert_to_uint80(const uint512& value) -> uint80
  {
    #if defined(WIDE_INTEGER_NAMESPACE)
    using WIDE_INTEGER_NAMESPACE::math::wide_integer::detail::make_lo;
    using WIDE_INTEGER_NAMESPACE::math::wide_integer::detail::make_hi;
    #else
    using ::math::wide_integer::detail::make_lo;
    using ::math::wide_integer::detail::make_hi;
    #endif

    static_assert(std::numeric_limits<typename uint80::limb_type>::digits * 2 == std::numeric_limits<typename uint512::limb_type>::digits,
                  "Error: Wrong input/output limb types for this conversion");

    using local_value_type = typename uint80::representation_type::value_type;

    return
      uint80::from_rep
      (
        {
          #if defined(WIDE_INTEGER_NAMESPACE)
          make_lo<local_value_type>(*WIDE_INTEGER_NAMESPACE::math::wide_integer::detail::advance_and_point(value.crepresentation().cbegin(), 0U)),
          make_hi<local_value_type>(*WIDE_INTEGER_NAMESPACE::math::wide_integer::detail::advance_and_point(value.crepresentation().cbegin(), 0U)),
          make_lo<local_value_type>(*WIDE_INTEGER_NAMESPACE::math::wide_integer::detail::advance_and_point(value.crepresentation().cbegin(), 1U)),
          make_hi<local_value_type>(*WIDE_INTEGER_NAMESPACE::math::wide_integer::detail::advance_and_point(value.crepresentation().cbegin(), 1U)),
          make_lo<local_value_type>(*WIDE_INTEGER_NAMESPACE::math::wide_integer::detail::advance_and_point(value.crepresentation().cbegin(), 2U))
          #else
          make_lo<local_value_type>(*::math::wide_integer::detail::advance_and_point(value.crepresentation().cbegin(), 0U)),
          make_hi<local_value_type>(*::math::wide_integer::detail::advance_and_point(value.crepresentation().cbegin(), 0U)),
          make_lo<local_value_type>(*::math::wide_integer::detail::advance_and_point(value.crepresentation().cbegin(), 1U)),
          make_hi<local_value_type>(*::math::wide_integer::detail::advance_and_point(value.crepresentation().cbegin(), 1U)),
          make_lo<local_value_type>(*::math::wide_integer::detail::advance_and_point(value.crepresentation().cbegin(), 2U))
          #endif
        }
      );
  }

  WIDE_INTEGER_CONSTEXPR auto convert_to_uint512(const uint80& value) -> uint512
  {
    #if defined(WIDE_INTEGER_NAMESPACE)
    using WIDE_INTEGER_NAMESPACE::math::wide_integer::detail::make_large;
    #else
    using ::math::wide_integer::detail::make_large;
    #endif

    static_assert(std::numeric_limits<typename uint80::limb_type>::digits * 2 == std::numeric_limits<typename uint512::limb_type>::digits,
                  "Error: Wrong input/output limb types for this conversion");

    using local_value_type = typename uint80::representation_type::value_type;

    return
      uint512::from_rep
      (
        {
          #if defined(WIDE_INTEGER_NAMESPACE)
          make_large(*WIDE_INTEGER_NAMESPACE::math::wide_integer::detail::advance_and_point(value.crepresentation().cbegin(), 0U),
                     *WIDE_INTEGER_NAMESPACE::math::wide_integer::detail::advance_and_point(value.crepresentation().cbegin(), 1U)),
          make_large(*WIDE_INTEGER_NAMESPACE::math::wide_integer::detail::advance_and_point(value.crepresentation().cbegin(), 2U),
                     *WIDE_INTEGER_NAMESPACE::math::wide_integer::detail::advance_and_point(value.crepresentation().cbegin(), 3U)),
          make_large(*WIDE_INTEGER_NAMESPACE::math::wide_integer::detail::advance_and_point(value.crepresentation().cbegin(), 4U),
          #else
          make_large(*::math::wide_integer::detail::advance_and_point(value.crepresentation().cbegin(), 0U),
                     *::math::wide_integer::detail::advance_and_point(value.crepresentation().cbegin(), 1U)),
          make_large(*::math::wide_integer::detail::advance_and_point(value.crepresentation().cbegin(), 2U),
                     *::math::wide_integer::detail::advance_and_point(value.crepresentation().cbegin(), 3U)),
          make_large(*::math::wide_integer::detail::advance_and_point(value.crepresentation().cbegin(), 4U),
          #endif
                     static_cast<local_value_type>(0U))
        }
      );
  }
} // namespace from_issue_234

namespace from_issue_145
{
  template<typename UnknownIntegerType>
  auto test_uintwide_t_spot_values_from_issue_145(const UnknownIntegerType& x) -> bool
  {
    // See also https://github.com/ckormanyos/wide-integer/issues/145#issuecomment-1006374713

    using local_unknown_integer_type = UnknownIntegerType;

    bool local_result_is_ok = true;

    #if (defined(__clang__) && (defined(__clang_major__) && (__clang_major__ > 6)))
    #pragma GCC diagnostic push
    #pragma GCC diagnostic ignored "-Wself-assign-overloaded"
    #endif

    {
      local_unknown_integer_type a = x; a += a; // NOLINT(clang-diagnostic-self-assign-overloaded)

      local_result_is_ok = ((a == (2U * x)) && local_result_is_ok);
    }

    {
      local_unknown_integer_type a = x; a -= a; // NOLINT(clang-diagnostic-self-assign-overloaded)

      local_result_is_ok = ((a == 0U) && local_result_is_ok);
    }

    {
      local_unknown_integer_type a = x; a /= a; // NOLINT(clang-diagnostic-self-assign-overloaded)

      local_result_is_ok = ((a == 1U) && local_result_is_ok);
    }

    #if (defined(__clang__) && (defined(__clang_major__) && (__clang_major__ > 6)))
    #pragma GCC diagnostic pop
    #endif

    return local_result_is_ok;
  }

} // namespace from_issue_145

namespace from_pull_request_130
{
  template<typename UnknownIntegerType>
  WIDE_INTEGER_CONSTEXPR auto test_uintwide_t_spot_values_from_pull_request_130() -> bool
  {
    // See also https://github.com/ckormanyos/wide-integer/pull/130

    using local_unknown_integer_type = UnknownIntegerType;

    using limits = std::numeric_limits<local_unknown_integer_type>;

    WIDE_INTEGER_CONSTEXPR auto expected
    {
      -1 - limits::max()
    };

    WIDE_INTEGER_CONSTEXPR auto actual
    {
      limits::lowest()
    };

    WIDE_INTEGER_CONSTEXPR bool b_ok = (expected == actual);

    return b_ok;
  }
} // namespace from_pull_request_130

namespace exercise_bad_string_input
{
  auto test_uintwide_t_spot_values_exercise_bad_string_input() -> bool
  {
    #if defined(WIDE_INTEGER_NAMESPACE)
    using local_uint128_t = WIDE_INTEGER_NAMESPACE::math::wide_integer::uint128_t;
    #else
    using local_uint128_t = ::math::wide_integer::uint128_t;
    #endif

    WIDE_INTEGER_CONSTEXPR local_uint128_t u("bad-string-input");

    const bool result_bad_string_input_is_ok = (u == (std::numeric_limits<local_uint128_t>::max)());

    #if(WIDE_INTEGER_CONSTEXPR_IS_COMPILE_TIME_CONST == 1)
    static_assert(u == (std::numeric_limits<local_uint128_t>::max)(), "Error: Reaction to bad string input is not OK");
    #endif

    return result_bad_string_input_is_ok;
  }
} // namespace exercise_bad_string_input

namespace exercise_pow_zero_one_two
{
  auto test_uintwide_t_spot_values_exercise_pow_zero_one_two() -> bool
  {
    #if defined(WIDE_INTEGER_NAMESPACE)
    using local_uint128_t = WIDE_INTEGER_NAMESPACE::math::wide_integer::uint128_t;
    #else
    using local_uint128_t = ::math::wide_integer::uint128_t;
    #endif

    WIDE_INTEGER_CONSTEXPR local_uint128_t u(UINT64_C(9999999978787878));

    using std::pow;

    WIDE_INTEGER_CONSTEXPR local_uint128_t u0 = pow(u, 0);
    WIDE_INTEGER_CONSTEXPR local_uint128_t u1 = pow(u, 1);
    WIDE_INTEGER_CONSTEXPR local_uint128_t u2 = pow(u, 2);

    const bool result_pow_is_ok =
      (
           (u0 == 1)
        && (u1 == u)
        && (u2 == u * u)
      );

    #if(WIDE_INTEGER_CONSTEXPR_IS_COMPILE_TIME_CONST == 1)
    static_assert(u0 == 1,     "Error: Power of zero is not OK");
    static_assert(u1 == u,     "Error: Power of one  is not OK");
    static_assert(u2 == u * u, "Error: Power of two  is not OK");
    #endif

    return result_pow_is_ok;
  }
} // namespace exercise_pow_zero_one_two

namespace exercise_octal
{
  auto test_uintwide_t_spot_values_exercise_octal() -> bool
  {
    #if defined(WIDE_INTEGER_NAMESPACE)
    using local_uint128_t = WIDE_INTEGER_NAMESPACE::math::wide_integer::uint128_t;
    #else
    using local_uint128_t = ::math::wide_integer::uint128_t;
    #endif

    WIDE_INTEGER_CONSTEXPR local_uint128_t u_dec("100000000000000000000777772222211111");
    WIDE_INTEGER_CONSTEXPR local_uint128_t u_oct("0464114134543515404256122464446501262047");

    auto result_conversion_is_ok = (u_dec == u_oct);

    #if(WIDE_INTEGER_CONSTEXPR_IS_COMPILE_TIME_CONST == 1)
    static_assert(u_dec == u_oct, "Error: Conversion decimal to octal is not OK");
    #endif

    std::stringstream strm;

    strm << std::showbase << std::oct << u_oct;

    result_conversion_is_ok = ((strm.str() == "0464114134543515404256122464446501262047") && result_conversion_is_ok);

    return result_conversion_is_ok;
  }
} // namespace exercise_octal

namespace local_test_spot_values
{
  auto test() -> bool;
} // namespace local_test_spot_values

auto local_test_spot_values::test() -> bool // NOLINT(readability-function-cognitive-complexity)
{
  auto result_is_ok = true;

  {
    // See also: https://github.com/ckormanyos/wide-integer/issues/362

    result_is_ok = (from_issue_362::test_uintwide_t_spot_values_from_issue_362() && result_is_ok);
  }

  {
    // See also: https://github.com/ckormanyos/wide-integer/issues/342

    const auto result_from_issue_342_is_ok =
      (   from_issue_342::test_uintwide_t_spot_values_from_issue_342_pos()
       && from_issue_342::test_uintwide_t_spot_values_from_issue_342_mix());

    result_is_ok = (result_from_issue_342_is_ok && result_is_ok);
  }

  {
    const auto result_from_issue_339_is_ok =
      from_issue_339::test_uintwide_t_spot_values_from_issue_339_underflow_2048_4096();

    result_is_ok = (result_from_issue_339_is_ok && result_is_ok);
  }

  {
    result_is_ok = (exercise_bad_string_input::test_uintwide_t_spot_values_exercise_bad_string_input() && result_is_ok);
  }

  {
    result_is_ok = (exercise_pow_zero_one_two::test_uintwide_t_spot_values_exercise_pow_zero_one_two() && result_is_ok);
  }

  {
    result_is_ok = (exercise_octal::test_uintwide_t_spot_values_exercise_octal() && result_is_ok);
  }

  {
    // See also: https://github.com/ckormanyos/wide-integer/issues/316

    result_is_ok = (from_issue_316::test_uintwide_t_spot_values_from_issue_316_import_export_original() && result_is_ok);
    result_is_ok = (from_issue_316::test_uintwide_t_spot_values_from_issue_316_import_export_extended() && result_is_ok);
  }

  {
    // See also: https://github.com/ckormanyos/wide-integer/issues/266

    result_is_ok = (from_issue_266::test_uintwide_t_spot_values_from_issue_266_inc() && result_is_ok);
    result_is_ok = (from_issue_266::test_uintwide_t_spot_values_from_issue_266_dec() && result_is_ok);
  }

  {
    // See also: https://github.com/ckormanyos/wide-integer/issues/234#issuecomment-1053733496

    #if defined(WIDE_INTEGER_NAMESPACE)
    using local_uint512_t = WIDE_INTEGER_NAMESPACE::math::wide_integer::uint512_t;
    using local_int512_t  = WIDE_INTEGER_NAMESPACE::math::wide_integer::int512_t;
    #else
    using local_uint512_t = ::math::wide_integer::uint512_t;
    using local_int512_t  = ::math::wide_integer::int512_t;
    #endif

    // BitOr[2^111, 31337]
    // 2596148429267413814265248164641385

    local_int512_t value = 1;
    std::uint32_t to_shift = 111;   // NOLINT(cppcoreguidelines-avoid-magic-numbers,readability-magic-numbers)
    local_uint512_t value2 = 31337; // NOLINT(cppcoreguidelines-avoid-magic-numbers,readability-magic-numbers)

    local_int512_t shift = (value << to_shift);
    value = shift | static_cast<local_int512_t>(value2); // ::math::wide_integer::int512_t | ::math::wide_integer::uint512_t

    result_is_ok = ((value == local_int512_t("2596148429267413814265248164641385")) && result_is_ok);
  }

  {
    // See also https://github.com/ckormanyos/wide-integer/issues/234#issuecomment-1052960210
    WIDE_INTEGER_CONSTEXPR from_issue_234::uint512 u512("0x123456780123456780");
    WIDE_INTEGER_CONSTEXPR from_issue_234::uint80  u80 = from_issue_234::convert_to_uint80(u512);

    const bool convert_512_to_80_is_ok = (u80 == from_issue_234::uint80("0x123456780123456780"));

    result_is_ok = (convert_512_to_80_is_ok && result_is_ok);

    #if(WIDE_INTEGER_CONSTEXPR_IS_COMPILE_TIME_CONST == 1)
    static_assert(convert_512_to_80_is_ok, "Error: Converting 512-bit type to 80-bit type is not OK");
    #endif
  }

  {
    // See also https://github.com/ckormanyos/wide-integer/issues/234#issuecomment-1052960210
    WIDE_INTEGER_CONSTEXPR from_issue_234::uint80  u80("0x123456780123456780");
    WIDE_INTEGER_CONSTEXPR from_issue_234::uint512 u512 = from_issue_234::convert_to_uint512(u80);

    const bool convert_80_to_512_is_ok = (u512 == from_issue_234::uint512("0x123456780123456780"));

    #if(WIDE_INTEGER_CONSTEXPR_IS_COMPILE_TIME_CONST == 1)
    static_assert(convert_80_to_512_is_ok, "Error: Converting 80-bit type to 512-bit type is not OK");
    #endif

    result_is_ok = (convert_80_to_512_is_ok && result_is_ok);
  }

  {
    // See also https://github.com/ckormanyos/wide-integer/issues/234
    // In particular, how do I synthesize uint80_t?

    #if defined(WIDE_INTEGER_NAMESPACE)
    using local_uint80_type = WIDE_INTEGER_NAMESPACE::math::wide_integer::uintwide_t<static_cast<WIDE_INTEGER_NAMESPACE::math::wide_integer::size_t>(UINT32_C(80)), std::uint16_t>;
    #else
    using local_uint80_type = ::math::wide_integer::uintwide_t<static_cast<math::wide_integer::size_t>(UINT32_C(80)), std::uint16_t>;
    #endif

    local_uint80_type u(123); // NOLINT(cppcoreguidelines-avoid-magic-numbers,readability-magic-numbers)
    local_uint80_type v(456); // NOLINT(cppcoreguidelines-avoid-magic-numbers,readability-magic-numbers)

    u *= u; // 15129
    u *= u; // 228886641
    u *= u; // 52389094428262881

    result_is_ok = ((u == local_uint80_type("52389094428262881")) && result_is_ok);

    v *= v; // 207936
    v *= v; // 43237380096
    v *= v; // 1869471037565976969216

    result_is_ok = ((v == local_uint80_type("1869471037565976969216")) && result_is_ok);

    const auto w = static_cast<std::uint16_t>(v / u);

    result_is_ok = ((w == UINT16_C(35684)) && result_is_ok);
  }

  {
    // See also https://github.com/ckormanyos/wide-integer/issues/234
    // In particular, how to find sint512_t.operator|(uint32_t).

    #if defined(WIDE_INTEGER_NAMESPACE)
    using local_sint512_type = WIDE_INTEGER_NAMESPACE::math::wide_integer::uintwide_t<static_cast<WIDE_INTEGER_NAMESPACE::math::wide_integer::size_t>(UINT32_C(512)), std::uint32_t, void, true>;
    #else
    using local_sint512_type = ::math::wide_integer::uintwide_t<static_cast<math::wide_integer::size_t>(UINT32_C(512)), std::uint32_t, void, true>;
    #endif

    WIDE_INTEGER_CONSTEXPR local_sint512_type u1("0x7FFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFF55555555");
    WIDE_INTEGER_CONSTEXPR std::uint32_t      v1 = UINT32_C(0xAAAAAAAA);
    WIDE_INTEGER_CONSTEXPR local_sint512_type w1 = u1 | v1;

    WIDE_INTEGER_CONSTEXPR local_sint512_type u2("0x7FFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFF55555555");
    WIDE_INTEGER_CONSTEXPR std::uint32_t      v2 = UINT32_C(0xAAAAAAAA);
    WIDE_INTEGER_CONSTEXPR local_sint512_type w2 = v2 | u2;

    WIDE_INTEGER_CONSTEXPR bool w1_is_ok = (w1  == (std::numeric_limits<local_sint512_type>::max)());
    WIDE_INTEGER_CONSTEXPR bool w2_is_ok = (w2  == (std::numeric_limits<local_sint512_type>::max)());

    #if(WIDE_INTEGER_CONSTEXPR_IS_COMPILE_TIME_CONST == 1)
    static_assert(w1_is_ok, "Error: Bitwise OR with built-in type is not OK");
    #endif

    #if(WIDE_INTEGER_CONSTEXPR_IS_COMPILE_TIME_CONST == 1)
    static_assert(w2_is_ok, "Error: Bitwise OR with built-in type is not OK");
    #endif

    result_is_ok = (w1_is_ok && result_is_ok);
    result_is_ok = (w2_is_ok && result_is_ok);
  }

  {
    // See also https://github.com/ckormanyos/wide-integer/issues/213

    #if defined(WIDE_INTEGER_NAMESPACE)
    using local_uint32_type = WIDE_INTEGER_NAMESPACE::math::wide_integer::uintwide_t<static_cast<WIDE_INTEGER_NAMESPACE::math::wide_integer::size_t>(UINT32_C(32)), std::uint32_t, void, false>;
    #else
    using local_uint32_type = ::math::wide_integer::uintwide_t<static_cast<math::wide_integer::size_t>(UINT32_C(32)), std::uint32_t, void, false>;
    #endif

    WIDE_INTEGER_CONSTEXPR local_uint32_type
      p
      (
        static_cast<typename local_uint32_type::limb_type>(UINT8_C(61))
      );

    WIDE_INTEGER_CONSTEXPR local_uint32_type
      q
      (
        static_cast<typename local_uint32_type::limb_type>(UINT8_C(53))
      );

    WIDE_INTEGER_CONSTEXPR local_uint32_type lcm_result = lcm(p - 1U, q - 1U);

    result_is_ok = ((static_cast<unsigned>(lcm_result) == static_cast<unsigned>(UINT16_C(780))) && result_is_ok);

    #if(WIDE_INTEGER_CONSTEXPR_IS_COMPILE_TIME_CONST == 1)
    static_assert((static_cast<unsigned>(lcm_result) == static_cast<unsigned>(UINT16_C(780))),
                  "Error: Rudimentary LCM calculation result is wrong");
    #endif
  }

  {
    // See also https://github.com/ckormanyos/wide-integer/issues/186

    // Here we statically test non-explicit construction/conversion

    #if defined(WIDE_INTEGER_NAMESPACE)
    using local_uint128_type = WIDE_INTEGER_NAMESPACE::math::wide_integer::uintwide_t<static_cast<WIDE_INTEGER_NAMESPACE::math::wide_integer::size_t>(UINT32_C(128)), std::uint32_t, void, false>;
    using local_int128_type  = WIDE_INTEGER_NAMESPACE::math::wide_integer::uintwide_t<static_cast<WIDE_INTEGER_NAMESPACE::math::wide_integer::size_t>(UINT32_C(128)), std::uint32_t, void, true>;
    using local_uint160_type = WIDE_INTEGER_NAMESPACE::math::wide_integer::uintwide_t<static_cast<WIDE_INTEGER_NAMESPACE::math::wide_integer::size_t>(UINT32_C(160)), std::uint32_t, void, false>;
    using local_int160_type  = WIDE_INTEGER_NAMESPACE::math::wide_integer::uintwide_t<static_cast<WIDE_INTEGER_NAMESPACE::math::wide_integer::size_t>(UINT32_C(160)), std::uint32_t, void, true>;
    #else
    using local_uint128_type = ::math::wide_integer::uintwide_t<static_cast<math::wide_integer::size_t>(UINT32_C(128)), std::uint32_t, void, false>;
    using local_int128_type  = ::math::wide_integer::uintwide_t<static_cast<math::wide_integer::size_t>(UINT32_C(128)), std::uint32_t, void, true>;
    using local_uint160_type = ::math::wide_integer::uintwide_t<static_cast<math::wide_integer::size_t>(UINT32_C(160)), std::uint32_t, void, false>;
    using local_int160_type  = ::math::wide_integer::uintwide_t<static_cast<math::wide_integer::size_t>(UINT32_C(160)), std::uint32_t, void, true>;
    #endif

    // Static test of construction rules.

    // Various construction(s)
    static_assert(std::is_default_constructible            <local_uint128_type>::value, "Error: Type is not default-constructible");
    static_assert(std::is_default_constructible            <local_int128_type >::value, "Error: Type is not default-constructible");
    static_assert(std::is_copy_constructible               <local_uint128_type>::value, "Error: Type is not copy-constructible");
    static_assert(std::is_copy_constructible               <local_int128_type >::value, "Error: Type is not copy-constructible");
    #if !defined(WIDE_INTEGER_DISABLE_WIDE_INTEGER_CONSTEXPR)
    static_assert(std::is_trivially_copy_constructible     <local_uint128_type>::value, "Error: Type is not trivially copy-constructible");
    static_assert(std::is_trivially_copy_constructible     <local_int128_type >::value, "Error: Type is not trivially copy-constructible");
    #endif
    static_assert(std::is_move_constructible               <local_uint128_type>::value, "Error: Type is not move-constructible");
    static_assert(std::is_move_constructible               <local_int128_type >::value, "Error: Type is not move-constructible");
    #if !defined(WIDE_INTEGER_DISABLE_WIDE_INTEGER_CONSTEXPR)
    static_assert(std::is_trivially_move_constructible     <local_uint128_type>::value, "Error: Type is not trivially move-constructible");
    static_assert(std::is_trivially_move_constructible     <local_int128_type >::value, "Error: Type is not trivially move-constructible");
    #endif

    // Constructible
    #if !defined(WIDE_INTEGER_DISABLE_WIDE_INTEGER_CONSTEXPR)
    static_assert(std::is_trivially_constructible<local_uint128_type, const local_uint128_type&>::value, "Error: Types are not trivially constructible");
    #endif
    static_assert(std::is_constructible          <local_uint128_type,       local_int128_type  >::value, "Error: Types are not constructible");
    static_assert(std::is_constructible          <local_uint128_type,       local_uint160_type >::value, "Error: Types are not constructible");
    static_assert(std::is_constructible          <local_uint128_type,       local_int160_type  >::value, "Error: Types are not constructible");

    static_assert(std::is_constructible          <local_int128_type,        local_uint128_type >::value, "Error: Types are not constructible");
    #if !defined(WIDE_INTEGER_DISABLE_WIDE_INTEGER_CONSTEXPR)
    static_assert(std::is_trivially_constructible<local_int128_type,  const local_int128_type& >::value, "Error: Types are not trivially constructible");
    #endif
    static_assert(std::is_constructible          <local_int128_type,        local_uint160_type >::value, "Error: Types are not constructible");
    static_assert(std::is_constructible          <local_int128_type,        local_int160_type  >::value, "Error: Types are not constructible");

    static_assert(std::is_constructible          <local_uint160_type,       local_uint128_type >::value, "Error: Types are not constructible");
    static_assert(std::is_constructible          <local_uint160_type,       local_int128_type  >::value, "Error: Types are not constructible");
    #if !defined(WIDE_INTEGER_DISABLE_WIDE_INTEGER_CONSTEXPR)
    static_assert(std::is_trivially_constructible<local_uint160_type, const local_uint160_type&>::value, "Error: Types are not trivially constructible");
    #endif
    static_assert(std::is_constructible          <local_uint160_type,       local_int160_type  >::value, "Error: Types are not constructible");

    static_assert(std::is_constructible          <local_int160_type,        local_uint128_type >::value, "Error: Types are not constructible");
    static_assert(std::is_constructible          <local_int160_type,        local_int128_type  >::value, "Error: Types are not constructible");
    static_assert(std::is_constructible          <local_int160_type,        local_uint160_type >::value, "Error: Types are not constructible");
    #if !defined(WIDE_INTEGER_DISABLE_WIDE_INTEGER_CONSTEXPR)
    static_assert(std::is_trivially_constructible<local_int160_type,  const local_int160_type& >::value, "Error: Types are not trivially constructible");
    #endif

    // Static test of conversion rules.
    //                               <local_uint128_type, local_uint128_type>
    static_assert(std::is_convertible<local_uint128_type, local_int128_type >::value, "Error: Types are not convertible");
    static_assert(std::is_convertible<local_uint128_type, local_uint160_type>::value, "Error: Types are not convertible");
    static_assert(std::is_convertible<local_uint128_type, local_int160_type >::value, "Error: Types are not convertible");

    static_assert(std::is_convertible<local_int128_type,  local_uint128_type>::value, "Error: Types are not convertible");
    //                               <local_int128_type,  local_int128_type >
    static_assert(std::is_convertible<local_int128_type,  local_uint160_type>::value, "Error: Types are not convertible");
    static_assert(std::is_convertible<local_int128_type,  local_int160_type >::value, "Error: Types are not convertible");

    static_assert(std::is_convertible<local_uint160_type, local_uint128_type>::value, "Error: Types are not convertible");
    static_assert(std::is_convertible<local_uint160_type, local_int128_type >::value, "Error: Types are not convertible");
    //                               <local_uint160_type, local_uint160_type>
    static_assert(std::is_convertible<local_uint160_type, local_int160_type >::value, "Error: Types are not convertible");

    static_assert(std::is_convertible<local_int160_type,  local_uint128_type>::value, "Error: Types are not convertible");
    static_assert(std::is_convertible<local_int160_type,  local_int128_type >::value, "Error: Types are not convertible");
    static_assert(std::is_convertible<local_int160_type,  local_uint160_type>::value, "Error: Types are not convertible");
    //                               <local_int160_type,  local_int160_type >
  }

  {
    // See also https://github.com/ckormanyos/wide-integer/issues/181

    // Here we test explicit construction/conversion both statically
    // as well as dynamically (i.e., dynamically in run-time).
    #if defined(WIDE_INTEGER_NAMESPACE)
    using local_uint128_type = WIDE_INTEGER_NAMESPACE::math::wide_integer::uintwide_t<static_cast<WIDE_INTEGER_NAMESPACE::math::wide_integer::size_t>(UINT32_C(128)), std::uint32_t, void, false>;
    using local_int128_type  = WIDE_INTEGER_NAMESPACE::math::wide_integer::uintwide_t<static_cast<WIDE_INTEGER_NAMESPACE::math::wide_integer::size_t>(UINT32_C(128)), std::uint32_t, void, true>;
    using local_uint160_type = WIDE_INTEGER_NAMESPACE::math::wide_integer::uintwide_t<static_cast<WIDE_INTEGER_NAMESPACE::math::wide_integer::size_t>(UINT32_C(160)), std::uint32_t, void, false>;
    using local_int160_type  = WIDE_INTEGER_NAMESPACE::math::wide_integer::uintwide_t<static_cast<WIDE_INTEGER_NAMESPACE::math::wide_integer::size_t>(UINT32_C(160)), std::uint32_t, void, true>;
    #else
    using local_uint128_type = ::math::wide_integer::uintwide_t<static_cast<math::wide_integer::size_t>(UINT32_C(128)), std::uint32_t, void, false>;
    using local_int128_type  = ::math::wide_integer::uintwide_t<static_cast<math::wide_integer::size_t>(UINT32_C(128)), std::uint32_t, void, true>;
    using local_uint160_type = ::math::wide_integer::uintwide_t<static_cast<math::wide_integer::size_t>(UINT32_C(160)), std::uint32_t, void, false>;
    using local_int160_type  = ::math::wide_integer::uintwide_t<static_cast<math::wide_integer::size_t>(UINT32_C(160)), std::uint32_t, void, true>;
    #endif

    // Get randoms via:
    // RandomInteger[{100000000000000000000000000000000000, 10000000000000000000000000000000000000}]

    WIDE_INTEGER_CONSTEXPR local_uint128_type u128_0("3076659267683009403742876678609501102");
    WIDE_INTEGER_CONSTEXPR local_uint128_type u128_1("9784355713321885697254484081284759103");
    WIDE_INTEGER_CONSTEXPR local_uint128_type u128_2("1759644461251476961796845209840363274");

    WIDE_INTEGER_CONSTEXPR auto u160_0 = local_uint160_type(u128_0);
    WIDE_INTEGER_CONSTEXPR auto u160_1 = local_uint160_type(u128_1);
    WIDE_INTEGER_CONSTEXPR auto u160_2 = local_uint160_type(u128_2);

    WIDE_INTEGER_CONSTEXPR auto v128_0 = local_uint128_type(u160_0);
    WIDE_INTEGER_CONSTEXPR auto v128_1 = local_uint128_type(u160_1);
    WIDE_INTEGER_CONSTEXPR auto v128_2 = local_uint128_type(u160_2);

    result_is_ok = ((u128_0 == v128_0) && result_is_ok);
    result_is_ok = ((u128_1 == v128_1) && result_is_ok);
    result_is_ok = ((u128_2 == v128_2) && result_is_ok);

    #if(WIDE_INTEGER_CONSTEXPR_IS_COMPILE_TIME_CONST == 1)
    static_assert(u128_0 == v128_0, "Error: Static check of inter-width casting (unsigned) is not OK");
    static_assert(u128_1 == v128_1, "Error: Static check of inter-width casting (unsigned) is not OK");
    static_assert(u128_2 == v128_2, "Error: Static check of inter-width casting (unsigned) is not OK");
    #endif

    WIDE_INTEGER_CONSTEXPR local_int128_type n128_0("-3076659267683009403742876678609501102");
    WIDE_INTEGER_CONSTEXPR local_int128_type n128_1("-9784355713321885697254484081284759103");
    WIDE_INTEGER_CONSTEXPR local_int128_type n128_2("-1759644461251476961796845209840363274");

    WIDE_INTEGER_CONSTEXPR auto n160_0 = local_int160_type(n128_0);
    WIDE_INTEGER_CONSTEXPR auto n160_1 = local_int160_type(n128_1);
    WIDE_INTEGER_CONSTEXPR auto n160_2 = local_int160_type(n128_2);

    WIDE_INTEGER_CONSTEXPR auto m128_0 = static_cast<local_int128_type>(n160_0);
    WIDE_INTEGER_CONSTEXPR auto m128_1 = static_cast<local_int128_type>(n160_1);
    WIDE_INTEGER_CONSTEXPR auto m128_2 = static_cast<local_int128_type>(n160_2);

    result_is_ok = ((n128_0 == m128_0) && result_is_ok);
    result_is_ok = ((n128_1 == m128_1) && result_is_ok);
    result_is_ok = ((n128_2 == m128_2) && result_is_ok);

    #if(WIDE_INTEGER_CONSTEXPR_IS_COMPILE_TIME_CONST == 1)
    static_assert(u128_0 == v128_0, "Error: Static check of inter-width casting (signed) is not OK");
    static_assert(u128_1 == v128_1, "Error: Static check of inter-width casting (signed) is not OK");
    static_assert(u128_2 == v128_2, "Error: Static check of inter-width casting (signed) is not OK");
    #endif

    WIDE_INTEGER_CONSTEXPR auto un160_0 = local_uint160_type(-n128_0);
    WIDE_INTEGER_CONSTEXPR auto un160_1 = local_uint160_type(-n128_1);
    WIDE_INTEGER_CONSTEXPR auto un160_2 = local_uint160_type(-n128_2);

    result_is_ok = ((un160_0 == u160_0) && result_is_ok);
    result_is_ok = ((un160_1 == u160_1) && result_is_ok);
    result_is_ok = ((un160_2 == u160_2) && result_is_ok);

    #if(WIDE_INTEGER_CONSTEXPR_IS_COMPILE_TIME_CONST == 1)
    static_assert(un160_0 == u160_0, "Error: Static check of inter-width casting (mixed signes) is not OK");
    static_assert(un160_1 == u160_1, "Error: Static check of inter-width casting (mixed signes) is not OK");
    static_assert(un160_2 == u160_2, "Error: Static check of inter-width casting (mixed signes) is not OK");
    #endif

    WIDE_INTEGER_CONSTEXPR auto s128_0 = local_int128_type(un160_0);
    WIDE_INTEGER_CONSTEXPR auto s128_1 = local_int128_type(un160_1);
    WIDE_INTEGER_CONSTEXPR auto s128_2 = local_int128_type(un160_2);

    result_is_ok = ((local_uint128_type(s128_0) == u128_0) && result_is_ok);
    result_is_ok = ((local_uint128_type(s128_1) == u128_1) && result_is_ok);
    result_is_ok = ((local_uint128_type(s128_2) == u128_2) && result_is_ok);

    #if(WIDE_INTEGER_CONSTEXPR_IS_COMPILE_TIME_CONST == 1)
    static_assert(static_cast<local_uint128_type>(s128_0) == u128_0, "Error: Static check of inter-width casting (mixed signes) is not OK");
    static_assert(static_cast<local_uint128_type>(s128_1) == u128_1, "Error: Static check of inter-width casting (mixed signes) is not OK");
    static_assert(static_cast<local_uint128_type>(s128_2) == u128_2, "Error: Static check of inter-width casting (mixed signes) is not OK");
    #endif
  }

  {
    // See also https://github.com/ckormanyos/wide-integer/issues/90

    #if defined(WIDE_INTEGER_NAMESPACE)
    using WIDE_INTEGER_NAMESPACE::math::wide_integer::uint128_t;
    using WIDE_INTEGER_NAMESPACE::math::wide_integer::int128_t;
    #else
    using ::math::wide_integer::uint128_t;
    using ::math::wide_integer::int128_t;
    #endif

    // Get randoms via:
    // RandomInteger[{100000000000000000000000000000000000, 10000000000000000000000000000000000000}]

    {
      WIDE_INTEGER_CONSTEXPR uint128_t u_sep("6'216'049'444'209'020'458'323'688'259'792'241'931");
      WIDE_INTEGER_CONSTEXPR uint128_t u    ("6216049444209020458323688259792241931");

      WIDE_INTEGER_CONSTEXPR uint128_t n_sep("-3000'424'814'887'742'920'043'278'044'817'737'744");
      WIDE_INTEGER_CONSTEXPR uint128_t n    ("-3000424814887742920043278044817737744");

      // BaseForm[6216049444209020458323688259792241931, 16]
      // 4ad2ae64368b98a810635e9cd49850b_16
      WIDE_INTEGER_CONSTEXPR uint128_t h_sep("0x4'AD'2A'E6'43'68'B9'8A'81'06'35'E9'CD'49'85'0B");

      result_is_ok = ((u_sep == u) && result_is_ok);
      result_is_ok = ((n_sep == n) && result_is_ok);
      result_is_ok = ((h_sep == u) && result_is_ok);

      #if(WIDE_INTEGER_CONSTEXPR_IS_COMPILE_TIME_CONST == 1)
      static_assert(u_sep == u, "Error: Static check of construction via string with digit separators fails");
      static_assert(n_sep == n, "Error: Static check of construction via string with digit separators fails");
      static_assert(h_sep == u, "Error: Static check of construction via string with digit separators fails");
      #endif
    }
  }

  {
    // See also https://github.com/ckormanyos/wide-integer/issues/145#issuecomment-1006374713

    #if defined(WIDE_INTEGER_NAMESPACE)
    using WIDE_INTEGER_NAMESPACE::math::wide_integer::uint128_t;
    using WIDE_INTEGER_NAMESPACE::math::wide_integer::int128_t;
    #else
    using ::math::wide_integer::uint128_t;
    using ::math::wide_integer::int128_t;
    #endif

    // Get randoms via:
    // RandomInteger[{100000000000000000000000000000000000, 10000000000000000000000000000000000000}]

    WIDE_INTEGER_CONSTEXPR uint128_t u0("3076659267683009403742876678609501102");
    WIDE_INTEGER_CONSTEXPR uint128_t u1("9784355713321885697254484081284759103");
    WIDE_INTEGER_CONSTEXPR uint128_t u2("1759644461251476961796845209840363274");

    result_is_ok = (from_issue_145::test_uintwide_t_spot_values_from_issue_145(u0) && result_is_ok);
    result_is_ok = (from_issue_145::test_uintwide_t_spot_values_from_issue_145(u1) && result_is_ok);
    result_is_ok = (from_issue_145::test_uintwide_t_spot_values_from_issue_145(u2) && result_is_ok);

    WIDE_INTEGER_CONSTEXPR int128_t n0("-3076659267683009403742876678609501102");
    WIDE_INTEGER_CONSTEXPR int128_t n1("-9784355713321885697254484081284759103");
    WIDE_INTEGER_CONSTEXPR int128_t n2("-1759644461251476961796845209840363274");

    result_is_ok = (from_issue_145::test_uintwide_t_spot_values_from_issue_145(n0) && result_is_ok);
    result_is_ok = (from_issue_145::test_uintwide_t_spot_values_from_issue_145(n1) && result_is_ok);
    result_is_ok = (from_issue_145::test_uintwide_t_spot_values_from_issue_145(n2) && result_is_ok);
  }

  {
    // See also https://github.com/ckormanyos/wide-integer/issues/154

    {
      #if defined(WIDE_INTEGER_NAMESPACE)
      using local_uint64_type    = WIDE_INTEGER_NAMESPACE::math::wide_integer::uint64_t;
      using local_uint128_type   = WIDE_INTEGER_NAMESPACE::math::wide_integer::uint128_t;
      using local_uint512_type   = WIDE_INTEGER_NAMESPACE::math::wide_integer::uint512_t;
      using local_uint1024_type  = WIDE_INTEGER_NAMESPACE::math::wide_integer::uint1024_t;
      using local_uint2048_type  = WIDE_INTEGER_NAMESPACE::math::wide_integer::uint2048_t;
      using local_uint4096_type  = WIDE_INTEGER_NAMESPACE::math::wide_integer::uint4096_t;
      using local_uint8192_type  = WIDE_INTEGER_NAMESPACE::math::wide_integer::uint8192_t;
      using local_uint16384_type = WIDE_INTEGER_NAMESPACE::math::wide_integer::uint16384_t;
      using local_uint32768_type = WIDE_INTEGER_NAMESPACE::math::wide_integer::uint32768_t;
      using local_uint65536_type = WIDE_INTEGER_NAMESPACE::math::wide_integer::uint65536_t;
      #else
      using local_uint64_type    = ::math::wide_integer::uint64_t;
      using local_uint128_type   = ::math::wide_integer::uint128_t;
      using local_uint512_type   = ::math::wide_integer::uint512_t;
      using local_uint1024_type  = ::math::wide_integer::uint1024_t;
      using local_uint2048_type  = ::math::wide_integer::uint2048_t;
      using local_uint4096_type  = ::math::wide_integer::uint4096_t;
      using local_uint8192_type  = ::math::wide_integer::uint8192_t;
      using local_uint16384_type = ::math::wide_integer::uint16384_t;
      using local_uint32768_type = ::math::wide_integer::uint32768_t;
      using local_uint65536_type = ::math::wide_integer::uint65536_t;
      #endif

      #if(WIDE_INTEGER_CONSTEXPR_IS_COMPILE_TIME_CONST == 1)
      static_assert((std::numeric_limits<local_uint64_type   >::max)() != 0U, "Error: Static check of convenience type fails");
      static_assert((std::numeric_limits<local_uint128_type  >::max)() != 0U, "Error: Static check of convenience type fails");
      static_assert((std::numeric_limits<local_uint512_type  >::max)() != 0U, "Error: Static check of convenience type fails");
      static_assert((std::numeric_limits<local_uint1024_type >::max)() != 0U, "Error: Static check of convenience type fails");
      static_assert((std::numeric_limits<local_uint2048_type >::max)() != 0U, "Error: Static check of convenience type fails");
      static_assert((std::numeric_limits<local_uint4096_type >::max)() != 0U, "Error: Static check of convenience type fails");
      static_assert((std::numeric_limits<local_uint8192_type >::max)() != 0U, "Error: Static check of convenience type fails");
      static_assert((std::numeric_limits<local_uint16384_type>::max)() != 0U, "Error: Static check of convenience type fails");
      static_assert((std::numeric_limits<local_uint32768_type>::max)() != 0U, "Error: Static check of convenience type fails");
      static_assert((std::numeric_limits<local_uint65536_type>::max)() != 0U, "Error: Static check of convenience type fails");

      static_assert((std::numeric_limits<local_uint64_type   >::min)() == 0U, "Error: Static check of convenience type fails");
      static_assert((std::numeric_limits<local_uint128_type  >::min)() == 0U, "Error: Static check of convenience type fails");
      static_assert((std::numeric_limits<local_uint512_type  >::min)() == 0U, "Error: Static check of convenience type fails");
      static_assert((std::numeric_limits<local_uint1024_type >::min)() == 0U, "Error: Static check of convenience type fails");
      static_assert((std::numeric_limits<local_uint2048_type >::min)() == 0U, "Error: Static check of convenience type fails");
      static_assert((std::numeric_limits<local_uint4096_type >::min)() == 0U, "Error: Static check of convenience type fails");
      static_assert((std::numeric_limits<local_uint8192_type >::min)() == 0U, "Error: Static check of convenience type fails");
      static_assert((std::numeric_limits<local_uint16384_type>::min)() == 0U, "Error: Static check of convenience type fails");
      static_assert((std::numeric_limits<local_uint32768_type>::min)() == 0U, "Error: Static check of convenience type fails");
      static_assert((std::numeric_limits<local_uint65536_type>::min)() == 0U, "Error: Static check of convenience type fails");
      #else
      assert((std::numeric_limits<local_uint64_type   >::max)() != 0U); // NOLINT
      assert((std::numeric_limits<local_uint128_type  >::max)() != 0U); // NOLINT
      assert((std::numeric_limits<local_uint512_type  >::max)() != 0U); // NOLINT
      assert((std::numeric_limits<local_uint1024_type >::max)() != 0U); // NOLINT
      assert((std::numeric_limits<local_uint2048_type >::max)() != 0U); // NOLINT
      assert((std::numeric_limits<local_uint4096_type >::max)() != 0U); // NOLINT
      assert((std::numeric_limits<local_uint8192_type >::max)() != 0U); // NOLINT
      assert((std::numeric_limits<local_uint16384_type>::max)() != 0U); // NOLINT
      assert((std::numeric_limits<local_uint32768_type>::max)() != 0U); // NOLINT
      assert((std::numeric_limits<local_uint65536_type>::max)() != 0U); // NOLINT // LCOV_EXCL_LINE

      assert((std::numeric_limits<local_uint64_type   >::min)() == 0U); // NOLINT
      assert((std::numeric_limits<local_uint128_type  >::min)() == 0U); // NOLINT
      assert((std::numeric_limits<local_uint512_type  >::min)() == 0U); // NOLINT
      assert((std::numeric_limits<local_uint1024_type >::min)() == 0U); // NOLINT
      assert((std::numeric_limits<local_uint2048_type >::min)() == 0U); // NOLINT
      assert((std::numeric_limits<local_uint4096_type >::min)() == 0U); // NOLINT
      assert((std::numeric_limits<local_uint8192_type >::min)() == 0U); // NOLINT
      assert((std::numeric_limits<local_uint16384_type>::min)() == 0U); // NOLINT
      assert((std::numeric_limits<local_uint32768_type>::min)() == 0U); // NOLINT
      assert((std::numeric_limits<local_uint65536_type>::min)() == 0U); // NOLINT
      #endif
    }

    {
      #if defined(WIDE_INTEGER_NAMESPACE)
      using local_int64_type    = WIDE_INTEGER_NAMESPACE::math::wide_integer::int64_t;
      using local_int128_type   = WIDE_INTEGER_NAMESPACE::math::wide_integer::int128_t;
      using local_int512_type   = WIDE_INTEGER_NAMESPACE::math::wide_integer::int512_t;
      using local_int1024_type  = WIDE_INTEGER_NAMESPACE::math::wide_integer::int1024_t;
      using local_int2048_type  = WIDE_INTEGER_NAMESPACE::math::wide_integer::int2048_t;
      using local_int4096_type  = WIDE_INTEGER_NAMESPACE::math::wide_integer::int4096_t;
      using local_int8192_type  = WIDE_INTEGER_NAMESPACE::math::wide_integer::int8192_t;
      using local_int16384_type = WIDE_INTEGER_NAMESPACE::math::wide_integer::int16384_t;
      using local_int32768_type = WIDE_INTEGER_NAMESPACE::math::wide_integer::int32768_t;
      using local_int65536_type = WIDE_INTEGER_NAMESPACE::math::wide_integer::int65536_t;
      #else
      using local_int64_type    = ::math::wide_integer::int64_t;
      using local_int128_type   = ::math::wide_integer::int128_t;
      using local_int512_type   = ::math::wide_integer::int512_t;
      using local_int1024_type  = ::math::wide_integer::int1024_t;
      using local_int2048_type  = ::math::wide_integer::int2048_t;
      using local_int4096_type  = ::math::wide_integer::int4096_t;
      using local_int8192_type  = ::math::wide_integer::int8192_t;
      using local_int16384_type = ::math::wide_integer::int16384_t;
      using local_int32768_type = ::math::wide_integer::int32768_t;
      using local_int65536_type = ::math::wide_integer::int65536_t;
      #endif

      #if(WIDE_INTEGER_CONSTEXPR_IS_COMPILE_TIME_CONST == 1)
      static_assert((std::numeric_limits<local_int64_type   >::max)() != 0, "Error: Static check of convenience type fails");
      static_assert((std::numeric_limits<local_int128_type  >::max)() != 0, "Error: Static check of convenience type fails");
      static_assert((std::numeric_limits<local_int512_type  >::max)() != 0, "Error: Static check of convenience type fails");
      static_assert((std::numeric_limits<local_int1024_type >::max)() != 0, "Error: Static check of convenience type fails");
      static_assert((std::numeric_limits<local_int2048_type >::max)() != 0, "Error: Static check of convenience type fails");
      static_assert((std::numeric_limits<local_int4096_type >::max)() != 0, "Error: Static check of convenience type fails");
      static_assert((std::numeric_limits<local_int8192_type >::max)() != 0, "Error: Static check of convenience type fails");
      static_assert((std::numeric_limits<local_int16384_type>::max)() != 0, "Error: Static check of convenience type fails");
      static_assert((std::numeric_limits<local_int32768_type>::max)() != 0, "Error: Static check of convenience type fails");
      static_assert((std::numeric_limits<local_int65536_type>::max)() != 0, "Error: Static check of convenience type fails");

      static_assert((std::numeric_limits<local_int64_type   >::min)() != 0, "Error: Static check of convenience type fails");
      static_assert((std::numeric_limits<local_int128_type  >::min)() != 0, "Error: Static check of convenience type fails");
      static_assert((std::numeric_limits<local_int512_type  >::min)() != 0, "Error: Static check of convenience type fails");
      static_assert((std::numeric_limits<local_int1024_type >::min)() != 0, "Error: Static check of convenience type fails");
      static_assert((std::numeric_limits<local_int2048_type >::min)() != 0, "Error: Static check of convenience type fails");
      static_assert((std::numeric_limits<local_int4096_type >::min)() != 0, "Error: Static check of convenience type fails");
      static_assert((std::numeric_limits<local_int8192_type >::min)() != 0, "Error: Static check of convenience type fails");
      static_assert((std::numeric_limits<local_int16384_type>::min)() != 0, "Error: Static check of convenience type fails");
      static_assert((std::numeric_limits<local_int32768_type>::min)() != 0, "Error: Static check of convenience type fails");
      static_assert((std::numeric_limits<local_int65536_type>::min)() != 0, "Error: Static check of convenience type fails");
      #else
      assert((std::numeric_limits<local_int64_type   >::max)() != 0); // NOLINT
      assert((std::numeric_limits<local_int128_type  >::max)() != 0); // NOLINT
      assert((std::numeric_limits<local_int512_type  >::max)() != 0); // NOLINT
      assert((std::numeric_limits<local_int1024_type >::max)() != 0); // NOLINT
      assert((std::numeric_limits<local_int2048_type >::max)() != 0); // NOLINT
      assert((std::numeric_limits<local_int4096_type >::max)() != 0); // NOLINT
      assert((std::numeric_limits<local_int8192_type >::max)() != 0); // NOLINT
      assert((std::numeric_limits<local_int16384_type>::max)() != 0); // NOLINT
      assert((std::numeric_limits<local_int32768_type>::max)() != 0); // NOLINT
      assert((std::numeric_limits<local_int65536_type>::max)() != 0); // NOLINT

      assert((std::numeric_limits<local_int64_type   >::min)() != 0); // NOLINT
      assert((std::numeric_limits<local_int128_type  >::min)() != 0); // NOLINT
      assert((std::numeric_limits<local_int512_type  >::min)() != 0); // NOLINT
      assert((std::numeric_limits<local_int1024_type >::min)() != 0); // NOLINT
      assert((std::numeric_limits<local_int2048_type >::min)() != 0); // NOLINT
      assert((std::numeric_limits<local_int4096_type >::min)() != 0); // NOLINT
      assert((std::numeric_limits<local_int8192_type >::min)() != 0); // NOLINT
      assert((std::numeric_limits<local_int16384_type>::min)() != 0); // NOLINT
      assert((std::numeric_limits<local_int32768_type>::min)() != 0); // NOLINT
      assert((std::numeric_limits<local_int65536_type>::min)() != 0); // NOLINT
      #endif
    }

    {
      #if defined(WIDE_INTEGER_NAMESPACE)
      using local_uint131072_type = WIDE_INTEGER_NAMESPACE::math::wide_integer::uintwide_t<static_cast<WIDE_INTEGER_NAMESPACE::math::wide_integer::size_t>(UINT32_C(131072)), std::uint32_t, std::allocator<void>, false>;
      #else
      using local_uint131072_type = ::math::wide_integer::uintwide_t<static_cast<math::wide_integer::size_t>(UINT32_C(131072)), std::uint32_t, std::allocator<void>, false>;
      #endif

      local_uint131072_type u(123U); // NOLINT(cppcoreguidelines-avoid-magic-numbers,readability-magic-numbers)
      local_uint131072_type v( 56U); // NOLINT(cppcoreguidelines-avoid-magic-numbers,readability-magic-numbers)

      // Multiply 123^256.
      u *= u; u *= u; u *= u; u *= u;
      u *= u; u *= u; u *= u; u *= u;

      // Multiply 56^256.
      v *= v; v *= v; v *= v; v *= v;
      v *= v; v *= v; v *= v; v *= v;

      {
        std::stringstream strm;

        // Divide 123^256 / 56^256 and verify the integral result.
        local_uint131072_type w = u / v;

        strm << w;

        result_is_ok =
          ((strm.str() == "3016988223108505362607102560314821693738482648596342283928988093842474437457679828842200") && result_is_ok);
      }
    }
  }

  {
    // See also https://github.com/ckormanyos/wide-integer/pull/134

    #if (defined(__clang__) && (__clang_major__ >= 10)) && (defined(__cplusplus) && (__cplusplus > 201703L))
      #if defined(__x86_64__)
      static_assert(WIDE_INTEGER_CONSTEXPR_IS_COMPILE_TIME_CONST == 1, "Error: clang constexpr is not properly configured");
      #endif
    #endif
  }

  {
    // See also https://github.com/ckormanyos/wide-integer/pull/130

    // The exact issue motivating this PR turned out to be
    // an incorect report. The tests, however, are useful
    // and these have been integrated into _spot_values().

    {
      #if defined(WIDE_INTEGER_NAMESPACE)
      using type = WIDE_INTEGER_NAMESPACE::math::wide_integer::uintwide_t<static_cast<WIDE_INTEGER_NAMESPACE::math::wide_integer::size_t>(UINT32_C(64)), std::uint32_t, void, true>;
      #else
      using type = ::math::wide_integer::uintwide_t<static_cast<math::wide_integer::size_t>(UINT32_C(64)), std::uint32_t, void, true>;
      #endif

      result_is_ok = (from_pull_request_130::test_uintwide_t_spot_values_from_pull_request_130<type>() && result_is_ok);

      #if(WIDE_INTEGER_CONSTEXPR_IS_COMPILE_TIME_CONST == 1)
      static_assert(from_pull_request_130::test_uintwide_t_spot_values_from_pull_request_130<type>(), "Error: Check conditions surrounding issue 130");
      #endif
    }

    {
      #if defined(WIDE_INTEGER_NAMESPACE)
      using type = WIDE_INTEGER_NAMESPACE::math::wide_integer::uintwide_t<static_cast<WIDE_INTEGER_NAMESPACE::math::wide_integer::size_t>(UINT32_C(64)), std::uint8_t, void, true>;
      #else
      using type = ::math::wide_integer::uintwide_t<static_cast<math::wide_integer::size_t>(UINT32_C(64)), std::uint8_t, void, true>;
      #endif

      result_is_ok = (from_pull_request_130::test_uintwide_t_spot_values_from_pull_request_130<type>() && result_is_ok);

      #if(WIDE_INTEGER_CONSTEXPR_IS_COMPILE_TIME_CONST == 1)
      static_assert(from_pull_request_130::test_uintwide_t_spot_values_from_pull_request_130<type>(), "Error: Check conditions surrounding issue 130");
      #endif
    }

    {
      #if defined(WIDE_INTEGER_NAMESPACE)
      using type = WIDE_INTEGER_NAMESPACE::math::wide_integer::uintwide_t<static_cast<WIDE_INTEGER_NAMESPACE::math::wide_integer::size_t>(UINT32_C(256)), std::uint32_t, void, true>;
      #else
      using type = ::math::wide_integer::uintwide_t<static_cast<math::wide_integer::size_t>(UINT32_C(256)), std::uint32_t, void, true>;
      #endif

      result_is_ok = (from_pull_request_130::test_uintwide_t_spot_values_from_pull_request_130<type>() && result_is_ok);

      #if(WIDE_INTEGER_CONSTEXPR_IS_COMPILE_TIME_CONST == 1)
      static_assert(from_pull_request_130::test_uintwide_t_spot_values_from_pull_request_130<type>(), "Error: Check conditions surrounding issue 130");
      #endif
    }

    {
      using type = std::int32_t;

      result_is_ok = (from_pull_request_130::test_uintwide_t_spot_values_from_pull_request_130<type>() && result_is_ok);

      #if(WIDE_INTEGER_CONSTEXPR_IS_COMPILE_TIME_CONST == 1)
      static_assert(from_pull_request_130::test_uintwide_t_spot_values_from_pull_request_130<type>(), "Error: Check conditions surrounding issue 130");
      #endif
    }

    {
      using type = std::int64_t;

      result_is_ok = (from_pull_request_130::test_uintwide_t_spot_values_from_pull_request_130<type>() && result_is_ok);

      #if(WIDE_INTEGER_CONSTEXPR_IS_COMPILE_TIME_CONST == 1)
      static_assert(from_pull_request_130::test_uintwide_t_spot_values_from_pull_request_130<type>(), "Error: Check conditions surrounding issue 130");
      #endif
    }
  }

  {
    #if defined(WIDE_INTEGER_NAMESPACE)
    using WIDE_INTEGER_NAMESPACE::math::wide_integer::uint256_t;
    #else
    using ::math::wide_integer::uint256_t;
    #endif

    // FromDigits["C9DD3EA24800F584CB28C25CC0E6FF1",16]
    // 16770224695321632575655872732632870897
    WIDE_INTEGER_CONSTEXPR uint256_t a("0xC9DD3EA24800F584CB28C25CC0E6FF1");

    // FromDigits["1E934A2EEA60A2AD14ECCAE7AD82C069",16]
    // 40641612127094559121321599356729737321
    WIDE_INTEGER_CONSTEXPR uint256_t b("0x1E934A2EEA60A2AD14ECCAE7AD82C069");

    WIDE_INTEGER_CONSTEXPR auto v  = b - 1U;
    WIDE_INTEGER_CONSTEXPR auto lm = lcm(a - 1U, v);
    WIDE_INTEGER_CONSTEXPR auto gd = gcd(a - 1U, v);

    // LCM[16770224695321632575655872732632870897 - 1, 40641612127094559121321599356729737321 - 1]
    result_is_ok = ((lm == uint256_t("28398706972978513348490390087175345493497748446743697820448222113648043280")) && result_is_ok);

    #if(WIDE_INTEGER_CONSTEXPR_IS_COMPILE_TIME_CONST == 1)
    static_assert(lm == uint256_t("28398706972978513348490390087175345493497748446743697820448222113648043280"),
                  "Error: Rudimentary LCM calculation result is wrong");
    #endif

    // GCD[16770224695321632575655872732632870897 - 1, 40641612127094559121321599356729737321 - 1]
    result_is_ok = ((gd == static_cast<unsigned>(UINT8_C(24))) && result_is_ok);

    #if(WIDE_INTEGER_CONSTEXPR_IS_COMPILE_TIME_CONST == 1)
    static_assert(gd == static_cast<unsigned>(UINT8_C(24)),
                  "Error: Rudimentary LCM calculation result is wrong");
    #endif

    {
      // Check GCD(0, v) to be equal to v (found mssing in code coverage analyses).

      using local_limb_type = typename uint256_t::limb_type;

      const auto gd0 = gcd(uint256_t(static_cast<local_limb_type>(UINT8_C(0))), v);

      result_is_ok = ((gd0 == v) && result_is_ok);
    }
  }

  {
    // See also https://github.com/ckormanyos/wide-integer/issues/111

    {
      #if defined(WIDE_INTEGER_NAMESPACE)
      using WIDE_INTEGER_NAMESPACE::math::wide_integer::int256_t;
      #else
      using ::math::wide_integer::int256_t;
      #endif

      int256_t a("-578960446186580977117854925043439539266349923328202820197287920"
                 "03956564819968");

      int256_t a_itself = (std::numeric_limits<int256_t>::min)();

      result_is_ok = ((a == (std::numeric_limits<int256_t>::min)()) && result_is_ok);

      int256_t b("1");

      const int256_t a_div_b = a / b;

      result_is_ok = ((a_div_b      == a) && result_is_ok);
      result_is_ok = ((a / a_itself == b) && result_is_ok);
    }

    {
      #if defined(WIDE_INTEGER_NAMESPACE)
      using my_int32_t = WIDE_INTEGER_NAMESPACE::math::wide_integer::uintwide_t<static_cast<WIDE_INTEGER_NAMESPACE::math::wide_integer::size_t>(UINT32_C(32)), std::uint32_t, void, true>;
      #else
      using my_int32_t = ::math::wide_integer::uintwide_t<static_cast<math::wide_integer::size_t>(UINT32_C(32)), std::uint32_t, void, true>;
      #endif

      my_int32_t c("-2147483648"); // NOLINT(cppcoreguidelines-avoid-magic-numbers,readability-magic-numbers)

      my_int32_t c_itself = (std::numeric_limits<my_int32_t>::min)();

      result_is_ok = ((c == (std::numeric_limits<my_int32_t>::min)()) && result_is_ok);

      my_int32_t d("1");

      const my_int32_t c_div_d = c / d;

      result_is_ok = ((c / c_itself == d) && result_is_ok);
      result_is_ok = ((c_div_d      == c) && result_is_ok);
    }
  }

  {
    // See also https://github.com/ckormanyos/wide-integer/issues/108
    {
      #if defined(WIDE_INTEGER_NAMESPACE)
      using w_t  = WIDE_INTEGER_NAMESPACE::math::wide_integer::uintwide_t<static_cast<WIDE_INTEGER_NAMESPACE::math::wide_integer::size_t>(UINT32_C(32)), std::uint32_t, void, true>;
      using ww_t = WIDE_INTEGER_NAMESPACE::math::wide_integer::uintwide_t<static_cast<WIDE_INTEGER_NAMESPACE::math::wide_integer::size_t>(UINT32_C(64)), std::uint32_t, void, true>;
      #else
      using w_t  = ::math::wide_integer::uintwide_t<static_cast<math::wide_integer::size_t>(UINT32_C(32)), std::uint32_t, void, true>;
      using ww_t = ::math::wide_integer::uintwide_t<static_cast<math::wide_integer::size_t>(UINT32_C(64)), std::uint32_t, void, true>;
      #endif

      w_t  neg     (-2);
      ww_t neg_wide(-2);
      ww_t neg_wide_cast = ww_t(neg);

      std::string str_neg;
      std::string str_neg_wide;
      std::string str_neg_wide_cast;

      { std::stringstream strm; strm << neg;           str_neg           = strm.str(); }
      { std::stringstream strm; strm << neg_wide;      str_neg_wide      = strm.str(); }
      { std::stringstream strm; strm << neg_wide_cast; str_neg_wide_cast = strm.str(); }

      const bool result_neg_is_ok           = (str_neg           == "-2");
      const bool result_neg_wide_is_ok      = (str_neg_wide      == "-2");
      const bool result_neg_wide_cast_is_ok = (str_neg_wide_cast == "-2");

      result_is_ok = ((   result_neg_is_ok
                       && result_neg_wide_is_ok
                       && result_neg_wide_cast_is_ok) && result_is_ok);
    }

    {
      #if defined(WIDE_INTEGER_NAMESPACE)
      using w_t  = WIDE_INTEGER_NAMESPACE::math::wide_integer::uintwide_t<static_cast<WIDE_INTEGER_NAMESPACE::math::wide_integer::size_t>(UINT32_C(32)), std::uint8_t, void, true>;
      using ww_t = WIDE_INTEGER_NAMESPACE::math::wide_integer::uintwide_t<static_cast<WIDE_INTEGER_NAMESPACE::math::wide_integer::size_t>(UINT32_C(64)), std::uint8_t, void, true>;
      #else
      using w_t  = ::math::wide_integer::uintwide_t<static_cast<math::wide_integer::size_t>(UINT32_C(32)), std::uint8_t, void, true>;
      using ww_t = ::math::wide_integer::uintwide_t<static_cast<math::wide_integer::size_t>(UINT32_C(64)), std::uint8_t, void, true>;
      #endif

      w_t  neg     (-2);
      ww_t neg_wide(-2);
      ww_t neg_wide_cast = ww_t(neg);

      std::string str_neg;
      std::string str_neg_wide;
      std::string str_neg_wide_cast;

      { std::stringstream strm; strm << neg;           str_neg           = strm.str(); }
      { std::stringstream strm; strm << neg_wide;      str_neg_wide      = strm.str(); }
      { std::stringstream strm; strm << neg_wide_cast; str_neg_wide_cast = strm.str(); }

      const bool result_neg_is_ok           = (str_neg           == "-2");
      const bool result_neg_wide_is_ok      = (str_neg_wide      == "-2");
      const bool result_neg_wide_cast_is_ok = (str_neg_wide_cast == "-2");

      result_is_ok = ((   result_neg_is_ok
                       && result_neg_wide_is_ok
                       && result_neg_wide_cast_is_ok) && result_is_ok);
    }
  }

  {
    // See also https://github.com/ckormanyos/wide-integer/issues/63

    WIDE_INTEGER_CONSTEXPR auto
    input
    {
      #if defined(WIDE_INTEGER_NAMESPACE)
      WIDE_INTEGER_NAMESPACE::math::wide_integer::uintwide_t<static_cast<WIDE_INTEGER_NAMESPACE::math::wide_integer::size_t>(UINT32_C(320)), std::uint32_t, void, true>
      #else
      ::math::wide_integer::uintwide_t<static_cast<math::wide_integer::size_t>(UINT32_C(320)), std::uint32_t, void, true>
      #endif
      {
        1729348762983LL // NOLINT(cppcoreguidelines-avoid-magic-numbers,readability-magic-numbers)
      }
    };

    WIDE_INTEGER_CONSTEXPR bool result_ll_is_ok = (static_cast<long long>(input) == 1729348762983LL); // NOLINT(cppcoreguidelines-avoid-magic-numbers,readability-magic-numbers,google-runtime-int)

    #if defined(WIDE_INTEGER_CONSTEXPR_IS_COMPILE_TIME_CONST) && (WIDE_INTEGER_CONSTEXPR_IS_COMPILE_TIME_CONST != 0)
    static_assert(result_ll_is_ok, "Error: test_uintwide_t_spot_values unsigned not OK!");
    #endif

    result_is_ok = (result_ll_is_ok && result_is_ok);
  }

  {
    #if defined(WIDE_INTEGER_NAMESPACE)
    using WIDE_INTEGER_NAMESPACE::math::wide_integer::uint512_t;
    #else
    using ::math::wide_integer::uint512_t;
    #endif

    WIDE_INTEGER_CONSTEXPR uint512_t a("698937339790347543053797400564366118744312537138445607919548628175822115805812983955794321304304417541511379093392776018867245622409026835324102460829431");
    WIDE_INTEGER_CONSTEXPR uint512_t b("100041341335406267530943777943625254875702684549707174207105689918734693139781");

    WIDE_INTEGER_CONSTEXPR uint512_t c = (a / b);
    WIDE_INTEGER_CONSTEXPR uint512_t d = (a % b);

    //   QuotientRemainder
    //     [698937339790347543053797400564366118744312537138445607919548628175822115805812983955794321304304417541511379093392776018867245622409026835324102460829431,
    //      100041341335406267530943777943625254875702684549707174207105689918734693139781]
    //
    //     {6986485091668619828842978360442127600954041171641881730123945989288792389271,
    //      100041341335406267530943777943625254875702684549707174207105689918734693139780}

    WIDE_INTEGER_CONSTEXPR bool c_is_ok = (c == "6986485091668619828842978360442127600954041171641881730123945989288792389271");
    WIDE_INTEGER_CONSTEXPR bool d_is_ok = (d == "100041341335406267530943777943625254875702684549707174207105689918734693139780");

    result_is_ok = ((c_is_ok && d_is_ok) && result_is_ok);

    #if(WIDE_INTEGER_CONSTEXPR_IS_COMPILE_TIME_CONST == 1)
    static_assert(c_is_ok, "Error: Static check of spot value division is not OK");
    static_assert(d_is_ok, "Error: Static check of spot value remainder is not OK");
    #endif
  }

  {
    #if defined(WIDE_INTEGER_NAMESPACE)
    using WIDE_INTEGER_NAMESPACE::math::wide_integer::uint256_t;
    #else
    using ::math::wide_integer::uint256_t;
    #endif

    static_assert(std::numeric_limits<uint256_t>::digits == 256, // NOLINT(cppcoreguidelines-avoid-magic-numbers,readability-magic-numbers)
                  "Error: Incorrect digit count for this example");

    // Note: Some of the comments in this file use the Wolfram Language(TM).
    //
    // Create two pseudo-random 256-bit unsigned integers.
    //   Table[IntegerString[RandomInteger[(2^256) - 1], 16], 2]
    //
    //   {F4DF741DE58BCB2F37F18372026EF9CBCFC456CB80AF54D53BDEED78410065DE,
    //    166D63E0202B3D90ECCEAA046341AB504658F55B974A7FD63733ECF89DD0DF75}
    //
    // Set the values of two random 256-bit unsigned integers.
    //   a = 0xF4DF741DE58BCB2F37F18372026EF9CBCFC456CB80AF54D53BDEED78410065DE
    //   b = 0x166D63E0202B3D90ECCEAA046341AB504658F55B974A7FD63733ECF89DD0DF75
    //
    // Multiply:
    //   a * b = 0xE491A360C57EB4306C61F9A04F7F7D99BE3676AAD2D71C5592D5AE70F84AF076
    //
    // Divide:
    //   a / b = 10
    //
    // Modulus:
    //   a % b = 0x14998D5CA3DB6385F7DEDF4621DE48A9104AC13797C6567713D7ABC216D7AB4C

    WIDE_INTEGER_CONSTEXPR uint256_t a("0xF4DF741DE58BCB2F37F18372026EF9CBCFC456CB80AF54D53BDEED78410065DE");

    WIDE_INTEGER_CONSTEXPR uint256_t b("0x166D63E0202B3D90ECCEAA046341AB504658F55B974A7FD63733ECF89DD0DF75");

    WIDE_INTEGER_CONSTEXPR uint256_t c("0xE491A360C57EB4306C61F9A04F7F7D99BE3676AAD2D71C5592D5AE70F84AF076");

    WIDE_INTEGER_CONSTEXPR auto result_mul_is_ok = ((a * b) == c);

    #if(WIDE_INTEGER_CONSTEXPR_IS_COMPILE_TIME_CONST == 1)
    static_assert(result_mul_is_ok, "Error: Static check of spot value multiplication is not OK");
    #endif

    result_is_ok = (result_mul_is_ok && result_is_ok);

    WIDE_INTEGER_CONSTEXPR uint256_t q(10U); // NOLINT(cppcoreguidelines-avoid-magic-numbers,readability-magic-numbers)

    WIDE_INTEGER_CONSTEXPR auto result_div_is_ok = ((a / b) == q);

    #if(WIDE_INTEGER_CONSTEXPR_IS_COMPILE_TIME_CONST == 1)
    static_assert(result_div_is_ok, "Error: Static check of spot value division is not OK");
    #endif

    result_is_ok = (result_div_is_ok && result_is_ok);

    WIDE_INTEGER_CONSTEXPR uint256_t m("0x14998D5CA3DB6385F7DEDF4621DE48A9104AC13797C6567713D7ABC216D7AB4C");

    WIDE_INTEGER_CONSTEXPR auto result_mod_is_ok = ((a % b) == m);

    #if(WIDE_INTEGER_CONSTEXPR_IS_COMPILE_TIME_CONST == 1)
    static_assert(result_mod_is_ok, "Error: Static check of spot value modulus is not OK");
    #endif

    result_is_ok = (result_mod_is_ok && result_is_ok);

    {
      // See also: https://github.com/ckormanyos/wide-integer/issues/274

      WIDE_INTEGER_CONSTEXPR auto result_mod1_is_ok  = ((a % 1) == 0);
      WIDE_INTEGER_CONSTEXPR auto result_mod7u_is_ok = ((a % 7U) == 3U); // NOLINT(cppcoreguidelines-avoid-magic-numbers,readability-magic-numbers)

      #if(WIDE_INTEGER_CONSTEXPR_IS_COMPILE_TIME_CONST == 1)
      static_assert(result_mod1_is_ok,  "Error: Static check of spot value modulus with 1 is not OK");
      static_assert(result_mod7u_is_ok, "Error: Static check of spot value modulus with 7U is not OK");
      #endif

      result_is_ok = ((result_mod1_is_ok && result_mod7u_is_ok) && result_is_ok);
    }
  }

  return result_is_ok;
}

#if defined(WIDE_INTEGER_NAMESPACE)
auto WIDE_INTEGER_NAMESPACE::math::wide_integer::test_uintwide_t_spot_values() -> bool // NOLINT(readability-function-cognitive-complexity)
#else
auto ::math::wide_integer::test_uintwide_t_spot_values() -> bool // NOLINT(readability-function-cognitive-complexity)
#endif
{
  return local_test_spot_values::test();
}
