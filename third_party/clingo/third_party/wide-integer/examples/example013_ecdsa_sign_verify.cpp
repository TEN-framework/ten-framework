///////////////////////////////////////////////////////////////////
//  Copyright Christopher Kormanyos 2023.                        //
//  Distributed under the Boost Software License,                //
//  Version 1.0. (See accompanying file LICENSE_1_0.txt          //
//  or copy at http://www.boost.org/LICENSE_1_0.txt)             //
///////////////////////////////////////////////////////////////////

// This work uses (significantly) translated and modified parts
// of andreacorbellini/ecc
//   see also: https://github.com/andreacorbellini/ecc
//   and also: https://github.com/andreacorbellini/ecc/blob/master/scripts/ecdsa.py

// Full original andreacorbellini/ecc copyright information follows.
/*----------------------------------------------------------------------------

The MIT License (MIT)

Copyright (c) 2015 Andrea Corbellini

Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in
all copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
THE SOFTWARE.

-----------------------------------------------------------------------------*/

// For algorithm description of ECDSA, please consult also:
//   D. Hankerson, A. Menezes, S. Vanstone, "Guide to Elliptic
//   Curve Cryptography", Springer 2004, Chapter 4, in particular
//   Algorithm 4.24 (keygen on page 180), and Algorithms 4.29 and 4.30.
//   Complete descriptions of sign/verify are featured on page 184.

// For another algorithm description of ECDSA,
//   see also: https://nvlpubs.nist.gov/nistpubs/FIPS/NIST.FIPS.186-5.pdf

// For algorithm description of SHA-2 HASH-256,
//   see also: https://nvlpubs.nist.gov/nistpubs/FIPS/NIST.FIPS.180-4.pdf

// The SHA-2 HASH-256 implementation has been taken (with slight modification)
//   from: https://github.com/imahjoub/hash_sha256

#include <algorithm>
#include <array>
#include <cstdint>
#include <random>
#include <string>
#include <utility>
#include <vector>

#include <examples/example_uintwide_t.h>
#include <math/wide_integer/uintwide_t.h>

namespace example013_ecdsa
{
  class hash_sha256
  {
  public:
    using result_type = std::array<std::uint8_t, static_cast<std::size_t>(UINT8_C(32))>;

    // LCOV_EXCL_START
    constexpr hash_sha256()                       = default;
    constexpr hash_sha256(const hash_sha256&)     = default;
    constexpr hash_sha256(hash_sha256&&) noexcept = default;
    ~hash_sha256() = default;

    constexpr auto operator=(hash_sha256&&) noexcept -> hash_sha256& = default;
    constexpr auto operator=(const hash_sha256&) ->     hash_sha256& = default;
    // LCOV_EXCL_STOP

    WIDE_INTEGER_CONSTEXPR auto hash(const std::uint8_t* msg, const size_t length) -> result_type
    {
      init();
      update(msg, length);
      return finalize();
    }

    WIDE_INTEGER_CONSTEXPR void init()
    {
      my_datalen = static_cast<std::uint32_t>(UINT8_C(0));
      my_bitlen  = static_cast<std::uint64_t>(UINT8_C(0));

      transform_context[static_cast<std::size_t>(UINT8_C(0))] = static_cast<std::uint32_t>(UINT32_C(0x6A09E667));
      transform_context[static_cast<std::size_t>(UINT8_C(1))] = static_cast<std::uint32_t>(UINT32_C(0xBB67AE85));
      transform_context[static_cast<std::size_t>(UINT8_C(2))] = static_cast<std::uint32_t>(UINT32_C(0x3C6EF372));
      transform_context[static_cast<std::size_t>(UINT8_C(3))] = static_cast<std::uint32_t>(UINT32_C(0xA54FF53A));
      transform_context[static_cast<std::size_t>(UINT8_C(4))] = static_cast<std::uint32_t>(UINT32_C(0x510E527F));
      transform_context[static_cast<std::size_t>(UINT8_C(5))] = static_cast<std::uint32_t>(UINT32_C(0x9B05688C));
      transform_context[static_cast<std::size_t>(UINT8_C(6))] = static_cast<std::uint32_t>(UINT32_C(0x1F83D9AB));
      transform_context[static_cast<std::size_t>(UINT8_C(7))] = static_cast<std::uint32_t>(UINT32_C(0x5BE0CD19));
    }

    WIDE_INTEGER_CONSTEXPR void update(const std::uint8_t* msg, const size_t length)
    {
      for (auto i = static_cast<std::size_t>(UINT8_C(0)); i < length; ++i)
      {
        my_data[my_datalen] = msg[i]; // NOLINT(cppcoreguidelines-pro-bounds-pointer-arithmetic,cppcoreguidelines-pro-bounds-constant-array-index)
        my_datalen++;

        if(my_datalen == static_cast<std::uint32_t>(UINT8_C(64)))
        {
          // LCOV_EXCL_START
          sha256_transform();

          my_datalen = static_cast<std::uint32_t>(UINT8_C(0));

          my_bitlen = static_cast<std::uint64_t>(my_bitlen + static_cast<std::uint_fast16_t>(UINT16_C(512)));
          // LCOV_EXCL_STOP
        }
      }
    }

    WIDE_INTEGER_CONSTEXPR auto finalize() -> result_type
    {
      result_type hash_result { };

      auto hash_index = static_cast<std::size_t>(my_datalen);

      my_data[hash_index] = static_cast<std::uint8_t>(UINT8_C(0x80)); // NOLINT(cppcoreguidelines-pro-bounds-pointer-arithmetic,cppcoreguidelines-pro-bounds-constant-array-index)

      ++hash_index;

      // Pad whatever data is left in the buffer.
      if(my_datalen < static_cast<std::uint32_t>(UINT8_C(56U)))
      {
        std::fill((my_data.begin() + hash_index), (my_data.begin() + static_cast<std::size_t>(UINT8_C(56))), static_cast<std::uint8_t>(UINT8_C(0)));
      }
      else
      {
        // LCOV_EXCL_START
        std::fill((my_data.begin() + hash_index), my_data.end(), static_cast<std::uint8_t>(UINT8_C(0)));

        sha256_transform();

        std::fill_n(my_data.begin(), static_cast<std::size_t>(UINT8_C(56)), static_cast<std::uint8_t>(UINT8_C(0)));
        // LCOV_EXCL_STOP
      }

      // Append to the padding the total message length (in bits) and subsequently transform.
      my_bitlen =
        static_cast<std::uint64_t>
        (
            my_bitlen
          + static_cast<std::uint64_t>
            (
              static_cast<std::uint64_t>(my_datalen) * static_cast<std::uint8_t>(UINT8_C(8))
            )
        );

      my_data[static_cast<std::size_t>(UINT8_C(63))] = static_cast<std::uint8_t>(my_bitlen >> static_cast<unsigned>(UINT8_C( 0)));
      my_data[static_cast<std::size_t>(UINT8_C(62))] = static_cast<std::uint8_t>(my_bitlen >> static_cast<unsigned>(UINT8_C( 8)));
      my_data[static_cast<std::size_t>(UINT8_C(61))] = static_cast<std::uint8_t>(my_bitlen >> static_cast<unsigned>(UINT8_C(16)));
      my_data[static_cast<std::size_t>(UINT8_C(60))] = static_cast<std::uint8_t>(my_bitlen >> static_cast<unsigned>(UINT8_C(24)));
      my_data[static_cast<std::size_t>(UINT8_C(59))] = static_cast<std::uint8_t>(my_bitlen >> static_cast<unsigned>(UINT8_C(32)));
      my_data[static_cast<std::size_t>(UINT8_C(58))] = static_cast<std::uint8_t>(my_bitlen >> static_cast<unsigned>(UINT8_C(40)));
      my_data[static_cast<std::size_t>(UINT8_C(57))] = static_cast<std::uint8_t>(my_bitlen >> static_cast<unsigned>(UINT8_C(48)));
      my_data[static_cast<std::size_t>(UINT8_C(56))] = static_cast<std::uint8_t>(my_bitlen >> static_cast<unsigned>(UINT8_C(56)));

      sha256_transform();

      // Since this implementation uses little endian byte ordering and SHA uses big endian,
      // reverse all the bytes when copying the final transform_context to the output hash.
      constexpr auto conversion_scale =
        static_cast<std::size_t>
        (
            std::numeric_limits<typename transform_context_type::value_type>::digits
          / std::numeric_limits<std::uint8_t>::digits
        );

      for(auto   output_index = static_cast<std::size_t>(UINT8_C(0));
                 output_index < std::tuple_size<result_type>::value;
               ++output_index)
      {
        const auto right_shift_amount =
          static_cast<std::size_t>
          (
            static_cast<std::size_t>
            (
                static_cast<std::size_t>
                (
                    static_cast<std::size_t>(conversion_scale - static_cast<std::size_t>(UINT8_C(1)))
                  - static_cast<std::size_t>(output_index % conversion_scale)
                )
              * static_cast<std::size_t>(UINT8_C(8))
            )
          );

        hash_result[output_index] =
          static_cast<std::uint8_t>
          (
            transform_context[(output_index / conversion_scale)] >> right_shift_amount
          );
      }

      return hash_result;
    }

  private:
    using transform_context_type = std::array<std::uint32_t, static_cast<std::size_t>(UINT8_C(8))>;

    std::uint32_t my_datalen { }; // NOLINT(readability-identifier-naming)
    std::uint64_t my_bitlen  { }; // NOLINT(readability-identifier-naming)

    std::array<std::uint8_t, static_cast<std::size_t>(UINT8_C(64))> my_data { }; // NOLINT(readability-identifier-naming)

    transform_context_type transform_context { }; // NOLINT(readability-identifier-naming)

    WIDE_INTEGER_CONSTEXPR auto sha256_transform() -> void
    {
      std::array<std::uint32_t, static_cast<std::size_t>(UINT8_C(64))> m { };

      for(auto   i = static_cast<std::size_t>(UINT8_C(0)), j = static_cast<std::size_t>(UINT8_C(0));
                 i < static_cast<std::size_t>(UINT8_C(16));
               ++i, j = static_cast<std::size_t>(j + static_cast<std::size_t>(UINT8_C(4))))
      {
        m[i] = // NOLINT(cppcoreguidelines-pro-bounds-pointer-arithmetic,cppcoreguidelines-pro-bounds-constant-array-index)
          static_cast<std::uint32_t>
          (
              static_cast<std::uint32_t>(static_cast<std::uint32_t>(my_data[j + static_cast<std::size_t>(UINT8_C(0))]) << static_cast<unsigned>(UINT8_C(24))) // NOLINT(cppcoreguidelines-pro-bounds-pointer-arithmetic,cppcoreguidelines-pro-bounds-constant-array-index)
            | static_cast<std::uint32_t>(static_cast<std::uint32_t>(my_data[j + static_cast<std::size_t>(UINT8_C(1))]) << static_cast<unsigned>(UINT8_C(16))) // NOLINT(cppcoreguidelines-pro-bounds-pointer-arithmetic,cppcoreguidelines-pro-bounds-constant-array-index)
            | static_cast<std::uint32_t>(static_cast<std::uint32_t>(my_data[j + static_cast<std::size_t>(UINT8_C(2))]) << static_cast<unsigned>(UINT8_C( 8))) // NOLINT(cppcoreguidelines-pro-bounds-pointer-arithmetic,cppcoreguidelines-pro-bounds-constant-array-index)
            | static_cast<std::uint32_t>(static_cast<std::uint32_t>(my_data[j + static_cast<std::size_t>(UINT8_C(3))]) << static_cast<unsigned>(UINT8_C( 0))) // NOLINT(cppcoreguidelines-pro-bounds-pointer-arithmetic,cppcoreguidelines-pro-bounds-constant-array-index)
          );
      }

      for(auto i = static_cast<std::size_t>(UINT8_C(16)) ; i < static_cast<std::size_t>(UINT8_C(64)); ++i)
      {
        m[i] = ssig1(m[i - static_cast<std::size_t>(UINT8_C(2))]) + m[i - static_cast<std::size_t>(UINT8_C(7))] + ssig0(m[i - static_cast<std::size_t>(UINT8_C(15))]) + m[i - static_cast<std::size_t>(UINT8_C(16))]; // NOLINT(cppcoreguidelines-pro-bounds-pointer-arithmetic,cppcoreguidelines-pro-bounds-constant-array-index)
      }

      constexpr std::array<std::uint32_t, 64U> transform_constants =
      {
        static_cast<std::uint32_t>(UINT32_C(0x428A2F98)), static_cast<std::uint32_t>(UINT32_C(0x71374491)), static_cast<std::uint32_t>(UINT32_C(0xB5C0FBCF)), static_cast<std::uint32_t>(UINT32_C(0xE9B5DBA5)),
        static_cast<std::uint32_t>(UINT32_C(0x3956C25B)), static_cast<std::uint32_t>(UINT32_C(0x59F111F1)), static_cast<std::uint32_t>(UINT32_C(0x923F82A4)), static_cast<std::uint32_t>(UINT32_C(0xAB1C5ED5)),
        static_cast<std::uint32_t>(UINT32_C(0xD807AA98)), static_cast<std::uint32_t>(UINT32_C(0x12835B01)), static_cast<std::uint32_t>(UINT32_C(0x243185BE)), static_cast<std::uint32_t>(UINT32_C(0x550C7DC3)),
        static_cast<std::uint32_t>(UINT32_C(0x72BE5D74)), static_cast<std::uint32_t>(UINT32_C(0x80DEB1FE)), static_cast<std::uint32_t>(UINT32_C(0x9BDC06A7)), static_cast<std::uint32_t>(UINT32_C(0xC19BF174)),
        static_cast<std::uint32_t>(UINT32_C(0xE49B69C1)), static_cast<std::uint32_t>(UINT32_C(0xEFBE4786)), static_cast<std::uint32_t>(UINT32_C(0x0FC19DC6)), static_cast<std::uint32_t>(UINT32_C(0x240CA1CC)),
        static_cast<std::uint32_t>(UINT32_C(0x2DE92C6F)), static_cast<std::uint32_t>(UINT32_C(0x4A7484AA)), static_cast<std::uint32_t>(UINT32_C(0x5CB0A9DC)), static_cast<std::uint32_t>(UINT32_C(0x76F988DA)),
        static_cast<std::uint32_t>(UINT32_C(0x983E5152)), static_cast<std::uint32_t>(UINT32_C(0xA831C66D)), static_cast<std::uint32_t>(UINT32_C(0xB00327C8)), static_cast<std::uint32_t>(UINT32_C(0xBF597FC7)),
        static_cast<std::uint32_t>(UINT32_C(0xC6E00BF3)), static_cast<std::uint32_t>(UINT32_C(0xD5A79147)), static_cast<std::uint32_t>(UINT32_C(0x06CA6351)), static_cast<std::uint32_t>(UINT32_C(0x14292967)),
        static_cast<std::uint32_t>(UINT32_C(0x27B70A85)), static_cast<std::uint32_t>(UINT32_C(0x2E1B2138)), static_cast<std::uint32_t>(UINT32_C(0x4D2C6DFC)), static_cast<std::uint32_t>(UINT32_C(0x53380D13)),
        static_cast<std::uint32_t>(UINT32_C(0x650A7354)), static_cast<std::uint32_t>(UINT32_C(0x766A0ABB)), static_cast<std::uint32_t>(UINT32_C(0x81C2C92E)), static_cast<std::uint32_t>(UINT32_C(0x92722C85)),
        static_cast<std::uint32_t>(UINT32_C(0xA2BFE8A1)), static_cast<std::uint32_t>(UINT32_C(0xA81A664B)), static_cast<std::uint32_t>(UINT32_C(0xC24B8B70)), static_cast<std::uint32_t>(UINT32_C(0xC76C51A3)),
        static_cast<std::uint32_t>(UINT32_C(0xD192E819)), static_cast<std::uint32_t>(UINT32_C(0xD6990624)), static_cast<std::uint32_t>(UINT32_C(0xF40E3585)), static_cast<std::uint32_t>(UINT32_C(0x106AA070)),
        static_cast<std::uint32_t>(UINT32_C(0x19A4C116)), static_cast<std::uint32_t>(UINT32_C(0x1E376C08)), static_cast<std::uint32_t>(UINT32_C(0x2748774C)), static_cast<std::uint32_t>(UINT32_C(0x34B0BCB5)),
        static_cast<std::uint32_t>(UINT32_C(0x391C0CB3)), static_cast<std::uint32_t>(UINT32_C(0x4ED8AA4A)), static_cast<std::uint32_t>(UINT32_C(0x5B9CCA4F)), static_cast<std::uint32_t>(UINT32_C(0x682E6FF3)),
        static_cast<std::uint32_t>(UINT32_C(0x748F82EE)), static_cast<std::uint32_t>(UINT32_C(0x78A5636F)), static_cast<std::uint32_t>(UINT32_C(0x84C87814)), static_cast<std::uint32_t>(UINT32_C(0x8CC70208)),
        static_cast<std::uint32_t>(UINT32_C(0x90BEFFFA)), static_cast<std::uint32_t>(UINT32_C(0xA4506CEB)), static_cast<std::uint32_t>(UINT32_C(0xBEF9A3F7)), static_cast<std::uint32_t>(UINT32_C(0xC67178F2))
      };

      transform_context_type state = transform_context;

      for(auto i = static_cast<std::size_t>(UINT8_C(0)); i < static_cast<std::size_t>(UINT8_C(64)); ++i)
      {
        const auto tmp1 =
          static_cast<std::uint32_t>
          (
              state[static_cast<std::size_t>(UINT8_C(7))]
            + bsig1(state[static_cast<std::size_t>(UINT8_C(4))])
            + ch(state[static_cast<std::size_t>(UINT8_C(4))], state[static_cast<std::size_t>(UINT8_C(5))], state[static_cast<std::size_t>(UINT8_C(6))])
            + transform_constants[i] // NOLINT(cppcoreguidelines-pro-bounds-constant-array-index)
            + m[i]                   // NOLINT(cppcoreguidelines-pro-bounds-constant-array-index)
          );

        const auto tmp2 =
          static_cast<std::uint32_t>
          (
              bsig0(state[static_cast<std::size_t>(UINT8_C(0))])
            + maj(state[static_cast<std::size_t>(UINT8_C(0))], state[static_cast<std::size_t>(UINT8_C(1))], state[static_cast<std::size_t>(UINT8_C(2))])
          );

        state[static_cast<std::size_t>(UINT8_C(7))] = state[static_cast<std::size_t>(UINT8_C(6))];
        state[static_cast<std::size_t>(UINT8_C(6))] = state[static_cast<std::size_t>(UINT8_C(5))];
        state[static_cast<std::size_t>(UINT8_C(5))] = state[static_cast<std::size_t>(UINT8_C(4))];
        state[static_cast<std::size_t>(UINT8_C(4))] = state[static_cast<std::size_t>(UINT8_C(3))] + tmp1;
        state[static_cast<std::size_t>(UINT8_C(3))] = state[static_cast<std::size_t>(UINT8_C(2))];
        state[static_cast<std::size_t>(UINT8_C(2))] = state[static_cast<std::size_t>(UINT8_C(1))];
        state[static_cast<std::size_t>(UINT8_C(1))] = state[static_cast<std::size_t>(UINT8_C(0))];
        state[static_cast<std::size_t>(UINT8_C(0))] = static_cast<std::uint32_t>(tmp1 + tmp2);
      }

      transform_context[static_cast<std::size_t>(UINT8_C(0))] += state[static_cast<std::size_t>(UINT8_C(0))];
      transform_context[static_cast<std::size_t>(UINT8_C(1))] += state[static_cast<std::size_t>(UINT8_C(1))];
      transform_context[static_cast<std::size_t>(UINT8_C(2))] += state[static_cast<std::size_t>(UINT8_C(2))];
      transform_context[static_cast<std::size_t>(UINT8_C(3))] += state[static_cast<std::size_t>(UINT8_C(3))];
      transform_context[static_cast<std::size_t>(UINT8_C(4))] += state[static_cast<std::size_t>(UINT8_C(4))];
      transform_context[static_cast<std::size_t>(UINT8_C(5))] += state[static_cast<std::size_t>(UINT8_C(5))];
      transform_context[static_cast<std::size_t>(UINT8_C(6))] += state[static_cast<std::size_t>(UINT8_C(6))];
      transform_context[static_cast<std::size_t>(UINT8_C(7))] += state[static_cast<std::size_t>(UINT8_C(7))];
    }

    static constexpr auto rotl(std::uint32_t a, unsigned b) -> std::uint32_t { return (static_cast<std::uint32_t>(a << b) | static_cast<std::uint32_t>(a >> (static_cast<unsigned>(UINT8_C(32)) - b))); }
    static constexpr auto rotr(std::uint32_t a, unsigned b) -> std::uint32_t { return (static_cast<std::uint32_t>(a >> b) | static_cast<std::uint32_t>(a << (static_cast<unsigned>(UINT8_C(32)) - b))); }

    static constexpr auto ch (std::uint32_t x, std::uint32_t y, std::uint32_t z) -> std::uint32_t { return (static_cast<std::uint32_t>(x & y) ^ static_cast<std::uint32_t>(~x & z)); }
    static constexpr auto maj(std::uint32_t x, std::uint32_t y, std::uint32_t z) -> std::uint32_t { return (static_cast<std::uint32_t>(x & y) ^ static_cast<std::uint32_t>(x & z) ^ static_cast<std::uint32_t>(y & z)); }

    static constexpr auto bsig0(std::uint32_t x) -> std::uint32_t { return (rotr(x, static_cast<unsigned>(UINT8_C( 2))) ^ rotr(x, static_cast<unsigned>(UINT8_C(13))) ^ rotr(x,   static_cast<unsigned>(UINT8_C(22)))); }
    static constexpr auto bsig1(std::uint32_t x) -> std::uint32_t { return (rotr(x, static_cast<unsigned>(UINT8_C( 6))) ^ rotr(x, static_cast<unsigned>(UINT8_C(11))) ^ rotr(x,   static_cast<unsigned>(UINT8_C(25)))); }
    static constexpr auto ssig0(std::uint32_t x) -> std::uint32_t { return (rotr(x, static_cast<unsigned>(UINT8_C( 7))) ^ rotr(x, static_cast<unsigned>(UINT8_C(18))) ^     (x >> static_cast<unsigned>(UINT8_C( 3)))); }
    static constexpr auto ssig1(std::uint32_t x) -> std::uint32_t { return (rotr(x, static_cast<unsigned>(UINT8_C(17))) ^ rotr(x, static_cast<unsigned>(UINT8_C(19))) ^     (x >> static_cast<unsigned>(UINT8_C(10)))); }
  };

  template<const unsigned CurveBits,
           typename LimbType,
           const char* CoordX,
           const char* CoordY>
  struct ecc_point
  {
    #if defined(WIDE_INTEGER_NAMESPACE)
    using uint_type = WIDE_INTEGER_NAMESPACE::math::wide_integer::uintwide_t<static_cast<WIDE_INTEGER_NAMESPACE::math::wide_integer::size_t>(CurveBits), LimbType, void, false>;
    #else
    using uint_type = ::math::wide_integer::uintwide_t<static_cast<::math::wide_integer::size_t>(CurveBits), LimbType, void, false>;
    #endif

    #if defined(WIDE_INTEGER_NAMESPACE)
    using double_sint_type = WIDE_INTEGER_NAMESPACE::math::wide_integer::uintwide_t<static_cast<WIDE_INTEGER_NAMESPACE::math::wide_integer::size_t>(std::numeric_limits<uint_type>::digits * static_cast<int>(INT8_C(2))), LimbType, void, true>;
    #else
    using double_sint_type = ::math::wide_integer::uintwide_t<static_cast<::math::wide_integer::size_t>(std::numeric_limits<uint_type>::digits * static_cast<int>(INT8_C(2))), LimbType, void, true>;
    #endif

    static_assert(static_cast<unsigned>(std::numeric_limits<uint_type>::digits) == CurveBits,
                  "Error: Wrong number of bits in the smallest unsigned type of the point");

    using limb_type = typename uint_type::limb_type;

    using point_type =
      struct point_type
      {
        constexpr point_type(double_sint_type x = static_cast<double_sint_type>(static_cast<unsigned>(UINT8_C(0))), // NOLINT(google-explicit-constructor,hicpp-explicit-conversions,bugprone-easily-swappable-parameters)
                             double_sint_type y = static_cast<double_sint_type>(static_cast<unsigned>(UINT8_C(0)))) noexcept
          : my_x(x),
            my_y(y) { } // LCOV_EXCL_LINE

        double_sint_type my_x; // NOLINT(misc-non-private-member-variables-in-classes)
        double_sint_type my_y; // NOLINT(misc-non-private-member-variables-in-classes)
      };
  };

  template<const unsigned CurveBits,
           typename LimbType,
           const char* CurveName,
           const char* FieldCharacteristicP,
           const char* CurveCoefficientA,
           const char* CurveCoefficientB,
           const char* CoordGx,
           const char* CoordGy,
           const char* SubGroupOrderN,
           const int   SubGroupCoFactorH>
  struct elliptic_curve : public ecc_point<CurveBits, LimbType, CoordGx, CoordGy>
  {
    using base_class_type = ecc_point<CurveBits, LimbType, CoordGx, CoordGy>; // NOLINT(cppcoreguidelines-pro-bounds-array-to-pointer-decay,hicpp-no-array-decay)

    using point_type       = typename base_class_type::point_type;
    using uint_type        = typename base_class_type::uint_type;
    using double_sint_type = typename base_class_type::double_sint_type;
    using limb_type        = typename base_class_type::limb_type;

    using keypair_type = std::pair<uint_type, std::pair<uint_type, uint_type>>;

    #if defined(WIDE_INTEGER_NAMESPACE)
    using quadruple_sint_type = WIDE_INTEGER_NAMESPACE::math::wide_integer::uintwide_t<static_cast<WIDE_INTEGER_NAMESPACE::math::wide_integer::size_t>(std::numeric_limits<uint_type>::digits * static_cast<int>(INT8_C(4))), limb_type, void, true>;
    #else
    using quadruple_sint_type = ::math::wide_integer::uintwide_t<static_cast<::math::wide_integer::size_t>(std::numeric_limits<uint_type>::digits * static_cast<int>(INT8_C(4))), limb_type, void, true>;
    #endif

    #if defined(WIDE_INTEGER_NAMESPACE)
    using sexatuple_sint_type = WIDE_INTEGER_NAMESPACE::math::wide_integer::uintwide_t<static_cast<WIDE_INTEGER_NAMESPACE::math::wide_integer::size_t>(std::numeric_limits<uint_type>::digits * static_cast<int>(INT8_C(6))), limb_type, void, true>;
    #else
    using sexatuple_sint_type = ::math::wide_integer::uintwide_t<static_cast<::math::wide_integer::size_t>(std::numeric_limits<uint_type>::digits * static_cast<int>(INT8_C(6))), limb_type, void, true>;
    #endif

    #if defined(WIDE_INTEGER_NAMESPACE)
    using duodectuple_sint_type = WIDE_INTEGER_NAMESPACE::math::wide_integer::uintwide_t<static_cast<WIDE_INTEGER_NAMESPACE::math::wide_integer::size_t>(std::numeric_limits<uint_type>::digits * static_cast<int>(INT8_C(12))), limb_type, void, true>;
    #else
    using duodectuple_sint_type = ::math::wide_integer::uintwide_t<static_cast<::math::wide_integer::size_t>(std::numeric_limits<uint_type>::digits * static_cast<int>(INT8_C(12))), limb_type, void, true>;
    #endif

    #if (defined(WIDE_INTEGER_CONSTEXPR_IS_COMPILE_TIME_CONST) && (WIDE_INTEGER_CONSTEXPR_IS_COMPILE_TIME_CONST == 1))
    static WIDE_INTEGER_CONSTEXPR auto curve_p () noexcept -> double_sint_type { return double_sint_type(FieldCharacteristicP); } // NOLINT(cppcoreguidelines-pro-bounds-array-to-pointer-decay,hicpp-no-array-decay)
    static WIDE_INTEGER_CONSTEXPR auto curve_a () noexcept -> double_sint_type { return double_sint_type(CurveCoefficientA); }    // NOLINT(cppcoreguidelines-pro-bounds-array-to-pointer-decay,hicpp-no-array-decay)
    static WIDE_INTEGER_CONSTEXPR auto curve_b () noexcept -> double_sint_type { return double_sint_type(CurveCoefficientB); }    // NOLINT(cppcoreguidelines-pro-bounds-array-to-pointer-decay,hicpp-no-array-decay)

    static WIDE_INTEGER_CONSTEXPR auto curve_gx() noexcept -> double_sint_type { return double_sint_type(CoordGx); }              // NOLINT(cppcoreguidelines-pro-bounds-array-to-pointer-decay,hicpp-no-array-decay)
    static WIDE_INTEGER_CONSTEXPR auto curve_gy() noexcept -> double_sint_type { return double_sint_type(CoordGy); }              // NOLINT(cppcoreguidelines-pro-bounds-array-to-pointer-decay,hicpp-no-array-decay)

    static WIDE_INTEGER_CONSTEXPR auto curve_n () noexcept -> double_sint_type { return double_sint_type(SubGroupOrderN); }       // NOLINT(cppcoreguidelines-pro-bounds-array-to-pointer-decay,hicpp-no-array-decay)
    #else
    static auto curve_p () noexcept -> const double_sint_type& { static const double_sint_type vp(FieldCharacteristicP); return vp;  } // NOLINT(cppcoreguidelines-pro-bounds-array-to-pointer-decay,hicpp-no-array-decay)
    static auto curve_a () noexcept -> const double_sint_type& { static const double_sint_type va(CurveCoefficientA);    return va;  } // NOLINT(cppcoreguidelines-pro-bounds-array-to-pointer-decay,hicpp-no-array-decay)
    static auto curve_b () noexcept -> const double_sint_type& { static const double_sint_type vb(CurveCoefficientB);    return vb;  } // NOLINT(cppcoreguidelines-pro-bounds-array-to-pointer-decay,hicpp-no-array-decay)

    static auto curve_gx() noexcept -> const double_sint_type& { static const double_sint_type vgx(CoordGx);             return vgx; } // NOLINT(cppcoreguidelines-pro-bounds-array-to-pointer-decay,hicpp-no-array-decay)
    static auto curve_gy() noexcept -> const double_sint_type& { static const double_sint_type vgy(CoordGy);             return vgy; } // NOLINT(cppcoreguidelines-pro-bounds-array-to-pointer-decay,hicpp-no-array-decay)

    static auto curve_n () noexcept -> const double_sint_type& { static const double_sint_type vn(SubGroupOrderN);       return vn;  } // NOLINT(cppcoreguidelines-pro-bounds-array-to-pointer-decay,hicpp-no-array-decay)
    #endif

    static auto inverse_mod(const double_sint_type& k, const double_sint_type& p) -> double_sint_type // NOLINT(misc-no-recursion)
    {
      // Returns the inverse of k modulo p.
      // This function returns the only integer x such that (x * k) % p == 1.
      // k must be non-zero and p must be a prime.

      if(k == 0)
      {
        // Error: Division by zero.
        return 0; // LCOV_EXCL_LINE
      }

      if(k < 0)
      {
        // k ** -1 = p - (-k) ** -1  (mod p)
        return p - inverse_mod(-k, p);
      }

      // Extended Euclidean algorithm.
      auto s     = double_sint_type(static_cast<unsigned>(UINT8_C(0)));
      auto old_s = double_sint_type(static_cast<unsigned>(UINT8_C(1)));

      auto r     = p;
      auto old_r = k;

      while(r != 0U) // NOLINT(altera-id-dependent-backward-branch)
      {
        const auto quotient = divmod(old_r, r).first;

        const auto tmp_r = r; r = old_r - (quotient * r); old_r = tmp_r;
        const auto tmp_s = s; s = old_s - (quotient * s); old_s = tmp_s;
      }

      return divmod(old_s, p).second;
    }

    // Functions that work on curve points

    static auto is_on_curve(const point_type& point) -> bool
    {
      // Returns True if the given point lies on the elliptic curve.
      if((point.my_x == 0) && (point.my_y == 0))
      {
        // None represents the point at infinity.
        return true; // LCOV_EXCL_LINE
      }

      // Test the condition:
      //   (y * y - x * x * x - curve.a * x -curve.b) % curve.p == 0

      const auto num =
        quadruple_sint_type
        (
            (quadruple_sint_type(point.my_y) *  quadruple_sint_type(point.my_y))
          - (quadruple_sint_type(point.my_x) * (quadruple_sint_type(point.my_x) * quadruple_sint_type(point.my_x)))
          - (quadruple_sint_type(point.my_x) *  quadruple_sint_type(curve_a()))
          -  quadruple_sint_type(curve_b())
        );

      const auto divmod_result = divmod(num, quadruple_sint_type(curve_p())).second;

      return (divmod_result == 0);
    }

    // LCOV_EXCL_START
    static constexpr auto point_neg(const point_type& point) -> point_type
    {
      // Returns the negation of the point on the curve (i.e., -point).

      return
      {
        ((point.my_x == 0) && (point.my_y == 0))
          ? point_type(0)
          : point_type
            {
               point.my_x,
              -divmod(point.my_y, curve_p()).second
            }
      };
    }
    // LCOV_EXCL_STOP

    static auto point_add(const point_type& point1, const point_type& point2) -> point_type
    {
      // Returns the result of (point1 + point2) according to the group law.

      const auto& x1 = point1.my_x; const auto& y1 = point1.my_y;
      const auto& x2 = point2.my_x; const auto& y2 = point2.my_y;

      if((x1 == 0) && (y1 == 0))
      {
        // 0 + point2 = point2
        return point_type(point2);
      }

      if((x2 == 0) && (y2 == 0))
      {
        // point1 + 0 = point1
        return point_type(point1); // LCOV_EXCL_LINE
      }

      if((x1 == x2) && (y1 != y2))
      {
        // point1 + (-point1) = 0
        return point_type { }; // LCOV_EXCL_LINE
      }

      // Differentiate the cases (point1 == point2) and (point1 != point2).

      const auto m =
        quadruple_sint_type
        (
          (x1 == x2)
            ? (quadruple_sint_type(x1) * quadruple_sint_type(x1) * 3 + quadruple_sint_type(curve_a())) * quadruple_sint_type(inverse_mod(y1 * 2, curve_p()))
            : quadruple_sint_type(y1 - y2) * quadruple_sint_type(inverse_mod(x1 - x2, curve_p()))
        );

      const auto x3 =
        duodectuple_sint_type
        (
          duodectuple_sint_type(m) * duodectuple_sint_type(m) - duodectuple_sint_type(x1 + x2)
        );

      auto y3 =
        duodectuple_sint_type
        (
          duodectuple_sint_type(y1) + duodectuple_sint_type(m) * (x3 - duodectuple_sint_type(x1))
        );

      // Negate y3 for the modulus operation below.
      y3.negate();

      return
      {
        double_sint_type(divmod(x3, duodectuple_sint_type(curve_p())).second),
        double_sint_type(divmod(y3, duodectuple_sint_type(curve_p())).second)
      };
    }

    static auto scalar_mult(const double_sint_type& k, const point_type& point) -> point_type // NOLINT(misc-no-recursion)
    {
      // Returns k * point computed using the double and point_add algorithm.

      if(((k % curve_n()) == 0) || ((point.my_x == 0) && (point.my_y == 0)))
      {
        return point_type { }; // LCOV_EXCL_LINE
      }

      if(k < 0)
      {
        // k * point = -k * (-point)
        return scalar_mult(-k, point_neg(point)); // LCOV_EXCL_LINE
      }

      point_type result { };
      point_type addend = point;

      double_sint_type k_val(k);

      while(k_val != 0) // NOLINT(altera-id-dependent-backward-branch)
      {
        const auto lo_bit =
          static_cast<unsigned>
          (
            static_cast<unsigned>(k_val) & static_cast<unsigned>(UINT8_C(1))
          );

        if(lo_bit != static_cast<unsigned>(UINT8_C(0)))
        {
          // Add.
          result = point_add(result, addend);
        }

        // Double.
        addend = point_add(addend, addend);

        k_val >>= static_cast<unsigned>(UINT8_C(1));
      }

      return result;
    }

    template<typename UnknownWideUintType>
    static auto get_pseudo_random_uint() -> UnknownWideUintType
    {
      using local_wide_unsigned_integer_type = UnknownWideUintType;

      #if defined(WIDE_INTEGER_NAMESPACE)
      using local_distribution_type = WIDE_INTEGER_NAMESPACE::math::wide_integer::uniform_int_distribution<local_wide_unsigned_integer_type::my_width2, typename local_wide_unsigned_integer_type::limb_type>;
      #else
      using local_distribution_type = ::math::wide_integer::uniform_int_distribution<local_wide_unsigned_integer_type::my_width2, typename local_wide_unsigned_integer_type::limb_type>;
      #endif

      using local_random_engine_type = std::linear_congruential_engine<std::uint32_t, UINT32_C(48271), UINT32_C(0), UINT32_C(2147483647)>;
      using local_random_device_type = std::random_device;

      local_random_device_type dev;

      const auto seed_value = static_cast<typename local_random_engine_type::result_type>(dev());

      local_random_engine_type generator(seed_value);

      local_distribution_type dist;

      const auto unsigned_pseudo_random_value = dist(generator);

      return unsigned_pseudo_random_value;
    }

    static auto make_keypair(const uint_type* p_uint_seed = nullptr) -> keypair_type
    {
      // This subroutine generate a random private-public key pair.
      // The input parameter p_uint_seed can, however, be used to
      // provide a fixed-input value for the private key.

      // TBD: Be sure to limit to random.randrange(1, curve.n).

      const auto private_key =
        uint_type
        (
          (p_uint_seed == nullptr) ? get_pseudo_random_uint<uint_type>() : *p_uint_seed
        );

      const auto public_key  = scalar_mult(private_key, { curve_gx(), curve_gy() } );

      return
      {
        private_key,
        {
          uint_type(public_key.my_x),
          uint_type(public_key.my_y)
        }
      };
    }

    template<typename MsgIteratorType>
    static auto hash_message(MsgIteratorType msg_first, MsgIteratorType msg_last) -> uint_type
    {
      // This subroutine returns the hash of the message (msg), where
      // the type of the hash is 256-bit SHA2, as implenebted locally above.

      // For those interested in the general case of ECC, a larger/smaller
      // bit-length hash needs to be left/right shifted for cases when there
      // are different hash/curve bit-lengths (as specified in FIPS 180).

      const auto message { std::vector<std::uint8_t>(msg_first, msg_last) };

      using hash_type = hash_sha256;

      hash_type hash_object;

      const auto hash_result = hash_object.hash(message.data(), message.size());

      const auto z =
        [&hash_result]()
        {
          auto u = uint_type { };

          static_cast<void>(import_bits(u, hash_result.cbegin(), hash_result.cend()));

          return u;
        }();

      return z;
    }

    template<typename MsgIteratorType>
    static auto sign_message(const uint_type&      private_key,
                                   MsgIteratorType msg_first,
                                   MsgIteratorType msg_last,
                             const uint_type*      p_uint_seed = nullptr) -> std::pair<uint_type, uint_type>
    {
      const auto z = sexatuple_sint_type(hash_message(msg_first, msg_last));

      double_sint_type r { };
      double_sint_type s { };

      const auto n = sexatuple_sint_type(curve_n());

      const auto pk = sexatuple_sint_type(private_key);

      while((r == 0) || (s == 0)) // NOLINT(altera-id-dependent-backward-branch)
      {
        // TBD: Be sure to limit to random.randrange(1, curve.n).
        const auto k =
          double_sint_type
          (
            (p_uint_seed == nullptr) ? static_cast<double_sint_type>(get_pseudo_random_uint<uint_type>()) : static_cast<double_sint_type>(*p_uint_seed)
          );

        const auto pt = scalar_mult(k, { curve_gx(), curve_gy() } );

        r = divmod(pt.my_x, curve_n()).second;

        const auto num =
        (
           (sexatuple_sint_type(z) + (sexatuple_sint_type(r) * pk))
          * sexatuple_sint_type(inverse_mod(k, curve_n()))
        );

        s = double_sint_type(divmod(num, n).second);
      }

      return
      {
        uint_type(r),
        uint_type(s)
      };
    }

    template<typename MsgIteratorType>
    static auto verify_signature(const std::pair<uint_type, uint_type>& pub,
                                       MsgIteratorType                  msg_first,
                                       MsgIteratorType                  msg_last,
                                 const std::pair<uint_type, uint_type>& sig) -> bool
    {
      const auto w = sexatuple_sint_type(inverse_mod(sig.second, curve_n()));

      const auto n = sexatuple_sint_type(curve_n());

      const auto z = hash_message(msg_first, msg_last);

      const auto u1 = double_sint_type(divmod(sexatuple_sint_type(z)         * w, n).second);
      const auto u2 = double_sint_type(divmod(sexatuple_sint_type(sig.first) * w, n).second);

      const auto pt =
        point_add
        (
          scalar_mult(u1, { curve_gx(), curve_gy() } ),
          scalar_mult(u2, { pub.first,  pub.second } )
        );

      return
      (
        divmod(double_sint_type(sig.first), curve_n()).second == divmod(pt.my_x, curve_n()).second
      );
    }
  };

  constexpr char CurveName           [] = "secp256k1";                                                          // NOLINT(cppcoreguidelines-avoid-c-arrays,hicpp-avoid-c-arrays,cppcoreguidelines-pro-bounds-array-to-pointer-decay,modernize-avoid-c-arrays)
  constexpr char FieldCharacteristicP[] = "0xFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFEFFFFFC2F"; // NOLINT(cppcoreguidelines-avoid-c-arrays,hicpp-avoid-c-arrays,cppcoreguidelines-pro-bounds-array-to-pointer-decay,modernize-avoid-c-arrays)
  constexpr char CurveCoefficientA   [] = "0x0";                                                                // NOLINT(cppcoreguidelines-avoid-c-arrays,hicpp-avoid-c-arrays,cppcoreguidelines-pro-bounds-array-to-pointer-decay,modernize-avoid-c-arrays)
  constexpr char CurveCoefficientB   [] = "0x7";                                                                // NOLINT(cppcoreguidelines-avoid-c-arrays,hicpp-avoid-c-arrays,cppcoreguidelines-pro-bounds-array-to-pointer-decay,modernize-avoid-c-arrays)
  constexpr char BasePointGx         [] = "0x79BE667EF9DCBBAC55A06295CE870B07029BFCDB2DCE28D959F2815B16F81798"; // NOLINT(cppcoreguidelines-avoid-c-arrays,hicpp-avoid-c-arrays,cppcoreguidelines-pro-bounds-array-to-pointer-decay,modernize-avoid-c-arrays)
  constexpr char BasePointGy         [] = "0x483ADA7726A3C4655DA4FBFC0E1108A8FD17B448A68554199C47D08FFB10D4B8"; // NOLINT(cppcoreguidelines-avoid-c-arrays,hicpp-avoid-c-arrays,cppcoreguidelines-pro-bounds-array-to-pointer-decay,modernize-avoid-c-arrays)
  constexpr char SubGroupOrderN      [] = "0xFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFEBAAEDCE6AF48A03BBFD25E8CD0364141"; // NOLINT(cppcoreguidelines-avoid-c-arrays,hicpp-avoid-c-arrays,cppcoreguidelines-pro-bounds-array-to-pointer-decay,modernize-avoid-c-arrays)
  constexpr auto SubGroupCoFactorH      = static_cast<int>(INT8_C(1));

} // namespace example013_ecdsa

#if defined(WIDE_INTEGER_NAMESPACE)
auto WIDE_INTEGER_NAMESPACE::math::wide_integer::example013_ecdsa_sign_verify() -> bool
#else
auto ::math::wide_integer::example013_ecdsa_sign_verify() -> bool
#endif
{
  auto result_is_ok = true;

  using elliptic_curve_type =
    example013_ecdsa::elliptic_curve<static_cast<unsigned>(UINT16_C(256)),
                                     std::uint32_t,
                                     example013_ecdsa::CurveName,            // NOLINT(cppcoreguidelines-pro-bounds-array-to-pointer-decay,hicpp-no-array-decay)
                                     example013_ecdsa::FieldCharacteristicP, // NOLINT(cppcoreguidelines-pro-bounds-array-to-pointer-decay,hicpp-no-array-decay)
                                     example013_ecdsa::CurveCoefficientA,    // NOLINT(cppcoreguidelines-pro-bounds-array-to-pointer-decay,hicpp-no-array-decay)
                                     example013_ecdsa::CurveCoefficientB,    // NOLINT(cppcoreguidelines-pro-bounds-array-to-pointer-decay,hicpp-no-array-decay)
                                     example013_ecdsa::BasePointGx,          // NOLINT(cppcoreguidelines-pro-bounds-array-to-pointer-decay,hicpp-no-array-decay)
                                     example013_ecdsa::BasePointGy,          // NOLINT(cppcoreguidelines-pro-bounds-array-to-pointer-decay,hicpp-no-array-decay)
                                     example013_ecdsa::SubGroupOrderN,       // NOLINT(cppcoreguidelines-pro-bounds-array-to-pointer-decay,hicpp-no-array-decay)
                                     example013_ecdsa::SubGroupCoFactorH>;

  #if (defined(WIDE_INTEGER_CONSTEXPR_IS_COMPILE_TIME_CONST) && (WIDE_INTEGER_CONSTEXPR_IS_COMPILE_TIME_CONST == 1))

  static_assert(elliptic_curve_type::curve_p() == elliptic_curve_type::double_sint_type(example013_ecdsa::FieldCharacteristicP), // NOLINT(cppcoreguidelines-pro-bounds-array-to-pointer-decay,hicpp-no-array-decay)
                "Error: Elliptic curve Field Characteristic p seems to be incorrect");

  static_assert(elliptic_curve_type::curve_a() == elliptic_curve_type::double_sint_type(example013_ecdsa::CurveCoefficientA), // NOLINT(cppcoreguidelines-pro-bounds-array-to-pointer-decay,hicpp-no-array-decay)
                "Error: Elliptic curve curve coefficient a seems to be incorrect");

  static_assert(elliptic_curve_type::curve_b() == elliptic_curve_type::double_sint_type(example013_ecdsa::CurveCoefficientB), // NOLINT(cppcoreguidelines-pro-bounds-array-to-pointer-decay,hicpp-no-array-decay)
                "Error: Elliptic curve curve coefficient b seems to be incorrect");

  static_assert(elliptic_curve_type::curve_gx() == elliptic_curve_type::double_sint_type(example013_ecdsa::BasePointGx), // NOLINT(cppcoreguidelines-pro-bounds-array-to-pointer-decay,hicpp-no-array-decay)
                "Error: Elliptic curve base-point Gx seems to be incorrect");

  static_assert(elliptic_curve_type::curve_gy() == elliptic_curve_type::double_sint_type(example013_ecdsa::BasePointGy), // NOLINT(cppcoreguidelines-pro-bounds-array-to-pointer-decay,hicpp-no-array-decay)
                "Error: Elliptic curve base-point Gx seems to be incorrect");

  static_assert(elliptic_curve_type::curve_n() == elliptic_curve_type::double_sint_type(example013_ecdsa::SubGroupOrderN), // NOLINT(cppcoreguidelines-pro-bounds-array-to-pointer-decay,hicpp-no-array-decay)
                "Error: Elliptic curve Sub-Group Order seems to be incorrect");

  #else
  static_cast<void>(elliptic_curve_type::curve_p ());
  static_cast<void>(elliptic_curve_type::curve_a ());
  static_cast<void>(elliptic_curve_type::curve_b ());
  static_cast<void>(elliptic_curve_type::curve_gx());
  static_cast<void>(elliptic_curve_type::curve_gy());
  static_cast<void>(elliptic_curve_type::curve_n ());
  #endif

  // Declare the message "Hello!" as an array of chars.
  constexpr std::array<char, static_cast<std::size_t>(UINT8_C(6))> msg_as_array { 'H', 'e', 'l', 'l', 'o', '!' };

  // Get the message to sign as a string and ensure that it is "Hello!".
  const auto msg_as_string = std::string(msg_as_array.cbegin(), msg_as_array.cend());

  const auto result_msg_as_string_is_ok = (msg_as_string == "Hello!");

  result_is_ok = (result_msg_as_string_is_ok && result_is_ok);

  {
    // Test the hash SHA-2 HASH-256 implementation.

    const auto hash_result = elliptic_curve_type::hash_message(msg_as_array.cbegin(), msg_as_array.cend());

    const auto result_hash_is_ok =
    (
      hash_result == elliptic_curve_type::uint_type("0x334D016F755CD6DC58C53A86E183882F8EC14F52FB05345887C8A5EDD42C87B7")
    );

    result_is_ok = (result_hash_is_ok && result_is_ok);
  }

  {
    // Test ECC key generation, sign and verify. In this case we use random (but pre-defined seeds
    // for both keygen as well as signing.

    const auto seed_keygen = elliptic_curve_type::uint_type("0xC6455BF2F380F6B81F5FD1A1DBC2392B3783ED1E7D91B62942706E5584BA0B92");

    const auto keypair = elliptic_curve_type::make_keypair(&seed_keygen);

    const auto result_is_on_curve_is_ok =
      elliptic_curve_type::is_on_curve
      (
        {
          std::get<1>(keypair).first,
          std::get<1>(keypair).second
        }
      );

    const auto result_private_is_ok  = (std::get<0>(keypair)        == "0xC6455BF2F380F6B81F5FD1A1DBC2392B3783ED1E7D91B62942706E5584BA0B92");
    const auto result_public_x_is_ok = (std::get<1>(keypair).first  == "0xC6235629F157690E1DF37248256C4FB7EFF073D0250F5BD85DF40B9E127A8461");
    const auto result_public_y_is_ok = (std::get<1>(keypair).second == "0xCBAA679F07F9B98F915C1FB7D85A379D0559A9EEE6735B1BE0CE0E2E2B2E94DE");

    const auto result_keygen_is_ok =
    (
         result_private_is_ok
      && result_public_x_is_ok
      && result_public_y_is_ok
    );

    result_is_ok = (result_is_on_curve_is_ok && result_keygen_is_ok && result_is_ok);

    const auto priv = elliptic_curve_type::uint_type("0x6F73D8E95D6DDBF0EB352A9F0B2CE91931511EDAF9AC8F128D5A4F877C4F0450");

    const auto sig =
      elliptic_curve_type::sign_message(std::get<0>(keypair), msg_as_string.cbegin(), msg_as_string.cend(), &priv);

    const auto result_sig_is_ok =
      (
        sig == std::make_pair
               (
                 elliptic_curve_type::uint_type("0x65717A860F315A21E6E23CDE411C8940DE42A69D8AB26C2465902BE8F3B75E7B"),
                 elliptic_curve_type::uint_type("0xDB8B8E75A7B0C2F0D9EB8DBF1B5236EDEB89B2116F5AEBD40E770F8CCC3D6605")
               )
      );

    result_is_ok = (result_sig_is_ok && result_is_ok);

    const auto result_verify_is_ok =
      elliptic_curve_type::verify_signature
      (
        std::get<1>(keypair),
        msg_as_string.cbegin(),
        msg_as_string.cend(),
        sig
      );

    result_is_ok = (result_verify_is_ok && result_is_ok);
  }

  {
    // We will now test 3 more successful keygen, sign, verify sequences.

    for(auto   count = static_cast<unsigned>(UINT8_C(0));
               count < static_cast<unsigned>(UINT8_C(3));
             ++count)
    {
      const auto keypair = elliptic_curve_type::make_keypair();

      const auto msg_str_append_index = msg_as_string + std::to_string(count);

      const auto sig =
        elliptic_curve_type::sign_message
        (
          std::get<0>(keypair),
          msg_str_append_index.cbegin(),
          msg_str_append_index.cend()
        );

      const auto result_verify_is_ok =
        elliptic_curve_type::verify_signature
        (
          std::get<1>(keypair),
          msg_str_append_index.cbegin(),
          msg_str_append_index.cend(),
          sig
        );

      result_is_ok = (result_verify_is_ok && result_is_ok);
    }
  }

  {
    // We will now test keygen, sign, and a (failing!) verify sequence,
    // where the message being verified has been artificially modified
    // and signature verification is expected to fail.

    const auto keypair = elliptic_curve_type::make_keypair();

    const auto sig =
      elliptic_curve_type::sign_message(std::get<0>(keypair), msg_as_string.cbegin(), msg_as_string.cend());

    const auto msg_str_to_fail = msg_as_string + "x";

    const auto result_verify_expected_fail_is_ok =
      (!elliptic_curve_type::verify_signature(std::get<1>(keypair), msg_str_to_fail.cbegin(), msg_str_to_fail.cend(), sig));

    result_is_ok = (result_verify_expected_fail_is_ok && result_is_ok);
  }

  return result_is_ok;
}

// Enable this if you would like to activate this main() as a standalone example.
#if defined(WIDE_INTEGER_STANDALONE_EXAMPLE013_ECDSA_SIGN_VERIFY)

#include <iomanip>
#include <iostream>

auto main() -> int
{
  #if defined(WIDE_INTEGER_NAMESPACE)
  const auto result_is_ok = WIDE_INTEGER_NAMESPACE::math::wide_integer::example013_ecdsa_sign_verify();
  #else
  const auto result_is_ok = ::math::wide_integer::example013_ecdsa_sign_verify();
  #endif

  std::cout << "result_is_ok: " << std::boolalpha << result_is_ok << std::endl;

  return (result_is_ok ? 0 : -1);
}

#endif
