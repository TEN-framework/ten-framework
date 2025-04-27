///////////////////////////////////////////////////////////////////
//  Copyright Christopher Kormanyos 2019 - 2022.                 //
//  Distributed under the Boost Software License,                //
//  Version 1.0. (See accompanying file LICENSE_1_0.txt          //
//  or copy at http://www.boost.org/LICENSE_1_0.txt)             //
///////////////////////////////////////////////////////////////////

#ifndef UINTWIDE_T_BACKEND_2019_12_15_HPP // NOLINT(llvm-header-guard)
  #define UINTWIDE_T_BACKEND_2019_12_15_HPP

  #include <cstdint>
  #include <limits>
  #include <string>
  #include <type_traits>
  #include <utility>
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

  #if (BOOST_VERSION < 107900)
  #include <boost/config.hpp>
  #endif
  #include <boost/multiprecision/number.hpp>

  #include <math/wide_integer/uintwide_t.h>

  #if(__cplusplus >= 201703L)
  namespace boost::multiprecision {
  #else
  namespace boost { namespace multiprecision { // NOLINT(modernize-concat-nested-namespaces)
  #endif

  // Forward declaration of the uintwide_t_backend multiple precision class.
  // This class binds native (WIDE_INTEGER_NAMESPACE)::math::wide_integer::uintwide_t
  // to boost::multiprecsion::uintwide_t_backend.
  template<
  #if defined(WIDE_INTEGER_NAMESPACE)
           const WIDE_INTEGER_NAMESPACE::math::wide_integer::size_t MyWidth2,
  #else
           const ::math::wide_integer::size_t MyWidth2,
  #endif
           typename MyLimbType = std::uint32_t,
           typename MyAllocatorType = void>
  class uintwide_t_backend;

  // Define the number category as an integer number kind
  // for the uintwide_t_backend. This is needed for properly
  // interacting as a backend with boost::muliprecision.
  #if (BOOST_VERSION <= 107200)
  template<
  #if defined(WIDE_INTEGER_NAMESPACE)
           const WIDE_INTEGER_NAMESPACE::math::wide_integer::size_t MyWidth2,
  #else
           const ::math::wide_integer::size_t MyWidth2,
  #endif
           typename MyLimbType,
           typename MyAllocatorType>
  struct number_category<uintwide_t_backend<MyWidth2, MyLimbType, MyAllocatorType>>
    : public boost::mpl::int_<number_kind_integer> { };
  #elif (BOOST_VERSION <= 107500)
  template<
  #if defined(WIDE_INTEGER_NAMESPACE)
           const WIDE_INTEGER_NAMESPACE::math::wide_integer::size_t MyWidth2,
  #else
           const ::math::wide_integer::size_t MyWidth2,
  #endif
           typename MyLimbType,
           typename MyAllocatorType>
  struct number_category<uintwide_t_backend<MyWidth2, MyLimbType, MyAllocatorType>>
    : public boost::integral_constant<unsigned int, number_kind_integer> { };
  #else
  template<
  #if defined(WIDE_INTEGER_NAMESPACE)
           const WIDE_INTEGER_NAMESPACE::math::wide_integer::size_t MyWidth2,
  #else
           const ::math::wide_integer::size_t MyWidth2,
  #endif
           typename MyLimbType,
           typename MyAllocatorType>
  struct number_category<uintwide_t_backend<MyWidth2, MyLimbType, MyAllocatorType>>
    : public std::integral_constant<unsigned int, number_kind_integer> { };
  #endif

  // This is the uintwide_t_backend multiple precision class.
  template<
  #if defined(WIDE_INTEGER_NAMESPACE)
           const WIDE_INTEGER_NAMESPACE::math::wide_integer::size_t MyWidth2,
  #else
           const ::math::wide_integer::size_t MyWidth2,
  #endif
           typename MyLimbType,
           typename MyAllocatorType>
  class uintwide_t_backend // NOLINT(cppcoreguidelines-special-member-functions,hicpp-special-member-functions)
  {
  public:
    using representation_type =
    #if defined(WIDE_INTEGER_NAMESPACE)
      WIDE_INTEGER_NAMESPACE::math::wide_integer::uintwide_t<MyWidth2, MyLimbType, MyAllocatorType>;
    #else
      ::math::wide_integer::uintwide_t<MyWidth2, MyLimbType, MyAllocatorType>;
    #endif

    #if (BOOST_VERSION <= 107500)
    using signed_types   = mpl::list<std::int64_t>;
    using unsigned_types = mpl::list<std::uint64_t>;
    using float_types    = mpl::list<long double>;
    #else
    using   signed_types = std::tuple<  signed char,   signed short,   signed int,   signed long,   signed long long, std::intmax_t>;  // NOLINT(google-runtime-int)
    using unsigned_types = std::tuple<unsigned char, unsigned short, unsigned int, unsigned long, unsigned long long, std::uintmax_t>; // NOLINT(google-runtime-int)
    using float_types    = std::tuple<float, double, long double>;
    #endif

    constexpr uintwide_t_backend() : m_value() { }

    explicit constexpr uintwide_t_backend(const representation_type& rep)
      : m_value(std::move(rep)) { }

    constexpr uintwide_t_backend(const uintwide_t_backend& other) : m_value(other.m_value) { }

    constexpr uintwide_t_backend(uintwide_t_backend&& other) noexcept
      : m_value(static_cast<representation_type&&>(other.m_value)) { }

    template<typename UnsignedIntegralType,
             std::enable_if_t<(   (std::is_integral<UnsignedIntegralType>::value)
                               && (std::is_unsigned<UnsignedIntegralType>::value))> const* = nullptr>
    constexpr uintwide_t_backend(UnsignedIntegralType u) : m_value(representation_type(static_cast<std::uint64_t>(u))) { } // NOLINT(google-explicit-constructor,hicpp-explicit-conversions)

    template<typename SignedIntegralType,
             std::enable_if_t<(   (std::is_integral<SignedIntegralType>::value)
                               && (std::is_signed  <SignedIntegralType>::value))> const* = nullptr>
    constexpr uintwide_t_backend(SignedIntegralType n) : m_value(representation_type(static_cast<std::int64_t>(n))) { } // NOLINT(google-explicit-constructor,hicpp-explicit-conversions)

    template<typename FloatingPointType,
             std::enable_if_t<std::is_floating_point<FloatingPointType>::value> const* = nullptr>
    constexpr uintwide_t_backend(FloatingPointType f) : m_value(representation_type(static_cast<long double>(f))) { } // NOLINT(google-explicit-constructor,hicpp-explicit-conversions)

    constexpr uintwide_t_backend(const char* c) : m_value(c) { } // NOLINT(google-explicit-constructor,hicpp-explicit-conversions)

    constexpr uintwide_t_backend(const std::string& str) : m_value(str) { } // NOLINT(google-explicit-constructor,hicpp-explicit-conversions)

    WIDE_INTEGER_CONSTEXPR ~uintwide_t_backend() = default;

    WIDE_INTEGER_CONSTEXPR auto operator=(const uintwide_t_backend& other) -> uintwide_t_backend& // NOLINT(cert-oop54-cpp)
    {
      if(this != &other)
      {
        m_value.representation() = other.m_value.crepresentation();
      }

      return *this;
    }

    WIDE_INTEGER_CONSTEXPR auto operator=(uintwide_t_backend&& other) noexcept -> uintwide_t_backend&
    {
      m_value = static_cast<representation_type&&>(other.m_value);

      return *this;
    }

    template<typename ArithmeticType,
             std::enable_if_t<std::is_arithmetic<ArithmeticType>::value> const* = nullptr>
    WIDE_INTEGER_CONSTEXPR auto operator=(const ArithmeticType& x) -> uintwide_t_backend&
    {
      m_value = representation_type(x);

      return *this;
    }

    WIDE_INTEGER_CONSTEXPR auto operator=(const std::string& str_rep)  -> uintwide_t_backend& { m_value = representation_type(str_rep);  return *this; }
    WIDE_INTEGER_CONSTEXPR auto operator=(const char*        char_ptr) -> uintwide_t_backend& { m_value = representation_type(char_ptr); return *this; }

    WIDE_INTEGER_CONSTEXPR auto swap(uintwide_t_backend& other) -> void
    {
      m_value.representation().swap(other.m_value.representation());
    }

    WIDE_INTEGER_CONSTEXPR auto swap(uintwide_t_backend&& other) noexcept -> void
    {
      auto tmp = std::move(m_value.representation());

      m_value.representation() = std::move(other.m_value.representation());

      other.m_value.representation() = std::move(tmp);
    }

                           WIDE_INTEGER_CONSTEXPR auto  representation()       ->       representation_type& { return m_value; }
    WIDE_INTEGER_NODISCARD WIDE_INTEGER_CONSTEXPR auto  representation() const -> const representation_type& { return m_value; }
    WIDE_INTEGER_NODISCARD WIDE_INTEGER_CONSTEXPR auto crepresentation() const -> const representation_type& { return m_value; }

    WIDE_INTEGER_NODISCARD auto str(std::streamsize number_of_digits, const std::ios::fmtflags format_flags) const -> std::string
    {
      static_cast<void>(number_of_digits);

      // Use simple vector dynamic memory here. When using uintwide_t as a
      // Boost.Multiprecision number backend, we assume vector is available.

      std::vector<char>
        pstr
        (
          static_cast<typename std::vector<char>::size_type>(representation_type::wr_string_max_buffer_size_dec())
        );

      const auto base_rep     = static_cast<std::uint_fast8_t>(((format_flags & std::ios::hex) != 0) ? 16U : 10U);
      const auto show_base    = ((format_flags & std::ios::showbase)  != 0);
      const auto show_pos     = ((format_flags & std::ios::showpos)   != 0);
      const auto is_uppercase = ((format_flags & std::ios::uppercase) != 0);

      const auto wr_string_is_ok = m_value.wr_string(pstr.data(), base_rep, show_base, show_pos, is_uppercase);

      return (wr_string_is_ok ? std::string(pstr.data()) : std::string());
    }

    WIDE_INTEGER_CONSTEXPR auto negate() -> void
    {
      m_value.negate();
    }

    WIDE_INTEGER_NODISCARD constexpr auto compare(const uintwide_t_backend& other_mp_cpp_backend) const -> int
    {
      return static_cast<int>(m_value.compare(other_mp_cpp_backend.crepresentation()));
    }

    template<typename ArithmeticType,
             std::enable_if_t<std::is_arithmetic<ArithmeticType>::value> const* = nullptr>
    WIDE_INTEGER_NODISCARD constexpr auto compare(ArithmeticType x) const -> int
    {
      return static_cast<int>(m_value.compare(representation_type(x)));
    }

    WIDE_INTEGER_NODISCARD WIDE_INTEGER_CONSTEXPR auto hash() const -> std::size_t
    {
      auto result = static_cast<std::size_t>(0U);

      #if (BOOST_VERSION < 107800)
      using boost::hash_combine;
      #else
      using boost::multiprecision::detail::hash_combine;
      #endif
      for(auto   i = static_cast<typename representation_type::representation_type::size_type>(0U);
                 i < crepresentation().crepresentation().size();
               ++i)
      {
        hash_combine(result, crepresentation().crepresentation()[i]);
      }

      return result;
    }

    auto operator=(const representation_type&) -> uintwide_t_backend& = delete;

  private:
    representation_type m_value; // NOLINT(readability-identifier-naming)
  };

  template<
  #if defined(WIDE_INTEGER_NAMESPACE)
           const WIDE_INTEGER_NAMESPACE::math::wide_integer::size_t MyWidth2,
  #else
           const ::math::wide_integer::size_t MyWidth2,
  #endif
           typename MyLimbType,
           typename MyAllocatorType>
  WIDE_INTEGER_CONSTEXPR auto eval_add(uintwide_t_backend<MyWidth2, MyLimbType, MyAllocatorType>& result, const uintwide_t_backend<MyWidth2, MyLimbType, MyAllocatorType>& x) -> void
  {
    result.representation() += x.crepresentation();
  }

  template<
  #if defined(WIDE_INTEGER_NAMESPACE)
           const WIDE_INTEGER_NAMESPACE::math::wide_integer::size_t MyWidth2,
  #else
           const ::math::wide_integer::size_t MyWidth2,
  #endif
           typename MyLimbType,
           typename MyAllocatorType>
  WIDE_INTEGER_CONSTEXPR auto eval_subtract(uintwide_t_backend<MyWidth2, MyLimbType, MyAllocatorType>& result, const uintwide_t_backend<MyWidth2, MyLimbType, MyAllocatorType>& x) -> void
  {
    result.representation() -= x.crepresentation();
  }

  template<
  #if defined(WIDE_INTEGER_NAMESPACE)
           const WIDE_INTEGER_NAMESPACE::math::wide_integer::size_t MyWidth2,
  #else
           const ::math::wide_integer::size_t MyWidth2,
  #endif
           typename MyLimbType,
           typename MyAllocatorType>
  WIDE_INTEGER_CONSTEXPR auto eval_multiply(uintwide_t_backend<MyWidth2, MyLimbType, MyAllocatorType>& result, const uintwide_t_backend<MyWidth2, MyLimbType, MyAllocatorType>& x) -> void
  {
    result.representation() *= x.crepresentation();
  }

  template<
  #if defined(WIDE_INTEGER_NAMESPACE)
           const WIDE_INTEGER_NAMESPACE::math::wide_integer::size_t MyWidth2,
  #else
           const ::math::wide_integer::size_t MyWidth2,
  #endif
           typename MyLimbType,
           typename MyAllocatorType,
           typename IntegralType,
           std::enable_if_t<(std::is_integral<IntegralType>::value)> const* = nullptr>
  WIDE_INTEGER_CONSTEXPR auto eval_multiply(uintwide_t_backend<MyWidth2, MyLimbType, MyAllocatorType>& result, const IntegralType& n) -> void
  {
    result.representation() *= n;
  }

  template<
  #if defined(WIDE_INTEGER_NAMESPACE)
           const WIDE_INTEGER_NAMESPACE::math::wide_integer::size_t MyWidth2,
  #else
           const ::math::wide_integer::size_t MyWidth2,
  #endif
           typename MyLimbType,
           typename MyAllocatorType>
  WIDE_INTEGER_CONSTEXPR auto eval_divide(uintwide_t_backend<MyWidth2, MyLimbType, MyAllocatorType>& result, const uintwide_t_backend<MyWidth2, MyLimbType, MyAllocatorType>& x) -> void
  {
    result.representation() /= x.crepresentation();
  }

  template<
  #if defined(WIDE_INTEGER_NAMESPACE)
           const WIDE_INTEGER_NAMESPACE::math::wide_integer::size_t MyWidth2,
  #else
           const ::math::wide_integer::size_t MyWidth2,
  #endif
           typename MyLimbType,
           typename MyAllocatorType,
           typename IntegralType,
           std::enable_if_t<(   (std::is_integral   <IntegralType>::value)
                             && (std::is_unsigned   <IntegralType>::value)
                             && (std::numeric_limits<IntegralType>::digits <= std::numeric_limits<MyLimbType>::digits))> const* = nullptr>
  WIDE_INTEGER_CONSTEXPR auto eval_divide(uintwide_t_backend<MyWidth2, MyLimbType, MyAllocatorType>& result, const IntegralType& n) -> void
  {
    using local_wide_integer_type = typename uintwide_t_backend<MyWidth2, MyLimbType, MyAllocatorType>::representation_type;

    using local_limb_type = typename local_wide_integer_type::limb_type;

    result.representation().eval_divide_by_single_limb(static_cast<local_limb_type>(n), 0U, nullptr);
  }

  template<
  #if defined(WIDE_INTEGER_NAMESPACE)
           const WIDE_INTEGER_NAMESPACE::math::wide_integer::size_t MyWidth2,
  #else
           const ::math::wide_integer::size_t MyWidth2,
  #endif
           typename MyLimbType,
           typename MyAllocatorType,
           typename IntegralType,
           std::enable_if_t<(   (std::is_integral   <IntegralType>::value)
                             && (std::is_unsigned   <IntegralType>::value)
                             && (std::numeric_limits<IntegralType>::digits) > std::numeric_limits<MyLimbType>::digits)> const* = nullptr>
  WIDE_INTEGER_CONSTEXPR auto eval_divide(uintwide_t_backend<MyWidth2, MyLimbType, MyAllocatorType>& result, const IntegralType& n) -> void
  {
    result.representation() /= n;
  }

  template<
  #if defined(WIDE_INTEGER_NAMESPACE)
           const WIDE_INTEGER_NAMESPACE::math::wide_integer::size_t MyWidth2,
  #else
           const ::math::wide_integer::size_t MyWidth2,
  #endif
           typename MyLimbType,
           typename MyAllocatorType>
  WIDE_INTEGER_CONSTEXPR auto eval_modulus(uintwide_t_backend<MyWidth2, MyLimbType, MyAllocatorType>& result, const uintwide_t_backend<MyWidth2, MyLimbType, MyAllocatorType>& x) -> void
  {
    result.representation() %= x.crepresentation();
  }

  template<
  #if defined(WIDE_INTEGER_NAMESPACE)
           const WIDE_INTEGER_NAMESPACE::math::wide_integer::size_t MyWidth2,
  #else
           const ::math::wide_integer::size_t MyWidth2,
  #endif
           typename MyLimbType,
           typename MyAllocatorType,
           typename IntegralType,
           std::enable_if_t<(   (std::is_integral   <IntegralType>::value)
                             && (std::is_unsigned   <IntegralType>::value)
                             && (std::numeric_limits<IntegralType>::digits <= std::numeric_limits<MyLimbType>::digits))> const* = nullptr>
  WIDE_INTEGER_CONSTEXPR auto eval_integer_modulus(uintwide_t_backend<MyWidth2, MyLimbType, MyAllocatorType>& x, const IntegralType& n) -> IntegralType
  {
    using local_wide_integer_type = typename uintwide_t_backend<MyWidth2, MyLimbType, MyAllocatorType>::representation_type;

    typename uintwide_t_backend<MyWidth2, MyLimbType, MyAllocatorType>::representation_type rem;

    local_wide_integer_type(x.crepresentation()).eval_divide_by_single_limb(n, 0U, &rem);

    return static_cast<IntegralType>(rem);
  }

  template<
  #if defined(WIDE_INTEGER_NAMESPACE)
           const WIDE_INTEGER_NAMESPACE::math::wide_integer::size_t MyWidth2,
  #else
           const ::math::wide_integer::size_t MyWidth2,
  #endif
           typename MyLimbType,
           typename MyAllocatorType,
           typename IntegralType,
           std::enable_if_t<(   (std::is_integral   <IntegralType>::value)
                             && (std::is_unsigned   <IntegralType>::value)
                             && (std::numeric_limits<IntegralType>::digits) > std::numeric_limits<MyLimbType>::digits)> const* = nullptr>
  WIDE_INTEGER_CONSTEXPR auto eval_integer_modulus(uintwide_t_backend<MyWidth2, MyLimbType, MyAllocatorType>& x, const IntegralType& n) -> IntegralType
  {
    const uintwide_t_backend<MyWidth2, MyLimbType, MyAllocatorType> rem = x.crepresentation() % uintwide_t_backend<MyWidth2, MyLimbType, MyAllocatorType>(n);

    return static_cast<IntegralType>(rem);
  }

  template<
  #if defined(WIDE_INTEGER_NAMESPACE)
           const WIDE_INTEGER_NAMESPACE::math::wide_integer::size_t MyWidth2,
  #else
           const ::math::wide_integer::size_t MyWidth2,
  #endif
           typename MyLimbType,
           typename MyAllocatorType>
  WIDE_INTEGER_CONSTEXPR auto eval_bitwise_and(uintwide_t_backend<MyWidth2, MyLimbType, MyAllocatorType>& result, const uintwide_t_backend<MyWidth2, MyLimbType, MyAllocatorType>& x) -> void
  {
    result.representation() &= x.crepresentation();
  }

  template<
  #if defined(WIDE_INTEGER_NAMESPACE)
           const WIDE_INTEGER_NAMESPACE::math::wide_integer::size_t MyWidth2,
  #else
           const ::math::wide_integer::size_t MyWidth2,
  #endif
           typename MyLimbType,
           typename MyAllocatorType>
  WIDE_INTEGER_CONSTEXPR auto eval_bitwise_or(      uintwide_t_backend<MyWidth2, MyLimbType, MyAllocatorType>& result,
                                              const uintwide_t_backend<MyWidth2, MyLimbType, MyAllocatorType>& x) -> void
  {
    result.representation() |= x.crepresentation();
  }

  template<
  #if defined(WIDE_INTEGER_NAMESPACE)
           const WIDE_INTEGER_NAMESPACE::math::wide_integer::size_t MyWidth2,
  #else
           const ::math::wide_integer::size_t MyWidth2,
  #endif
           typename MyLimbType,
           typename MyAllocatorType>
  WIDE_INTEGER_CONSTEXPR auto eval_bitwise_xor(uintwide_t_backend<MyWidth2, MyLimbType, MyAllocatorType>& result, const uintwide_t_backend<MyWidth2, MyLimbType, MyAllocatorType>& x) -> void
  {
    result.representation() ^= x.crepresentation();
  }

  template<
  #if defined(WIDE_INTEGER_NAMESPACE)
           const WIDE_INTEGER_NAMESPACE::math::wide_integer::size_t MyWidth2,
  #else
           const ::math::wide_integer::size_t MyWidth2,
  #endif
           typename MyLimbType,
           typename MyAllocatorType>
  WIDE_INTEGER_CONSTEXPR auto eval_complement(      uintwide_t_backend<MyWidth2, MyLimbType, MyAllocatorType>& result,
                                              const uintwide_t_backend<MyWidth2, MyLimbType, MyAllocatorType>& x) -> void
  {
    using local_limb_array_type =
      typename uintwide_t_backend<MyWidth2, MyLimbType, MyAllocatorType>::representation_type::representation_type;

    using local_size_type = typename local_limb_array_type::size_type;

    for(auto   i = static_cast<local_size_type>(0U);
               i < result.crepresentation().crepresentation().size();
             ++i)
    {
      using local_value_type = typename local_limb_array_type::value_type;

      result.representation().representation()[i] =
        static_cast<local_value_type>
        (
          ~x.crepresentation().crepresentation()[i]
        );
    }
  }

  template<
  #if defined(WIDE_INTEGER_NAMESPACE)
           const WIDE_INTEGER_NAMESPACE::math::wide_integer::size_t MyWidth2,
  #else
           const ::math::wide_integer::size_t MyWidth2,
  #endif
           typename MyLimbType,
           typename MyAllocatorType>
  WIDE_INTEGER_CONSTEXPR auto eval_powm(      uintwide_t_backend<MyWidth2, MyLimbType, MyAllocatorType>& result,
                                        const uintwide_t_backend<MyWidth2, MyLimbType, MyAllocatorType>& b,
                                        const uintwide_t_backend<MyWidth2, MyLimbType, MyAllocatorType>& p,
                                        const uintwide_t_backend<MyWidth2, MyLimbType, MyAllocatorType>& m) -> void
  {
    result.representation() = powm(b.crepresentation(),
                                   p.crepresentation(),
                                   m.crepresentation());
  }

  template<
  #if defined(WIDE_INTEGER_NAMESPACE)
           const WIDE_INTEGER_NAMESPACE::math::wide_integer::size_t MyWidth2,
  #else
           const ::math::wide_integer::size_t MyWidth2,
  #endif
           typename MyLimbType,
           typename MyAllocatorType,
           typename OtherIntegralTypeM,
           std::enable_if_t<(std::is_integral<OtherIntegralTypeM>::value)> const* = nullptr>
  WIDE_INTEGER_CONSTEXPR auto eval_powm(      uintwide_t_backend<MyWidth2, MyLimbType, MyAllocatorType>& result,
                                        const uintwide_t_backend<MyWidth2, MyLimbType, MyAllocatorType>& b,
                                        const uintwide_t_backend<MyWidth2, MyLimbType, MyAllocatorType>& p,
                                        const OtherIntegralTypeM                                         m) -> void
  {
    result.representation() = powm(b.crepresentation(),
                                   p.crepresentation(),
                                   m);
  }

  template<
  #if defined(WIDE_INTEGER_NAMESPACE)
           const WIDE_INTEGER_NAMESPACE::math::wide_integer::size_t MyWidth2,
  #else
           const ::math::wide_integer::size_t MyWidth2,
  #endif
           typename MyLimbType,
           typename MyAllocatorType,
           typename OtherIntegralTypeP,
           std::enable_if_t<(std::is_integral<OtherIntegralTypeP>::value)> const* = nullptr>
  WIDE_INTEGER_CONSTEXPR auto eval_powm(      uintwide_t_backend<MyWidth2, MyLimbType, MyAllocatorType>& result,
                                        const uintwide_t_backend<MyWidth2, MyLimbType, MyAllocatorType>& b,
                                        const OtherIntegralTypeP                                         p,
                                        const uintwide_t_backend<MyWidth2, MyLimbType, MyAllocatorType>& m) -> void
  {
    result.representation() = powm(b.crepresentation(),
                                   p,
                                   m.crepresentation());
  }

  template<
  #if defined(WIDE_INTEGER_NAMESPACE)
           const WIDE_INTEGER_NAMESPACE::math::wide_integer::size_t MyWidth2,
  #else
           const ::math::wide_integer::size_t MyWidth2,
  #endif
           typename MyLimbType,
           typename MyAllocatorType,
           typename IntegralType,
           std::enable_if_t<(std::is_integral<IntegralType>::value)> const* = nullptr>
  WIDE_INTEGER_CONSTEXPR auto eval_left_shift(uintwide_t_backend<MyWidth2, MyLimbType, MyAllocatorType>& result, const IntegralType& n) -> void
  {
    result.representation() <<= n;
  }

  template<
  #if defined(WIDE_INTEGER_NAMESPACE)
           const WIDE_INTEGER_NAMESPACE::math::wide_integer::size_t MyWidth2,
  #else
           const ::math::wide_integer::size_t MyWidth2,
  #endif
           typename MyLimbType,
           typename MyAllocatorType,
           typename IntegralType,
           std::enable_if_t<(std::is_integral<IntegralType>::value)> const* = nullptr>
  WIDE_INTEGER_CONSTEXPR auto eval_right_shift(uintwide_t_backend<MyWidth2, MyLimbType, MyAllocatorType>& result, const IntegralType& n) -> void
  {
    result.representation() >>= n;
  }

  template<
  #if defined(WIDE_INTEGER_NAMESPACE)
           const WIDE_INTEGER_NAMESPACE::math::wide_integer::size_t MyWidth2,
  #else
           const ::math::wide_integer::size_t MyWidth2,
  #endif
           typename MyLimbType,
           typename MyAllocatorType>
  constexpr auto eval_lsb(const uintwide_t_backend<MyWidth2, MyLimbType, MyAllocatorType>& a) -> unsigned
  {
    return static_cast<unsigned>(lsb(a.crepresentation()));
  }

  template<
  #if defined(WIDE_INTEGER_NAMESPACE)
           const WIDE_INTEGER_NAMESPACE::math::wide_integer::size_t MyWidth2,
  #else
           const ::math::wide_integer::size_t MyWidth2,
  #endif
           typename MyLimbType,
           typename MyAllocatorType>
  constexpr auto eval_msb(const uintwide_t_backend<MyWidth2, MyLimbType, MyAllocatorType>& a) -> unsigned
  {
    return static_cast<unsigned>(msb(a.crepresentation()));
  }

  template<
  #if defined(WIDE_INTEGER_NAMESPACE)
           const WIDE_INTEGER_NAMESPACE::math::wide_integer::size_t MyWidth2,
  #else
           const ::math::wide_integer::size_t MyWidth2,
  #endif
           typename MyLimbType,
           typename MyAllocatorType>
  constexpr auto eval_eq(const uintwide_t_backend<MyWidth2, MyLimbType, MyAllocatorType>& a,
                         const uintwide_t_backend<MyWidth2, MyLimbType, MyAllocatorType>& b) -> bool
  {
    return (a.compare(b) == 0);
  }

  template<
  #if defined(WIDE_INTEGER_NAMESPACE)
           const WIDE_INTEGER_NAMESPACE::math::wide_integer::size_t MyWidth2,
  #else
           const ::math::wide_integer::size_t MyWidth2,
  #endif
           typename MyLimbType,
           typename MyAllocatorType,
           typename ArithmeticType,
           std::enable_if_t<(std::is_arithmetic <ArithmeticType>::value)> const* = nullptr>
  constexpr auto eval_eq(const uintwide_t_backend<MyWidth2, MyLimbType, MyAllocatorType>& a,
                               ArithmeticType                                             b) -> bool
  {
    return (a.compare(b) == 0);
  }

  template<
  #if defined(WIDE_INTEGER_NAMESPACE)
           const WIDE_INTEGER_NAMESPACE::math::wide_integer::size_t MyWidth2,
  #else
           const ::math::wide_integer::size_t MyWidth2,
  #endif
           typename MyLimbType,
           typename MyAllocatorType,
           typename ArithmeticType,
           std::enable_if_t<(std::is_arithmetic <ArithmeticType>::value)> const* = nullptr>
  constexpr auto eval_eq(      ArithmeticType                                             a,
                         const uintwide_t_backend<MyWidth2, MyLimbType, MyAllocatorType>& b) -> bool
  {
    return (uintwide_t_backend<MyWidth2, MyLimbType, MyAllocatorType>(a).compare(b) == 0);
  }

  template<
  #if defined(WIDE_INTEGER_NAMESPACE)
           const WIDE_INTEGER_NAMESPACE::math::wide_integer::size_t MyWidth2,
  #else
           const ::math::wide_integer::size_t MyWidth2,
  #endif
           typename MyLimbType,
           typename MyAllocatorType>
  constexpr auto eval_gt(const uintwide_t_backend<MyWidth2, MyLimbType, MyAllocatorType>& a,
                         const uintwide_t_backend<MyWidth2, MyLimbType, MyAllocatorType>& b) -> bool
  {
    return (a.compare(b) == 1);
  }

  template<
  #if defined(WIDE_INTEGER_NAMESPACE)
           const WIDE_INTEGER_NAMESPACE::math::wide_integer::size_t MyWidth2,
  #else
           const ::math::wide_integer::size_t MyWidth2,
  #endif
           typename MyLimbType,
           typename MyAllocatorType,
           typename ArithmeticType,
           std::enable_if_t<(std::is_arithmetic <ArithmeticType>::value)> const* = nullptr>
  constexpr auto eval_gt(const uintwide_t_backend<MyWidth2, MyLimbType, MyAllocatorType>& a,
                               ArithmeticType                                             b) -> bool
  {
    return (a.compare(b) == 1);
  }

  template<
  #if defined(WIDE_INTEGER_NAMESPACE)
           const WIDE_INTEGER_NAMESPACE::math::wide_integer::size_t MyWidth2,
  #else
           const ::math::wide_integer::size_t MyWidth2,
  #endif
           typename MyLimbType,
           typename MyAllocatorType,
           typename ArithmeticType,
           std::enable_if_t<(std::is_arithmetic <ArithmeticType>::value)> const* = nullptr>
  constexpr auto eval_gt(      ArithmeticType                                             a,
                         const uintwide_t_backend<MyWidth2, MyLimbType, MyAllocatorType>& b) -> bool
  {
    return (uintwide_t_backend<MyWidth2, MyLimbType, MyAllocatorType>(a).compare(b) == 1);
  }

  template<
  #if defined(WIDE_INTEGER_NAMESPACE)
           const WIDE_INTEGER_NAMESPACE::math::wide_integer::size_t MyWidth2,
  #else
           const ::math::wide_integer::size_t MyWidth2,
  #endif
           typename MyLimbType,
           typename MyAllocatorType>
  constexpr auto eval_lt(const uintwide_t_backend<MyWidth2, MyLimbType, MyAllocatorType>& a,
                         const uintwide_t_backend<MyWidth2, MyLimbType, MyAllocatorType>& b) -> bool
  {
    return (a.compare(b) == -1);
  }

  template<
  #if defined(WIDE_INTEGER_NAMESPACE)
           const WIDE_INTEGER_NAMESPACE::math::wide_integer::size_t MyWidth2,
  #else
           const ::math::wide_integer::size_t MyWidth2,
  #endif
           typename MyLimbType,
           typename MyAllocatorType,
           typename ArithmeticType,
           std::enable_if_t<(std::is_arithmetic <ArithmeticType>::value)> const* = nullptr>
  constexpr auto eval_lt(const uintwide_t_backend<MyWidth2, MyLimbType, MyAllocatorType>& a,
                               ArithmeticType                                             b) -> bool
  {
    return (a.compare(b) == -1);
  }

  template<
  #if defined(WIDE_INTEGER_NAMESPACE)
           const WIDE_INTEGER_NAMESPACE::math::wide_integer::size_t MyWidth2,
  #else
           const ::math::wide_integer::size_t MyWidth2,
  #endif
           typename MyLimbType,
           typename MyAllocatorType,
           typename ArithmeticType,
           std::enable_if_t<(std::is_arithmetic <ArithmeticType>::value)> const* = nullptr>
  constexpr auto eval_lt(      ArithmeticType                                             a,
                         const uintwide_t_backend<MyWidth2, MyLimbType, MyAllocatorType>& b) -> bool
  {
    return (uintwide_t_backend<MyWidth2, MyLimbType, MyAllocatorType>(a).compare(b) == -1);
  }

  template<
  #if defined(WIDE_INTEGER_NAMESPACE)
           const WIDE_INTEGER_NAMESPACE::math::wide_integer::size_t MyWidth2,
  #else
           const ::math::wide_integer::size_t MyWidth2,
  #endif
           typename MyLimbType,
           typename MyAllocatorType>
  constexpr auto eval_is_zero(const uintwide_t_backend<MyWidth2, MyLimbType, MyAllocatorType>& x) -> bool
  {
    return (x.crepresentation().is_zero());
  }

  template<
  #if defined(WIDE_INTEGER_NAMESPACE)
           const WIDE_INTEGER_NAMESPACE::math::wide_integer::size_t MyWidth2,
  #else
           const ::math::wide_integer::size_t MyWidth2,
  #endif
           typename MyLimbType,
           typename MyAllocatorType>
  constexpr auto eval_get_sign(const uintwide_t_backend<MyWidth2, MyLimbType, MyAllocatorType>& x) -> int
  {
    return (eval_is_zero(x) ? 0 : 1);
  }

  template<typename UnsignedIntegralType,
  #if defined(WIDE_INTEGER_NAMESPACE)
           const WIDE_INTEGER_NAMESPACE::math::wide_integer::size_t MyWidth2,
  #else
           const ::math::wide_integer::size_t MyWidth2,
  #endif
           typename MyLimbType,
           typename MyAllocatorType>
  WIDE_INTEGER_CONSTEXPR auto eval_convert_to
  (
          UnsignedIntegralType*                                                                 result,
    const uintwide_t_backend<MyWidth2, MyLimbType, MyAllocatorType>&                            val,
          std::enable_if_t<(    (std::is_integral<UnsignedIntegralType>::value)
                            && (!std::is_signed  <UnsignedIntegralType>::value))>* p_nullparam = nullptr
  ) -> void
  {
    static_cast<void>(p_nullparam);

    using local_unsigned_integral_type = UnsignedIntegralType;

    static_assert((!std::is_signed<local_unsigned_integral_type>::value),
                  "Error: Wrong signed instantiation (destination type should be unsigned).");

    *result = static_cast<local_unsigned_integral_type>(val.crepresentation());
  }

  template<typename SignedIntegralType,
  #if defined(WIDE_INTEGER_NAMESPACE)
           const WIDE_INTEGER_NAMESPACE::math::wide_integer::size_t MyWidth2,
  #else
           const ::math::wide_integer::size_t MyWidth2,
  #endif
           typename MyLimbType,
           typename MyAllocatorType>
  WIDE_INTEGER_CONSTEXPR auto eval_convert_to
  (
          SignedIntegralType*                                                                result,
    const uintwide_t_backend<MyWidth2, MyLimbType, MyAllocatorType>&                         val,
          std::enable_if_t<(   (std::is_integral<SignedIntegralType>::value)
                            && (std::is_signed  <SignedIntegralType>::value))>* p_nullparam = nullptr
  ) -> void
  {
    static_cast<void>(p_nullparam);

    using local_signed_integral_type = SignedIntegralType;

    static_assert(std::is_signed<local_signed_integral_type>::value,
                  "Error: Wrong unsigned instantiation (destination type should be signed).");

    *result = static_cast<local_signed_integral_type>(val.crepresentation());
  }

  template<
  #if defined(WIDE_INTEGER_NAMESPACE)
           const WIDE_INTEGER_NAMESPACE::math::wide_integer::size_t MyWidth2,
  #else
           const ::math::wide_integer::size_t MyWidth2,
  #endif
           typename MyLimbType,
           typename MyAllocatorType>
  WIDE_INTEGER_CONSTEXPR auto eval_convert_to(long double* result,
                                              const uintwide_t_backend<MyWidth2, MyLimbType, MyAllocatorType>& val) -> void
  {
    *result = static_cast<long double>(val.crepresentation());
  }

  template<
  #if defined(WIDE_INTEGER_NAMESPACE)
           const WIDE_INTEGER_NAMESPACE::math::wide_integer::size_t MyWidth2,
  #else
           const ::math::wide_integer::size_t MyWidth2,
  #endif
           typename MyLimbType,
           typename MyAllocatorType>
  constexpr auto hash_value(const uintwide_t_backend<MyWidth2, MyLimbType, MyAllocatorType>& val) -> std::size_t
  {
    return val.hash();
  }

  #if(__cplusplus >= 201703L)
  } // namespace boost::multiprecision
  #else
  } // namespace multiprecision
  } // namespace boost
  #endif

  #if (BOOST_VERSION < 107900)

  #if(__cplusplus >= 201703L)
  namespace boost::math::policies {
  #else
  namespace boost { namespace math { namespace policies { // NOLINT(modernize-concat-nested-namespaces)
  #endif

  // Specialization of the precision structure.
  template<
  #if defined(WIDE_INTEGER_NAMESPACE)
           const WIDE_INTEGER_NAMESPACE::math::wide_integer::size_t MyWidth2,
  #else
           const ::math::wide_integer::size_t MyWidth2,
  #endif
           typename MyLimbType,
           typename MyAllocatorType,
           typename ThisPolicy,
           const boost::multiprecision::expression_template_option ExpressionTemplatesOptions>
  struct precision<boost::multiprecision::number<boost::multiprecision::uintwide_t_backend<MyWidth2, MyLimbType, MyAllocatorType>,
                                                 ExpressionTemplatesOptions>,
                   ThisPolicy>
  {
    using precision_type = typename ThisPolicy::precision_type;

    using local_digits_2 = digits2<MyWidth2>;

    #if (BOOST_VERSION <= 107500)
    using type = typename mpl::if_c       <((local_digits_2::value <= precision_type::value) || (precision_type::value <= 0)),
                                             local_digits_2,
                                             precision_type>::type;
    #else
    using type = typename std::conditional<((local_digits_2::value <= precision_type::value) || (precision_type::value <= 0)),
                                             local_digits_2,
                                             precision_type>::type;
    #endif
  };

  #if(__cplusplus >= 201703L)
  } // namespace boost::math::policies
  #else
  } // namespace policies
  } // namespace math
  } // namespace boost
  #endif

  #endif

  namespace std // NOLINT(cert-dcl58-cpp)
  {
    template<
    #if defined(WIDE_INTEGER_NAMESPACE)
             const WIDE_INTEGER_NAMESPACE::math::wide_integer::size_t MyWidth2,
    #else
             const ::math::wide_integer::size_t MyWidth2,
    #endif
             typename MyLimbType,
             typename MyAllocatorType,
             const boost::multiprecision::expression_template_option ExpressionTemplatesOptions>
    class numeric_limits<boost::multiprecision::number<boost::multiprecision::uintwide_t_backend<MyWidth2, MyLimbType, MyAllocatorType>,
                                                       ExpressionTemplatesOptions>>
    {
    public:
      static constexpr bool is_specialized = true;
      static constexpr bool is_signed      = false;
      static constexpr bool is_integer     = true;
      static constexpr bool is_exact       = true;
      static constexpr bool is_bounded     = true;
      static constexpr bool is_modulo      = false;
      static constexpr bool is_iec559      = false;
      static constexpr int  digits         = MyWidth2;
      static constexpr int  digits10       = static_cast<int>((MyWidth2 * 301LL) / 1000LL);
      static constexpr int  max_digits10   = static_cast<int>((MyWidth2 * 301LL) / 1000LL);

      #if defined(WIDE_INTEGER_NAMESPACE)
      static constexpr int max_exponent    = std::numeric_limits<WIDE_INTEGER_NAMESPACE::math::wide_integer::uintwide_t<MyWidth2, MyLimbType, MyAllocatorType>>::max_exponent;
      static constexpr int max_exponent10  = std::numeric_limits<WIDE_INTEGER_NAMESPACE::math::wide_integer::uintwide_t<MyWidth2, MyLimbType, MyAllocatorType>>::max_exponent10;
      static constexpr int min_exponent    = std::numeric_limits<WIDE_INTEGER_NAMESPACE::math::wide_integer::uintwide_t<MyWidth2, MyLimbType, MyAllocatorType>>::min_exponent;
      static constexpr int min_exponent10  = std::numeric_limits<WIDE_INTEGER_NAMESPACE::math::wide_integer::uintwide_t<MyWidth2, MyLimbType, MyAllocatorType>>::min_exponent10;
      #else
      static constexpr int max_exponent    = std::numeric_limits<::math::wide_integer::uintwide_t<MyWidth2, MyLimbType, MyAllocatorType>>::max_exponent;
      static constexpr int max_exponent10  = std::numeric_limits<::math::wide_integer::uintwide_t<MyWidth2, MyLimbType, MyAllocatorType>>::max_exponent10;
      static constexpr int min_exponent    = std::numeric_limits<::math::wide_integer::uintwide_t<MyWidth2, MyLimbType, MyAllocatorType>>::min_exponent;
      static constexpr int min_exponent10  = std::numeric_limits<::math::wide_integer::uintwide_t<MyWidth2, MyLimbType, MyAllocatorType>>::min_exponent10;
      #endif

      static constexpr int                     radix             = 2;
      static constexpr std::float_round_style  round_style       = std::round_to_nearest;
      static constexpr bool                    has_infinity      = false;
      static constexpr bool                    has_quiet_NaN     = false;
      static constexpr bool                    has_signaling_NaN = false;
      static constexpr std::float_denorm_style has_denorm        = std::denorm_absent;
      static constexpr bool                    has_denorm_loss   = false;
      static constexpr bool                    traps             = false;
      static constexpr bool                    tinyness_before   = false;

      #if defined(WIDE_INTEGER_NAMESPACE)
      static WIDE_INTEGER_CONSTEXPR auto (min)        () -> boost::multiprecision::number<boost::multiprecision::uintwide_t_backend<MyWidth2, MyLimbType, MyAllocatorType>, ExpressionTemplatesOptions> { return boost::multiprecision::uintwide_t_backend<MyWidth2, MyLimbType, MyAllocatorType>((std::numeric_limits<WIDE_INTEGER_NAMESPACE::math::wide_integer::uintwide_t<MyWidth2, MyLimbType, MyAllocatorType>>::min)()       ); }
      static WIDE_INTEGER_CONSTEXPR auto (max)        () -> boost::multiprecision::number<boost::multiprecision::uintwide_t_backend<MyWidth2, MyLimbType, MyAllocatorType>, ExpressionTemplatesOptions> { return boost::multiprecision::uintwide_t_backend<MyWidth2, MyLimbType, MyAllocatorType>((std::numeric_limits<WIDE_INTEGER_NAMESPACE::math::wide_integer::uintwide_t<MyWidth2, MyLimbType, MyAllocatorType>>::max)()       ); }
      static WIDE_INTEGER_CONSTEXPR auto lowest       () -> boost::multiprecision::number<boost::multiprecision::uintwide_t_backend<MyWidth2, MyLimbType, MyAllocatorType>, ExpressionTemplatesOptions> { return boost::multiprecision::uintwide_t_backend<MyWidth2, MyLimbType, MyAllocatorType> (std::numeric_limits<WIDE_INTEGER_NAMESPACE::math::wide_integer::uintwide_t<MyWidth2, MyLimbType, MyAllocatorType>>::lowest       ); }
      static WIDE_INTEGER_CONSTEXPR auto epsilon      () -> boost::multiprecision::number<boost::multiprecision::uintwide_t_backend<MyWidth2, MyLimbType, MyAllocatorType>, ExpressionTemplatesOptions> { return boost::multiprecision::uintwide_t_backend<MyWidth2, MyLimbType, MyAllocatorType> (std::numeric_limits<WIDE_INTEGER_NAMESPACE::math::wide_integer::uintwide_t<MyWidth2, MyLimbType, MyAllocatorType>>::epsilon      ); }
      static WIDE_INTEGER_CONSTEXPR auto round_error  () -> boost::multiprecision::number<boost::multiprecision::uintwide_t_backend<MyWidth2, MyLimbType, MyAllocatorType>, ExpressionTemplatesOptions> { return boost::multiprecision::uintwide_t_backend<MyWidth2, MyLimbType, MyAllocatorType> (std::numeric_limits<WIDE_INTEGER_NAMESPACE::math::wide_integer::uintwide_t<MyWidth2, MyLimbType, MyAllocatorType>>::round_error  ); }
      static WIDE_INTEGER_CONSTEXPR auto infinity     () -> boost::multiprecision::number<boost::multiprecision::uintwide_t_backend<MyWidth2, MyLimbType, MyAllocatorType>, ExpressionTemplatesOptions> { return boost::multiprecision::uintwide_t_backend<MyWidth2, MyLimbType, MyAllocatorType> (std::numeric_limits<WIDE_INTEGER_NAMESPACE::math::wide_integer::uintwide_t<MyWidth2, MyLimbType, MyAllocatorType>>::infinity     ); }
      static WIDE_INTEGER_CONSTEXPR auto quiet_NaN    () -> boost::multiprecision::number<boost::multiprecision::uintwide_t_backend<MyWidth2, MyLimbType, MyAllocatorType>, ExpressionTemplatesOptions> { return boost::multiprecision::uintwide_t_backend<MyWidth2, MyLimbType, MyAllocatorType> (std::numeric_limits<WIDE_INTEGER_NAMESPACE::math::wide_integer::uintwide_t<MyWidth2, MyLimbType, MyAllocatorType>>::quiet_NaN    ); } // NOLINT(readability-identifier-naming)
      static WIDE_INTEGER_CONSTEXPR auto signaling_NaN() -> boost::multiprecision::number<boost::multiprecision::uintwide_t_backend<MyWidth2, MyLimbType, MyAllocatorType>, ExpressionTemplatesOptions> { return boost::multiprecision::uintwide_t_backend<MyWidth2, MyLimbType, MyAllocatorType> (std::numeric_limits<WIDE_INTEGER_NAMESPACE::math::wide_integer::uintwide_t<MyWidth2, MyLimbType, MyAllocatorType>>::signaling_NaN); } // NOLINT(readability-identifier-naming)
      static WIDE_INTEGER_CONSTEXPR auto denorm_min   () -> boost::multiprecision::number<boost::multiprecision::uintwide_t_backend<MyWidth2, MyLimbType, MyAllocatorType>, ExpressionTemplatesOptions> { return boost::multiprecision::uintwide_t_backend<MyWidth2, MyLimbType, MyAllocatorType> (std::numeric_limits<WIDE_INTEGER_NAMESPACE::math::wide_integer::uintwide_t<MyWidth2, MyLimbType, MyAllocatorType>>::denorm_min   ); }
      #else
      static WIDE_INTEGER_CONSTEXPR auto (min)        () -> boost::multiprecision::number<boost::multiprecision::uintwide_t_backend<MyWidth2, MyLimbType, MyAllocatorType>, ExpressionTemplatesOptions> { return boost::multiprecision::uintwide_t_backend<MyWidth2, MyLimbType, MyAllocatorType>((std::numeric_limits<::math::wide_integer::uintwide_t<MyWidth2, MyLimbType, MyAllocatorType>>::min)()       ); }
      static WIDE_INTEGER_CONSTEXPR auto (max)        () -> boost::multiprecision::number<boost::multiprecision::uintwide_t_backend<MyWidth2, MyLimbType, MyAllocatorType>, ExpressionTemplatesOptions> { return boost::multiprecision::uintwide_t_backend<MyWidth2, MyLimbType, MyAllocatorType>((std::numeric_limits<::math::wide_integer::uintwide_t<MyWidth2, MyLimbType, MyAllocatorType>>::max)()       ); }
      static WIDE_INTEGER_CONSTEXPR auto lowest       () -> boost::multiprecision::number<boost::multiprecision::uintwide_t_backend<MyWidth2, MyLimbType, MyAllocatorType>, ExpressionTemplatesOptions> { return boost::multiprecision::uintwide_t_backend<MyWidth2, MyLimbType, MyAllocatorType> (std::numeric_limits<::math::wide_integer::uintwide_t<MyWidth2, MyLimbType, MyAllocatorType>>::lowest       ); }
      static WIDE_INTEGER_CONSTEXPR auto epsilon      () -> boost::multiprecision::number<boost::multiprecision::uintwide_t_backend<MyWidth2, MyLimbType, MyAllocatorType>, ExpressionTemplatesOptions> { return boost::multiprecision::uintwide_t_backend<MyWidth2, MyLimbType, MyAllocatorType> (std::numeric_limits<::math::wide_integer::uintwide_t<MyWidth2, MyLimbType, MyAllocatorType>>::epsilon      ); }
      static WIDE_INTEGER_CONSTEXPR auto round_error  () -> boost::multiprecision::number<boost::multiprecision::uintwide_t_backend<MyWidth2, MyLimbType, MyAllocatorType>, ExpressionTemplatesOptions> { return boost::multiprecision::uintwide_t_backend<MyWidth2, MyLimbType, MyAllocatorType> (std::numeric_limits<::math::wide_integer::uintwide_t<MyWidth2, MyLimbType, MyAllocatorType>>::round_error  ); }
      static WIDE_INTEGER_CONSTEXPR auto infinity     () -> boost::multiprecision::number<boost::multiprecision::uintwide_t_backend<MyWidth2, MyLimbType, MyAllocatorType>, ExpressionTemplatesOptions> { return boost::multiprecision::uintwide_t_backend<MyWidth2, MyLimbType, MyAllocatorType> (std::numeric_limits<::math::wide_integer::uintwide_t<MyWidth2, MyLimbType, MyAllocatorType>>::infinity     ); }
      static WIDE_INTEGER_CONSTEXPR auto quiet_NaN    () -> boost::multiprecision::number<boost::multiprecision::uintwide_t_backend<MyWidth2, MyLimbType, MyAllocatorType>, ExpressionTemplatesOptions> { return boost::multiprecision::uintwide_t_backend<MyWidth2, MyLimbType, MyAllocatorType> (std::numeric_limits<::math::wide_integer::uintwide_t<MyWidth2, MyLimbType, MyAllocatorType>>::quiet_NaN    ); } // NOLINT(readability-identifier-naming)
      static WIDE_INTEGER_CONSTEXPR auto signaling_NaN() -> boost::multiprecision::number<boost::multiprecision::uintwide_t_backend<MyWidth2, MyLimbType, MyAllocatorType>, ExpressionTemplatesOptions> { return boost::multiprecision::uintwide_t_backend<MyWidth2, MyLimbType, MyAllocatorType> (std::numeric_limits<::math::wide_integer::uintwide_t<MyWidth2, MyLimbType, MyAllocatorType>>::signaling_NaN); } // NOLINT(readability-identifier-naming)
      static WIDE_INTEGER_CONSTEXPR auto denorm_min   () -> boost::multiprecision::number<boost::multiprecision::uintwide_t_backend<MyWidth2, MyLimbType, MyAllocatorType>, ExpressionTemplatesOptions> { return boost::multiprecision::uintwide_t_backend<MyWidth2, MyLimbType, MyAllocatorType> (std::numeric_limits<::math::wide_integer::uintwide_t<MyWidth2, MyLimbType, MyAllocatorType>>::denorm_min   ); }
      #endif
    };

    #ifndef BOOST_NO_INCLASS_MEMBER_INITIALIZATION

    #if defined(WIDE_INTEGER_NAMESPACE)
    template<const WIDE_INTEGER_NAMESPACE::math::wide_integer::size_t MyWidth2, typename MyLimbType, typename MyAllocatorType, const boost::multiprecision::expression_template_option ExpressionTemplatesOptions> constexpr bool std::numeric_limits<boost::multiprecision::number<boost::multiprecision::uintwide_t_backend<MyWidth2, MyLimbType, MyAllocatorType>, ExpressionTemplatesOptions>>::is_specialized; // NOLINT(readability-redundant-declaration)
    template<const WIDE_INTEGER_NAMESPACE::math::wide_integer::size_t MyWidth2, typename MyLimbType, typename MyAllocatorType, const boost::multiprecision::expression_template_option ExpressionTemplatesOptions> constexpr bool std::numeric_limits<boost::multiprecision::number<boost::multiprecision::uintwide_t_backend<MyWidth2, MyLimbType, MyAllocatorType>, ExpressionTemplatesOptions>>::is_signed;      // NOLINT(readability-redundant-declaration)
    template<const WIDE_INTEGER_NAMESPACE::math::wide_integer::size_t MyWidth2, typename MyLimbType, typename MyAllocatorType, const boost::multiprecision::expression_template_option ExpressionTemplatesOptions> constexpr bool std::numeric_limits<boost::multiprecision::number<boost::multiprecision::uintwide_t_backend<MyWidth2, MyLimbType, MyAllocatorType>, ExpressionTemplatesOptions>>::is_integer;     // NOLINT(readability-redundant-declaration)
    template<const WIDE_INTEGER_NAMESPACE::math::wide_integer::size_t MyWidth2, typename MyLimbType, typename MyAllocatorType, const boost::multiprecision::expression_template_option ExpressionTemplatesOptions> constexpr bool std::numeric_limits<boost::multiprecision::number<boost::multiprecision::uintwide_t_backend<MyWidth2, MyLimbType, MyAllocatorType>, ExpressionTemplatesOptions>>::is_exact;       // NOLINT(readability-redundant-declaration)
    template<const WIDE_INTEGER_NAMESPACE::math::wide_integer::size_t MyWidth2, typename MyLimbType, typename MyAllocatorType, const boost::multiprecision::expression_template_option ExpressionTemplatesOptions> constexpr bool std::numeric_limits<boost::multiprecision::number<boost::multiprecision::uintwide_t_backend<MyWidth2, MyLimbType, MyAllocatorType>, ExpressionTemplatesOptions>>::is_bounded;     // NOLINT(readability-redundant-declaration)
    template<const WIDE_INTEGER_NAMESPACE::math::wide_integer::size_t MyWidth2, typename MyLimbType, typename MyAllocatorType, const boost::multiprecision::expression_template_option ExpressionTemplatesOptions> constexpr bool std::numeric_limits<boost::multiprecision::number<boost::multiprecision::uintwide_t_backend<MyWidth2, MyLimbType, MyAllocatorType>, ExpressionTemplatesOptions>>::is_modulo;      // NOLINT(readability-redundant-declaration)
    template<const WIDE_INTEGER_NAMESPACE::math::wide_integer::size_t MyWidth2, typename MyLimbType, typename MyAllocatorType, const boost::multiprecision::expression_template_option ExpressionTemplatesOptions> constexpr bool std::numeric_limits<boost::multiprecision::number<boost::multiprecision::uintwide_t_backend<MyWidth2, MyLimbType, MyAllocatorType>, ExpressionTemplatesOptions>>::is_iec559;      // NOLINT(readability-redundant-declaration)
    template<const WIDE_INTEGER_NAMESPACE::math::wide_integer::size_t MyWidth2, typename MyLimbType, typename MyAllocatorType, const boost::multiprecision::expression_template_option ExpressionTemplatesOptions> constexpr int  std::numeric_limits<boost::multiprecision::number<boost::multiprecision::uintwide_t_backend<MyWidth2, MyLimbType, MyAllocatorType>, ExpressionTemplatesOptions>>::digits;         // NOLINT(readability-redundant-declaration)
    template<const WIDE_INTEGER_NAMESPACE::math::wide_integer::size_t MyWidth2, typename MyLimbType, typename MyAllocatorType, const boost::multiprecision::expression_template_option ExpressionTemplatesOptions> constexpr int  std::numeric_limits<boost::multiprecision::number<boost::multiprecision::uintwide_t_backend<MyWidth2, MyLimbType, MyAllocatorType>, ExpressionTemplatesOptions>>::digits10;       // NOLINT(readability-redundant-declaration)
    template<const WIDE_INTEGER_NAMESPACE::math::wide_integer::size_t MyWidth2, typename MyLimbType, typename MyAllocatorType, const boost::multiprecision::expression_template_option ExpressionTemplatesOptions> constexpr int  std::numeric_limits<boost::multiprecision::number<boost::multiprecision::uintwide_t_backend<MyWidth2, MyLimbType, MyAllocatorType>, ExpressionTemplatesOptions>>::max_digits10;   // NOLINT(readability-redundant-declaration)

    template<const WIDE_INTEGER_NAMESPACE::math::wide_integer::size_t MyWidth2, typename MyLimbType, typename MyAllocatorType, const boost::multiprecision::expression_template_option ExpressionTemplatesOptions> constexpr int std::numeric_limits<boost::multiprecision::number<boost::multiprecision::uintwide_t_backend<MyWidth2, MyLimbType, MyAllocatorType>, ExpressionTemplatesOptions>>::max_exponent;   // NOLINT(readability-redundant-declaration)
    template<const WIDE_INTEGER_NAMESPACE::math::wide_integer::size_t MyWidth2, typename MyLimbType, typename MyAllocatorType, const boost::multiprecision::expression_template_option ExpressionTemplatesOptions> constexpr int std::numeric_limits<boost::multiprecision::number<boost::multiprecision::uintwide_t_backend<MyWidth2, MyLimbType, MyAllocatorType>, ExpressionTemplatesOptions>>::max_exponent10; // NOLINT(readability-redundant-declaration)
    template<const WIDE_INTEGER_NAMESPACE::math::wide_integer::size_t MyWidth2, typename MyLimbType, typename MyAllocatorType, const boost::multiprecision::expression_template_option ExpressionTemplatesOptions> constexpr int std::numeric_limits<boost::multiprecision::number<boost::multiprecision::uintwide_t_backend<MyWidth2, MyLimbType, MyAllocatorType>, ExpressionTemplatesOptions>>::min_exponent;   // NOLINT(readability-redundant-declaration)
    template<const WIDE_INTEGER_NAMESPACE::math::wide_integer::size_t MyWidth2, typename MyLimbType, typename MyAllocatorType, const boost::multiprecision::expression_template_option ExpressionTemplatesOptions> constexpr int std::numeric_limits<boost::multiprecision::number<boost::multiprecision::uintwide_t_backend<MyWidth2, MyLimbType, MyAllocatorType>, ExpressionTemplatesOptions>>::min_exponent10; // NOLINT(readability-redundant-declaration)

    template<const WIDE_INTEGER_NAMESPACE::math::wide_integer::size_t MyWidth2, typename MyLimbType, typename MyAllocatorType, const boost::multiprecision::expression_template_option ExpressionTemplatesOptions> constexpr int                     std::numeric_limits<boost::multiprecision::number<boost::multiprecision::uintwide_t_backend<MyWidth2, MyLimbType, MyAllocatorType>, ExpressionTemplatesOptions>>::radix;             // NOLINT(readability-redundant-declaration)
    template<const WIDE_INTEGER_NAMESPACE::math::wide_integer::size_t MyWidth2, typename MyLimbType, typename MyAllocatorType, const boost::multiprecision::expression_template_option ExpressionTemplatesOptions> constexpr std::float_round_style  std::numeric_limits<boost::multiprecision::number<boost::multiprecision::uintwide_t_backend<MyWidth2, MyLimbType, MyAllocatorType>, ExpressionTemplatesOptions>>::round_style;       // NOLINT(readability-redundant-declaration)
    template<const WIDE_INTEGER_NAMESPACE::math::wide_integer::size_t MyWidth2, typename MyLimbType, typename MyAllocatorType, const boost::multiprecision::expression_template_option ExpressionTemplatesOptions> constexpr bool                    std::numeric_limits<boost::multiprecision::number<boost::multiprecision::uintwide_t_backend<MyWidth2, MyLimbType, MyAllocatorType>, ExpressionTemplatesOptions>>::has_infinity;      // NOLINT(readability-redundant-declaration)
    template<const WIDE_INTEGER_NAMESPACE::math::wide_integer::size_t MyWidth2, typename MyLimbType, typename MyAllocatorType, const boost::multiprecision::expression_template_option ExpressionTemplatesOptions> constexpr bool                    std::numeric_limits<boost::multiprecision::number<boost::multiprecision::uintwide_t_backend<MyWidth2, MyLimbType, MyAllocatorType>, ExpressionTemplatesOptions>>::has_quiet_NaN;     // NOLINT(readability-redundant-declaration)
    template<const WIDE_INTEGER_NAMESPACE::math::wide_integer::size_t MyWidth2, typename MyLimbType, typename MyAllocatorType, const boost::multiprecision::expression_template_option ExpressionTemplatesOptions> constexpr bool                    std::numeric_limits<boost::multiprecision::number<boost::multiprecision::uintwide_t_backend<MyWidth2, MyLimbType, MyAllocatorType>, ExpressionTemplatesOptions>>::has_signaling_NaN; // NOLINT(readability-redundant-declaration)
    template<const WIDE_INTEGER_NAMESPACE::math::wide_integer::size_t MyWidth2, typename MyLimbType, typename MyAllocatorType, const boost::multiprecision::expression_template_option ExpressionTemplatesOptions> constexpr std::float_denorm_style std::numeric_limits<boost::multiprecision::number<boost::multiprecision::uintwide_t_backend<MyWidth2, MyLimbType, MyAllocatorType>, ExpressionTemplatesOptions>>::has_denorm;        // NOLINT(readability-redundant-declaration)
    template<const WIDE_INTEGER_NAMESPACE::math::wide_integer::size_t MyWidth2, typename MyLimbType, typename MyAllocatorType, const boost::multiprecision::expression_template_option ExpressionTemplatesOptions> constexpr bool                    std::numeric_limits<boost::multiprecision::number<boost::multiprecision::uintwide_t_backend<MyWidth2, MyLimbType, MyAllocatorType>, ExpressionTemplatesOptions>>::has_denorm_loss;   // NOLINT(readability-redundant-declaration)
    template<const WIDE_INTEGER_NAMESPACE::math::wide_integer::size_t MyWidth2, typename MyLimbType, typename MyAllocatorType, const boost::multiprecision::expression_template_option ExpressionTemplatesOptions> constexpr bool                    std::numeric_limits<boost::multiprecision::number<boost::multiprecision::uintwide_t_backend<MyWidth2, MyLimbType, MyAllocatorType>, ExpressionTemplatesOptions>>::traps;             // NOLINT(readability-redundant-declaration)
    template<const WIDE_INTEGER_NAMESPACE::math::wide_integer::size_t MyWidth2, typename MyLimbType, typename MyAllocatorType, const boost::multiprecision::expression_template_option ExpressionTemplatesOptions> constexpr bool                    std::numeric_limits<boost::multiprecision::number<boost::multiprecision::uintwide_t_backend<MyWidth2, MyLimbType, MyAllocatorType>, ExpressionTemplatesOptions>>::tinyness_before;   // NOLINT(readability-redundant-declaration)
    #else
    template<const ::math::wide_integer::size_t MyWidth2, typename MyLimbType, typename MyAllocatorType, const boost::multiprecision::expression_template_option ExpressionTemplatesOptions> constexpr bool std::numeric_limits<boost::multiprecision::number<boost::multiprecision::uintwide_t_backend<MyWidth2, MyLimbType, MyAllocatorType>, ExpressionTemplatesOptions>>::is_specialized; // NOLINT(readability-redundant-declaration)
    template<const ::math::wide_integer::size_t MyWidth2, typename MyLimbType, typename MyAllocatorType, const boost::multiprecision::expression_template_option ExpressionTemplatesOptions> constexpr bool std::numeric_limits<boost::multiprecision::number<boost::multiprecision::uintwide_t_backend<MyWidth2, MyLimbType, MyAllocatorType>, ExpressionTemplatesOptions>>::is_signed;      // NOLINT(readability-redundant-declaration)
    template<const ::math::wide_integer::size_t MyWidth2, typename MyLimbType, typename MyAllocatorType, const boost::multiprecision::expression_template_option ExpressionTemplatesOptions> constexpr bool std::numeric_limits<boost::multiprecision::number<boost::multiprecision::uintwide_t_backend<MyWidth2, MyLimbType, MyAllocatorType>, ExpressionTemplatesOptions>>::is_integer;     // NOLINT(readability-redundant-declaration)
    template<const ::math::wide_integer::size_t MyWidth2, typename MyLimbType, typename MyAllocatorType, const boost::multiprecision::expression_template_option ExpressionTemplatesOptions> constexpr bool std::numeric_limits<boost::multiprecision::number<boost::multiprecision::uintwide_t_backend<MyWidth2, MyLimbType, MyAllocatorType>, ExpressionTemplatesOptions>>::is_exact;       // NOLINT(readability-redundant-declaration)
    template<const ::math::wide_integer::size_t MyWidth2, typename MyLimbType, typename MyAllocatorType, const boost::multiprecision::expression_template_option ExpressionTemplatesOptions> constexpr bool std::numeric_limits<boost::multiprecision::number<boost::multiprecision::uintwide_t_backend<MyWidth2, MyLimbType, MyAllocatorType>, ExpressionTemplatesOptions>>::is_bounded;     // NOLINT(readability-redundant-declaration)
    template<const ::math::wide_integer::size_t MyWidth2, typename MyLimbType, typename MyAllocatorType, const boost::multiprecision::expression_template_option ExpressionTemplatesOptions> constexpr bool std::numeric_limits<boost::multiprecision::number<boost::multiprecision::uintwide_t_backend<MyWidth2, MyLimbType, MyAllocatorType>, ExpressionTemplatesOptions>>::is_modulo;      // NOLINT(readability-redundant-declaration)
    template<const ::math::wide_integer::size_t MyWidth2, typename MyLimbType, typename MyAllocatorType, const boost::multiprecision::expression_template_option ExpressionTemplatesOptions> constexpr bool std::numeric_limits<boost::multiprecision::number<boost::multiprecision::uintwide_t_backend<MyWidth2, MyLimbType, MyAllocatorType>, ExpressionTemplatesOptions>>::is_iec559;      // NOLINT(readability-redundant-declaration)
    template<const ::math::wide_integer::size_t MyWidth2, typename MyLimbType, typename MyAllocatorType, const boost::multiprecision::expression_template_option ExpressionTemplatesOptions> constexpr int  std::numeric_limits<boost::multiprecision::number<boost::multiprecision::uintwide_t_backend<MyWidth2, MyLimbType, MyAllocatorType>, ExpressionTemplatesOptions>>::digits;         // NOLINT(readability-redundant-declaration)
    template<const ::math::wide_integer::size_t MyWidth2, typename MyLimbType, typename MyAllocatorType, const boost::multiprecision::expression_template_option ExpressionTemplatesOptions> constexpr int  std::numeric_limits<boost::multiprecision::number<boost::multiprecision::uintwide_t_backend<MyWidth2, MyLimbType, MyAllocatorType>, ExpressionTemplatesOptions>>::digits10;       // NOLINT(readability-redundant-declaration)
    template<const ::math::wide_integer::size_t MyWidth2, typename MyLimbType, typename MyAllocatorType, const boost::multiprecision::expression_template_option ExpressionTemplatesOptions> constexpr int  std::numeric_limits<boost::multiprecision::number<boost::multiprecision::uintwide_t_backend<MyWidth2, MyLimbType, MyAllocatorType>, ExpressionTemplatesOptions>>::max_digits10;   // NOLINT(readability-redundant-declaration)

    template<const ::math::wide_integer::size_t MyWidth2, typename MyLimbType, typename MyAllocatorType, const boost::multiprecision::expression_template_option ExpressionTemplatesOptions> constexpr int std::numeric_limits<boost::multiprecision::number<boost::multiprecision::uintwide_t_backend<MyWidth2, MyLimbType, MyAllocatorType>, ExpressionTemplatesOptions>>::max_exponent;    // NOLINT(readability-redundant-declaration)
    template<const ::math::wide_integer::size_t MyWidth2, typename MyLimbType, typename MyAllocatorType, const boost::multiprecision::expression_template_option ExpressionTemplatesOptions> constexpr int std::numeric_limits<boost::multiprecision::number<boost::multiprecision::uintwide_t_backend<MyWidth2, MyLimbType, MyAllocatorType>, ExpressionTemplatesOptions>>::max_exponent10;  // NOLINT(readability-redundant-declaration)
    template<const ::math::wide_integer::size_t MyWidth2, typename MyLimbType, typename MyAllocatorType, const boost::multiprecision::expression_template_option ExpressionTemplatesOptions> constexpr int std::numeric_limits<boost::multiprecision::number<boost::multiprecision::uintwide_t_backend<MyWidth2, MyLimbType, MyAllocatorType>, ExpressionTemplatesOptions>>::min_exponent;    // NOLINT(readability-redundant-declaration)
    template<const ::math::wide_integer::size_t MyWidth2, typename MyLimbType, typename MyAllocatorType, const boost::multiprecision::expression_template_option ExpressionTemplatesOptions> constexpr int std::numeric_limits<boost::multiprecision::number<boost::multiprecision::uintwide_t_backend<MyWidth2, MyLimbType, MyAllocatorType>, ExpressionTemplatesOptions>>::min_exponent10;  // NOLINT(readability-redundant-declaration)

    template<const ::math::wide_integer::size_t MyWidth2, typename MyLimbType, typename MyAllocatorType, const boost::multiprecision::expression_template_option ExpressionTemplatesOptions> constexpr int                     std::numeric_limits<boost::multiprecision::number<boost::multiprecision::uintwide_t_backend<MyWidth2, MyLimbType, MyAllocatorType>, ExpressionTemplatesOptions>>::radix;             // NOLINT(readability-redundant-declaration)
    template<const ::math::wide_integer::size_t MyWidth2, typename MyLimbType, typename MyAllocatorType, const boost::multiprecision::expression_template_option ExpressionTemplatesOptions> constexpr std::float_round_style  std::numeric_limits<boost::multiprecision::number<boost::multiprecision::uintwide_t_backend<MyWidth2, MyLimbType, MyAllocatorType>, ExpressionTemplatesOptions>>::round_style;       // NOLINT(readability-redundant-declaration)
    template<const ::math::wide_integer::size_t MyWidth2, typename MyLimbType, typename MyAllocatorType, const boost::multiprecision::expression_template_option ExpressionTemplatesOptions> constexpr bool                    std::numeric_limits<boost::multiprecision::number<boost::multiprecision::uintwide_t_backend<MyWidth2, MyLimbType, MyAllocatorType>, ExpressionTemplatesOptions>>::has_infinity;      // NOLINT(readability-redundant-declaration)
    template<const ::math::wide_integer::size_t MyWidth2, typename MyLimbType, typename MyAllocatorType, const boost::multiprecision::expression_template_option ExpressionTemplatesOptions> constexpr bool                    std::numeric_limits<boost::multiprecision::number<boost::multiprecision::uintwide_t_backend<MyWidth2, MyLimbType, MyAllocatorType>, ExpressionTemplatesOptions>>::has_quiet_NaN;     // NOLINT(readability-redundant-declaration)
    template<const ::math::wide_integer::size_t MyWidth2, typename MyLimbType, typename MyAllocatorType, const boost::multiprecision::expression_template_option ExpressionTemplatesOptions> constexpr bool                    std::numeric_limits<boost::multiprecision::number<boost::multiprecision::uintwide_t_backend<MyWidth2, MyLimbType, MyAllocatorType>, ExpressionTemplatesOptions>>::has_signaling_NaN; // NOLINT(readability-redundant-declaration)
    template<const ::math::wide_integer::size_t MyWidth2, typename MyLimbType, typename MyAllocatorType, const boost::multiprecision::expression_template_option ExpressionTemplatesOptions> constexpr std::float_denorm_style std::numeric_limits<boost::multiprecision::number<boost::multiprecision::uintwide_t_backend<MyWidth2, MyLimbType, MyAllocatorType>, ExpressionTemplatesOptions>>::has_denorm;        // NOLINT(readability-redundant-declaration)
    template<const ::math::wide_integer::size_t MyWidth2, typename MyLimbType, typename MyAllocatorType, const boost::multiprecision::expression_template_option ExpressionTemplatesOptions> constexpr bool                    std::numeric_limits<boost::multiprecision::number<boost::multiprecision::uintwide_t_backend<MyWidth2, MyLimbType, MyAllocatorType>, ExpressionTemplatesOptions>>::has_denorm_loss;   // NOLINT(readability-redundant-declaration)
    template<const ::math::wide_integer::size_t MyWidth2, typename MyLimbType, typename MyAllocatorType, const boost::multiprecision::expression_template_option ExpressionTemplatesOptions> constexpr bool                    std::numeric_limits<boost::multiprecision::number<boost::multiprecision::uintwide_t_backend<MyWidth2, MyLimbType, MyAllocatorType>, ExpressionTemplatesOptions>>::traps;             // NOLINT(readability-redundant-declaration)
    template<const ::math::wide_integer::size_t MyWidth2, typename MyLimbType, typename MyAllocatorType, const boost::multiprecision::expression_template_option ExpressionTemplatesOptions> constexpr bool                    std::numeric_limits<boost::multiprecision::number<boost::multiprecision::uintwide_t_backend<MyWidth2, MyLimbType, MyAllocatorType>, ExpressionTemplatesOptions>>::tinyness_before;   // NOLINT(readability-redundant-declaration)
    #endif

    #endif // !BOOST_NO_INCLASS_MEMBER_INITIALIZATION

  } // namespace std

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

#endif // UINTWIDE_T_BACKEND_2019_12_15_HPP
