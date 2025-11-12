///////////////////////////////////////////////////////////////////////////////
//  Copyright Christopher Kormanyos 2013 - 2024.
//  Distributed under the Boost Software License,
//  Version 1.0. (See accompanying file LICENSE_1_0.txt
//  or copy at http://www.boost.org/LICENSE_1_0.txt)
//

#ifndef STOPWATCH_2024_03_28_H // NOLINT(llvm-header-guard)
  #define STOPWATCH_2024_03_28_H

  #include <cstdint>
  #include <ctime>

  #if defined(_MSC_VER) && !defined(__GNUC__)
  #define STOPWATCH_NODISCARD
  #else
  #if (defined(__cplusplus) && (__cplusplus >= 201703L))
  #define STOPWATCH_NODISCARD  [[nodiscard]] // NOLINT(cppcoreguidelines-macro-usage)
  #else
  #define STOPWATCH_NODISCARD
  #endif
  #endif

  // See also: https://godbolt.org/z/37a4n9f4Y

  namespace concurrency {

  struct stopwatch
  {
  public:
    using time_point_type = std::uintmax_t;

    auto reset() -> void
    {
      m_start = now();
    }

    template<typename RepresentationRequestedTimeType>
    static auto elapsed_time(const stopwatch& my_stopwatch) noexcept -> RepresentationRequestedTimeType
    {
      using local_time_type = RepresentationRequestedTimeType;

      return
        local_time_type
        {
            static_cast<local_time_type>(my_stopwatch.elapsed())
          / local_time_type { UINTMAX_C(1000000000) }
        };
    }

  private:
    time_point_type m_start { now() }; // NOLINT(readability-identifier-naming)

    STOPWATCH_NODISCARD static auto now() -> time_point_type
    {
      #if defined(__CYGWIN__)

      return static_cast<time_point_type>(std::clock());

      #else

      timespec ts { };

      const int ntsp { timespec_get(&ts, TIME_UTC) };

      static_cast<void>(ntsp);

      return
        static_cast<time_point_type>
        (
            static_cast<time_point_type>(static_cast<time_point_type>(ts.tv_sec) * UINTMAX_C(1000000000))
          + static_cast<time_point_type>(ts.tv_nsec)
        );

      #endif
    }

    STOPWATCH_NODISCARD auto elapsed() const -> time_point_type
    {
      const time_point_type stop { now() };

      #if defined(__CYGWIN__)

      const time_point_type
        elapsed_ns
        {
          static_cast<time_point_type>
          (
              static_cast<time_point_type>(static_cast<time_point_type>(stop - m_start) * UINTMAX_C(1000000000))
            / static_cast<time_point_type>(CLOCKS_PER_SEC)
          )
        };

      #else

      const time_point_type
        elapsed_ns
        {
          static_cast<time_point_type>
          (
            stop - m_start
          )
        };

      #endif

      return elapsed_ns;
    }
  };

  } // namespace concurrency

#endif // STOPWATCH_2024_03_28_H
