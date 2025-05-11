///////////////////////////////////////////////////////////////////////////////
//  Copyright Christopher Kormanyos 2023.
//  Distributed under the Boost Software License,
//  Version 1.0. (See accompanying file LICENSE_1_0.txt
//  or copy at http://www.boost.org/LICENSE_1_0.txt)
//

#ifndef UTIL_PSEUDORANDOM_TIME_POINT_SEED_2023_10_27_H // NOLINT(llvm-header-guard)
  #define UTIL_PSEUDORANDOM_TIME_POINT_SEED_2023_10_27_H

  #include <algorithm>
  #include <array>
  #include <cstddef>
  #include <cstdint>
  #include <ctime>
  #include <iomanip>
  #include <limits>
  #include <sstream>
  #include <string>

  namespace util {

  struct util_pseudorandom_time_point_seed
  {
  public:
    template<typename IntegralType>
    static auto value() -> IntegralType
    {
      const std::uint64_t t_now { now() };

      std::stringstream strm { };

      using strtime_uint8_array_type = std::array<std::uint8_t, static_cast<std::size_t>(UINT8_C(16))>;

      strtime_uint8_array_type buf_u8 { }; buf_u8.fill(static_cast<std::uint8_t>(UINT8_C(0)));

      // Get the string representation of the time point.
      strm << std::setw(std::tuple_size<strtime_uint8_array_type>::value) << std::hex << std::uppercase << std::setfill('0') << t_now;

      const std::string str_tm { strm.str() };

      std::copy(str_tm.cbegin(), str_tm.cend(), buf_u8.begin());

      using local_integral_type = IntegralType;

      return static_cast<local_integral_type>(crc_crc64(buf_u8.data(), buf_u8.size()));
    }

    static constexpr auto test() noexcept -> bool;

  private:
    static auto now() -> std::uint64_t
    {
      #if defined(__CYGWIN__)

      return static_cast<std::uint64_t>(std::clock());

      #else

      // Get the time (t_now).
      timespec ts { };

      timespec_get(&ts, TIME_UTC);

      return
        static_cast<std::uint64_t>
        (
            static_cast<std::uint64_t>(static_cast<std::uint64_t>(ts.tv_sec) * UINT64_C(1000000000))
          + static_cast<std::uint64_t>(ts.tv_nsec)
        );

      #endif
    }

    template<const std::size_t NumberOfBits,
             typename UnsignedIntegralType>
    static constexpr auto crc_bitwise_template(const std::uint8_t*        message,
                                               const std::size_t          count,
                                               const UnsignedIntegralType polynomial, // NOLINT(bugprone-easily-swappable-parameters)
                                               const UnsignedIntegralType initial_value,
                                               const UnsignedIntegralType final_xor_value) -> UnsignedIntegralType
    {
      using value_type = UnsignedIntegralType;

      // The data_type is fixed to exactly 8-bits in width at the moment.
      using data_type = std::uint8_t;

      value_type crc = initial_value;

      // Perform the polynomial division, one element at a time.
      for(auto data_index = static_cast<std::size_t>(UINT8_C(0)); data_index < count; ++data_index)
      {
        // Obtain the next data element (and reflect it if necessary).
        const data_type next_data_element = message[data_index]; // NOLINT(cppcoreguidelines-pro-bounds-pointer-arithmetic)

        {
          constexpr auto left_shift_amount =
            static_cast<std::size_t>
            (
              std::numeric_limits<value_type>::digits - std::numeric_limits<data_type >::digits
            );

          crc ^= static_cast<value_type>(static_cast<value_type>(next_data_element) << left_shift_amount);
        }

        // Process the next data byte, one bit at a time.
        for(std::uint_fast8_t   index = 0U;
                                index < static_cast<std::uint_fast8_t>(std::numeric_limits<data_type>::digits);
                              ++index)
        {
          const auto high_bit_value =
            static_cast<value_type>
            (
                crc
              & static_cast<value_type>
                (
                  static_cast<std::uintmax_t>(UINT8_C(1)) << static_cast<unsigned>(std::numeric_limits<value_type>::digits - 1)
                )
            );

          const bool high_bit_of_crc_is_set = (high_bit_value != static_cast<value_type>(UINT8_C(0)));

          crc = crc << static_cast<unsigned>(UINT8_C(1));

          if(high_bit_of_crc_is_set)
          {
            // Shift through the polynomial. Also left-justify the
            // polynomial within the width of value_type, if necessary.

            crc ^= static_cast<value_type>(polynomial);
          }
        }
      }

      // Perform the final XOR on the result.
      crc ^= final_xor_value;

      return crc;
    }

    static constexpr auto crc_crc64(const std::uint8_t* message, const std::size_t count) -> std::uint64_t
    {
      // check: 0x6C40DF5F0B497347
      return crc_bitwise_template<static_cast<std::size_t>(UINT8_C(64)), std::uint64_t>
      (
        message,
        count,
        static_cast<std::uint64_t>(UINT64_C(0x42F0E1EBA9EA3693)),
        static_cast<std::uint64_t>(UINT64_C(0x0000000000000000)),
        static_cast<std::uint64_t>(UINT64_C(0x0000000000000000))
      );
    }
  };

  constexpr auto util_pseudorandom_time_point_seed::test() noexcept -> bool
  {
    constexpr std::uint8_t crc64_test_data[static_cast<std::size_t>(UINT8_C(9))] = // NOLINT(cppcoreguidelines-avoid-c-arrays,hicpp-avoid-c-arrays,modernize-avoid-c-arrays)
    {
      0x31U, 0x32U, 0x33U, 0x34U, 0x35U, 0x36U, 0x37U, 0x38U, 0x39U
    };

    constexpr auto crc64_test_result =  crc_bitwise_template<static_cast<std::size_t>(UINT8_C(64)), std::uint64_t>
    (
      crc64_test_data, // NOLINT(cppcoreguidelines-pro-bounds-array-to-pointer-decay,hicpp-no-array-decay)
      sizeof(crc64_test_data),
      static_cast<std::uint64_t>(UINT64_C(0x42F0E1EBA9EA3693)),
      static_cast<std::uint64_t>(UINT64_C(0x0000000000000000)),
      static_cast<std::uint64_t>(UINT64_C(0x0000000000000000))
    );

    // check: 0x6C40DF5F0B497347
    return (crc64_test_result == static_cast<std::uint64_t>(UINT64_C(0x6C40DF5F0B497347)));
  }

  static_assert(util::util_pseudorandom_time_point_seed::test(), "Error: crc64 implementation is not working properly");

  } // namespace util

#endif // UTIL_PSEUDORANDOM_TIME_POINT_SEED_2023_10_27_H
