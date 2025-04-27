///////////////////////////////////////////////////////////////////
//  Copyright Christopher Kormanyos 1999 - 2023.                 //
//  Distributed under the Boost Software License,                //
//  Version 1.0. (See accompanying file LICENSE_1_0.txt          //
//  or copy at http://www.boost.org/LICENSE_1_0.txt)             //
///////////////////////////////////////////////////////////////////

#ifndef UINTWIDE_T_2018_10_02_H // NOLINT(llvm-header-guard)
  #define UINTWIDE_T_2018_10_02_H

  #if defined(WIDE_INTEGER_TEST_REPRESENTATION_AS_STD_LIST)
  #if !defined(WIDE_INTEGER_DISABLE_WIDE_INTEGER_CONSTEXPR)
  #define WIDE_INTEGER_DISABLE_WIDE_INTEGER_CONSTEXPR
  #endif
  #endif

  #if ((__cplusplus < 202100L) || (defined(__GNUC__) && defined(__AVR__)))
  #include <ciso646>
  #else
  #include <version>
  #endif

  #include <algorithm>
  #include <array>
  #if (defined(__cpp_lib_to_chars) && (__cpp_lib_to_chars >= 201611L))
  #include <charconv>
  #endif
  #include <cinttypes>
  #if !defined(WIDE_INTEGER_DISABLE_FLOAT_INTEROP)
  #include <cmath>
  #endif
  #include <cstddef>
  #include <cstdint>
  #include <cstdlib>
  #include <cstring>
  #include <initializer_list>
  #if !defined(WIDE_INTEGER_DISABLE_IOSTREAM)
  #include <iomanip>
  #include <istream>
  #endif
  #include <iterator>
  #include <limits>
  #if defined(WIDE_INTEGER_TEST_REPRESENTATION_AS_STD_LIST)
  #include <list>
  #endif
  #if !defined(WIDE_INTEGER_DISABLE_IMPLEMENT_UTIL_DYNAMIC_ARRAY)
  #include <memory>
  #endif
  #if (defined(__cpp_lib_gcd_lcm) && (__cpp_lib_gcd_lcm >= 201606L))
  #include <numeric>
  #endif
  #if !defined(WIDE_INTEGER_DISABLE_IOSTREAM)
  #include <ostream>
  #include <sstream>
  #endif
  #if !defined(WIDE_INTEGER_DISABLE_TO_STRING)
  #include <string>
  #endif
  #include <type_traits>

  #if (defined(__clang__) && (__clang_major__ <= 9))
  #define WIDE_INTEGER_NUM_LIMITS_CLASS_TYPE struct // NOLINT(cppcoreguidelines-macro-usage)
  #else
  #define WIDE_INTEGER_NUM_LIMITS_CLASS_TYPE class  // NOLINT(cppcoreguidelines-macro-usage)
  #endif

  #if (defined(_MSC_VER) && (!defined(__GNUC__) && !defined(__clang__)))
    #if (_MSC_VER >= 1900) && defined(_HAS_CXX20) && (_HAS_CXX20 != 0)
      #define WIDE_INTEGER_CONSTEXPR constexpr               // NOLINT(cppcoreguidelines-macro-usage)
      #define WIDE_INTEGER_CONSTEXPR_IS_COMPILE_TIME_CONST 1 // NOLINT(cppcoreguidelines-macro-usage)
      #define WIDE_INTEGER_NODISCARD [[nodiscard]]           // NOLINT(cppcoreguidelines-macro-usage)
    #else
      #define WIDE_INTEGER_CONSTEXPR
      #define WIDE_INTEGER_CONSTEXPR_IS_COMPILE_TIME_CONST 0 // NOLINT(cppcoreguidelines-macro-usage)
      #define WIDE_INTEGER_NODISCARD
    #endif
  #else
    #if (defined(__cplusplus) && (__cplusplus >= 201402L))
      #if defined(__AVR__) && (!defined(__GNUC__) || (defined(__GNUC__) && (__cplusplus >= 202002L)))
      #define WIDE_INTEGER_CONSTEXPR constexpr               // NOLINT(cppcoreguidelines-macro-usage)
      #define WIDE_INTEGER_CONSTEXPR_IS_COMPILE_TIME_CONST 1 // NOLINT(cppcoreguidelines-macro-usage)
      #define WIDE_INTEGER_NODISCARD [[nodiscard]]           // NOLINT(cppcoreguidelines-macro-usage)
      #elif (defined(__cpp_lib_constexpr_algorithms) && (__cpp_lib_constexpr_algorithms >= 201806))
        #if defined(__clang__)
          #if (__clang_major__ > 9)
          #define WIDE_INTEGER_CONSTEXPR constexpr               // NOLINT(cppcoreguidelines-macro-usage)
          #define WIDE_INTEGER_CONSTEXPR_IS_COMPILE_TIME_CONST 1 // NOLINT(cppcoreguidelines-macro-usage)
          #define WIDE_INTEGER_NODISCARD [[nodiscard]]           // NOLINT(cppcoreguidelines-macro-usage)
          #else
          #define WIDE_INTEGER_CONSTEXPR
          #define WIDE_INTEGER_CONSTEXPR_IS_COMPILE_TIME_CONST 0 // NOLINT(cppcoreguidelines-macro-usage)
          #define WIDE_INTEGER_NODISCARD
          #endif
        #else
        #define WIDE_INTEGER_CONSTEXPR constexpr               // NOLINT(cppcoreguidelines-macro-usage)
        #define WIDE_INTEGER_CONSTEXPR_IS_COMPILE_TIME_CONST 1 // NOLINT(cppcoreguidelines-macro-usage)
        #define WIDE_INTEGER_NODISCARD [[nodiscard]]           // NOLINT(cppcoreguidelines-macro-usage)
        #endif
      #elif (defined(__clang__) && (__clang_major__ >= 10)) && (defined(__cplusplus) && (__cplusplus > 201703L))
        #if defined(__x86_64__)
        #define WIDE_INTEGER_CONSTEXPR constexpr               // NOLINT(cppcoreguidelines-macro-usage)
        #define WIDE_INTEGER_CONSTEXPR_IS_COMPILE_TIME_CONST 1 // NOLINT(cppcoreguidelines-macro-usage)
        #define WIDE_INTEGER_NODISCARD [[nodiscard]]           // NOLINT(cppcoreguidelines-macro-usage)
        #else
        #define WIDE_INTEGER_CONSTEXPR
        #define WIDE_INTEGER_CONSTEXPR_IS_COMPILE_TIME_CONST 0 // NOLINT(cppcoreguidelines-macro-usage)
        #define WIDE_INTEGER_NODISCARD
        #endif
      #else
      #define WIDE_INTEGER_CONSTEXPR
      #define WIDE_INTEGER_CONSTEXPR_IS_COMPILE_TIME_CONST 0 // NOLINT(cppcoreguidelines-macro-usage)
      #define WIDE_INTEGER_NODISCARD
      #endif
    #else
      #define WIDE_INTEGER_CONSTEXPR
      #define WIDE_INTEGER_CONSTEXPR_IS_COMPILE_TIME_CONST 0 // NOLINT(cppcoreguidelines-macro-usage)
      #define WIDE_INTEGER_NODISCARD
    #endif
  #endif

  #if defined(WIDE_INTEGER_DISABLE_WIDE_INTEGER_CONSTEXPR)
  #undef WIDE_INTEGER_CONSTEXPR
  #define WIDE_INTEGER_CONSTEXPR
  #undef WIDE_INTEGER_CONSTEXPR_IS_COMPILE_TIME_CONST
  #define WIDE_INTEGER_CONSTEXPR_IS_COMPILE_TIME_CONST 0
  #if !defined(WIDE_INTEGER_DISABLE_TRIVIAL_COPY_AND_STD_LAYOUT_CHECKS)
  #define WIDE_INTEGER_DISABLE_TRIVIAL_COPY_AND_STD_LAYOUT_CHECKS
  #endif
  #endif

  #if defined(WIDE_INTEGER_NAMESPACE_BEGIN) || defined(WIDE_INTEGER_NAMESPACE_END)
    #error internal pre-processor macro already defined
  #endif

  #if defined(WIDE_INTEGER_NAMESPACE)
    #define WIDE_INTEGER_NAMESPACE_BEGIN namespace WIDE_INTEGER_NAMESPACE {   // NOLINT(cppcoreguidelines-macro-usage)
    #define WIDE_INTEGER_NAMESPACE_END } // namespace WIDE_INTEGER_NAMESPACE  // NOLINT(cppcoreguidelines-macro-usage)
  #else
    #define WIDE_INTEGER_NAMESPACE_BEGIN
    #define WIDE_INTEGER_NAMESPACE_END
  #endif

  // Forward declaration needed for class-friendship with the uintwide_t template class.
  namespace test_uintwide_t_edge { auto test_various_isolated_edge_cases() -> bool; } // namespace test_uintwide_t_edge

  WIDE_INTEGER_NAMESPACE_BEGIN

  #if(__cplusplus >= 201703L)
  namespace math::wide_integer::detail {
  #else
  namespace math { namespace wide_integer { namespace detail { // NOLINT(modernize-concat-nested-namespaces)
  #endif

  // Use a local, constexpr, unsafe implementation of the abs-function.
  template<typename ArithmeticType>
  WIDE_INTEGER_CONSTEXPR auto abs_unsafe(ArithmeticType val) -> ArithmeticType
  {
    return ((val > static_cast<int>(INT8_C(0))) ? val : -val);
  }

  // Use a local, constexpr, unsafe implementation of the fill-function.
  template<typename DestinationIterator,
           typename ValueType>
  WIDE_INTEGER_CONSTEXPR auto fill_unsafe(DestinationIterator first, DestinationIterator last, ValueType val) -> void
  {
    while(first != last)
    {
      using local_destination_value_type = typename std::iterator_traits<DestinationIterator>::value_type;

      *first++ = static_cast<local_destination_value_type>(val);
    }
  }

  // Use a local, constexpr, unsafe implementation of the copy-function.
  template<typename InputIterator,
           typename DestinationIterator>
  WIDE_INTEGER_CONSTEXPR auto copy_unsafe(InputIterator first, InputIterator last, DestinationIterator dest) -> DestinationIterator
  {
    while(first != last)
    {
      using local_destination_value_type = typename std::iterator_traits<DestinationIterator>::value_type;

      #if (defined(__GNUC__) && (__GNUC__ > 9))
      #pragma GCC diagnostic ignored "-Wstringop-overflow"
      #endif
      *dest++ = static_cast<local_destination_value_type>(*first++);
      #if (defined(__GNUC__) && (__GNUC__ > 9))
      #pragma GCC diagnostic pop
      #endif
    }

    return dest;
  }

  template <class UnsignedIntegralType>
  WIDE_INTEGER_CONSTEXPR auto clz_unsafe(UnsignedIntegralType v) noexcept -> std::enable_if_t<(   (std::is_integral<UnsignedIntegralType>::value)
                                                                                               && (std::is_unsigned<UnsignedIntegralType>::value)), unsigned>
  {
    using local_unsigned_integral_type = UnsignedIntegralType;

    auto yy_val = static_cast<local_unsigned_integral_type>(UINT8_C(0));

    auto nn_val = static_cast<unsigned>(std::numeric_limits<local_unsigned_integral_type>::digits);

    auto cc_val = // NOLINT(altera-id-dependent-backward-branch)
      static_cast<unsigned>
      (
        std::numeric_limits<local_unsigned_integral_type>::digits / static_cast<int>(INT8_C(2))
      );

    do
    {
      yy_val = static_cast<local_unsigned_integral_type>(v >> cc_val);

      if(yy_val != static_cast<local_unsigned_integral_type>(UINT8_C(0)))
      {
        nn_val -= cc_val;

        v = yy_val;
      }

      cc_val >>= static_cast<unsigned>(UINT8_C(1));
    }
    while(cc_val != static_cast<unsigned>(UINT8_C(0))); // NOLINT(altera-id-dependent-backward-branch)

    return
      static_cast<unsigned>
      (
        static_cast<unsigned>(nn_val) - static_cast<unsigned>(v)
      );
  }

  template<typename UnsignedIntegralType>
  WIDE_INTEGER_CONSTEXPR auto ctz_unsafe(const UnsignedIntegralType v) noexcept -> std::enable_if_t<(   (std::is_integral<UnsignedIntegralType>::value)
                                                                                                     && (std::is_unsigned<UnsignedIntegralType>::value)), unsigned>
  {
    using local_unsigned_integral_type = UnsignedIntegralType;

    constexpr auto local_digits = static_cast<unsigned>(std::numeric_limits<local_unsigned_integral_type>::digits);

    const auto clz_mask =
      static_cast<local_unsigned_integral_type>
      (
          static_cast<local_unsigned_integral_type>(~v)
        & static_cast<local_unsigned_integral_type>(v - static_cast<local_unsigned_integral_type>(UINT8_C(1)))
      );

    return static_cast<unsigned>(local_digits - clz_unsafe(clz_mask));
  }

  template<typename UnsignedIntegralType>
  WIDE_INTEGER_CONSTEXPR auto gcd_unsafe(UnsignedIntegralType u, UnsignedIntegralType v) -> std::enable_if_t<(   (std::is_integral<UnsignedIntegralType>::value) // NOLINT(altera-id-dependent-backward-branch)
                                                                                                              && (std::is_unsigned<UnsignedIntegralType>::value)), UnsignedIntegralType>
  {
    using local_unsigned_integral_type = UnsignedIntegralType;

    // Handle cases having (u != 0) and (v != 0).
    if(u == static_cast<local_unsigned_integral_type>(UINT8_C(0))) { return v; }

    if(v == static_cast<local_unsigned_integral_type>(UINT8_C(0))) { return u; }

    // Shift the greatest power of 2 dividing both u and v.
    const auto trz = static_cast<unsigned>(ctz_unsafe(u));

    const auto shift_amount = (std::min)(trz, ctz_unsafe(v));

    v >>= shift_amount;
    u >>= trz;

    do
    {
      // Reduce the GCD.

      v >>= ctz_unsafe(v);

      if(u > v)
      {
        std::swap(u, v);
      }

      v -= u;
    }
    while(v != static_cast<local_unsigned_integral_type>(UINT8_C(0))); // NOLINT(altera-id-dependent-backward-branch)

    return static_cast<local_unsigned_integral_type>(u << shift_amount);
  }

  #if(__cplusplus >= 201703L)
  } // namespace math::wide_integer::detail
  #else
  } // namespace detail
  } // namespace wide_integer
  } // namespace math
  #endif

  WIDE_INTEGER_NAMESPACE_END

  #if !defined(WIDE_INTEGER_DISABLE_IMPLEMENT_UTIL_DYNAMIC_ARRAY)

  WIDE_INTEGER_NAMESPACE_BEGIN

  namespace util {

  template<typename ValueType,
           typename AllocatorType = std::allocator<ValueType>,
           typename SizeType      = std::size_t,
           typename DiffType      = std::ptrdiff_t>
  class dynamic_array;

  template<typename ValueType,
           typename AllocatorType,
           typename SizeType,
           typename DiffType>
  class dynamic_array
  {
  public:
    // Type definitions.
    using allocator_type         = typename std::allocator_traits<AllocatorType>::template rebind_alloc<ValueType>;
    using value_type             = typename allocator_type::value_type;
    using reference              =       value_type&;
    using const_reference        = const value_type&;
    using iterator               =       value_type*;
    using const_iterator         = const value_type*;
    using pointer                =       value_type*;
    using const_pointer          = const value_type*;
    using size_type              =       SizeType;
    using difference_type        =       DiffType;
    using reverse_iterator       =       std::reverse_iterator<      value_type*>;
    using const_reverse_iterator =       std::reverse_iterator<const value_type*>;

    // Constructors.
    constexpr dynamic_array() : elem_count(static_cast<size_type>(UINT8_C(0))) { }

    explicit WIDE_INTEGER_CONSTEXPR dynamic_array(      size_type       count_in,
                                                        const_reference value_in = value_type(),
                                                  const allocator_type& alloc_in = allocator_type())
      : elem_count(count_in)
    {
      if(elem_count > static_cast<size_type>(UINT8_C(0)))
      {
        allocator_type my_alloc(alloc_in);

        elems = std::allocator_traits<allocator_type>::allocate(my_alloc, elem_count);

        iterator it = begin();

        while(it != end())
        {
          std::allocator_traits<allocator_type>::construct(my_alloc, it, value_in);

          ++it;
        }
      }
    }

    WIDE_INTEGER_CONSTEXPR dynamic_array(const dynamic_array& other)
      : elem_count(other.size())
    {
      allocator_type my_alloc;

      if(elem_count > static_cast<size_type>(UINT8_C(0)))
      {
        elems = std::allocator_traits<allocator_type>::allocate(my_alloc, elem_count);
      }

      math::wide_integer::detail::copy_unsafe(other.elems, other.elems + elem_count, elems);
    }

    template<typename input_iterator>
    WIDE_INTEGER_CONSTEXPR dynamic_array(input_iterator first,
                                         input_iterator last,
                                         const allocator_type& alloc_in = allocator_type())
      : elem_count(static_cast<size_type>(last - first))
    {
      allocator_type my_alloc(alloc_in);

      if(elem_count > static_cast<size_type>(UINT8_C(0)))
      {
        elems = std::allocator_traits<allocator_type>::allocate(my_alloc, elem_count);
      }

      math::wide_integer::detail::copy_unsafe(first, last, elems);
    }

    WIDE_INTEGER_CONSTEXPR dynamic_array(std::initializer_list<value_type> lst,
                                         const allocator_type& alloc_in = allocator_type())
      : elem_count(lst.size())
    {
      allocator_type my_alloc(alloc_in);

      if(elem_count > static_cast<size_type>(UINT8_C(0)))
      {
        elems = std::allocator_traits<allocator_type>::allocate(my_alloc, elem_count);
      }

      math::wide_integer::detail::copy_unsafe(lst.begin(), lst.end(), elems);
    }

    // Move constructor.
    WIDE_INTEGER_CONSTEXPR dynamic_array(dynamic_array&& other) noexcept : elem_count(other.elem_count),
                                                                           elems     (other.elems)
    {
      other.elem_count = static_cast<size_type>(UINT8_C(0));
      other.elems      = nullptr;
    }

    // Destructor.
    WIDE_INTEGER_CONSTEXPR virtual ~dynamic_array()
    {
      if(!empty())
      {
        using local_allocator_traits_type = std::allocator_traits<allocator_type>;

        allocator_type my_alloc;

        auto p = begin(); // NOLINT(llvm-qualified-auto,readability-qualified-auto)

        while(p != end())
        {
          local_allocator_traits_type::destroy(my_alloc, p);

          ++p;
        }

        // Destroy the elements and deallocate the range.
        local_allocator_traits_type::deallocate(my_alloc, elems, elem_count);
      }
    }

    // Assignment operator.
    WIDE_INTEGER_CONSTEXPR auto operator=(const dynamic_array& other) -> dynamic_array&
    {
      if(this != &other)
      {
        math::wide_integer::detail::copy_unsafe(other.elems,
                                                other.elems + (std::min)(elem_count, other.elem_count),
                                                elems);
      }

      return *this;
    }

    // Move assignment operator.
    WIDE_INTEGER_CONSTEXPR auto operator=(dynamic_array&& other) noexcept -> dynamic_array&
    {
      std::swap(elem_count, other.elem_count);
      std::swap(elems,      other.elems);

      return *this;
    }

    // Iterator members:
    WIDE_INTEGER_CONSTEXPR auto begin  ()       -> iterator               { return elems; }
    WIDE_INTEGER_CONSTEXPR auto end    ()       -> iterator               { return elems + elem_count; }
    WIDE_INTEGER_CONSTEXPR auto begin  () const -> const_iterator         { return elems; }
    WIDE_INTEGER_CONSTEXPR auto end    () const -> const_iterator         { return elems + elem_count; }
    WIDE_INTEGER_CONSTEXPR auto cbegin () const -> const_iterator         { return elems; }
    WIDE_INTEGER_CONSTEXPR auto cend   () const -> const_iterator         { return elems + elem_count; }
    WIDE_INTEGER_CONSTEXPR auto rbegin ()       -> reverse_iterator       { return reverse_iterator(elems + elem_count); }
    WIDE_INTEGER_CONSTEXPR auto rend   ()       -> reverse_iterator       { return reverse_iterator(elems); }
    WIDE_INTEGER_CONSTEXPR auto rbegin () const -> const_reverse_iterator { return const_reverse_iterator(elems + elem_count); }
    WIDE_INTEGER_CONSTEXPR auto rend   () const -> const_reverse_iterator { return const_reverse_iterator(elems); }
    WIDE_INTEGER_CONSTEXPR auto crbegin() const -> const_reverse_iterator { return const_reverse_iterator(elems + elem_count); }
    WIDE_INTEGER_CONSTEXPR auto crend  () const -> const_reverse_iterator { return const_reverse_iterator(elems); }

    // Raw pointer access.
    WIDE_INTEGER_CONSTEXPR auto data()       -> pointer       { return elems; }
    WIDE_INTEGER_CONSTEXPR auto data() const -> const_pointer { return elems; }

    // Size and capacity.
    constexpr auto size    () const -> size_type { return  elem_count; }
    constexpr auto max_size() const -> size_type { return  elem_count; }
    constexpr auto empty   () const -> bool      { return (elem_count == static_cast<size_type>(UINT8_C(0))); }

    // Element access members.
    WIDE_INTEGER_CONSTEXPR auto operator[](const size_type i)       -> reference       { return elems[i]; }
    WIDE_INTEGER_CONSTEXPR auto operator[](const size_type i) const -> const_reference { return elems[i]; }

    WIDE_INTEGER_CONSTEXPR auto front()       -> reference       { return elems[static_cast<size_type>(UINT8_C(0))]; }
    WIDE_INTEGER_CONSTEXPR auto front() const -> const_reference { return elems[static_cast<size_type>(UINT8_C(0))]; }

    WIDE_INTEGER_CONSTEXPR auto back()       -> reference       { return ((elem_count > static_cast<size_type>(UINT8_C(0))) ? elems[static_cast<size_type>(elem_count - static_cast<size_type>(UINT8_C(1)))] : elems[static_cast<size_type>(UINT8_C(0))]); }
    WIDE_INTEGER_CONSTEXPR auto back() const -> const_reference { return ((elem_count > static_cast<size_type>(UINT8_C(0))) ? elems[static_cast<size_type>(elem_count - static_cast<size_type>(UINT8_C(1)))] : elems[static_cast<size_type>(UINT8_C(0))]); }

    WIDE_INTEGER_CONSTEXPR auto at(const size_type i)       -> reference       { return ((i < elem_count) ? elems[i] : elems[static_cast<size_type>(UINT8_C(0))]); }
    WIDE_INTEGER_CONSTEXPR auto at(const size_type i) const -> const_reference { return ((i < elem_count) ? elems[i] : elems[static_cast<size_type>(UINT8_C(0))]); }

    // Element manipulation members.
    WIDE_INTEGER_CONSTEXPR auto fill(const value_type& value_in) -> void
    {
      math::wide_integer::detail::fill_unsafe(begin(), begin() + elem_count, value_in);
    }

    WIDE_INTEGER_CONSTEXPR auto swap(dynamic_array& other) noexcept -> void
    {
      if(this != &other)
      {
        std::swap(elems,      other.elems);
        std::swap(elem_count, other.elem_count);
      }
    }

    WIDE_INTEGER_CONSTEXPR auto swap(dynamic_array&& other) noexcept -> void
    {
      const auto tmp = std::move(*this);

      *this = std::move(other);
      other = std::move(tmp);
    }

  private:
    mutable size_type elem_count;        // NOLINT(readability-identifier-naming)
    pointer           elems { nullptr }; // NOLINT(readability-identifier-naming,altera-id-dependent-backward-branch)
  };

  template<typename ValueType, typename AllocatorType>
  constexpr auto operator==(const dynamic_array<ValueType, AllocatorType>& lhs,
                            const dynamic_array<ValueType, AllocatorType>& rhs) -> bool
  {
    using local_size_type = typename dynamic_array<ValueType, AllocatorType>::size_type;

    return
    (
         (lhs.size() == rhs.size())
      && (
              (lhs.size() == static_cast<local_size_type>(UINT8_C(0)))
           || std::equal(lhs.cbegin(), lhs.cend(), rhs.cbegin())
         )
    );
  }

  template<typename ValueType, typename AllocatorType>
  WIDE_INTEGER_CONSTEXPR auto operator<(const dynamic_array<ValueType, AllocatorType>& lhs,
                                        const dynamic_array<ValueType, AllocatorType>& rhs) -> bool
  {
    using size_type = typename dynamic_array<ValueType, AllocatorType>::size_type;

    const auto size_of_left_is_zero = (lhs.size() == static_cast<size_type>(UINT8_C(0)));

    bool b_result { };

    if(size_of_left_is_zero)
    {
      const auto size_of_right_is_zero = (rhs.size() == static_cast<size_type>(UINT8_C(0)));

      b_result = (!size_of_right_is_zero);
    }
    else
    {
      if(size_of_left_is_zero)
      {
        const auto size_of_right_is_zero = (rhs.size() == static_cast<size_type>(UINT8_C(0)));

        b_result = (!size_of_right_is_zero);
      }
      else
      {
        const size_type my_count = (std::min)(lhs.size(), rhs.size());

        b_result= std::lexicographical_compare(lhs.cbegin(),
                                               lhs.cbegin() + my_count,
                                               rhs.cbegin(),
                                               rhs.cbegin() + my_count);
      }
    }

    return b_result;
  }

  template<typename ValueType, typename AllocatorType>
  WIDE_INTEGER_CONSTEXPR auto operator!=(const dynamic_array<ValueType, AllocatorType>& lhs,
                                         const dynamic_array<ValueType, AllocatorType>& rhs) -> bool
  {
    return (!(lhs == rhs));
  }

  template<typename ValueType, typename AllocatorType>
  WIDE_INTEGER_CONSTEXPR auto operator>(const dynamic_array<ValueType, AllocatorType>& lhs,
                                        const dynamic_array<ValueType, AllocatorType>& rhs) -> bool
  {
    return (rhs < lhs);
  }

  template<typename ValueType, typename AllocatorType>
  WIDE_INTEGER_CONSTEXPR auto operator>=(const dynamic_array<ValueType, AllocatorType>& lhs,
                                         const dynamic_array<ValueType, AllocatorType>& rhs) -> bool
  {
    return (!(lhs < rhs));
  }

  template<typename ValueType, typename AllocatorType>
  WIDE_INTEGER_CONSTEXPR auto operator<=(const dynamic_array<ValueType, AllocatorType>& lhs,
                                         const dynamic_array<ValueType, AllocatorType>& rhs) -> bool
  {
    return (!(rhs < lhs));
  }

  template<typename ValueType, typename AllocatorType>
  WIDE_INTEGER_CONSTEXPR auto swap(dynamic_array<ValueType, AllocatorType>& x,
                                   dynamic_array<ValueType, AllocatorType>& y) noexcept -> void
  {
    x.swap(y);
  }

  } // namespace util

  WIDE_INTEGER_NAMESPACE_END

  WIDE_INTEGER_NAMESPACE_BEGIN

  #if(__cplusplus >= 201703L)
  namespace math::wide_integer::detail {
  #else
  namespace math { namespace wide_integer { namespace detail { // NOLINT(modernize-concat-nested-namespaces)
  #endif

  using util::dynamic_array;

  #if(__cplusplus >= 201703L)
  } // namespace math::wide_integer::detail
  #else
  } // namespace detail
  } // namespace wide_integer
  } // namespace math
  #endif

  WIDE_INTEGER_NAMESPACE_END

  #else

  #include <util/utility/util_dynamic_array.h>

  WIDE_INTEGER_NAMESPACE_BEGIN

  #if(__cplusplus >= 201703L)
  namespace math::wide_integer::detail {
  #else
  namespace math { namespace wide_integer { namespace detail { // NOLINT(modernize-concat-nested-namespaces)
  #endif

  using util::dynamic_array;

  #if(__cplusplus >= 201703L)
  } // namespace math::wide_integer::detail
  #else
  } // namespace detail
  } // namespace wide_integer
  } // namespace math
  #endif

  WIDE_INTEGER_NAMESPACE_END

  #endif

  WIDE_INTEGER_NAMESPACE_BEGIN

  #if(__cplusplus >= 201703L)
  namespace math::wide_integer {
  #else
  namespace math { namespace wide_integer { // NOLINT(modernize-concat-nested-namespaces)
  #endif

  namespace detail {

  using size_t    = std::uint32_t;
  using ptrdiff_t = std::int32_t;

  static_assert((  (std::numeric_limits<size_t>::digits        >= std::numeric_limits<std::uint16_t>::digits)
                && (std::numeric_limits<ptrdiff_t>::digits + 1 >= std::numeric_limits<std::uint16_t>::digits)),
                "Error: size type and pointer difference type must be at least 16 bits in width (or wider)");

  template<const size_t Width2> struct verify_power_of_two // NOLINT(altera-struct-pack-align)
  {
    // TBD: Which powers should be checked if size_t is not 32 bits?
    static constexpr auto conditional_value =
         (Width2 == static_cast<size_t>(1ULL <<  0U)) || (Width2 == static_cast<size_t>(1ULL <<  1U)) || (Width2 == static_cast<size_t>(1ULL <<  2U)) || (Width2 == static_cast<size_t>(1ULL <<  3U))
      || (Width2 == static_cast<size_t>(1ULL <<  4U)) || (Width2 == static_cast<size_t>(1ULL <<  5U)) || (Width2 == static_cast<size_t>(1ULL <<  6U)) || (Width2 == static_cast<size_t>(1ULL <<  7U))
      || (Width2 == static_cast<size_t>(1ULL <<  8U)) || (Width2 == static_cast<size_t>(1ULL <<  9U)) || (Width2 == static_cast<size_t>(1ULL << 10U)) || (Width2 == static_cast<size_t>(1ULL << 11U))
      || (Width2 == static_cast<size_t>(1ULL << 12U)) || (Width2 == static_cast<size_t>(1ULL << 13U)) || (Width2 == static_cast<size_t>(1ULL << 14U)) || (Width2 == static_cast<size_t>(1ULL << 15U))
      || (Width2 == static_cast<size_t>(1ULL << 16U)) || (Width2 == static_cast<size_t>(1ULL << 17U)) || (Width2 == static_cast<size_t>(1ULL << 18U)) || (Width2 == static_cast<size_t>(1ULL << 19U))
      || (Width2 == static_cast<size_t>(1ULL << 20U)) || (Width2 == static_cast<size_t>(1ULL << 21U)) || (Width2 == static_cast<size_t>(1ULL << 22U)) || (Width2 == static_cast<size_t>(1ULL << 23U))
      || (Width2 == static_cast<size_t>(1ULL << 24U)) || (Width2 == static_cast<size_t>(1ULL << 25U)) || (Width2 == static_cast<size_t>(1ULL << 26U)) || (Width2 == static_cast<size_t>(1ULL << 27U))
      || (Width2 == static_cast<size_t>(1ULL << 28U)) || (Width2 == static_cast<size_t>(1ULL << 29U)) || (Width2 == static_cast<size_t>(1ULL << 30U)) || (Width2 == static_cast<size_t>(1ULL << 31U))
      ;
  };

  template<const size_t BitCount,
           typename EnableType = void>
  struct uint_type_helper
  {
  private:
    static constexpr auto bit_count   () -> size_t { return BitCount; }
    static constexpr auto bit_count_lo() -> size_t { return static_cast<size_t>(UINT8_C(8)); }
    #if defined(WIDE_INTEGER_HAS_LIMB_TYPE_UINT64)
    static constexpr auto bit_count_hi() -> size_t { return static_cast<size_t>(UINT8_C(128)); }
    #else
    static constexpr auto bit_count_hi() -> size_t { return static_cast<size_t>(UINT8_C(64)); }
    #endif

    static_assert((   ((bit_count() >= bit_count_lo()) && (BitCount <= bit_count_hi())) // NOLINT(cppcoreguidelines-avoid-magic-numbers,readability-magic-numbers)
                   && (verify_power_of_two<bit_count()>::conditional_value)),
                  "Error: uint_type_helper is not intended to be used for this BitCount");

  public:
    using exact_unsigned_type = std::uintmax_t;
    using exact_signed_type   = std::intmax_t;
    using fast_unsigned_type  = std::uintmax_t;
    using fast_signed_type    = std::intmax_t;
  };

  template<const size_t BitCount> struct uint_type_helper<BitCount, std::enable_if_t<                                                  (BitCount <= static_cast<size_t>(UINT8_C(  8)))>> { using exact_unsigned_type = std::uint8_t;      using exact_signed_type = std::int8_t;     using fast_unsigned_type = std::uint_fast8_t;  using fast_signed_type = std::int_fast8_t;  };
  template<const size_t BitCount> struct uint_type_helper<BitCount, std::enable_if_t<(BitCount >= static_cast<size_t>(UINT8_C( 9))) && (BitCount <= static_cast<size_t>(UINT8_C( 16)))>> { using exact_unsigned_type = std::uint16_t;     using exact_signed_type = std::int16_t;    using fast_unsigned_type = std::uint_fast16_t; using fast_signed_type = std::int_fast16_t; };
  template<const size_t BitCount> struct uint_type_helper<BitCount, std::enable_if_t<(BitCount >= static_cast<size_t>(UINT8_C(17))) && (BitCount <= static_cast<size_t>(UINT8_C( 32)))>> { using exact_unsigned_type = std::uint32_t;     using exact_signed_type = std::int32_t;    using fast_unsigned_type = std::uint_fast32_t; using fast_signed_type = std::int_fast32_t; };
  template<const size_t BitCount> struct uint_type_helper<BitCount, std::enable_if_t<(BitCount >= static_cast<size_t>(UINT8_C(33))) && (BitCount <= static_cast<size_t>(UINT8_C( 64)))>> { using exact_unsigned_type = std::uint64_t;     using exact_signed_type = std::int64_t;    using fast_unsigned_type = std::uint_fast64_t; using fast_signed_type = std::int_fast64_t; };
  #if defined(WIDE_INTEGER_HAS_LIMB_TYPE_UINT64)
  #if (defined(__GNUC__) && !defined(__clang__))
  #pragma GCC diagnostic push
  #pragma GCC diagnostic ignored "-Wpedantic"
  #endif
  template<const size_t BitCount> struct uint_type_helper<BitCount, std::enable_if_t<(BitCount >= static_cast<size_t>(UINT8_C(65))) && (BitCount <= static_cast<size_t>(UINT8_C(128)))>> { using exact_unsigned_type = unsigned __int128; using exact_signed_type = signed __int128; using fast_unsigned_type = unsigned __int128;  using fast_signed_type = signed __int128;   };
  #if (defined(__GNUC__) && !defined(__clang__))
  #pragma GCC diagnostic pop
  #endif
  #endif

  using unsigned_fast_type = typename uint_type_helper<static_cast<size_t>(std::numeric_limits<size_t   >::digits + 0)>::fast_unsigned_type;
  using   signed_fast_type = typename uint_type_helper<static_cast<size_t>(std::numeric_limits<ptrdiff_t>::digits + 1)>::fast_signed_type;

  #if !defined(WIDE_INTEGER_DISABLE_FLOAT_INTEROP)
  namespace my_own {

  template<typename FloatingPointType> WIDE_INTEGER_CONSTEXPR auto frexp     (FloatingPointType x, int* expptr) -> std::enable_if_t<(std::is_floating_point<FloatingPointType>::value &&   std::numeric_limits<FloatingPointType>::is_iec559 ), FloatingPointType>;
  template<typename FloatingPointType> WIDE_INTEGER_CONSTEXPR auto frexp     (FloatingPointType x, int* expptr) -> std::enable_if_t<(std::is_floating_point<FloatingPointType>::value && (!std::numeric_limits<FloatingPointType>::is_iec559)), FloatingPointType>;
  template<typename FloatingPointType> WIDE_INTEGER_CONSTEXPR auto (isfinite)(FloatingPointType x)              -> std::enable_if_t<(std::is_floating_point<FloatingPointType>::value &&   std::numeric_limits<FloatingPointType>::is_iec559 ), bool>;
  template<typename FloatingPointType> WIDE_INTEGER_CONSTEXPR auto (isfinite)(FloatingPointType x)              -> std::enable_if_t<(std::is_floating_point<FloatingPointType>::value && (!std::numeric_limits<FloatingPointType>::is_iec559)), bool>;

  } // namespace my_own
  #endif

  template<typename ForwardIterator,
           typename OutputIterator>
  WIDE_INTEGER_CONSTEXPR
  auto import_export_helper(      ForwardIterator    in,
                                  OutputIterator     out,
                            const   signed_fast_type total_bits_to_use, // NOLINT(bugprone-easily-swappable-parameters)
                            const unsigned_fast_type chunk_size_in,
                            const unsigned_fast_type chunk_size_out) -> OutputIterator
  {
    const auto size_to_loop_through =
      (std::max)
      (
        static_cast<signed_fast_type>(total_bits_to_use - static_cast<signed_fast_type>(INT8_C(1))),
        static_cast<signed_fast_type>(INT8_C(-1))
      );

    if(size_to_loop_through > static_cast<signed_fast_type>(INT8_C(-1)))
    {
      using local_output_value_type = typename std::iterator_traits<OutputIterator>::value_type;

      *out = static_cast<local_output_value_type>(UINT8_C(0));

      for(auto   i  = size_to_loop_through;
                 i >= static_cast<signed_fast_type>(INT8_C(0)); // NOLINT(altera-id-dependent-backward-branch)
               --i)
      {
        const auto input_bpos =
          static_cast<unsigned_fast_type>
          (
            static_cast<unsigned_fast_type>(i) % chunk_size_in
          );

        using local_input_value_type  = typename std::iterator_traits<ForwardIterator>::value_type;

        const auto input_bval_is_set =
        (
          static_cast<local_input_value_type>
          (
              *in
            & static_cast<local_input_value_type>(static_cast<local_input_value_type>(UINT8_C(1)) << input_bpos)
          )
          != static_cast<local_input_value_type>(UINT8_C(0))
        );

        const auto result_bpos =
          static_cast<local_output_value_type>
          (
            static_cast<unsigned_fast_type>(i) % chunk_size_out
          );

        if(input_bval_is_set)
        {
          *out =
            static_cast<local_output_value_type>
            (
                *out
              | static_cast<local_output_value_type>
                (
                  static_cast<local_output_value_type>(UINT8_C(1)) << result_bpos
                )
            );
        }

        const auto go_to_next_result_elem = (result_bpos == static_cast<local_output_value_type>(UINT8_C(0)));

        if(go_to_next_result_elem && (i != static_cast<signed_fast_type>(INT8_C(0))))
        {
          *(++out) = static_cast<local_output_value_type>(UINT8_C(0));
        }

        const auto go_to_next_input_elem = (input_bpos == static_cast<unsigned_fast_type>(UINT8_C(0)));

        if(go_to_next_input_elem && (i != static_cast<signed_fast_type>(INT8_C(0))))
        {
          ++in;
        }
      }
    }

    return out;
  }

  } // namespace detail

  using detail::size_t;
  using detail::ptrdiff_t;
  using detail::unsigned_fast_type;
  using detail::signed_fast_type;

  // Forward declaration of the uintwide_t template class.
  template<const size_t Width2,
           typename LimbType = std::uint32_t,
           typename AllocatorType = void,
           const bool IsSigned = false>
  class uintwide_t;

  // Forward declarations of non-member binary add, sub, mul, div, mod of (uintwide_t op uintwide_t).
  template<const size_t Width2, typename LimbType, typename AllocatorType, const bool IsSigned> constexpr auto operator+(const uintwide_t<Width2, LimbType, AllocatorType, IsSigned>& u, const uintwide_t<Width2, LimbType, AllocatorType, IsSigned>& v) -> uintwide_t<Width2, LimbType, AllocatorType, IsSigned>;
  template<const size_t Width2, typename LimbType, typename AllocatorType, const bool IsSigned> constexpr auto operator-(const uintwide_t<Width2, LimbType, AllocatorType, IsSigned>& u, const uintwide_t<Width2, LimbType, AllocatorType, IsSigned>& v) -> uintwide_t<Width2, LimbType, AllocatorType, IsSigned>;
  template<const size_t Width2, typename LimbType, typename AllocatorType, const bool IsSigned> constexpr auto operator*(const uintwide_t<Width2, LimbType, AllocatorType, IsSigned>& u, const uintwide_t<Width2, LimbType, AllocatorType, IsSigned>& v) -> uintwide_t<Width2, LimbType, AllocatorType, IsSigned>;
  template<const size_t Width2, typename LimbType, typename AllocatorType, const bool IsSigned> constexpr auto operator/(const uintwide_t<Width2, LimbType, AllocatorType, IsSigned>& u, const uintwide_t<Width2, LimbType, AllocatorType, IsSigned>& v) -> uintwide_t<Width2, LimbType, AllocatorType, IsSigned>;
  template<const size_t Width2, typename LimbType, typename AllocatorType, const bool IsSigned> constexpr auto operator%(const uintwide_t<Width2, LimbType, AllocatorType, IsSigned>& u, const uintwide_t<Width2, LimbType, AllocatorType, IsSigned>& v) -> uintwide_t<Width2, LimbType, AllocatorType, IsSigned>;

  // Forward declarations of non-member binary logic operations of (uintwide_t op uintwide_t).
  template<const size_t Width2, typename LimbType, typename AllocatorType, const bool IsSigned> constexpr auto operator| (const uintwide_t<Width2, LimbType, AllocatorType, IsSigned>& u, const uintwide_t<Width2, LimbType, AllocatorType, IsSigned>& v) -> uintwide_t<Width2, LimbType, AllocatorType, IsSigned>;
  template<const size_t Width2, typename LimbType, typename AllocatorType, const bool IsSigned> constexpr auto operator^ (const uintwide_t<Width2, LimbType, AllocatorType, IsSigned>& u, const uintwide_t<Width2, LimbType, AllocatorType, IsSigned>& v) -> uintwide_t<Width2, LimbType, AllocatorType, IsSigned>;
  template<const size_t Width2, typename LimbType, typename AllocatorType, const bool IsSigned> constexpr auto operator& (const uintwide_t<Width2, LimbType, AllocatorType, IsSigned>& u, const uintwide_t<Width2, LimbType, AllocatorType, IsSigned>& v) -> uintwide_t<Width2, LimbType, AllocatorType, IsSigned>;

  // Forward declarations of non-member binary add, sub, mul, div, mod of (uintwide_t op IntegralType).
  template<typename IntegralType, const size_t Width2, typename LimbType, typename AllocatorType, const bool IsSigned> constexpr auto operator+(const uintwide_t<Width2, LimbType, AllocatorType, IsSigned>& u, const IntegralType& v) -> std::enable_if_t<std::is_integral<IntegralType>::value, uintwide_t<Width2, LimbType, AllocatorType, IsSigned>>;
  template<typename IntegralType, const size_t Width2, typename LimbType, typename AllocatorType, const bool IsSigned> constexpr auto operator-(const uintwide_t<Width2, LimbType, AllocatorType, IsSigned>& u, const IntegralType& v) -> std::enable_if_t<std::is_integral<IntegralType>::value, uintwide_t<Width2, LimbType, AllocatorType, IsSigned>>;
  template<typename IntegralType, const size_t Width2, typename LimbType, typename AllocatorType, const bool IsSigned> constexpr auto operator*(const uintwide_t<Width2, LimbType, AllocatorType, IsSigned>& u, const IntegralType& v) -> std::enable_if_t<std::is_integral<IntegralType>::value, uintwide_t<Width2, LimbType, AllocatorType, IsSigned>>;
  template<typename IntegralType, const size_t Width2, typename LimbType, typename AllocatorType, const bool IsSigned> constexpr auto operator/(const uintwide_t<Width2, LimbType, AllocatorType, IsSigned>& u, const IntegralType& v) -> std::enable_if_t<std::is_integral<IntegralType>::value, uintwide_t<Width2, LimbType, AllocatorType, IsSigned>>;

  template<typename IntegralType, const size_t Width2, typename LimbType, typename AllocatorType, const bool IsSigned>
  constexpr auto operator%(const uintwide_t<Width2, LimbType, AllocatorType, IsSigned>& u, const IntegralType& v) -> std::enable_if_t<(   std::is_integral<IntegralType>::value
                                                                                                                                       && std::is_signed<IntegralType>::value),
                                                                                                                                          uintwide_t<Width2, LimbType, AllocatorType, IsSigned>>;

  template<typename IntegralType, const size_t Width2, typename LimbType, typename AllocatorType, const bool IsSigned>
  WIDE_INTEGER_CONSTEXPR auto operator%(const uintwide_t<Width2, LimbType, AllocatorType, IsSigned>& u, const IntegralType& v) -> std::enable_if_t<(    std::is_integral   <IntegralType>::value
                                                                                                                                                    &&  std::is_unsigned   <IntegralType>::value
                                                                                                                                                    && (std::numeric_limits<IntegralType>::digits <= std::numeric_limits<LimbType>::digits)),
                                                                                                                                                    typename uintwide_t<Width2, LimbType, AllocatorType, IsSigned>::limb_type>;

  template<typename IntegralType, const size_t Width2, typename LimbType, typename AllocatorType, const bool IsSigned>
  constexpr auto operator%(const uintwide_t<Width2, LimbType, AllocatorType, IsSigned>& u, const IntegralType& v) -> std::enable_if_t<(    std::is_integral   <IntegralType>::value
                                                                                                                                       &&  std::is_unsigned   <IntegralType>::value
                                                                                                                                       && (std::numeric_limits<IntegralType>::digits > std::numeric_limits<LimbType>::digits)),
                                                                                                                                       uintwide_t<Width2, LimbType, AllocatorType, IsSigned>>;

  // Forward declarations of non-member binary add, sub, mul, div, mod of (IntegralType op uintwide_t).
  template<typename IntegralType, const size_t Width2, typename LimbType, typename AllocatorType, const bool IsSigned> constexpr auto operator+(const IntegralType& u, const uintwide_t<Width2, LimbType, AllocatorType, IsSigned>& v) -> std::enable_if_t<std::is_integral<IntegralType>::value, uintwide_t<Width2, LimbType, AllocatorType, IsSigned>>;
  template<typename IntegralType, const size_t Width2, typename LimbType, typename AllocatorType, const bool IsSigned> constexpr auto operator-(const IntegralType& u, const uintwide_t<Width2, LimbType, AllocatorType, IsSigned>& v) -> std::enable_if_t<std::is_integral<IntegralType>::value, uintwide_t<Width2, LimbType, AllocatorType, IsSigned>>;
  template<typename IntegralType, const size_t Width2, typename LimbType, typename AllocatorType, const bool IsSigned> constexpr auto operator*(const IntegralType& u, const uintwide_t<Width2, LimbType, AllocatorType, IsSigned>& v) -> std::enable_if_t<std::is_integral<IntegralType>::value, uintwide_t<Width2, LimbType, AllocatorType, IsSigned>>;
  template<typename IntegralType, const size_t Width2, typename LimbType, typename AllocatorType, const bool IsSigned> constexpr auto operator/(const IntegralType& u, const uintwide_t<Width2, LimbType, AllocatorType, IsSigned>& v) -> std::enable_if_t<std::is_integral<IntegralType>::value, uintwide_t<Width2, LimbType, AllocatorType, IsSigned>>;
  template<typename IntegralType, const size_t Width2, typename LimbType, typename AllocatorType, const bool IsSigned> constexpr auto operator%(const IntegralType& u, const uintwide_t<Width2, LimbType, AllocatorType, IsSigned>& v) -> std::enable_if_t<std::is_integral<IntegralType>::value, uintwide_t<Width2, LimbType, AllocatorType, IsSigned>>;

  #if !defined(WIDE_INTEGER_DISABLE_FLOAT_INTEROP)
  // Forward declarations of non-member binary add, sub, mul, div, mod of (uintwide_t op FloatingPointType).
  template<typename FloatingPointType, const size_t Width2, typename LimbType, typename AllocatorType, const bool IsSigned> constexpr auto operator+(const uintwide_t<Width2, LimbType, AllocatorType, IsSigned>& u, const FloatingPointType& f) -> std::enable_if_t<std::is_floating_point<FloatingPointType>::value, uintwide_t<Width2, LimbType, AllocatorType, IsSigned>>;
  template<typename FloatingPointType, const size_t Width2, typename LimbType, typename AllocatorType, const bool IsSigned> constexpr auto operator-(const uintwide_t<Width2, LimbType, AllocatorType, IsSigned>& u, const FloatingPointType& f) -> std::enable_if_t<std::is_floating_point<FloatingPointType>::value, uintwide_t<Width2, LimbType, AllocatorType, IsSigned>>;
  template<typename FloatingPointType, const size_t Width2, typename LimbType, typename AllocatorType, const bool IsSigned> constexpr auto operator*(const uintwide_t<Width2, LimbType, AllocatorType, IsSigned>& u, const FloatingPointType& f) -> std::enable_if_t<std::is_floating_point<FloatingPointType>::value, uintwide_t<Width2, LimbType, AllocatorType, IsSigned>>;
  template<typename FloatingPointType, const size_t Width2, typename LimbType, typename AllocatorType, const bool IsSigned> constexpr auto operator/(const uintwide_t<Width2, LimbType, AllocatorType, IsSigned>& u, const FloatingPointType& f) -> std::enable_if_t<std::is_floating_point<FloatingPointType>::value, uintwide_t<Width2, LimbType, AllocatorType, IsSigned>>;
  template<typename FloatingPointType, const size_t Width2, typename LimbType, typename AllocatorType, const bool IsSigned> constexpr auto operator%(const uintwide_t<Width2, LimbType, AllocatorType, IsSigned>& u, const FloatingPointType& f) -> std::enable_if_t<std::is_floating_point<FloatingPointType>::value, uintwide_t<Width2, LimbType, AllocatorType, IsSigned>>;

  // Forward declarations of non-member binary add, sub, mul, div, mod of (FloatingPointType op uintwide_t).
  template<typename FloatingPointType, const size_t Width2, typename LimbType, typename AllocatorType, const bool IsSigned> constexpr auto operator+(const FloatingPointType& f, const uintwide_t<Width2, LimbType, AllocatorType, IsSigned>& v) -> std::enable_if_t<std::is_floating_point<FloatingPointType>::value, uintwide_t<Width2, LimbType, AllocatorType, IsSigned>>;
  template<typename FloatingPointType, const size_t Width2, typename LimbType, typename AllocatorType, const bool IsSigned> constexpr auto operator-(const FloatingPointType& f, const uintwide_t<Width2, LimbType, AllocatorType, IsSigned>& v) -> std::enable_if_t<std::is_floating_point<FloatingPointType>::value, uintwide_t<Width2, LimbType, AllocatorType, IsSigned>>;
  template<typename FloatingPointType, const size_t Width2, typename LimbType, typename AllocatorType, const bool IsSigned> constexpr auto operator*(const FloatingPointType& f, const uintwide_t<Width2, LimbType, AllocatorType, IsSigned>& v) -> std::enable_if_t<std::is_floating_point<FloatingPointType>::value, uintwide_t<Width2, LimbType, AllocatorType, IsSigned>>;
  template<typename FloatingPointType, const size_t Width2, typename LimbType, typename AllocatorType, const bool IsSigned> constexpr auto operator/(const FloatingPointType& f, const uintwide_t<Width2, LimbType, AllocatorType, IsSigned>& v) -> std::enable_if_t<std::is_floating_point<FloatingPointType>::value, uintwide_t<Width2, LimbType, AllocatorType, IsSigned>>;
  template<typename FloatingPointType, const size_t Width2, typename LimbType, typename AllocatorType, const bool IsSigned> constexpr auto operator%(const FloatingPointType& f, const uintwide_t<Width2, LimbType, AllocatorType, IsSigned>& v) -> std::enable_if_t<std::is_floating_point<FloatingPointType>::value, uintwide_t<Width2, LimbType, AllocatorType, IsSigned>>;
  #endif

  // Forward declarations of non-member binary logic operations of (uintwide_t op IntegralType).
  template<typename IntegralType, const size_t Width2, typename LimbType, typename AllocatorType, const bool IsSigned> constexpr auto operator|(const uintwide_t<Width2, LimbType, AllocatorType, IsSigned>& u, const IntegralType& v) -> std::enable_if_t<std::is_integral<IntegralType>::value, uintwide_t<Width2, LimbType, AllocatorType, IsSigned>>;
  template<typename IntegralType, const size_t Width2, typename LimbType, typename AllocatorType, const bool IsSigned> constexpr auto operator^(const uintwide_t<Width2, LimbType, AllocatorType, IsSigned>& u, const IntegralType& v) -> std::enable_if_t<std::is_integral<IntegralType>::value, uintwide_t<Width2, LimbType, AllocatorType, IsSigned>>;
  template<typename IntegralType, const size_t Width2, typename LimbType, typename AllocatorType, const bool IsSigned> constexpr auto operator&(const uintwide_t<Width2, LimbType, AllocatorType, IsSigned>& u, const IntegralType& v) -> std::enable_if_t<std::is_integral<IntegralType>::value, uintwide_t<Width2, LimbType, AllocatorType, IsSigned>>;

  // Forward declarations of non-member binary binary logic operations of (IntegralType op uintwide_t).
  template<typename IntegralType, const size_t Width2, typename LimbType, typename AllocatorType, const bool IsSigned> constexpr auto operator|(const IntegralType& u, const uintwide_t<Width2, LimbType, AllocatorType, IsSigned>& v) -> std::enable_if_t<std::is_integral<IntegralType>::value, uintwide_t<Width2, LimbType, AllocatorType, IsSigned>>;
  template<typename IntegralType, const size_t Width2, typename LimbType, typename AllocatorType, const bool IsSigned> constexpr auto operator^(const IntegralType& u, const uintwide_t<Width2, LimbType, AllocatorType, IsSigned>& v) -> std::enable_if_t<std::is_integral<IntegralType>::value, uintwide_t<Width2, LimbType, AllocatorType, IsSigned>>;
  template<typename IntegralType, const size_t Width2, typename LimbType, typename AllocatorType, const bool IsSigned> constexpr auto operator&(const IntegralType& u, const uintwide_t<Width2, LimbType, AllocatorType, IsSigned>& v) -> std::enable_if_t<std::is_integral<IntegralType>::value, uintwide_t<Width2, LimbType, AllocatorType, IsSigned>>;

  // Forward declarations of non-member shift functions of (uintwide_t shift IntegralType).
  template<typename IntegralType, const size_t Width2, typename LimbType, typename AllocatorType, const bool IsSigned> constexpr auto operator<<(const uintwide_t<Width2, LimbType, AllocatorType, IsSigned>& u, const IntegralType n) -> std::enable_if_t<std::is_integral<IntegralType>::value, uintwide_t<Width2, LimbType, AllocatorType, IsSigned>>; // NOLINT(readability-avoid-const-params-in-decls)
  template<typename IntegralType, const size_t Width2, typename LimbType, typename AllocatorType, const bool IsSigned> constexpr auto operator>>(const uintwide_t<Width2, LimbType, AllocatorType, IsSigned>& u, const IntegralType n) -> std::enable_if_t<std::is_integral<IntegralType>::value, uintwide_t<Width2, LimbType, AllocatorType, IsSigned>>; // NOLINT(readability-avoid-const-params-in-decls)

  // Forward declarations of non-member comparison functions of (uintwide_t cmp uintwide_t).
  template<const size_t Width2, typename LimbType, typename AllocatorType, const bool IsSigned> constexpr auto operator==(const uintwide_t<Width2, LimbType, AllocatorType, IsSigned>& u, const uintwide_t<Width2, LimbType, AllocatorType, IsSigned>& v) -> bool;
  template<const size_t Width2, typename LimbType, typename AllocatorType, const bool IsSigned> constexpr auto operator!=(const uintwide_t<Width2, LimbType, AllocatorType, IsSigned>& u, const uintwide_t<Width2, LimbType, AllocatorType, IsSigned>& v) -> bool;
  template<const size_t Width2, typename LimbType, typename AllocatorType, const bool IsSigned> constexpr auto operator> (const uintwide_t<Width2, LimbType, AllocatorType, IsSigned>& u, const uintwide_t<Width2, LimbType, AllocatorType, IsSigned>& v) -> bool;
  template<const size_t Width2, typename LimbType, typename AllocatorType, const bool IsSigned> constexpr auto operator< (const uintwide_t<Width2, LimbType, AllocatorType, IsSigned>& u, const uintwide_t<Width2, LimbType, AllocatorType, IsSigned>& v) -> bool;
  template<const size_t Width2, typename LimbType, typename AllocatorType, const bool IsSigned> constexpr auto operator>=(const uintwide_t<Width2, LimbType, AllocatorType, IsSigned>& u, const uintwide_t<Width2, LimbType, AllocatorType, IsSigned>& v) -> bool;
  template<const size_t Width2, typename LimbType, typename AllocatorType, const bool IsSigned> constexpr auto operator<=(const uintwide_t<Width2, LimbType, AllocatorType, IsSigned>& u, const uintwide_t<Width2, LimbType, AllocatorType, IsSigned>& v) -> bool;

  // Forward declarations of non-member comparison functions of (uintwide_t cmp IntegralType).
  template<typename IntegralType, const size_t Width2, typename LimbType, typename AllocatorType, const bool IsSigned> constexpr auto operator==(const uintwide_t<Width2, LimbType, AllocatorType, IsSigned>& u, const IntegralType& v) -> std::enable_if_t<std::is_integral<IntegralType>::value, bool>;
  template<typename IntegralType, const size_t Width2, typename LimbType, typename AllocatorType, const bool IsSigned> constexpr auto operator!=(const uintwide_t<Width2, LimbType, AllocatorType, IsSigned>& u, const IntegralType& v) -> std::enable_if_t<std::is_integral<IntegralType>::value, bool>;
  template<typename IntegralType, const size_t Width2, typename LimbType, typename AllocatorType, const bool IsSigned> constexpr auto operator> (const uintwide_t<Width2, LimbType, AllocatorType, IsSigned>& u, const IntegralType& v) -> std::enable_if_t<std::is_integral<IntegralType>::value, bool>;
  template<typename IntegralType, const size_t Width2, typename LimbType, typename AllocatorType, const bool IsSigned> constexpr auto operator< (const uintwide_t<Width2, LimbType, AllocatorType, IsSigned>& u, const IntegralType& v) -> std::enable_if_t<std::is_integral<IntegralType>::value, bool>;
  template<typename IntegralType, const size_t Width2, typename LimbType, typename AllocatorType, const bool IsSigned> constexpr auto operator>=(const uintwide_t<Width2, LimbType, AllocatorType, IsSigned>& u, const IntegralType& v) -> std::enable_if_t<std::is_integral<IntegralType>::value, bool>;
  template<typename IntegralType, const size_t Width2, typename LimbType, typename AllocatorType, const bool IsSigned> constexpr auto operator<=(const uintwide_t<Width2, LimbType, AllocatorType, IsSigned>& u, const IntegralType& v) -> std::enable_if_t<std::is_integral<IntegralType>::value, bool>;

  // Forward declarations of non-member comparison functions of (IntegralType cmp uintwide_t).
  template<typename IntegralType, const size_t Width2, typename LimbType, typename AllocatorType, const bool IsSigned> constexpr auto operator==(const IntegralType& u, const uintwide_t<Width2, LimbType, AllocatorType, IsSigned>& v) -> std::enable_if_t<std::is_integral<IntegralType>::value, bool>;
  template<typename IntegralType, const size_t Width2, typename LimbType, typename AllocatorType, const bool IsSigned> constexpr auto operator!=(const IntegralType& u, const uintwide_t<Width2, LimbType, AllocatorType, IsSigned>& v) -> std::enable_if_t<std::is_integral<IntegralType>::value, bool>;
  template<typename IntegralType, const size_t Width2, typename LimbType, typename AllocatorType, const bool IsSigned> constexpr auto operator> (const IntegralType& u, const uintwide_t<Width2, LimbType, AllocatorType, IsSigned>& v) -> std::enable_if_t<std::is_integral<IntegralType>::value, bool>;
  template<typename IntegralType, const size_t Width2, typename LimbType, typename AllocatorType, const bool IsSigned> constexpr auto operator< (const IntegralType& u, const uintwide_t<Width2, LimbType, AllocatorType, IsSigned>& v) -> std::enable_if_t<std::is_integral<IntegralType>::value, bool>;
  template<typename IntegralType, const size_t Width2, typename LimbType, typename AllocatorType, const bool IsSigned> constexpr auto operator>=(const IntegralType& u, const uintwide_t<Width2, LimbType, AllocatorType, IsSigned>& v) -> std::enable_if_t<std::is_integral<IntegralType>::value, bool>;
  template<typename IntegralType, const size_t Width2, typename LimbType, typename AllocatorType, const bool IsSigned> constexpr auto operator<=(const IntegralType& u, const uintwide_t<Width2, LimbType, AllocatorType, IsSigned>& v) -> std::enable_if_t<std::is_integral<IntegralType>::value, bool>;

  #if !defined(WIDE_INTEGER_DISABLE_FLOAT_INTEROP)
  // Non-member comparison functions of (uintwide_t cmp FloatingPointType).
  template<typename FloatingPointType, const size_t Width2, typename LimbType, typename AllocatorType, const bool IsSigned> constexpr auto operator==(const uintwide_t<Width2, LimbType, AllocatorType, IsSigned>& u, const FloatingPointType& f) -> std::enable_if_t<std::is_floating_point<FloatingPointType>::value, bool>;
  template<typename FloatingPointType, const size_t Width2, typename LimbType, typename AllocatorType, const bool IsSigned> constexpr auto operator!=(const uintwide_t<Width2, LimbType, AllocatorType, IsSigned>& u, const FloatingPointType& f) -> std::enable_if_t<std::is_floating_point<FloatingPointType>::value, bool>;
  template<typename FloatingPointType, const size_t Width2, typename LimbType, typename AllocatorType, const bool IsSigned> constexpr auto operator> (const uintwide_t<Width2, LimbType, AllocatorType, IsSigned>& u, const FloatingPointType& f) -> std::enable_if_t<std::is_floating_point<FloatingPointType>::value, bool>;
  template<typename FloatingPointType, const size_t Width2, typename LimbType, typename AllocatorType, const bool IsSigned> constexpr auto operator< (const uintwide_t<Width2, LimbType, AllocatorType, IsSigned>& u, const FloatingPointType& f) -> std::enable_if_t<std::is_floating_point<FloatingPointType>::value, bool>;
  template<typename FloatingPointType, const size_t Width2, typename LimbType, typename AllocatorType, const bool IsSigned> constexpr auto operator>=(const uintwide_t<Width2, LimbType, AllocatorType, IsSigned>& u, const FloatingPointType& f) -> std::enable_if_t<std::is_floating_point<FloatingPointType>::value, bool>;
  template<typename FloatingPointType, const size_t Width2, typename LimbType, typename AllocatorType, const bool IsSigned> constexpr auto operator<=(const uintwide_t<Width2, LimbType, AllocatorType, IsSigned>& u, const FloatingPointType& f) -> std::enable_if_t<std::is_floating_point<FloatingPointType>::value, bool>;

  // Non-member comparison functions of (FloatingPointType cmp uintwide_t).
  template<typename FloatingPointType, const size_t Width2, typename LimbType, typename AllocatorType, const bool IsSigned> constexpr auto operator==(const FloatingPointType& f, const uintwide_t<Width2, LimbType, AllocatorType, IsSigned>& v) -> std::enable_if_t<std::is_floating_point<FloatingPointType>::value, bool>;
  template<typename FloatingPointType, const size_t Width2, typename LimbType, typename AllocatorType, const bool IsSigned> constexpr auto operator!=(const FloatingPointType& f, const uintwide_t<Width2, LimbType, AllocatorType, IsSigned>& v) -> std::enable_if_t<std::is_floating_point<FloatingPointType>::value, bool>;
  template<typename FloatingPointType, const size_t Width2, typename LimbType, typename AllocatorType, const bool IsSigned> constexpr auto operator> (const FloatingPointType& f, const uintwide_t<Width2, LimbType, AllocatorType, IsSigned>& v) -> std::enable_if_t<std::is_floating_point<FloatingPointType>::value, bool>;
  template<typename FloatingPointType, const size_t Width2, typename LimbType, typename AllocatorType, const bool IsSigned> constexpr auto operator< (const FloatingPointType& f, const uintwide_t<Width2, LimbType, AllocatorType, IsSigned>& v) -> std::enable_if_t<std::is_floating_point<FloatingPointType>::value, bool>;
  template<typename FloatingPointType, const size_t Width2, typename LimbType, typename AllocatorType, const bool IsSigned> constexpr auto operator>=(const FloatingPointType& f, const uintwide_t<Width2, LimbType, AllocatorType, IsSigned>& v) -> std::enable_if_t<std::is_floating_point<FloatingPointType>::value, bool>;
  template<typename FloatingPointType, const size_t Width2, typename LimbType, typename AllocatorType, const bool IsSigned> constexpr auto operator<=(const FloatingPointType& f, const uintwide_t<Width2, LimbType, AllocatorType, IsSigned>& v) -> std::enable_if_t<std::is_floating_point<FloatingPointType>::value, bool>;
  #endif

  #if !defined(WIDE_INTEGER_DISABLE_IOSTREAM)

  // Forward declarations of I/O streaming functions.
  template<typename char_type,
           typename traits_type,
           const size_t Width2,
           typename LimbType,
           typename AllocatorType,
           const bool IsSigned>
  auto operator<<(std::basic_ostream<char_type, traits_type>& out,
                                     const uintwide_t<Width2, LimbType, AllocatorType, IsSigned>& x) -> std::basic_ostream<char_type, traits_type>&;

  template<typename char_type,
           typename traits_type,
           const size_t Width2,
           typename LimbType,
           typename AllocatorType,
           const bool IsSigned>
  auto operator>>(std::basic_istream<char_type, traits_type>& in,
                  uintwide_t<Width2, LimbType, AllocatorType, IsSigned>& x) -> std::basic_istream<char_type, traits_type>&;

  #endif

  // Forward declarations of various number-theoretical tools.
  template<const size_t Width2,
           typename LimbType,
           typename AllocatorType,
           const bool IsSigned>
  WIDE_INTEGER_CONSTEXPR auto swap(uintwide_t<Width2, LimbType, AllocatorType, IsSigned>& x,
                                   uintwide_t<Width2, LimbType, AllocatorType, IsSigned>& y) noexcept -> void;

  template<const size_t Width2,
           typename LimbType,
           typename AllocatorType,
           const bool IsSigned>
  WIDE_INTEGER_CONSTEXPR auto lsb(const uintwide_t<Width2, LimbType, AllocatorType, IsSigned>& x) -> unsigned_fast_type;

  template<const size_t Width2,
           typename LimbType,
           typename AllocatorType,
           const bool IsSigned>
  WIDE_INTEGER_CONSTEXPR auto msb(const uintwide_t<Width2, LimbType, AllocatorType, IsSigned>& x) -> unsigned_fast_type;

  template<const size_t Width2,
           typename LimbType,
           typename AllocatorType,
           const bool IsSigned>
  constexpr auto abs(const uintwide_t<Width2, LimbType, AllocatorType, IsSigned>& x) -> uintwide_t<Width2, LimbType, AllocatorType, IsSigned>;

  template<const size_t Width2,
           typename LimbType,
           typename AllocatorType,
           const bool IsSigned>
  WIDE_INTEGER_CONSTEXPR auto sqrt(const uintwide_t<Width2, LimbType, AllocatorType, IsSigned>& m) -> uintwide_t<Width2, LimbType, AllocatorType, IsSigned>;

  template<const size_t Width2,
           typename LimbType,
           typename AllocatorType,
           const bool IsSigned>
  WIDE_INTEGER_CONSTEXPR auto cbrt(const uintwide_t<Width2, LimbType, AllocatorType, IsSigned>& m) -> uintwide_t<Width2, LimbType, AllocatorType, IsSigned>;

  template<const size_t Width2,
           typename LimbType,
           typename AllocatorType,
           const bool IsSigned>
  WIDE_INTEGER_CONSTEXPR auto rootk(const uintwide_t<Width2, LimbType, AllocatorType, IsSigned>& m, const std::uint_fast8_t k) -> uintwide_t<Width2, LimbType, AllocatorType, IsSigned>; // NOLINT(readability-avoid-const-params-in-decls)

  template<typename OtherUnsignedIntegralTypeP,
           const size_t Width2,
           typename LimbType,
           typename AllocatorType,
           const bool IsSigned>
  WIDE_INTEGER_CONSTEXPR auto pow(const uintwide_t<Width2, LimbType, AllocatorType, IsSigned>& b, const OtherUnsignedIntegralTypeP& p) -> uintwide_t<Width2, LimbType, AllocatorType, IsSigned>;

  template<typename OtherUnsignedIntegralTypeP,
           typename OtherUnsignedIntegralTypeM,
           const size_t Width2,
           typename LimbType,
           typename AllocatorType,
           const bool IsSigned>
  WIDE_INTEGER_CONSTEXPR auto powm(const uintwide_t<Width2, LimbType, AllocatorType, IsSigned>& b,
                                   const OtherUnsignedIntegralTypeP&    p,
                                   const OtherUnsignedIntegralTypeM&    m) -> uintwide_t<Width2, LimbType, AllocatorType, IsSigned>;

  template<const size_t Width2,
           typename LimbType,
           typename AllocatorType,
           const bool IsSigned>
  WIDE_INTEGER_CONSTEXPR auto gcd(const uintwide_t<Width2, LimbType, AllocatorType, IsSigned>& a,
                                  const uintwide_t<Width2, LimbType, AllocatorType, IsSigned>& b) -> uintwide_t<Width2, LimbType, AllocatorType, IsSigned>;

  template<typename UnsignedShortType>
  WIDE_INTEGER_CONSTEXPR auto gcd(const UnsignedShortType& u, const UnsignedShortType& v) -> std::enable_if_t<(   (std::is_integral<UnsignedShortType>::value)
                                                                                                               && (std::is_unsigned<UnsignedShortType>::value)), UnsignedShortType>;

  template<const size_t Width2,
           typename LimbType,
           typename AllocatorType,
           const bool IsSigned>
  WIDE_INTEGER_CONSTEXPR auto lcm(const uintwide_t<Width2, LimbType, AllocatorType, IsSigned>& a,
                                  const uintwide_t<Width2, LimbType, AllocatorType, IsSigned>& b) -> uintwide_t<Width2, LimbType, AllocatorType, IsSigned>;

  template<typename UnsignedShortType>
  WIDE_INTEGER_CONSTEXPR auto lcm(const UnsignedShortType& a, const UnsignedShortType& b) -> std::enable_if_t<(   (std::is_integral<UnsignedShortType>::value)
                                                                                                               && (std::is_unsigned<UnsignedShortType>::value)), UnsignedShortType>;

  template<const size_t Width2,
           typename LimbType,
           typename AllocatorType,
           const bool IsSignedLeft,
           const bool IsSignedRight,
           std::enable_if_t<((!IsSignedLeft) && (!IsSignedRight))> const* = nullptr>
  WIDE_INTEGER_CONSTEXPR
  auto divmod(const uintwide_t<Width2, LimbType, AllocatorType, IsSignedLeft >& a,
              const uintwide_t<Width2, LimbType, AllocatorType, IsSignedRight>& b) -> std::pair<uintwide_t<Width2, LimbType, AllocatorType, IsSignedLeft>, uintwide_t<Width2, LimbType, AllocatorType, IsSignedRight>>;

  template<const size_t Width2,
           typename LimbType,
           typename AllocatorType,
           const bool IsSignedLeft,
           const bool IsSignedRight,
           std::enable_if_t<(IsSignedLeft || IsSignedRight)> const* = nullptr>
  WIDE_INTEGER_CONSTEXPR
  auto divmod(const uintwide_t<Width2, LimbType, AllocatorType, IsSignedLeft >& a,
              const uintwide_t<Width2, LimbType, AllocatorType, IsSignedRight>& b) -> std::pair<uintwide_t<Width2, LimbType, AllocatorType, IsSignedLeft>, uintwide_t<Width2, LimbType, AllocatorType, IsSignedRight>>;

  template<const size_t Width2,
           typename LimbType = std::uint32_t,
           typename AllocatorType = void,
           const bool IsSigned = false>
  class default_random_engine;

  template<const size_t Width2,
           typename LimbType = std::uint32_t,
           typename AllocatorType = void,
           const bool IsSigned = false>
  class uniform_int_distribution;

  template<const size_t Width2,
           typename LimbType,
           typename AllocatorType,
           const bool IsSigned>
  constexpr auto operator==(const uniform_int_distribution<Width2, LimbType, AllocatorType, IsSigned>& lhs,
                            const uniform_int_distribution<Width2, LimbType, AllocatorType, IsSigned>& rhs) -> bool;

  template<const size_t Width2,
           typename LimbType,
           typename AllocatorType,
           const bool IsSigned>
  constexpr auto operator!=(const uniform_int_distribution<Width2, LimbType, AllocatorType, IsSigned>& lhs,
                            const uniform_int_distribution<Width2, LimbType, AllocatorType, IsSigned>& rhs) -> bool;

  template<typename DistributionType,
           typename GeneratorType,
           const size_t Width2,
           typename LimbType,
           typename AllocatorType,
           const bool IsSigned>
  auto miller_rabin(const uintwide_t<Width2, LimbType, AllocatorType, IsSigned>& n,
                    const unsigned_fast_type                                     number_of_trials, // NOLINT(readability-avoid-const-params-in-decls)
                          DistributionType&                                      distribution,
                          GeneratorType&                                         generator) -> bool;

  #if (defined(__cpp_lib_to_chars) && (__cpp_lib_to_chars >= 201611L))
  template<const size_t Width2,
           typename LimbType,
           typename AllocatorType,
           const bool IsSigned>
  auto to_chars(char* first,
                char* last,
                const uintwide_t<Width2, LimbType, AllocatorType, IsSigned>& x,
                int base = static_cast<int>(INT8_C(10))) -> std::to_chars_result;
  #endif

  #if !defined(WIDE_INTEGER_DISABLE_TO_STRING)
  template<const size_t Width2,
           typename LimbType,
           typename AllocatorType,
           const bool IsSigned>
  auto to_string(const uintwide_t<Width2, LimbType, AllocatorType, IsSigned>& x) -> std::string;
  #endif

  template<typename ForwardIterator,
           const size_t Width2,
           typename LimbType,
           typename AllocatorType,
           std::enable_if_t<std::numeric_limits<typename std::iterator_traits<ForwardIterator>::value_type>::digits == std::numeric_limits<LimbType>::digits> const* = nullptr>
  WIDE_INTEGER_CONSTEXPR
  auto import_bits(uintwide_t<Width2, LimbType, AllocatorType, false>& val,
                   ForwardIterator first,
                   ForwardIterator last,
                   unsigned        chunk_size = static_cast<unsigned>(UINT8_C(0)),
                   bool            msv_first  = true) -> uintwide_t<Width2, LimbType, AllocatorType, false>&;

  template<typename ForwardIterator,
           const size_t Width2,
           typename LimbType,
           typename AllocatorType,
           std::enable_if_t<!(std::numeric_limits<typename std::iterator_traits<ForwardIterator>::value_type>::digits == std::numeric_limits<LimbType>::digits)> const* = nullptr>
  WIDE_INTEGER_CONSTEXPR
  auto import_bits(uintwide_t<Width2, LimbType, AllocatorType, false>& val,
                   ForwardIterator first,
                   ForwardIterator last,
                   unsigned        chunk_size = static_cast<unsigned>(UINT8_C(0)),
                   bool            msv_first  = true) -> uintwide_t<Width2, LimbType, AllocatorType, false>&;

  template<typename OutputIterator,
           const size_t Width2,
           typename LimbType,
           typename AllocatorType,
           const bool IsSigned,
           std::enable_if_t<std::numeric_limits<typename std::iterator_traits<OutputIterator>::value_type>::digits == std::numeric_limits<LimbType>::digits> const* = nullptr>
  WIDE_INTEGER_CONSTEXPR
  auto export_bits(const uintwide_t<Width2, LimbType, AllocatorType, IsSigned>& val,
                         OutputIterator out,
                         unsigned       chunk_size,
                         bool           msv_first = true) -> OutputIterator;

  template<typename OutputIterator,
           const size_t Width2,
           typename LimbType,
           typename AllocatorType,
           const bool IsSigned,
           std::enable_if_t<!(std::numeric_limits<typename std::iterator_traits<OutputIterator>::value_type>::digits == std::numeric_limits<LimbType>::digits)> const* = nullptr>
  WIDE_INTEGER_CONSTEXPR
  auto export_bits(const uintwide_t<Width2, LimbType, AllocatorType, IsSigned>& val,
                         OutputIterator out,
                         unsigned       chunk_size,
                         bool           msv_first = true) -> OutputIterator;

  #if(__cplusplus >= 201703L)
  } // namespace math::wide_integer
  #else
  } // namespace wide_integer
  } // namespace math
  #endif

  WIDE_INTEGER_NAMESPACE_END

  namespace std
  {
    // Forward declaration of specialization of std::numeric_limits<uintwide_t>.
    #if defined(WIDE_INTEGER_NAMESPACE)
    template<const WIDE_INTEGER_NAMESPACE::math::wide_integer::size_t Width2,
             typename LimbType,
             typename AllocatorType,
             const bool IsSigned>
    WIDE_INTEGER_NUM_LIMITS_CLASS_TYPE numeric_limits<WIDE_INTEGER_NAMESPACE::math::wide_integer::uintwide_t<Width2, LimbType, AllocatorType, IsSigned>>;
    #else
    template<const ::math::wide_integer::size_t Width2,
             typename LimbType,
             typename AllocatorType,
             const bool IsSigned>
    WIDE_INTEGER_NUM_LIMITS_CLASS_TYPE numeric_limits<::math::wide_integer::uintwide_t<Width2, LimbType, AllocatorType, IsSigned>>;
    #endif
  } // namespace std

  WIDE_INTEGER_NAMESPACE_BEGIN

  #if(__cplusplus >= 201703L)
  namespace math::wide_integer::detail {
  #else
  namespace math { namespace wide_integer { namespace detail { // NOLINT(modernize-concat-nested-namespaces)
  #endif

  template<typename MyType,
           const size_t MySize,
           typename MyAlloc>
  class fixed_dynamic_array final : public detail::dynamic_array<MyType, MyAlloc, size_t, ptrdiff_t>
  {
  private:
    using base_class_type = detail::dynamic_array<MyType, MyAlloc, size_t, ptrdiff_t>;

  public:
    static constexpr auto static_size() -> typename base_class_type::size_type { return MySize; }

    explicit WIDE_INTEGER_CONSTEXPR fixed_dynamic_array(const typename base_class_type::size_type       size_in  = MySize,
                                                        const typename base_class_type::value_type&     value_in = typename base_class_type::value_type(),
                                                        const typename base_class_type::allocator_type& alloc_in = typename base_class_type::allocator_type())
      : base_class_type(MySize, typename base_class_type::value_type(), alloc_in)
    {
      detail::fill_unsafe(base_class_type::begin(),
                          base_class_type::begin() + (std::min)(MySize, static_cast<typename base_class_type::size_type>(size_in)),
                          value_in);
    }

    constexpr fixed_dynamic_array(const fixed_dynamic_array& other_array) = default;

    constexpr fixed_dynamic_array(fixed_dynamic_array&& other_array) noexcept = default;

    WIDE_INTEGER_CONSTEXPR fixed_dynamic_array(std::initializer_list<typename base_class_type::value_type> lst)
      : base_class_type(MySize)
    {
      detail::copy_unsafe(lst.begin(),
                          lst.begin() + (std::min)(static_cast<typename base_class_type::size_type>(lst.size()), MySize),
                          base_class_type::begin());
    }

    WIDE_INTEGER_CONSTEXPR auto operator=(const fixed_dynamic_array& other_array) -> fixed_dynamic_array& = default;

    WIDE_INTEGER_CONSTEXPR auto operator=(fixed_dynamic_array&& other_array) noexcept -> fixed_dynamic_array& = default;

    WIDE_INTEGER_CONSTEXPR ~fixed_dynamic_array() override = default;
  };

  template<typename MyType,
           const size_t MySize>
  class fixed_static_array final : public std::array<MyType, static_cast<std::size_t>(MySize)>
  {
  private:
    using base_class_type = std::array<MyType, static_cast<std::size_t>(MySize)>;

  public:
    using size_type      = size_t;
    using value_type     = typename base_class_type::value_type;
    using allocator_type = std::allocator<void>;

    static constexpr auto static_size() -> size_type { return MySize; }

    constexpr fixed_static_array() = default;

    explicit WIDE_INTEGER_CONSTEXPR fixed_static_array(const size_type   size_in,
                                                       const value_type& value_in = value_type(),
                                                       allocator_type    alloc_in = allocator_type())
    {
      static_cast<void>(alloc_in);

      if(size_in < static_size())
      {
        detail::fill_unsafe(base_class_type::begin(),     base_class_type::begin() + size_in, value_in);
        detail::fill_unsafe(base_class_type::begin() + size_in, base_class_type::end(),       value_type());
      }
      else
      {
        // Exclude this line from code coverage, even though explicit
        // test cases (search for "result_overshift_is_ok") are known
        // to cover this line.
        detail::fill_unsafe(base_class_type::begin(), base_class_type::end(), value_in); // LCOV_EXCL_LINE
      }
    }

    WIDE_INTEGER_CONSTEXPR fixed_static_array(const fixed_static_array&) = default;
    WIDE_INTEGER_CONSTEXPR fixed_static_array(fixed_static_array&&) noexcept = default;

    WIDE_INTEGER_CONSTEXPR fixed_static_array(std::initializer_list<typename base_class_type::value_type> lst)
    {
      const auto size_to_copy =
        (std::min)(static_cast<size_type>(lst.size()),
                   MySize);

      if(size_to_copy < static_cast<size_type>(base_class_type::size()))
      {
        detail::copy_unsafe(lst.begin(),
                            lst.begin() + size_to_copy,
                            base_class_type::begin());

        detail::fill_unsafe(base_class_type::begin() + size_to_copy,
                            base_class_type::end(),
                            static_cast<typename base_class_type::value_type>(UINT8_C(0)));
      }
      else
      {
        detail::copy_unsafe(lst.begin(),
                            lst.begin() + size_to_copy,
                            base_class_type::begin());
      }
    }

    WIDE_INTEGER_CONSTEXPR ~fixed_static_array() = default;

    WIDE_INTEGER_CONSTEXPR auto operator=(const fixed_static_array& other_array) -> fixed_static_array& = default;
    WIDE_INTEGER_CONSTEXPR auto operator=(fixed_static_array&& other_array) noexcept -> fixed_static_array& = default;

    WIDE_INTEGER_CONSTEXPR auto operator[](const size_type i)       -> typename base_class_type::reference       { return base_class_type::operator[](static_cast<typename base_class_type::size_type>(i)); }
    WIDE_INTEGER_CONSTEXPR auto operator[](const size_type i) const -> typename base_class_type::const_reference { return base_class_type::operator[](static_cast<typename base_class_type::size_type>(i)); }
  };

  template<const size_t Width2> struct verify_power_of_two_times_granularity_one_sixty_fourth // NOLINT(altera-struct-pack-align)
  {
    // List of numbers used to identify the form 2^n times 1...63.
    static constexpr auto conditional_value =
       (   verify_power_of_two<static_cast<size_t>(Width2 /  1U)>::conditional_value || verify_power_of_two<static_cast<size_t>(Width2 /  3U)>::conditional_value
        || verify_power_of_two<static_cast<size_t>(Width2 /  5U)>::conditional_value || verify_power_of_two<static_cast<size_t>(Width2 /  7U)>::conditional_value
        || verify_power_of_two<static_cast<size_t>(Width2 /  9U)>::conditional_value || verify_power_of_two<static_cast<size_t>(Width2 / 11U)>::conditional_value
        || verify_power_of_two<static_cast<size_t>(Width2 / 13U)>::conditional_value || verify_power_of_two<static_cast<size_t>(Width2 / 15U)>::conditional_value
        || verify_power_of_two<static_cast<size_t>(Width2 / 17U)>::conditional_value || verify_power_of_two<static_cast<size_t>(Width2 / 19U)>::conditional_value
        || verify_power_of_two<static_cast<size_t>(Width2 / 21U)>::conditional_value || verify_power_of_two<static_cast<size_t>(Width2 / 23U)>::conditional_value
        || verify_power_of_two<static_cast<size_t>(Width2 / 25U)>::conditional_value || verify_power_of_two<static_cast<size_t>(Width2 / 27U)>::conditional_value
        || verify_power_of_two<static_cast<size_t>(Width2 / 29U)>::conditional_value || verify_power_of_two<static_cast<size_t>(Width2 / 31U)>::conditional_value
        || verify_power_of_two<static_cast<size_t>(Width2 / 33U)>::conditional_value || verify_power_of_two<static_cast<size_t>(Width2 / 35U)>::conditional_value
        || verify_power_of_two<static_cast<size_t>(Width2 / 37U)>::conditional_value || verify_power_of_two<static_cast<size_t>(Width2 / 39U)>::conditional_value
        || verify_power_of_two<static_cast<size_t>(Width2 / 41U)>::conditional_value || verify_power_of_two<static_cast<size_t>(Width2 / 43U)>::conditional_value
        || verify_power_of_two<static_cast<size_t>(Width2 / 45U)>::conditional_value || verify_power_of_two<static_cast<size_t>(Width2 / 47U)>::conditional_value
        || verify_power_of_two<static_cast<size_t>(Width2 / 49U)>::conditional_value || verify_power_of_two<static_cast<size_t>(Width2 / 51U)>::conditional_value
        || verify_power_of_two<static_cast<size_t>(Width2 / 53U)>::conditional_value || verify_power_of_two<static_cast<size_t>(Width2 / 55U)>::conditional_value
        || verify_power_of_two<static_cast<size_t>(Width2 / 57U)>::conditional_value || verify_power_of_two<static_cast<size_t>(Width2 / 59U)>::conditional_value
        || verify_power_of_two<static_cast<size_t>(Width2 / 61U)>::conditional_value || verify_power_of_two<static_cast<size_t>(Width2 / 63U)>::conditional_value);
  };

  template<typename UnsignedIntegralType>
  inline WIDE_INTEGER_CONSTEXPR auto lsb_helper(const UnsignedIntegralType& u) -> unsigned_fast_type;

  template<typename UnsignedIntegralType>
  inline WIDE_INTEGER_CONSTEXPR auto msb_helper(const UnsignedIntegralType& u) -> unsigned_fast_type;

  template<>
  inline WIDE_INTEGER_CONSTEXPR auto msb_helper<std::uint32_t>(const std::uint32_t& u) -> unsigned_fast_type;

  template<>
  inline WIDE_INTEGER_CONSTEXPR auto msb_helper<std::uint16_t>(const std::uint16_t& u) -> unsigned_fast_type;

  template<>
  inline WIDE_INTEGER_CONSTEXPR auto msb_helper<std::uint8_t>(const std::uint8_t& u) -> unsigned_fast_type;

  // Use a local implementation of string copy.
  template<typename DestinationIterator,
           typename SourceIterator>
  inline WIDE_INTEGER_CONSTEXPR auto strcpy_unsafe(DestinationIterator dst, SourceIterator src) -> DestinationIterator
  {
    while((*dst++ = *src++) != '\0') { ; } // NOLINT(cppcoreguidelines-pro-bounds-pointer-arithmetic)

    return dst;
  }

  // Use a local implementation of string length.
  inline WIDE_INTEGER_CONSTEXPR auto strlen_unsafe(const char* p_str) -> unsigned_fast_type
  {
    auto str_len_count = static_cast<unsigned_fast_type>(UINT8_C(0));

    while(*p_str != '\0') { ++p_str; ++str_len_count; } // NOLINT(cppcoreguidelines-pro-bounds-pointer-arithmetic,altera-id-dependent-backward-branch)

    return str_len_count;
  }

  template<typename InputIterator,
           typename IntegralType>
  #if !defined(WIDE_INTEGER_DISABLE_WIDE_INTEGER_CONSTEXPR)
  constexpr auto advance_and_point(InputIterator it, IntegralType n) -> InputIterator
  #else
  WIDE_INTEGER_CONSTEXPR auto advance_and_point(InputIterator it, IntegralType n) -> InputIterator
  #endif
  {
    using local_signed_integral_type =
      std::conditional_t<std::is_signed<IntegralType>::value,
                         IntegralType,
                         typename detail::uint_type_helper<static_cast<size_t>(std::numeric_limits<IntegralType>::digits)>::exact_signed_type>;

    using local_difference_type = typename std::iterator_traits<InputIterator>::difference_type;

    #if !defined(WIDE_INTEGER_DISABLE_WIDE_INTEGER_CONSTEXPR)
    return it + static_cast<local_difference_type>(static_cast<local_signed_integral_type>(n));
    #else
    auto my_it = it;

    std::advance(my_it, static_cast<local_difference_type>(static_cast<local_signed_integral_type>(n)));

    return my_it;
    #endif
  }

  template<typename UnsignedShortType,
           typename UnsignedLargeType = typename detail::uint_type_helper<static_cast<size_t>(std::numeric_limits<UnsignedShortType>::digits * 2)>::exact_unsigned_type>
  constexpr auto make_lo(const UnsignedLargeType& u) -> UnsignedShortType
  {
    // From an unsigned integral input parameter of type UnsignedLargeType,
    // extract the low part of it. The type of the extracted
    // low part is UnsignedShortType, which has half the width of UnsignedLargeType.

    using local_ushort_type = UnsignedShortType;
    using local_ularge_type = UnsignedLargeType;

    // Compile-time checks.
    #if defined(WIDE_INTEGER_HAS_LIMB_TYPE_UINT64)
    static_assert(((sizeof(local_ushort_type) * 2U) == sizeof(local_ularge_type)),
                   "Error: Please check the characteristics of the template parameters UnsignedShortType and UnsignedLargeType");
    #else
    static_assert((    ( std::numeric_limits<local_ushort_type>::is_integer)
                   &&  ( std::numeric_limits<local_ularge_type>::is_integer)
                   &&  (!std::numeric_limits<local_ushort_type>::is_signed)
                   &&  (!std::numeric_limits<local_ularge_type>::is_signed)
                   &&  ((sizeof(local_ushort_type) * 2U) == sizeof(local_ularge_type))),
                   "Error: Please check the characteristics of the template parameters UnsignedShortType and UnsignedLargeType");
    #endif

    return static_cast<local_ushort_type>(u);
  }

  template<typename UnsignedShortType,
           typename UnsignedLargeType = typename detail::uint_type_helper<static_cast<size_t>(std::numeric_limits<UnsignedShortType>::digits * 2)>::exact_unsigned_type>
  constexpr auto make_hi(const UnsignedLargeType& u) -> UnsignedShortType
  {
    // From an unsigned integral input parameter of type UnsignedLargeType,
    // extract the high part of it. The type of the extracted
    // high part is UnsignedShortType, which has half the width of UnsignedLargeType.

    using local_ushort_type = UnsignedShortType;
    using local_ularge_type = UnsignedLargeType;

    // Compile-time checks.
    #if defined(WIDE_INTEGER_HAS_LIMB_TYPE_UINT64)
    static_assert(((sizeof(local_ushort_type) * 2U) == sizeof(local_ularge_type)),
                   "Error: Please check the characteristics of the template parameters UnsignedShortType and UnsignedLargeType");
    #else
    static_assert((    ( std::numeric_limits<local_ushort_type>::is_integer)
                   &&  ( std::numeric_limits<local_ularge_type>::is_integer)
                   &&  (!std::numeric_limits<local_ushort_type>::is_signed)
                   &&  (!std::numeric_limits<local_ularge_type>::is_signed)
                   &&  ((sizeof(local_ushort_type) * 2U) == sizeof(local_ularge_type))),
                   "Error: Please check the characteristics of the template parameters UnsignedShortType and UnsignedLargeType");
    #endif

    return static_cast<local_ushort_type>(u >> static_cast<local_ushort_type>(std::numeric_limits<local_ushort_type>::digits));
  }

  template<typename UnsignedShortType,
           typename UnsignedLargeType = typename detail::uint_type_helper<static_cast<size_t>(std::numeric_limits<UnsignedShortType>::digits * 2)>::exact_unsigned_type>
  constexpr auto make_large(const UnsignedShortType& lo, const UnsignedShortType& hi) -> UnsignedLargeType
  {
    // Create a composite unsigned integral value having type UnsignedLargeType.
    // Two constituents are used having type UnsignedShortType, whereby the
    // width of UnsignedShortType is half the width of UnsignedLargeType.

    using local_ushort_type = UnsignedShortType;
    using local_ularge_type = UnsignedLargeType;

    // Compile-time checks.
    #if defined(WIDE_INTEGER_HAS_LIMB_TYPE_UINT64)
    static_assert(((sizeof(local_ushort_type) * 2U) == sizeof(local_ularge_type)),
                   "Error: Please check the characteristics of the template parameters UnsignedShortType and UnsignedLargeType");
    #else
    static_assert((    ( std::numeric_limits<local_ushort_type>::is_integer)
                   &&  ( std::numeric_limits<local_ularge_type>::is_integer)
                   &&  (!std::numeric_limits<local_ushort_type>::is_signed)
                   &&  (!std::numeric_limits<local_ularge_type>::is_signed)
                   &&  ((sizeof(local_ushort_type) * 2U) == sizeof(local_ularge_type))),
                   "Error: Please check the characteristics of the template parameters UnsignedShortType and UnsignedLargeType");
    #endif

    return
      static_cast<local_ularge_type>
      (
          static_cast<local_ularge_type>
          (
            static_cast<local_ularge_type>(hi) << static_cast<unsigned>(std::numeric_limits<UnsignedShortType>::digits)
          )
        | lo
      );
  }

  template<typename UnsignedIntegralType>
  constexpr auto negate(UnsignedIntegralType u) -> std::enable_if_t<(   std::is_integral<UnsignedIntegralType>::value
                                                                     && std::is_unsigned<UnsignedIntegralType>::value), UnsignedIntegralType>
  {
    using local_unsigned_integral_type = UnsignedIntegralType;

    return
      static_cast<local_unsigned_integral_type>
      (
          static_cast<local_unsigned_integral_type>(~u)
        + static_cast<local_unsigned_integral_type>(UINT8_C(1))
      );
  }

  template<typename SignedIntegralType>
  constexpr auto negate(SignedIntegralType n) -> std::enable_if_t<(   std::is_integral<SignedIntegralType>::value
                                                                   && std::is_signed<SignedIntegralType>::value), SignedIntegralType>
  {
    using local_signed_integral_type = SignedIntegralType;

    using local_unsigned_integral_type =
      typename detail::uint_type_helper<static_cast<size_t>(std::numeric_limits<local_signed_integral_type>::digits + 1)>::exact_unsigned_type;

    return
      static_cast<local_signed_integral_type>
      (
        negate(static_cast<local_unsigned_integral_type>(n))
      );
  }

  #if !defined(WIDE_INTEGER_DISABLE_FLOAT_INTEROP)
  template<typename FloatingPointType>
  class native_float_parts final
  {
  public:
    // Emphasize: This template class can be used with native floating-point
    // types like float, double and long double. Note: For long double,
    // you need to verify that the mantissa fits in unsigned long long.
    explicit WIDE_INTEGER_CONSTEXPR native_float_parts(const FloatingPointType f)
    {
      using native_float_type = FloatingPointType;

      static_assert(std::numeric_limits<native_float_type>::digits <= std::numeric_limits<unsigned long long>::digits, // NOLINT(google-runtime-int)
                    "Error: The width of the mantissa does not fit in unsigned long long");

      const auto ff =
        static_cast<native_float_type>
        (
          (f < static_cast<native_float_type>(0.0F)) ? static_cast<native_float_type>(-f) : f
        );

      if(ff < (std::numeric_limits<native_float_type>::min)())
      {
        return;
      }

      using my_own::frexp;

      // Get the fraction and base-2 exponent.
      auto man = static_cast<native_float_type>(frexp(f, &my_exponent_part));

      auto n2 = static_cast<unsigned>(UINT8_C(0));

      for(auto   i = static_cast<std::uint_fast16_t>(UINT8_C(0));
                 i < static_cast<std::uint_fast16_t>(std::numeric_limits<native_float_type>::digits);
               ++i)
      {
        // Extract the mantissa of the floating-point type in base-2
        // (one bit at a time) and store it in an unsigned long long.
        man *= static_cast<int>(INT8_C(2));

        n2   = static_cast<unsigned>(man);
        man -= static_cast<native_float_type>(n2);

        if(n2 != static_cast<unsigned>(UINT8_C(0)))
        {
          my_mantissa_part |= static_cast<unsigned>(UINT8_C(1));
        }

        if(i < static_cast<unsigned>(static_cast<int>(std::numeric_limits<native_float_type>::digits - static_cast<int>(INT8_C(1)))))
        {
          my_mantissa_part <<= static_cast<unsigned>(UINT8_C(1));
        }
      }

      // Ensure that the value is normalized and adjust the exponent.
      my_mantissa_part |= static_cast<unsigned long long>(1ULL << static_cast<unsigned>(std::numeric_limits<native_float_type>::digits - 1)); // NOLINT(google-runtime-int)
      my_exponent_part -= static_cast<int>(INT8_C(1));
    }

    constexpr native_float_parts(const native_float_parts& other) : my_mantissa_part(other.my_mantissa_part),
                                                                    my_exponent_part(other.my_exponent_part) { }

    constexpr native_float_parts(native_float_parts&& other) noexcept : my_mantissa_part(other.my_mantissa_part),
                                                                        my_exponent_part(other.my_exponent_part) { }

    WIDE_INTEGER_CONSTEXPR ~native_float_parts() = default;

    WIDE_INTEGER_CONSTEXPR auto operator=(const native_float_parts& other) noexcept -> native_float_parts& // NOLINT(cert-oop54-cpp)
    {
      if(this != &other)
      {
        my_mantissa_part = other.my_mantissa_part;
        my_exponent_part = other.my_exponent_part;
      }

      return *this;
    }

    WIDE_INTEGER_CONSTEXPR auto operator=(native_float_parts&& other) noexcept -> native_float_parts&
    {
      my_mantissa_part = other.my_mantissa_part;
      my_exponent_part = other.my_exponent_part;

      return *this;
    }

    WIDE_INTEGER_NODISCARD WIDE_INTEGER_CONSTEXPR auto get_mantissa() const -> unsigned long long { return my_mantissa_part; } // NOLINT(google-runtime-int)
    WIDE_INTEGER_NODISCARD WIDE_INTEGER_CONSTEXPR auto get_exponent() const -> int                { return my_exponent_part; }

    WIDE_INTEGER_CONSTEXPR native_float_parts() = delete;

  private:
    unsigned long long my_mantissa_part { }; // NOLINT(readability-identifier-naming,google-runtime-int)
    int                my_exponent_part { }; // NOLINT(readability-identifier-naming)
  };
  #endif

  #if(__cplusplus >= 201703L)
  } // namespace math::wide_integer::detail
  #else
  } // namespace detail
  } // namespace wide_integer
  } // namespace math
  #endif

  #if(__cplusplus >= 201703L)
  namespace math::wide_integer {
  #else
  namespace math { namespace wide_integer { // NOLINT(modernize-concat-nested-namespaces)
  #endif

  template<const size_t Width2,
           typename LimbType,
           typename AllocatorType,
           const bool IsSigned>
  class uintwide_t
  {
  public:
    template<const size_t OtherWidth2,
             typename OtherLimbType,
             typename OtherAllocatorType,
             const bool OtherIsSigned>
    friend class uintwide_t;

    // Class-local type definitions.
    using limb_type = LimbType;

    using double_limb_type =
      typename detail::uint_type_helper<static_cast<size_t>(static_cast<int>(std::numeric_limits<limb_type>::digits * static_cast<int>(INT8_C(2))))>::exact_unsigned_type;

    // Legacy ularge and ushort types. These are no longer used
    // in the class, but provided for legacy compatibility.
    using ushort_type = limb_type;
    using ularge_type = double_limb_type;

    // More compile-time checks.
    #if defined(WIDE_INTEGER_HAS_LIMB_TYPE_UINT64)
    static_assert(((sizeof(limb_type) * 2U) == sizeof(double_limb_type)),
                   "Error: Please check the characteristics of the template parameters UnsignedShortType and UnsignedLargeType");
    #else
    static_assert((    ( std::numeric_limits<limb_type>::is_integer)
                   &&  ( std::numeric_limits<double_limb_type>::is_integer)
                   &&  (!std::numeric_limits<limb_type>::is_signed)
                   &&  (!std::numeric_limits<double_limb_type>::is_signed)
                   &&  ((sizeof(limb_type) * 2U) == sizeof(double_limb_type))),
                   "Error: Please check the characteristics of the template parameters UnsignedShortType and UnsignedLargeType");
    #endif

    // Helper constants for the digit characteristics.
    static constexpr size_t my_width2 = Width2;

    // The number of limbs.
    static constexpr size_t number_of_limbs =
      static_cast<size_t>
      (
        Width2 / static_cast<size_t>(std::numeric_limits<limb_type>::digits)
      );

    static constexpr size_t number_of_limbs_karatsuba_threshold =
      static_cast<size_t>
      (
        static_cast<unsigned>
        (
            static_cast<unsigned>(UINT8_C(128))
          + static_cast<unsigned>(UINT8_C(1))
        )
      );

    // Verify that the Width2 template parameter (mirrored with my_width2):
    //   * Is equal to 2^n times 1...63.
    //   * And that there are at least 16, 24 or 32 binary digits, or more.
    //   * And that the number of binary digits is an exact multiple of the number of limbs.
    static_assert(    detail::verify_power_of_two_times_granularity_one_sixty_fourth<my_width2>::conditional_value
                  && (my_width2 >= static_cast<size_t>(UINT8_C(16)))
                  && (my_width2 == static_cast<size_t>(number_of_limbs * static_cast<size_t>(std::numeric_limits<limb_type>::digits))),
                  "Error: Width2 must be 2^n times 1...63 (with n >= 3), while being 16, 24, 32 or larger, and exactly divisible by limb count");

    // The type of the internal data representation.
    #if !defined(WIDE_INTEGER_TEST_REPRESENTATION_AS_STD_LIST)
    using representation_type =
      std::conditional_t
        <std::is_same<AllocatorType, void>::value,
         detail::fixed_static_array <limb_type,
                                     number_of_limbs>,
         detail::fixed_dynamic_array<limb_type,
                                     number_of_limbs,
                                     typename std::allocator_traits<std::conditional_t<std::is_same<AllocatorType, void>::value,
                                                                                       std::allocator<void>,
                                                                                       AllocatorType>>::template rebind_alloc<limb_type>>>;
    #else
    using representation_type = std::list<limb_type>;
    #endif

    // The iterator types of the internal data representation.
    using iterator               = typename representation_type::iterator;
    using const_iterator         = typename representation_type::const_iterator;
    using reverse_iterator       = typename representation_type::reverse_iterator;
    using const_reverse_iterator = typename representation_type::const_reverse_iterator;

    // Define a class-local type that has double the width of *this.
    using double_width_type = uintwide_t<static_cast<size_t>(Width2 * static_cast<size_t>(UINT8_C(2))), limb_type, AllocatorType, IsSigned>;

    // Default constructor.
    constexpr uintwide_t() = default;

    // Constructors from built-in unsigned integral types that
    // are less wide than limb_type or exactly as wide as limb_type.
    template<typename UnsignedIntegralType>
    WIDE_INTEGER_CONSTEXPR
    uintwide_t(const UnsignedIntegralType v, // NOLINT(google-explicit-constructor,hicpp-explicit-conversions)
               std::enable_if_t<(    std::is_integral   <UnsignedIntegralType>::value
                                 &&  std::is_unsigned   <UnsignedIntegralType>::value
                                 && (std::numeric_limits<UnsignedIntegralType>::digits <= std::numeric_limits<limb_type>::digits))>* = nullptr) // NOLINT(hicpp-named-parameter,readability-named-parameter)
    {
      values.front() = v;
    }

    // Constructors from built-in unsigned integral types that
    // are wider than limb_type, and do not have exactly the
    // same width as limb_type.
    template<typename UnsignedIntegralType>
    WIDE_INTEGER_CONSTEXPR uintwide_t(const UnsignedIntegralType v, // NOLINT(google-explicit-constructor,hicpp-explicit-conversions)
                                      std::enable_if_t<(    std::is_integral   <UnsignedIntegralType>::value
                                                        &&  std::is_unsigned   <UnsignedIntegralType>::value
                                                        && (std::numeric_limits<UnsignedIntegralType>::digits > std::numeric_limits<limb_type>::digits))>* p_nullparam = nullptr)
    {
      static_cast<void>(p_nullparam == nullptr);

      auto u_it = values.begin(); // NOLINT(llvm-qualified-auto,readability-qualified-auto)
      auto vr   = v;

      using local_unsigned_integral_type = UnsignedIntegralType;

      while(vr != static_cast<local_unsigned_integral_type>(UINT8_C(0))) // NOLINT(altera-id-dependent-backward-branch)
      {
        if(u_it != values.end())
        {
          *u_it = static_cast<limb_type>(vr);

          ++u_it;

          vr = static_cast<local_unsigned_integral_type>(vr >> static_cast<unsigned>(std::numeric_limits<limb_type>::digits));
        }
        else
        {
          break;
        }
      }

      detail::fill_unsafe(u_it, values.end(), static_cast<limb_type>(UINT8_C(0)));
    }

    // Constructors from built-in signed integral types.
    template<typename SignedIntegralType>
    WIDE_INTEGER_CONSTEXPR uintwide_t(const SignedIntegralType v, // NOLINT(google-explicit-constructor,hicpp-explicit-conversions)
                                      std::enable_if_t<(   std::is_integral<SignedIntegralType>::value
                                                        && std::is_signed  <SignedIntegralType>::value)>* p_nullparam = nullptr)
      : values(number_of_limbs)
    {
      static_cast<void>(p_nullparam == nullptr);

      using local_signed_integral_type   = SignedIntegralType;
      using local_unsigned_integral_type =
        typename detail::uint_type_helper<static_cast<size_t>(std::numeric_limits<local_signed_integral_type>::digits + 1)>::exact_unsigned_type;

      const auto v_is_neg = (v < static_cast<local_signed_integral_type>(0));

      const local_unsigned_integral_type u =
        ((!v_is_neg) ? static_cast<local_unsigned_integral_type>(v)
                     : static_cast<local_unsigned_integral_type>(detail::negate(v)));

      operator=(uintwide_t(u));

      if(v_is_neg) { negate(); }
    }

    #if !defined(WIDE_INTEGER_DISABLE_FLOAT_INTEROP)
    template<typename FloatingPointType,
             std::enable_if_t<(std::is_floating_point<FloatingPointType>::value)> const* = nullptr>
    WIDE_INTEGER_CONSTEXPR uintwide_t(const FloatingPointType f) // NOLINT(google-explicit-constructor,hicpp-explicit-conversions)
    {
      using local_builtin_float_type = FloatingPointType;

      using detail::my_own::isfinite;

      if(!(isfinite)(f))
      {
        operator=(static_cast<unsigned>(UINT8_C(0)));
      }
      else
      {
        const auto f_is_neg = (f < static_cast<local_builtin_float_type>(0.0F));

        const auto a =
          static_cast<local_builtin_float_type>
          (
            (!f_is_neg) ? f : static_cast<local_builtin_float_type>(-f)
          );

        const auto a_is_zero = (a < static_cast<local_builtin_float_type>(1.0F));

        if(!a_is_zero)
        {
          const detail::native_float_parts<local_builtin_float_type> ld_parts(a);

          // Create a decwide_t from the fractional part of the
          // mantissa expressed as an unsigned long long.
          *this = uintwide_t(ld_parts.get_mantissa());

          // Scale the unsigned long long representation to the fractional
          // part of the long double and multiply with the base-2 exponent.
          const auto p2 =
            static_cast<int>
            (
                ld_parts.get_exponent()
              - static_cast<int>(std::numeric_limits<FloatingPointType>::digits - static_cast<int>(INT8_C(1)))
            );

          if     (p2 <   static_cast<int>(INT8_C(0))) { *this >>= static_cast<unsigned>(-p2); }
          else if(p2 ==  static_cast<int>(INT8_C(0))) { ; }
          else                                        { *this <<= static_cast<unsigned>( p2); }

          if(f_is_neg)
          {
            negate();
          }
        }
        else
        {
          operator=(static_cast<unsigned>(UINT8_C(0)));
        }
      }
    }
    #endif

    // Copy constructor.
    #if !defined(WIDE_INTEGER_DISABLE_TRIVIAL_COPY_AND_STD_LAYOUT_CHECKS)
    constexpr uintwide_t(const uintwide_t& other) = default;
    #else
    constexpr uintwide_t(const uintwide_t& other) : values(other.values) { }
    #endif

    // Copy-like constructor from the other signed-ness type.
    template<const bool OtherIsSigned,
             std::enable_if_t<(OtherIsSigned != IsSigned)> const* = nullptr>
    constexpr uintwide_t(const uintwide_t<Width2, LimbType, AllocatorType, OtherIsSigned>& other) // NOLINT(google-explicit-constructor,hicpp-explicit-conversions)
      : values(other.values) { }

    // Copy-like constructor from the another type having width that is wider
    // (but has the same limb type) and possibly a different signed-ness.
    template<const size_t OtherWidth2,
             const bool OtherIsSigned,
             std::enable_if_t<(Width2 < OtherWidth2)> const* = nullptr>
    explicit WIDE_INTEGER_CONSTEXPR uintwide_t(const uintwide_t<OtherWidth2, LimbType, AllocatorType, OtherIsSigned>& v)
    {
      using other_wide_integer_type = uintwide_t<OtherWidth2, LimbType, AllocatorType, OtherIsSigned>;

      const auto v_is_neg = (other_wide_integer_type::is_neg(v));

      constexpr auto sz = static_cast<size_t>(number_of_limbs);

      if(!v_is_neg)
      {
        detail::copy_unsafe(v.crepresentation().cbegin(),
                            detail::advance_and_point(v.crepresentation().cbegin(), sz),
                            values.begin());
      }
      else
      {
        const other_wide_integer_type uv(-v);

        detail::copy_unsafe(uv.crepresentation().cbegin(),
                            detail::advance_and_point(uv.crepresentation().cbegin(), sz),
                            values.begin());

        negate();
      }
    }

    // Copy-like constructor from the another type having width that is less wide
    // (but has the same limb type) and possibly a different signed-ness.
    template<const size_t OtherWidth2,
             const bool OtherIsSigned,
             std::enable_if_t<(Width2 > OtherWidth2)> const* = nullptr>
    explicit WIDE_INTEGER_CONSTEXPR uintwide_t(const uintwide_t<OtherWidth2, LimbType, AllocatorType, OtherIsSigned>& v)
    {
      using other_wide_integer_type = uintwide_t<OtherWidth2, LimbType, AllocatorType, OtherIsSigned>;

      constexpr auto sz = static_cast<size_t>(other_wide_integer_type::number_of_limbs);

      if(!other_wide_integer_type::is_neg(v))
      {
        detail::copy_unsafe(v.crepresentation().cbegin(),
                            detail::advance_and_point(v.crepresentation().cbegin(), sz),
                            values.begin());

        detail::fill_unsafe(detail::advance_and_point(values.begin(), sz), values.end(), static_cast<limb_type>(UINT8_C(0)));
      }
      else
      {
        const other_wide_integer_type uv(-v);

        detail::copy_unsafe(uv.crepresentation().cbegin(),
                            detail::advance_and_point(uv.crepresentation().cbegin(), sz),
                            values.begin());

        detail::fill_unsafe(detail::advance_and_point(values.begin(), sz), values.end(), static_cast<limb_type>(UINT8_C(0)));

        negate();
      }
    }

    // Constructor from a constant character string.
    WIDE_INTEGER_CONSTEXPR uintwide_t(const char* str_input) // NOLINT(google-explicit-constructor,hicpp-explicit-conversions)
      : values
        {
          static_cast<typename representation_type::size_type>(number_of_limbs),
          static_cast<typename representation_type::value_type>(UINT8_C(0)),
          typename representation_type::allocator_type()
        }
    {
      if(!rd_string(str_input))
      {
        static_cast<void>(operator=((std::numeric_limits<uintwide_t>::max)()));
      }
    }

    // Move constructor.
    constexpr uintwide_t(uintwide_t&&) noexcept = default;

    // Move-like constructor from the other signed-ness type.
    // This constructor is non-explicit because it is a trivial conversion.
    template<const bool OtherIsSigned,
             std::enable_if_t<(IsSigned != OtherIsSigned)> const* = nullptr>
    constexpr uintwide_t(uintwide_t<Width2, LimbType, AllocatorType, OtherIsSigned>&& other) // NOLINT(google-explicit-constructor,hicpp-explicit-conversions)
      : values(static_cast<representation_type&&>(other.values)) { }

    // Default destructor.
    WIDE_INTEGER_CONSTEXPR ~uintwide_t() = default;

    // Assignment operator.
    WIDE_INTEGER_CONSTEXPR auto operator=(const uintwide_t&) -> uintwide_t& = default;

    // Assignment operator from the other signed-ness type.
    template<const bool OtherIsSigned,
             std::enable_if_t<(OtherIsSigned != IsSigned)> const* = nullptr>
    WIDE_INTEGER_CONSTEXPR auto operator=(const uintwide_t<Width2, LimbType, AllocatorType, OtherIsSigned>& other) -> uintwide_t&
    {
      values = other.values;

      return *this;
    }

    // Trivial move assignment operator.
    WIDE_INTEGER_CONSTEXPR auto operator=(uintwide_t&& other) noexcept -> uintwide_t& = default;

    // Trivial move assignment operator from the other signed-ness type.
    template<const bool OtherIsSigned,
             std::enable_if_t<(IsSigned != OtherIsSigned)> const* = nullptr>
    WIDE_INTEGER_CONSTEXPR auto operator=(uintwide_t<Width2, LimbType, AllocatorType, OtherIsSigned>&& other) -> uintwide_t&
    {
      values = static_cast<representation_type&&>(other.values);

      return *this;
    }

    #if !defined(WIDE_INTEGER_DISABLE_FLOAT_INTEROP)
    explicit constexpr operator long double() const { return extract_builtin_floating_point_type<long double>(); }
    explicit constexpr operator double     () const { return extract_builtin_floating_point_type<double>     (); }
    explicit constexpr operator float      () const { return extract_builtin_floating_point_type<float>      (); }
    #endif

    template<typename IntegralType,
             typename = std::enable_if_t<std::is_integral<IntegralType>::value>>
    explicit constexpr operator IntegralType() const
    {
      using local_integral_type = IntegralType;

      return
        (
          (!is_neg(*this))
            ? extract_builtin_integral_type<local_integral_type>()
            : detail::negate((-*this).template extract_builtin_integral_type<local_integral_type>())
        );
    }

    // Cast operator to built-in Boolean type.
    explicit constexpr operator bool() const { return (!is_zero()); }

    // Cast operator that casts to a uintwide_t possibly having a different width
    // and/or possibly having a different signed-ness, but having the same limb type.
    template<const size_t OtherWidth2,
             const bool OtherIsSigned>
    WIDE_INTEGER_CONSTEXPR operator uintwide_t<OtherWidth2, LimbType, AllocatorType, OtherIsSigned>() const // NOLINT(hicpp-explicit-conversions,google-explicit-constructor)
    {
      const auto this_is_neg = is_neg(*this);

      using other_wide_integer_type = uintwide_t<OtherWidth2, LimbType, AllocatorType, OtherIsSigned>;

      constexpr auto sz =
        static_cast<size_t>
        (
          (Width2 < OtherWidth2) ? static_cast<size_t>(number_of_limbs)
                                 : static_cast<size_t>(other_wide_integer_type::number_of_limbs)
        );

      other_wide_integer_type other;

      if(!this_is_neg)
      {
        detail::copy_unsafe(crepresentation().cbegin(),
                            detail::advance_and_point(crepresentation().cbegin(), sz),
                            other.values.begin());

        // TBD: Can proper/better template specialization remove the need for if constexpr here?
        #if (   (defined(_MSC_VER) && ((_MSC_VER >= 1900) && defined(_HAS_CXX17) && (_HAS_CXX17 != 0))) \
             || (defined(__cplusplus) && ((__cplusplus >= 201703L) || defined(__cpp_if_constexpr))))
        if constexpr(Width2 < OtherWidth2)
        #else
        if(Width2 < OtherWidth2)
        #endif
        {
          detail::fill_unsafe(detail::advance_and_point(other.values.begin(), sz), other.values.end(), static_cast<limb_type>(UINT8_C(0)));
        }
      }
      else
      {
        other_wide_integer_type uv(*this);

        uv.negate();

        detail::copy_unsafe(uv.crepresentation().cbegin(),
                            detail::advance_and_point(uv.crepresentation().cbegin(), sz),
                            other.values.begin());

        // TBD: Can proper/better template specialization remove the need for if constexpr here?
        #if (   (defined(_MSC_VER) && ((_MSC_VER >= 1900) && defined(_HAS_CXX17) && (_HAS_CXX17 != 0))) \
             || (defined(__cplusplus) && ((__cplusplus >= 201703L) || defined(__cpp_if_constexpr))))
        if constexpr(Width2 < OtherWidth2)
        #else
        if(Width2 < OtherWidth2)
        #endif
        {
          detail::fill_unsafe(detail::advance_and_point(other.values.begin(), sz), other.values.end(), static_cast<limb_type>(UINT8_C(0)));
        }

        other.negate();
      }

      return other;
    }

    // Provide a user interface to the internal data representation.
                           WIDE_INTEGER_CONSTEXPR auto  representation()       ->       representation_type& { return values; }
    WIDE_INTEGER_NODISCARD WIDE_INTEGER_CONSTEXPR auto  representation() const -> const representation_type& { return values; }
    WIDE_INTEGER_NODISCARD WIDE_INTEGER_CONSTEXPR auto crepresentation() const -> const representation_type& { return values; }

    // Unary operators: not, plus and minus.
    WIDE_INTEGER_CONSTEXPR auto operator+() const -> const uintwide_t& { return *this; }
    WIDE_INTEGER_CONSTEXPR auto operator-() const ->       uintwide_t  { uintwide_t tmp(*this); tmp.negate(); return tmp; }

    WIDE_INTEGER_CONSTEXPR auto operator+=(const uintwide_t& other) -> uintwide_t&
    {
      if(this == &other)
      {
        const uintwide_t self(other);

        // Unary addition function.
        const auto carry = eval_add_n(values.begin(), // LCOV_EXCL_LINE
                                      values.cbegin(),
                                      self.values.cbegin(),
                                      static_cast<unsigned_fast_type>(number_of_limbs),
                                      static_cast<limb_type>(UINT8_C(0)));

        static_cast<void>(carry);
      }
      else
      {
        // Unary addition function.
        const auto carry = eval_add_n(values.begin(),
                                      values.cbegin(),
                                      other.values.cbegin(),
                                      static_cast<unsigned_fast_type>(number_of_limbs),
                                      static_cast<limb_type>(UINT8_C(0)));

        static_cast<void>(carry);
      }

      return *this;
    }

    WIDE_INTEGER_CONSTEXPR auto operator-=(const uintwide_t& other) -> uintwide_t&
    {
      if(this == &other)
      {
        detail::fill_unsafe(values.begin(), values.end(), static_cast<typename representation_type::value_type>(UINT8_C(0))); // LCOV_EXCL_LINE
      }
      else
      {
        // Unary subtraction function.
        const auto has_borrow = eval_subtract_n(values.begin(),
                                                values.cbegin(),
                                                other.values.cbegin(),
                                                number_of_limbs,
                                                false);

        static_cast<void>(has_borrow);
      }

      return *this;
    }

    WIDE_INTEGER_CONSTEXPR auto operator*=(const uintwide_t& other) -> uintwide_t&
    {
      if(this == &other)
      {
        const uintwide_t other_as_self_copy(other); // NOLINT(performance-unnecessary-copy-initialization)

        eval_mul_unary(*this, other_as_self_copy);
      }
      else
      {
        eval_mul_unary(*this, other);
      }

      return *this;
    }

    WIDE_INTEGER_CONSTEXPR auto mul_by_limb(const limb_type v) -> uintwide_t&
    {
      if(v == static_cast<limb_type>(UINT8_C(0)))
      {
        detail::fill_unsafe(values.begin(), values.end(), static_cast<typename representation_type::value_type>(UINT8_C(0)));
      }
      else if(v > static_cast<limb_type>(UINT8_C(1)))
      {
        static_cast<void>(eval_multiply_1d(values.begin(),
                                           values.cbegin(),
                                           v,
                                           number_of_limbs));
      }

      return *this;
    }

    WIDE_INTEGER_CONSTEXPR auto operator/=(const uintwide_t& other) -> uintwide_t&
    {
      if(this == &other)
      {
        values.front() = static_cast<limb_type>(UINT8_C(1));

        detail::fill_unsafe(detail::advance_and_point(values.begin(), 1U), values.end(), static_cast<limb_type>(UINT8_C(0))); // LCOV_EXCL_LINE
      }
      else if(other.is_zero())
      {
        *this = limits_helper_max(IsSigned);
      }
      else
      {
        // Unary division function.

        const auto numer_was_neg = is_neg(*this);
        const auto denom_was_neg = is_neg(other);

        if(numer_was_neg || denom_was_neg)
        {
          using local_unsigned_wide_type = uintwide_t<Width2, limb_type, AllocatorType, false>;

          local_unsigned_wide_type a(*this);
          local_unsigned_wide_type b(other);

          if(numer_was_neg) { a.negate(); }
          if(denom_was_neg) { b.negate(); }

          a.eval_divide_knuth(b);

          if(numer_was_neg != denom_was_neg) { a.negate(); }

          values = a.values;
        }
        else
        {
          eval_divide_knuth(other);
        }
      }

      return *this;
    }

    WIDE_INTEGER_CONSTEXPR auto operator%=(const uintwide_t& other) -> uintwide_t&
    {
      if(this == &other)
      {
        detail::fill_unsafe(values.begin(), values.end(), static_cast<limb_type>(UINT8_C(0))); // LCOV_EXCL_LINE
      }
      else
      {
        // Unary modulus function.
        const auto numer_was_neg = is_neg(*this);
        const auto denom_was_neg = is_neg(other);

        if(numer_was_neg || denom_was_neg)
        {
          using local_unsigned_wide_type = uintwide_t<Width2, limb_type, AllocatorType, false>;

          local_unsigned_wide_type a(*this);
          local_unsigned_wide_type b(other);

          if(numer_was_neg) { a.negate(); }
          if(denom_was_neg) { b.negate(); }

          local_unsigned_wide_type remainder_unsigned { };

          a.eval_divide_knuth(b, &remainder_unsigned);

          // The sign of the remainder follows the sign of the denominator.
          if(numer_was_neg) { remainder_unsigned.negate(); }

          values = remainder_unsigned.values;
        }
        else
        {
          uintwide_t remainder { };

          eval_divide_knuth(other, &remainder);

          values = remainder.values;
        }
      }

      return *this;
    }

    // Operators pre-increment and pre-decrement.
    WIDE_INTEGER_CONSTEXPR auto operator++()  -> uintwide_t& { preincrement(); return *this; }
    WIDE_INTEGER_CONSTEXPR auto operator--()  -> uintwide_t& { predecrement(); return *this; }

    // Operators post-increment and post-decrement.
    WIDE_INTEGER_CONSTEXPR auto operator++(int) -> uintwide_t { const uintwide_t w(*this); preincrement(); return w; }
    WIDE_INTEGER_CONSTEXPR auto operator--(int) -> uintwide_t { const uintwide_t w(*this); predecrement(); return w; }

    WIDE_INTEGER_CONSTEXPR auto operator~() -> uintwide_t&
    {
      // Perform bitwise NOT.
      bitwise_not();

      return *this;
    }

    WIDE_INTEGER_CONSTEXPR auto operator|=(const uintwide_t& other) -> uintwide_t& // LCOV_EXCL_LINE
    {
      if(this != &other) // LCOV_EXCL_LINE
      {
        auto bi = other.values.cbegin(); // NOLINT(llvm-qualified-auto,readability-qualified-auto)

        // Perform bitwise OR.
        for(auto& a : values)
        {
          a = static_cast<limb_type>(a | *bi);

          ++bi;
        }
      }

      return *this; // LCOV_EXCL_LINE
    }

    WIDE_INTEGER_CONSTEXPR auto operator^=(const uintwide_t& other) -> uintwide_t& // LCOV_EXCL_LINE
    {
      if(this == &other) // LCOV_EXCL_LINE
      {
        detail::fill_unsafe(values.begin(), values.end(), static_cast<typename representation_type::value_type>(UINT8_C(0))); // LCOV_EXCL_LINE
      }
      else
      {
        auto bi = other.values.cbegin(); // NOLINT(llvm-qualified-auto,readability-qualified-auto)

        // Perform bitwise XOR.
        for(auto& a : values)
        {
          a = static_cast<limb_type>(a ^ *bi);

          ++bi;
        }
      }

      return *this; // LCOV_EXCL_LINE
    }

    WIDE_INTEGER_CONSTEXPR auto operator&=(const uintwide_t& other) -> uintwide_t&
    {
      if(this != &other) // LCOV_EXCL_LINE
      {
        auto bi = other.values.cbegin(); // NOLINT(llvm-qualified-auto,readability-qualified-auto)

        // Perform bitwise AND.
        for(auto& a : values)
        {
          a = static_cast<limb_type>(a & *bi);

          ++bi;
        }
      }

      return *this;
    }

    template<typename SignedIntegralType>
    WIDE_INTEGER_CONSTEXPR auto operator<<=(const SignedIntegralType n) -> std::enable_if_t<(   std::is_integral<SignedIntegralType>::value
                                                                                             && std::is_signed  <SignedIntegralType>::value), uintwide_t>&
    {
      // Implement left-shift operator for signed integral argument.
      if(n < static_cast<SignedIntegralType>(0))
      {
        using local_unsigned_type =
          typename detail::uint_type_helper<static_cast<size_t>(std::numeric_limits<SignedIntegralType>::digits + 1)>::exact_unsigned_type;

        operator>>=(static_cast<local_unsigned_type>(detail::negate(n)));
      }
      else if(n > static_cast<SignedIntegralType>(0))
      {
        if(exceeds_width(n))
        {
          // Exclude this line from code coverage, even though explicit
          // test cases (search for "result_overshift_is_ok") are known
          // to cover this line.
          detail::fill_unsafe(values.begin(), values.end(), static_cast<limb_type>(UINT8_C(0))); // LCOV_EXCL_LINE
        }
        else
        {
          shl(n);
        }
      }

      return *this;
    }

    template<typename UnsignedIntegralType>
    WIDE_INTEGER_CONSTEXPR auto operator<<=(const UnsignedIntegralType n) -> std::enable_if_t<(     std::is_integral<UnsignedIntegralType>::value
                                                                                               && (!std::is_signed  <UnsignedIntegralType>::value)), uintwide_t>&
    {
      // Implement left-shift operator for unsigned integral argument.
      if(n != static_cast<UnsignedIntegralType>(0))
      {
        if(exceeds_width(n))
        {
          // Exclude this line from code coverage, even though explicit
          // test cases (search for "result_overshift_is_ok") are known
          // to cover this line.
          detail::fill_unsafe(values.begin(), values.end(), static_cast<limb_type>(UINT8_C(0))); // LCOV_EXCL_LINE
        }
        else
        {
          shl(n);
        }
      }

      return *this;
    }

    template<typename SignedIntegralType>
    WIDE_INTEGER_CONSTEXPR auto operator>>=(const SignedIntegralType n) -> std::enable_if_t<(   std::is_integral<SignedIntegralType>::value
                                                                                             && std::is_signed  <SignedIntegralType>::value), uintwide_t>&
    {
      // Implement right-shift operator for signed integral argument.
      if(n < static_cast<SignedIntegralType>(0))
      {
        using local_unsigned_type =
          typename detail::uint_type_helper<static_cast<size_t>(std::numeric_limits<SignedIntegralType>::digits + 1)>::exact_unsigned_type;

        operator<<=(static_cast<local_unsigned_type>(detail::negate(n)));
      }
      else if(n > static_cast<SignedIntegralType>(0))
      {
        if(exceeds_width(n))
        {
          // Fill with either 0's or 1's. Note also the implementation-defined
          // behavior of excessive right-shift of negative value.

          // Exclude this line from code coverage, even though explicit
          // test cases (search for "result_overshift_is_ok") are known
          // to cover this line.
          detail::fill_unsafe(values.begin(), values.end(), right_shift_fill_value()); // LCOV_EXCL_LINE
        }
        else
        {
          shr(n);
        }
      }

      return *this;
    }

    template<typename UnsignedIntegralType>
    WIDE_INTEGER_CONSTEXPR auto operator>>=(const UnsignedIntegralType n) -> std::enable_if_t<(     std::is_integral<UnsignedIntegralType>::value
                                                                                               && (!std::is_signed  <UnsignedIntegralType>::value)), uintwide_t>&
    {
      // Implement right-shift operator for unsigned integral argument.
      if(n != static_cast<UnsignedIntegralType>(0))
      {
        if(exceeds_width(n))
        {
          // Fill with either 0's or 1's. Note also the implementation-defined
          // behavior of excessive right-shift of negative value.

          // Exclude this line from code coverage, even though explicit
          // test cases (search for "result_overshift_is_ok") are known
          // to cover this line.
          detail::fill_unsafe(values.begin(), values.end(), right_shift_fill_value()); // LCOV_EXCL_LINE
        }
        else
        {
          shr(n);
        }
      }

      return *this;
    }

    // Implement comparison operators.
    constexpr auto operator==(const uintwide_t& other) const -> bool { return (compare(other) == static_cast<std::int_fast8_t>( 0)); }
    constexpr auto operator< (const uintwide_t& other) const -> bool { return (compare(other) == static_cast<std::int_fast8_t>(-1)); }
    constexpr auto operator> (const uintwide_t& other) const -> bool { return (compare(other) == static_cast<std::int_fast8_t>( 1)); }
    constexpr auto operator!=(const uintwide_t& other) const -> bool { return (compare(other) != static_cast<std::int_fast8_t>( 0)); }
    constexpr auto operator<=(const uintwide_t& other) const -> bool { return (compare(other) <= static_cast<std::int_fast8_t>( 0)); }
    constexpr auto operator>=(const uintwide_t& other) const -> bool { return (compare(other) >= static_cast<std::int_fast8_t>( 0)); }

    // Helper functions for supporting std::numeric_limits<>.
    static constexpr auto limits_helper_max(bool is_signed) -> uintwide_t
    {
      return
      (!is_signed)
        ? from_rep
          (
            representation_type
            (
              number_of_limbs, (std::numeric_limits<limb_type>::max)()
            )
          )
        :   from_rep
            (
              representation_type
              (
                number_of_limbs, (std::numeric_limits<limb_type>::max)() // LCOV_EXCL_LINE
              )
            )
          ^
            (
                 uintwide_t(static_cast<std::uint8_t>(UINT8_C(1)))
              << static_cast<std::uint32_t>(my_width2 - static_cast<std::uint8_t>(UINT8_C(1)))
            )
        ;
    }

    static constexpr auto limits_helper_min(bool is_signed) -> uintwide_t
    {
      return
      (!is_signed)
        ? from_rep
          (
            representation_type
            (
              number_of_limbs, static_cast<limb_type>(UINT8_C(0))
            )
          )
        :   from_rep
            (
              representation_type
              (
                number_of_limbs, static_cast<limb_type>(UINT8_C(0)) // LCOV_EXCL_LINE
              )
            )
          |
            (
                 uintwide_t(static_cast<std::uint8_t>(UINT8_C(1)))
              << static_cast<std::uint32_t>(my_width2 - static_cast<std::uint8_t>(UINT8_C(1)))
            )
        ;
    }

    static constexpr auto limits_helper_lowest(bool is_signed) -> uintwide_t
    {
      return
      (!is_signed)
        ? from_rep
          (
            representation_type
            (
              number_of_limbs, static_cast<limb_type>(UINT8_C(0))
            )
          )
        :   from_rep
            (
              representation_type
              (
                number_of_limbs, static_cast<limb_type>(UINT8_C(0))
              )
            )
          |
            (
                 uintwide_t(static_cast<std::uint8_t>(UINT8_C(1)))
              << static_cast<std::uint32_t>(my_width2 - static_cast<std::uint8_t>(UINT8_C(1)))
            )
        ;
    }

    // Write string function.
    template<typename OutputStrIterator>
    WIDE_INTEGER_CONSTEXPR auto wr_string(      OutputStrIterator  str_result, // NOLINT(readability-function-cognitive-complexity)
                                          const std::uint_fast8_t  base_rep      = static_cast<std::uint_fast8_t>(UINT8_C(0x10)),
                                          const bool               show_base     = true,
                                          const bool               show_pos      = false,
                                          const bool               is_uppercase  = true,
                                                unsigned_fast_type field_width   = static_cast<unsigned_fast_type>(UINT8_C(0)),
                                          const char               fill_char_str = '0') const -> bool
    {
      auto wr_string_is_ok = true;

      if(base_rep == static_cast<std::uint_fast8_t>(UINT8_C(8)))
      {
        uintwide_t t(*this);

        const auto mask = static_cast<limb_type>(static_cast<std::uint8_t>(0x7U));

        using string_storage_oct_type =
          std::conditional_t
            <my_width2 <= static_cast<size_t>(UINT32_C(2048)),
             detail::fixed_static_array <char,
                                         wr_string_max_buffer_size_oct()>,
             detail::fixed_dynamic_array<char,
                                         wr_string_max_buffer_size_oct(),
                                         typename std::allocator_traits<std::conditional_t<std::is_same<AllocatorType, void>::value,
                                                                                           std::allocator<void>,
                                                                                           AllocatorType>>::template rebind_alloc<limb_type>>>;

        string_storage_oct_type str_temp; // LCOV_EXCL_LINE

        auto pos = // LCOV_EXCL_LINE
          static_cast<unsigned_fast_type>
          (
            str_temp.size() - static_cast<size_t>(UINT8_C(1)) // LCOV_EXCL_LINE
          );

        if(t.is_zero())
        {
          str_temp[static_cast<typename string_storage_oct_type::size_type>(--pos)] = '0';
        }
        else
        {
          if(!is_neg(t))
          {
            while(!t.is_zero()) // NOLINT(altera-id-dependent-backward-branch)
            {
              auto c = static_cast<char>(*t.values.cbegin() & mask);

              if(c <= static_cast<char>(INT8_C(8))) { c = static_cast<char>(c + static_cast<char>(INT8_C(0x30))); }

              str_temp[static_cast<typename string_storage_oct_type::size_type>(--pos)] = c;

              t >>= static_cast<unsigned>(UINT8_C(3));
            }
          }
          else
          {
            uintwide_t<my_width2, limb_type, AllocatorType, false> tu(t);

            while(!tu.is_zero()) // NOLINT(altera-id-dependent-backward-branch)
            {
              auto c = static_cast<char>(*tu.values.cbegin() & mask);

              if(c <= static_cast<char>(INT8_C(8))) { c = static_cast<char>(c + static_cast<char>(INT8_C(0x30))); }

              str_temp[static_cast<typename string_storage_oct_type::size_type>(--pos)] = c;

              tu >>= static_cast<unsigned>(UINT8_C(3));
            }
          }
        }

        if(show_base)
        {
          str_temp[static_cast<typename string_storage_oct_type::size_type>(--pos)] = '0';
        }

        if(show_pos)
        {
          str_temp[static_cast<typename string_storage_oct_type::size_type>(--pos)] = '+';
        }

        if(field_width != static_cast<unsigned_fast_type>(UINT8_C(0)))
        {
          field_width = (std::min)(field_width, static_cast<unsigned_fast_type>(str_temp.size() - static_cast<size_t>(UINT8_C(1))));

          while(static_cast<signed_fast_type>(pos) > static_cast<signed_fast_type>((str_temp.size() - static_cast<size_t>(UINT8_C(1))) - field_width)) // NOLINT(altera-id-dependent-backward-branch)
          {
            str_temp[static_cast<typename string_storage_oct_type::size_type>(--pos)] = fill_char_str;
          }
        }

        str_temp[static_cast<typename string_storage_oct_type::size_type>(str_temp.size() - static_cast<size_t>(UINT8_C(1)))] = '\0';

        detail::strcpy_unsafe(str_result, str_temp.data() + pos);
      }
      else if(base_rep == static_cast<std::uint_fast8_t>(UINT8_C(10)))
      {
        uintwide_t t(*this);

        const auto str_has_neg_sign = is_neg(t);

        if(str_has_neg_sign)
        {
          t.negate();
        }

        using string_storage_dec_type =
          std::conditional_t
            <my_width2 <= static_cast<size_t>(UINT32_C(2048)),
             detail::fixed_static_array <char,
                                         wr_string_max_buffer_size_dec()>,
             detail::fixed_dynamic_array<char,
                                         wr_string_max_buffer_size_dec(),
                                         typename std::allocator_traits<std::conditional_t<std::is_same<AllocatorType, void>::value,
                                                                                           std::allocator<void>,
                                                                                           AllocatorType>>::template rebind_alloc<limb_type>>>;

        string_storage_dec_type str_temp;

        auto pos = static_cast<unsigned_fast_type>(str_temp.size() - static_cast<size_t>(UINT8_C(1)));

        if(t.is_zero())
        {
          str_temp[static_cast<typename string_storage_dec_type::size_type>(--pos)] = '0';
        }
        else
        {
          while(!t.is_zero())
          {
            const uintwide_t tmp(t);

            t.eval_divide_by_single_limb(static_cast<limb_type>(UINT8_C(10)), 0U, nullptr);

            str_temp[static_cast<typename string_storage_dec_type::size_type>(--pos)] =
              static_cast<char>
              (
                  static_cast<limb_type>
                  (
                    tmp - (uintwide_t(t).mul_by_limb(static_cast<limb_type>(UINT8_C(10))))
                  )
                + static_cast<limb_type>(UINT8_C(0x30))
              );
          }
        }

        if(show_pos && (!str_has_neg_sign))
        {
          str_temp[static_cast<typename string_storage_dec_type::size_type>(--pos)] = '+';
        }
        else if(str_has_neg_sign)
        {
          str_temp[static_cast<typename string_storage_dec_type::size_type>(--pos)] = '-';
        }

        if(field_width != static_cast<unsigned_fast_type>(UINT8_C(0)))
        {
          field_width = (std::min)(field_width, static_cast<unsigned_fast_type>(str_temp.size() - static_cast<size_t>(UINT8_C(1))));

          while(static_cast<signed_fast_type>(pos) > static_cast<signed_fast_type>((str_temp.size() - static_cast<size_t>(UINT8_C(1))) - field_width)) // NOLINT(altera-id-dependent-backward-branch)
          {
            str_temp[static_cast<typename string_storage_dec_type::size_type>(--pos)] = fill_char_str;
          }
        }

        str_temp[static_cast<typename string_storage_dec_type::size_type>(str_temp.size() - static_cast<size_t>(UINT8_C(1)))] = '\0';

        detail::strcpy_unsafe(str_result, str_temp.data() + pos);
      }
      else if(base_rep == static_cast<std::uint_fast8_t>(UINT8_C(16)))
      {
        uintwide_t<my_width2, limb_type, AllocatorType, false> t(*this);

        using string_storage_hex_type =
          std::conditional_t
            <my_width2 <= static_cast<size_t>(UINT32_C(2048)),
             detail::fixed_static_array <char,
                                         wr_string_max_buffer_size_hex()>,
             detail::fixed_dynamic_array<char,
                                         wr_string_max_buffer_size_hex(),
                                         typename std::allocator_traits<std::conditional_t<std::is_same<AllocatorType, void>::value,
                                                                                           std::allocator<void>,
                                                                                           AllocatorType>>::template rebind_alloc<limb_type>>>;

        string_storage_hex_type str_temp;

        auto pos = static_cast<unsigned_fast_type>(str_temp.size() - static_cast<size_t>(UINT8_C(1)));

        if(t.is_zero())
        {
          str_temp[static_cast<typename string_storage_hex_type::size_type>(--pos)] = '0';
        }
        else
        {
          const auto dst =
            extract_hex_digits<false>
            (
              t,
              &str_temp[static_cast<typename string_storage_hex_type::size_type>(pos)],
              is_uppercase
            );

          pos -= dst;
        }

        if(show_base)
        {
          str_temp[static_cast<typename string_storage_hex_type::size_type>(--pos)] = (is_uppercase ? 'X' : 'x');

          str_temp[static_cast<typename string_storage_hex_type::size_type>(--pos)] = '0';
        }

        if(show_pos)
        {
          str_temp[static_cast<typename string_storage_hex_type::size_type>(--pos)] = '+';
        }

        if(field_width != static_cast<unsigned_fast_type>(UINT8_C(0)))
        {
          field_width = (std::min)(field_width, static_cast<unsigned_fast_type>(str_temp.size() - static_cast<size_t>(UINT8_C(1))));

          while(static_cast<signed_fast_type>(pos) > static_cast<signed_fast_type>((str_temp.size() - static_cast<size_t>(UINT8_C(1))) - field_width)) // NOLINT(altera-id-dependent-backward-branch)
          {
            str_temp[static_cast<typename string_storage_hex_type::size_type>(--pos)] = fill_char_str;
          }
        }

        str_temp[static_cast<typename string_storage_hex_type::size_type>(str_temp.size() - static_cast<size_t>(UINT8_C(1)))] = '\0';

        detail::strcpy_unsafe(str_result, str_temp.data() + pos);
      }
      else
      {
        wr_string_is_ok = false;
      }

      return wr_string_is_ok;
    }

    template<const bool RePhraseIsSigned = IsSigned,
             std::enable_if_t<(!RePhraseIsSigned)> const* = nullptr>
    WIDE_INTEGER_NODISCARD WIDE_INTEGER_CONSTEXPR auto compare(const uintwide_t<Width2, LimbType, AllocatorType, RePhraseIsSigned>& other) const -> std::int_fast8_t
    {
      return compare_ranges(values.cbegin(),
                            other.values.cbegin(),
                            uintwide_t<Width2, LimbType, AllocatorType, RePhraseIsSigned>::number_of_limbs);
    }

    template<const bool RePhraseIsSigned = IsSigned,
             std::enable_if_t<RePhraseIsSigned> const* = nullptr>
    WIDE_INTEGER_NODISCARD WIDE_INTEGER_CONSTEXPR auto compare(const uintwide_t<Width2, LimbType, AllocatorType, RePhraseIsSigned>& other) const -> std::int_fast8_t
    {
      auto n_result = std::int_fast8_t { };

      const auto other_is_neg = is_neg(other);
      const auto my_is_neg    = is_neg(*this);

      if(my_is_neg && (!other_is_neg))
      {
        n_result = static_cast<std::int_fast8_t>(INT8_C(-1));
      }
      else if((!my_is_neg) && other_is_neg)
      {
        n_result = static_cast<std::int_fast8_t>(INT8_C(1));
      }
      else
      {
        n_result = compare_ranges(values.cbegin(),
                                  other.values.cbegin(),
                                  uintwide_t<Width2, LimbType, AllocatorType, RePhraseIsSigned>::number_of_limbs);
      }

      return n_result;
    }

    // What seems to be an optimization or parsing error prevents
    // getting any LCOV hits in the negate() subroutine, even though
    // this is known to be used in the coverage tests.

    // LCOV_EXCL_START
    WIDE_INTEGER_CONSTEXPR auto negate() -> void
    {
      bitwise_not();

      preincrement();
    }
    // LCOV_EXCL_STOP

    WIDE_INTEGER_CONSTEXPR auto eval_divide_by_single_limb(const limb_type          short_denominator,
                                                           const unsigned_fast_type u_offset,
                                                                 uintwide_t*        remainder) -> void
    {
      // The denominator has one single limb.
      // Use a one-dimensional division algorithm.

      auto long_numerator = double_limb_type { };
      auto hi_part        = static_cast<limb_type>(UINT8_C(0));

      {
        auto ri =
          static_cast<typename representation_type::reverse_iterator>
          (
            detail::advance_and_point
            (
              values.begin(),
              static_cast<size_t>(number_of_limbs - static_cast<size_t>(u_offset))
            )
          );

        for( ; ri != values.rend(); ++ri) // NOLINT(altera-id-dependent-backward-branch)
        {
          long_numerator =
            static_cast<double_limb_type>
            (
               *ri
             + static_cast<double_limb_type>
               (
                    static_cast<double_limb_type>
                    (
                        long_numerator
                      - static_cast<double_limb_type>(static_cast<double_limb_type>(short_denominator) * hi_part)
                    )
                 << static_cast<unsigned>(std::numeric_limits<limb_type>::digits)
               )
            );

          *ri = detail::make_lo<limb_type>(static_cast<double_limb_type>(long_numerator / short_denominator));

          hi_part = *ri;
        }
      }

      if(remainder != nullptr)
      {
        long_numerator =
          static_cast<double_limb_type>
          (
             static_cast<double_limb_type>(*values.cbegin())
           + static_cast<double_limb_type>(static_cast<double_limb_type>(long_numerator - static_cast<double_limb_type>(static_cast<double_limb_type>(short_denominator) * hi_part)) << static_cast<unsigned>(std::numeric_limits<limb_type>::digits))
          );

        *remainder = static_cast<limb_type>(long_numerator >> static_cast<unsigned>(std::numeric_limits<limb_type>::digits));
      }
    }

    WIDE_INTEGER_NODISCARD WIDE_INTEGER_CONSTEXPR auto is_zero() const -> bool
    {
      auto it = values.cbegin(); // NOLINT(llvm-qualified-auto,readability-qualified-auto)

      while((it != values.cend()) && (*it == static_cast<limb_type>(UINT8_C(0)))) // NOLINT(altera-id-dependent-backward-branch)
      {
        ++it;
      }

      return (it == values.cend());
    }

    template<const bool RePhraseIsSigned = IsSigned,
             std::enable_if_t<(!RePhraseIsSigned)> const* = nullptr>
    static constexpr auto is_neg(const uintwide_t<Width2, LimbType, AllocatorType, RePhraseIsSigned>&) -> bool // NOLINT(hicpp-named-parameter,readability-named-parameter)
    {
      return false;
    }

    template<const bool RePhraseIsSigned = IsSigned,
             std::enable_if_t<RePhraseIsSigned> const* = nullptr>
    static constexpr auto is_neg(const uintwide_t<Width2, LimbType, AllocatorType, RePhraseIsSigned>& a) -> bool
    {
      return (static_cast<std::uint_fast8_t>(static_cast<std::uint_fast8_t>(a.values.back() >> static_cast<size_t>(std::numeric_limits<typename uintwide_t<Width2, LimbType, AllocatorType, RePhraseIsSigned>::limb_type>::digits - 1)) & 1U) != 0U);
    }

    static WIDE_INTEGER_CONSTEXPR auto from_rep(const representation_type& other_rep) -> uintwide_t
    {
      uintwide_t result { };

      // Create a factory-like object from another (possibly different)
      // internal data representation.
      if(number_of_limbs == other_rep.size())
      {
        result = uintwide_t(other_rep);
      }
      else
      {
        // In the from_rep() function it is actually possible to have
        // a source representation that differs in size from the destination
        // representation. This can happen when using non-standard container
        // representations for the uintwide_t storage (such as std::vector).
        // In this case, scale to the size of the destination.

        constexpr auto local_number_of_limbs =
          static_cast<size_t>
          (
            Width2 / static_cast<size_t>(std::numeric_limits<limb_type>::digits)
          );

        representation_type my_rep(local_number_of_limbs, static_cast<limb_type>(UINT8_C(0)));

        detail::copy_unsafe(other_rep.cbegin(),
                            detail::advance_and_point(other_rep.cbegin(), (std::min)(local_number_of_limbs, static_cast<size_t>(other_rep.size()))),
                            my_rep.begin());

        result = uintwide_t(static_cast<representation_type&&>(my_rep));
      }

      return result;
    }

    static WIDE_INTEGER_CONSTEXPR auto from_rep(representation_type&& other_rep) noexcept -> uintwide_t
    {
      uintwide_t result { };

      // Create a factory-like object from another (possibly different)
      // internal data representation (via move semantics).
      if(number_of_limbs == other_rep.size())
      {
        result = uintwide_t(static_cast<representation_type&&>(other_rep));
      }
      else
      {
        // In the from_rep() function it is actually possible to have
        // a source representation that differs in size from the destination
        // representation. This can happen when using non-standard container
        // representations for the uintwide_t storage (such as std::vector).
        // In this case, scale to the size of the destination.

        // LCOV_EXCL_START
        constexpr auto local_number_of_limbs =
          static_cast<size_t>
          (
            Width2 / static_cast<size_t>(std::numeric_limits<limb_type>::digits)
          );

        representation_type my_rep(local_number_of_limbs, static_cast<limb_type>(UINT8_C(0)));

        detail::copy_unsafe(other_rep.cbegin(),
                            detail::advance_and_point(other_rep.cbegin(), (std::min)(local_number_of_limbs, static_cast<size_t>(other_rep.size()))),
                            my_rep.begin());

        result = uintwide_t(static_cast<representation_type&&>(my_rep));
        // LCOV_EXCL_STOP
      }

      return result;
    }

    static constexpr auto my_fill_char() -> char { return '.'; }

    static constexpr auto is_not_fill_char(char c) -> bool { return (c != my_fill_char()); }

    // Define the maximum buffer sizes for extracting
    // octal, decimal and hexadecimal string representations.
    static constexpr auto wr_string_max_buffer_size_oct() -> size_t
    {
      return
        static_cast<size_t>
        (
            static_cast<size_t>(UINT8_C(8))
          + static_cast<size_t>
            (
              (static_cast<size_t>(my_width2 % static_cast<size_t>(UINT8_C(3))) != static_cast<size_t>(UINT8_C(0)))
                ? static_cast<size_t>(UINT8_C(1))
                : static_cast<size_t>(UINT8_C(0))
            )
          + static_cast<size_t>(my_width2 / static_cast<size_t>(UINT8_C(3)))
        );
    }

    static constexpr auto wr_string_max_buffer_size_hex() -> size_t
    {
      return
        static_cast<size_t>
        (
            static_cast<size_t>(UINT8_C(8))
          + static_cast<size_t>
            (
              (static_cast<size_t>(my_width2 % static_cast<size_t>(UINT8_C(4))) != static_cast<size_t>(UINT8_C(0)))
                ? static_cast<size_t>(UINT8_C(1))
                : static_cast<size_t>(UINT8_C(0))
            )
          + static_cast<size_t>(my_width2 / static_cast<size_t>(UINT8_C(4)))
        );
    }

    static constexpr auto wr_string_max_buffer_size_dec() -> size_t
    {
      return
        static_cast<size_t>
        (
            static_cast<size_t>(UINT8_C(10))
          + static_cast<size_t>
            (
                static_cast<std::uintmax_t>(static_cast<std::uintmax_t>(my_width2) * static_cast<std::uintmax_t>(UINTMAX_C(301)))
              / static_cast<std::uintmax_t>(UINTMAX_C(1000))
            )
        );
    }

  #if !defined(WIDE_INTEGER_DISABLE_PRIVATE_CLASS_DATA_MEMBERS)
  private:
  #endif
    representation_type
      values // NOLINT(readability-identifier-naming)
      {
        static_cast<typename representation_type::size_type>(number_of_limbs),
        static_cast<typename representation_type::value_type>(UINT8_C(0)),
        typename representation_type::allocator_type()
      };

  #if defined(WIDE_INTEGER_DISABLE_PRIVATE_CLASS_DATA_MEMBERS)
  private:
  #endif
    friend auto ::test_uintwide_t_edge::test_various_isolated_edge_cases() -> bool;

    template<const size_t OtherWidth2,
             typename OtherLimbType,
             typename OtherAllocatorType,
             const bool OtherIsSignedLeft,
             const bool OtherIsSignedRight,
             std::enable_if_t<((!OtherIsSignedLeft) && (!OtherIsSignedRight))> const*>
    friend WIDE_INTEGER_CONSTEXPR auto divmod(const uintwide_t<OtherWidth2, OtherLimbType, OtherAllocatorType, OtherIsSignedLeft >& a, // NOLINT(readability-redundant-declaration)
                                              const uintwide_t<OtherWidth2, OtherLimbType, OtherAllocatorType, OtherIsSignedRight>& b) -> std::pair<uintwide_t<OtherWidth2, OtherLimbType, OtherAllocatorType, OtherIsSignedLeft>, uintwide_t<OtherWidth2, OtherLimbType, OtherAllocatorType, OtherIsSignedRight>>;

    template<const size_t OtherWidth2,
             typename OtherLimbType,
             typename OtherAllocatorType,
             const bool OtherIsSignedLeft,
             const bool OtherIsSignedRight,
             std::enable_if_t<(OtherIsSignedLeft || OtherIsSignedRight)> const*>
    friend WIDE_INTEGER_CONSTEXPR auto divmod(const uintwide_t<OtherWidth2, OtherLimbType, OtherAllocatorType, OtherIsSignedLeft >& a, // NOLINT(readability-redundant-declaration)
                                              const uintwide_t<OtherWidth2, OtherLimbType, OtherAllocatorType, OtherIsSignedRight>& b) -> std::pair<uintwide_t<OtherWidth2, OtherLimbType, OtherAllocatorType, OtherIsSignedLeft>, uintwide_t<OtherWidth2, OtherLimbType, OtherAllocatorType, OtherIsSignedRight>>;

    explicit constexpr uintwide_t(const representation_type& other_rep)
      : values(static_cast<const representation_type&>(other_rep)) { }

    explicit constexpr uintwide_t(representation_type&& other_rep)
      : values(static_cast<representation_type&&>(other_rep)) { }

    template<const bool RePhraseIsSigned,
             std::enable_if_t<(!RePhraseIsSigned)> const* = nullptr>
    static WIDE_INTEGER_CONSTEXPR auto extract_hex_digits(uintwide_t<Width2, LimbType, AllocatorType, RePhraseIsSigned>& tu,
                                                          char* pstr,
                                                          const bool is_uppercase) -> unsigned_fast_type
    {
      constexpr auto mask = static_cast<limb_type>(UINT8_C(0xF));

      auto dst = static_cast<unsigned_fast_type>(UINT8_C(0));

      while(!tu.is_zero()) // NOLINT(altera-id-dependent-backward-branch)
      {
        auto c = static_cast<char>(*tu.values.cbegin() & mask);

        if      (c <= static_cast<char>(INT8_C(  9)))                                           { c = static_cast<char>(c + static_cast<char>(INT8_C(0x30))); }
        else if((c >= static_cast<char>(INT8_C(0xA))) && (c <= static_cast<char>(INT8_C(0xF)))) { c = static_cast<char>(c + (is_uppercase ? static_cast<char>(INT8_C(55)) : static_cast<char>(INT8_C(87)))); }

        *(--pstr) = c; // NOLINT(cppcoreguidelines-pro-bounds-pointer-arithmetic)

        ++dst;

        tu >>= static_cast<unsigned>(UINT8_C(4));
      }

      return dst;
    }

    template<typename InputIteratorLeftType,
             typename InputIteratorRightType>
    static WIDE_INTEGER_CONSTEXPR auto compare_ranges(      InputIteratorLeftType  a,
                                                            InputIteratorRightType b,
                                                      const unsigned_fast_type     count) -> std::int_fast8_t
    {
      auto n_return = static_cast<std::int_fast8_t>(INT8_C(0));

      std::reverse_iterator<InputIteratorLeftType>  pa(detail::advance_and_point(a, count));
      std::reverse_iterator<InputIteratorRightType> pb(detail::advance_and_point(b, count));

      while(pa != std::reverse_iterator<InputIteratorLeftType>(a)) // NOLINT(altera-id-dependent-backward-branch)
      {
        using value_left_type = typename std::iterator_traits<InputIteratorLeftType>::value_type;

        const auto value_a = *pa++;
        const auto value_b = static_cast<value_left_type>(*pb++);

        if(value_a != value_b)
        {
          n_return =
            static_cast<std::int_fast8_t>
            (
              (value_a > value_b)
                ? static_cast<std::int_fast8_t>(INT8_C(1))
                : static_cast<std::int_fast8_t>(INT8_C(-1))
            );

          break;
        }
      }

      return n_return;
    }

    template<typename UnknownBuiltInIntegralType>
    struct digits_ratio
    {
      using local_unknown_builtin_integral_type  = UnknownBuiltInIntegralType;

      using local_unsigned_conversion_type =
        typename detail::uint_type_helper<
          std::numeric_limits<local_unknown_builtin_integral_type>::is_signed
            ? static_cast<size_t>(std::numeric_limits<local_unknown_builtin_integral_type>::digits + 1)
            : static_cast<size_t>(std::numeric_limits<local_unknown_builtin_integral_type>::digits + 0)>::exact_unsigned_type;

      static constexpr unsigned_fast_type value =
        static_cast<unsigned_fast_type>(  std::numeric_limits<local_unsigned_conversion_type>::digits
                                        / std::numeric_limits<limb_type>::digits);

      template<typename InputIteratorLeft>
      static WIDE_INTEGER_CONSTEXPR auto extract(InputIteratorLeft p_limb, unsigned_fast_type limb_count) -> local_unknown_builtin_integral_type
      {
        using local_limb_type      = typename std::iterator_traits<InputIteratorLeft>::value_type;
        using left_difference_type = typename std::iterator_traits<InputIteratorLeft>::difference_type;

        auto u = static_cast<local_unsigned_conversion_type>(UINT8_C(0));

        constexpr auto shift_lim =
          static_cast<unsigned_fast_type>
          (
            std::numeric_limits<local_unsigned_conversion_type>::digits
          );

        for(auto   i = static_cast<unsigned_fast_type>(UINT8_C(0));
                   (    // NOLINT(altera-id-dependent-backward-branch)
                        (i < limb_count)
                     && (static_cast<unsigned_fast_type>(static_cast<unsigned_fast_type>(std::numeric_limits<local_limb_type>::digits) * i) < shift_lim)
                   );
                 ++i)
        {
          u =
            static_cast<local_unsigned_conversion_type>
            (
                u
              | static_cast<local_unsigned_conversion_type>(static_cast<local_unsigned_conversion_type>(*detail::advance_and_point(p_limb, static_cast<left_difference_type>(i))) << static_cast<unsigned_fast_type>(static_cast<unsigned_fast_type>(std::numeric_limits<local_limb_type>::digits) * i))
            );
        }

        return static_cast<local_unknown_builtin_integral_type>(u);
      }
    };

    // Implement a function that extracts any built-in signed or unsigned integral type.
    template<typename UnknownBuiltInIntegralType,
             typename = std::enable_if_t<std::is_integral<UnknownBuiltInIntegralType>::value>>
    WIDE_INTEGER_NODISCARD WIDE_INTEGER_CONSTEXPR auto extract_builtin_integral_type() const -> UnknownBuiltInIntegralType
    {
      using local_unknown_integral_type = UnknownBuiltInIntegralType;
      using digits_ratio_type           = digits_ratio<local_unknown_integral_type>;

      const auto ilim = (std::min)(static_cast<unsigned_fast_type>(digits_ratio_type::value),
                                   static_cast<unsigned_fast_type>(values.size()));

      // Handle cases for which the input parameter is less wide
      // or equally as wide as the limb width or wider than the limb width.
      return ((digits_ratio_type::value < static_cast<unsigned_fast_type>(UINT8_C(2)))
               ? static_cast<local_unknown_integral_type>(*values.cbegin())
               : digits_ratio_type::extract(values.cbegin(), ilim));
    }

    #if !defined(WIDE_INTEGER_DISABLE_FLOAT_INTEROP)
    // Implement a function that extracts any built-in floating-point type.
    template<typename FloatingPointType,
             typename = std::enable_if_t<std::is_floating_point<FloatingPointType>::value>>
    WIDE_INTEGER_NODISCARD WIDE_INTEGER_CONSTEXPR auto extract_builtin_floating_point_type() const -> FloatingPointType
    {
      using local_unsigned_wide_integer_type = uintwide_t<Width2, limb_type, AllocatorType, false>;
      using local_builtin_float_type         = FloatingPointType;

      const auto u_is_neg = is_neg(*this);

      const local_unsigned_wide_integer_type u((!u_is_neg) ? *this : -*this);

      const auto my_msb = static_cast<size_t>(msb(u));
      const auto ilim   = static_cast<size_t>
                          (
                             static_cast<size_t>(                     static_cast<size_t>(my_msb + static_cast<size_t>(1U)) / static_cast<size_t>(std::numeric_limits<limb_type>::digits))
                           + static_cast<size_t>((static_cast<size_t>(static_cast<size_t>(my_msb + static_cast<size_t>(1U)) % static_cast<size_t>(std::numeric_limits<limb_type>::digits)) != static_cast<size_t>(UINT8_C(0))) ? static_cast<size_t>(1U) : static_cast<size_t>(UINT8_C(0)))
                          );

      auto a = static_cast<local_builtin_float_type>(0.0F);

      constexpr auto one_ldbl = static_cast<long double>(1.0L);

      auto ldexp_runner = one_ldbl;

      auto ui = detail::advance_and_point(u.values.cbegin(), static_cast<size_t>(UINT8_C(0))); // NOLINT(llvm-qualified-auto,readability-qualified-auto)

      for(auto i = static_cast<size_t>(UINT8_C(0)); i < ilim; ++i) // NOLINT(altera-id-dependent-backward-branch)
      {
        auto ld      = static_cast<long double>(0.0L);
        auto lm_mask = static_cast<limb_type>(UINT8_C(1));

        for(auto   j = static_cast<size_t>(UINT8_C(0));
                   j < static_cast<size_t>(std::numeric_limits<limb_type>::digits);
                 ++j)
        {
          if(static_cast<limb_type>(*ui & lm_mask) != static_cast<limb_type>(UINT8_C(0)))
          {
            ld = static_cast<long double>(ld + ldexp_runner);
          }

          constexpr auto two_ldbl = static_cast<long double>(2.0L);

          lm_mask      = static_cast<limb_type>  (lm_mask << static_cast<unsigned>(UINT8_C(1)));
          ldexp_runner = static_cast<long double>(ldexp_runner * two_ldbl);
        }

        a += static_cast<local_builtin_float_type>(ld);

        ++ui;
      }

      return static_cast<local_builtin_float_type>((!u_is_neg) ? a : static_cast<local_builtin_float_type>(-a));
    }
    #endif

    template<const size_t OtherWidth2>
    static WIDE_INTEGER_CONSTEXPR auto eval_mul_unary(      uintwide_t<OtherWidth2, LimbType, AllocatorType, IsSigned>& u,
                                                      const uintwide_t<OtherWidth2, LimbType, AllocatorType, IsSigned>& v,
                                                      std::enable_if_t<((OtherWidth2 / std::numeric_limits<LimbType>::digits) < number_of_limbs_karatsuba_threshold)>* p_nullparam = nullptr) -> void
    {
      static_cast<void>(p_nullparam == nullptr);

      // Unary multiplication function using schoolbook multiplication,
      // but we only need to retain the low half of the n*n algorithm.
      // In other words, this is an n*n->n bit multiplication.
      using local_other_wide_integer_type = uintwide_t<OtherWidth2, LimbType, AllocatorType, IsSigned>;

      const auto local_other_number_of_limbs = local_other_wide_integer_type::number_of_limbs;

      using local_other_representation_type = typename local_other_wide_integer_type::representation_type;

      local_other_representation_type result
      {
        static_cast<typename representation_type::size_type>(local_other_number_of_limbs),
        static_cast<typename representation_type::value_type>(UINT8_C(0)),
        typename representation_type::allocator_type()
      };

      eval_multiply_n_by_n_to_lo_part(result.begin(),
                                      u.values.cbegin(),
                                      v.values.cbegin(),
                                      local_other_number_of_limbs);

      detail::copy_unsafe(result.cbegin(),
                          detail::advance_and_point(result.cbegin(), local_other_number_of_limbs),
                          u.values.begin());
    }

    template<const size_t OtherWidth2>
    static WIDE_INTEGER_CONSTEXPR auto eval_mul_unary(      uintwide_t<OtherWidth2, LimbType, AllocatorType, IsSigned>& u,
                                                      const uintwide_t<OtherWidth2, LimbType, AllocatorType, IsSigned>& v,
                                                      std::enable_if_t<((OtherWidth2 / std::numeric_limits<LimbType>::digits) >= number_of_limbs_karatsuba_threshold)>* p_nullparam = nullptr) -> void
    {
      static_cast<void>(p_nullparam == nullptr);

      // Unary multiplication function using Karatsuba multiplication.

      constexpr auto local_number_of_limbs = uintwide_t<OtherWidth2, LimbType, AllocatorType, IsSigned>::number_of_limbs;

      // TBD: Can use specialized allocator or memory pool for these arrays.
      // Good examples for this (both threaded as well as non-threaded)
      // can be found in the wide_decimal project.
      using result_array_type =
        std::conditional_t<std::is_same<AllocatorType, void>::value,
                           detail::fixed_static_array <limb_type,
                                                       static_cast<size_t>(number_of_limbs * static_cast<size_t>(UINT8_C(2)))>,
                           detail::fixed_dynamic_array<limb_type,
                                                       static_cast<size_t>(number_of_limbs * static_cast<size_t>(UINT8_C(2))),
                                                       typename std::allocator_traits<std::conditional_t<std::is_same<AllocatorType, void>::value,
                                                                                                         std::allocator<void>,
                                                                                                         AllocatorType>>::template rebind_alloc<limb_type>>>;

      using storage_array_type =
        std::conditional_t<std::is_same<AllocatorType, void>::value,
                           detail::fixed_static_array <limb_type,
                                                       static_cast<size_t>(number_of_limbs * static_cast<size_t>(UINT8_C(4)))>,
                           detail::fixed_dynamic_array<limb_type,
                                                       static_cast<size_t>(number_of_limbs * static_cast<size_t>(UINT8_C(4))),
                                                       typename std::allocator_traits<std::conditional_t<std::is_same<AllocatorType, void>::value,
                                                                                                         std::allocator<void>,
                                                                                                         AllocatorType>>::template rebind_alloc<limb_type>>>;

      result_array_type  result;
      storage_array_type t;

      eval_multiply_kara_n_by_n_to_2n(result.begin(),
                                      u.values.cbegin(),
                                      v.values.cbegin(),
                                      local_number_of_limbs,
                                      t.begin());

      detail::copy_unsafe(result.cbegin(),
                          result.cbegin() + local_number_of_limbs,
                          u.values.begin());
    }

    template<typename ResultIterator,
             typename InputIteratorLeft,
             typename InputIteratorRight>
    static WIDE_INTEGER_CONSTEXPR auto eval_add_n(      ResultIterator     r,
                                                        InputIteratorLeft  u,
                                                        InputIteratorRight v,
                                                  const unsigned_fast_type count,
                                                  const limb_type          carry_in = static_cast<limb_type>(UINT8_C(0))) -> limb_type
    {
      auto carry_out = static_cast<std::uint_fast8_t>(carry_in);

      using local_limb_type = typename std::iterator_traits<ResultIterator>::value_type;

      static_assert
      (
           (std::numeric_limits<local_limb_type>::digits == std::numeric_limits<typename std::iterator_traits<InputIteratorLeft>::value_type>::digits)
        && (std::numeric_limits<local_limb_type>::digits == std::numeric_limits<typename std::iterator_traits<InputIteratorRight>::value_type>::digits),
        "Error: Internals require same widths for left-right-result limb_types at the moment"
      );

      using local_double_limb_type =
        typename detail::uint_type_helper<static_cast<size_t>(std::numeric_limits<local_limb_type>::digits * 2)>::exact_unsigned_type;

      using result_difference_type = typename std::iterator_traits<ResultIterator>::difference_type;

      for(auto i = static_cast<unsigned_fast_type>(UINT8_C(0)); i < count; ++i)
      {
        const auto uv_as_ularge =
          static_cast<local_double_limb_type>
          (
              static_cast<local_double_limb_type>(static_cast<local_double_limb_type>(*u++) + *v++)
            + carry_out
          );

        carry_out = static_cast<std::uint_fast8_t>(detail::make_hi<local_limb_type>(uv_as_ularge));

        *detail::advance_and_point(r, static_cast<result_difference_type>(i)) = static_cast<local_limb_type>(uv_as_ularge);
      }

      return static_cast<limb_type>(carry_out);
    }

    template<typename ResultIterator,
             typename InputIteratorLeft,
             typename InputIteratorRight>
    static WIDE_INTEGER_CONSTEXPR auto eval_subtract_n(      ResultIterator     r,
                                                             InputIteratorLeft  u,
                                                             InputIteratorRight v,
                                                       const unsigned_fast_type count,
                                                       const bool               has_borrow_in = false) -> bool
    {
      auto has_borrow_out =
        static_cast<std::uint_fast8_t>
        (
          has_borrow_in ? static_cast<std::uint_fast8_t>(UINT8_C(1))
                        : static_cast<std::uint_fast8_t>(UINT8_C(0))
        );

      using local_limb_type = typename std::iterator_traits<ResultIterator>::value_type;

      static_assert
      (
           (std::numeric_limits<local_limb_type>::digits == std::numeric_limits<typename std::iterator_traits<InputIteratorLeft>::value_type>::digits)
        && (std::numeric_limits<local_limb_type>::digits == std::numeric_limits<typename std::iterator_traits<InputIteratorRight>::value_type>::digits),
        "Error: Internals require same widths for left-right-result limb_types at the moment"
      );

      using local_double_limb_type =
        typename detail::uint_type_helper<static_cast<size_t>(std::numeric_limits<local_limb_type>::digits * 2)>::exact_unsigned_type;

      using result_difference_type = typename std::iterator_traits<ResultIterator>::difference_type;

      for(auto i = static_cast<unsigned_fast_type>(UINT8_C(0)); i < count; ++i)
      {
        const auto uv_as_ularge =
          static_cast<local_double_limb_type>
          (
              static_cast<local_double_limb_type>(static_cast<local_double_limb_type>(*u++) - *v++)
            - has_borrow_out
          );

        has_borrow_out =
          static_cast<std::uint_fast8_t>
          (
            (detail::make_hi<local_limb_type>(uv_as_ularge) != static_cast<local_limb_type>(UINT8_C(0)))
              ? static_cast<std::uint_fast8_t>(UINT8_C(1))
              : static_cast<std::uint_fast8_t>(UINT8_C(0))
          );

        *detail::advance_and_point(r, static_cast<result_difference_type>(i)) = static_cast<local_limb_type>(uv_as_ularge);
      }

      return (has_borrow_out != static_cast<std::uint_fast8_t>(UINT8_C(0)));
    }

    template<typename ResultIterator,
             typename InputIteratorLeft,
             typename InputIteratorRight,
             const size_t RePhraseWidth2 = Width2,
             std::enable_if_t<(uintwide_t<RePhraseWidth2, LimbType, AllocatorType, IsSigned>::number_of_limbs == 4U)> const* = nullptr>
    static WIDE_INTEGER_CONSTEXPR auto eval_multiply_n_by_n_to_lo_part(      ResultIterator     r,
                                                                             InputIteratorLeft  a,
                                                                             InputIteratorRight b,
                                                                       const unsigned_fast_type count) -> void
    {
      static_cast<void>(count);

      using local_limb_type = typename std::iterator_traits<ResultIterator>::value_type;

      static_assert
      (
           (std::numeric_limits<local_limb_type>::digits == std::numeric_limits<typename std::iterator_traits<InputIteratorLeft>::value_type>::digits)
        && (std::numeric_limits<local_limb_type>::digits == std::numeric_limits<typename std::iterator_traits<InputIteratorRight>::value_type>::digits),
        "Error: Internals require same widths for left-right-result limb_types at the moment"
      );

      using local_double_limb_type =
        typename detail::uint_type_helper<static_cast<size_t>(static_cast<int>(std::numeric_limits<local_limb_type>::digits * static_cast<int>(INT8_C(2))))>::exact_unsigned_type;

      using result_difference_type = typename std::iterator_traits<ResultIterator>::difference_type;
      using left_difference_type   = typename std::iterator_traits<InputIteratorLeft>::difference_type;
      using left_value_type        = typename std::iterator_traits<InputIteratorLeft>::value_type;
      using right_difference_type  = typename std::iterator_traits<InputIteratorRight>::difference_type;

      // The algorithm has been derived from the polynomial multiplication.
      // After the multiplication terms of equal order are grouped
      // together and retained up to order(3). The carries from the
      // multiplications are included when adding up the terms.
      // The results of the intermediate multiplications are stored
      // in local variables in memory.

      //   Column[CoefficientList[Expand[(a0 + a1 x + a2 x^2 + a3 x^3) (b0 + b1 x + b2 x^2 + b3 x^3)], x]]
      //   a0b0
      //   a1b0 + a0b1
      //   a2b0 + a1b1 + a0b2
      //   a3b0 + a2b1 + a1b2 + a0b3

      // See also Wolfram Alpha at:
      // https://www.wolframalpha.com/input/?i=Column%5BCoefficientList%5B+++Expand%5B%28a0+%2B+a1+x+%2B+a2+x%5E2+%2B+a3+x%5E3%29+%28b0+%2B+b1+x+%2B+b2+x%5E2+%2B+b3+x%5E3%29%5D%2C++++x%5D%5D
      // ... and take the upper half of the pyramid.

      // Performance improvement:
      //   (old) kops_per_sec: 33173.50
      //   (new) kops_per_sec: 95069.43

      local_double_limb_type r1;
      local_double_limb_type r2;

      const auto a0b0 = static_cast<local_double_limb_type>(*detail::advance_and_point(a, static_cast<left_difference_type>(0)) * static_cast<local_double_limb_type>(*detail::advance_and_point(b, static_cast<right_difference_type>(INT8_C(0)))));
      const auto a0b1 = static_cast<local_double_limb_type>(*detail::advance_and_point(a, static_cast<left_difference_type>(0)) * static_cast<local_double_limb_type>(*detail::advance_and_point(b, static_cast<right_difference_type>(INT8_C(1)))));
      const auto a1b0 = static_cast<local_double_limb_type>(*detail::advance_and_point(a, static_cast<left_difference_type>(1)) * static_cast<local_double_limb_type>(*detail::advance_and_point(b, static_cast<right_difference_type>(INT8_C(0)))));
      const auto a1b1 = static_cast<local_double_limb_type>(*detail::advance_and_point(a, static_cast<left_difference_type>(1)) * static_cast<local_double_limb_type>(*detail::advance_and_point(b, static_cast<right_difference_type>(INT8_C(1)))));

      // One special case is considered, the case of multiplication
      // of the form BITS/2 * BITS/2 = BITS. In this case, the algorithm
      // can be significantly simplified by using only the 'lower-halves'
      // of the data.
      if(    (*detail::advance_and_point(a, static_cast<left_difference_type>(INT8_C(2))) == static_cast<left_value_type>(UINT8_C(0))) && (*detail::advance_and_point(b, static_cast<right_difference_type>(INT8_C(2))) == static_cast<left_value_type>(UINT8_C(0)))
          && (*detail::advance_and_point(a, static_cast<left_difference_type>(INT8_C(3))) == static_cast<left_value_type>(UINT8_C(0))) && (*detail::advance_and_point(b, static_cast<right_difference_type>(INT8_C(3))) == static_cast<left_value_type>(UINT8_C(0))))
      {
        r1    = static_cast<local_double_limb_type>
                (
                  static_cast<local_double_limb_type>
                  (
                    detail::make_hi<local_limb_type>(a0b0) // LCOV_EXCL_LINE
                  )
                  + detail::make_lo<local_limb_type>(a1b0)
                  + detail::make_lo<local_limb_type>(a0b1)
                )
                ;
        r2    = static_cast<local_double_limb_type>
                (
                  static_cast<local_double_limb_type>
                  (
                    detail::make_hi<local_limb_type>(r1) // LCOV_EXCL_LINE
                  )
                  + detail::make_lo<local_limb_type>(a1b1)
                  + detail::make_hi<local_limb_type>(a0b1)
                  + detail::make_hi<local_limb_type>(a1b0)
                )
                ;
        *detail::advance_and_point(r, static_cast<result_difference_type>(INT8_C(3)))
              = static_cast<local_limb_type>
                (
                    detail::make_hi<local_limb_type>(r2)
                  + detail::make_hi<local_limb_type>(a1b1)
                )
                ;
      }
      else
      {
        const auto a0b2 = static_cast<local_double_limb_type>(*detail::advance_and_point(a, static_cast<left_difference_type>(INT8_C(0))) * static_cast<local_double_limb_type>(*detail::advance_and_point(b, static_cast<right_difference_type>(INT8_C(2)))));
        const auto a2b0 = static_cast<local_double_limb_type>(*detail::advance_and_point(a, static_cast<left_difference_type>(INT8_C(2))) * static_cast<local_double_limb_type>(*detail::advance_and_point(b, static_cast<right_difference_type>(INT8_C(0)))));

        r1    = static_cast<local_double_limb_type>
                (
                  static_cast<local_double_limb_type>
                  (
                    detail::make_hi<local_limb_type>(a0b0)
                  )
                  + detail::make_lo<local_limb_type>(a1b0)
                  + detail::make_lo<local_limb_type>(a0b1)
                )
                ;
        r2    = static_cast<local_double_limb_type>
                (
                  static_cast<local_double_limb_type>
                  (
                    detail::make_hi<local_limb_type>(r1)
                  )
                  + detail::make_lo<local_limb_type>(a2b0)
                  + detail::make_lo<local_limb_type>(a1b1)
                  + detail::make_lo<local_limb_type>(a0b2)
                  + detail::make_hi<local_limb_type>(a1b0)
                  + detail::make_hi<local_limb_type>(a0b1)
                )
                ;
        *detail::advance_and_point(r, static_cast<result_difference_type>(3))
              = static_cast<local_limb_type>
                (
                    detail::make_hi<local_limb_type>(r2)
                  + static_cast<local_limb_type>    (*detail::advance_and_point(a, static_cast<left_difference_type>(INT8_C(3))) * static_cast<local_double_limb_type>(*detail::advance_and_point(b, static_cast<right_difference_type>(INT8_C(0)))))
                  + static_cast<local_limb_type>    (*detail::advance_and_point(a, static_cast<left_difference_type>(INT8_C(2))) * static_cast<local_double_limb_type>(*detail::advance_and_point(b, static_cast<right_difference_type>(INT8_C(1)))))
                  + static_cast<local_limb_type>    (*detail::advance_and_point(a, static_cast<left_difference_type>(INT8_C(1))) * static_cast<local_double_limb_type>(*detail::advance_and_point(b, static_cast<right_difference_type>(INT8_C(2)))))
                  + static_cast<local_limb_type>    (*detail::advance_and_point(a, static_cast<left_difference_type>(INT8_C(0))) * static_cast<local_double_limb_type>(*detail::advance_and_point(b, static_cast<right_difference_type>(INT8_C(3)))))
                  + detail::make_hi<local_limb_type>(a2b0)
                  + detail::make_hi<local_limb_type>(a1b1)
                  + detail::make_hi<local_limb_type>(a0b2)
                )
                ;
      }

      *detail::advance_and_point(r, static_cast<result_difference_type>(INT8_C(0))) = static_cast<local_limb_type>(a0b0);
      *detail::advance_and_point(r, static_cast<result_difference_type>(INT8_C(1))) = static_cast<local_limb_type>(r1);
      *detail::advance_and_point(r, static_cast<result_difference_type>(INT8_C(2))) = static_cast<local_limb_type>(r2);
    }

    #if defined(WIDE_INTEGER_HAS_MUL_8_BY_8_UNROLL)
    template<typename ResultIterator,
             typename InputIteratorLeft,
             typename InputIteratorRight,
             const size_t RePhraseWidth2 = Width2,
             std::enable_if_t<(uintwide_t<RePhraseWidth2, LimbType, AllocatorType, IsSigned>::number_of_limbs == static_cast<size_t>(UINT32_C(8)))> const* = nullptr>
    static WIDE_INTEGER_CONSTEXPR auto eval_multiply_n_by_n_to_lo_part(      ResultIterator     r,
                                                                             InputIteratorLeft  a,
                                                                             InputIteratorRight b,
                                                                       const unsigned_fast_type count) -> void
    {
      static_cast<void>(count);

      static_assert
      (
           (std::numeric_limits<typename std::iterator_traits<ResultIterator>::value_type>::digits == std::numeric_limits<typename std::iterator_traits<InputIteratorLeft>::value_type>::digits)
        && (std::numeric_limits<typename std::iterator_traits<ResultIterator>::value_type>::digits == std::numeric_limits<typename std::iterator_traits<InputIteratorRight>::value_type>::digits),
        "Error: Internals require same widths for left-right-result limb_types at the moment"
      );

      using local_limb_type = typename std::iterator_traits<ResultIterator>::value_type;

      using local_double_limb_type =
        typename detail::uint_type_helper<static_cast<size_t>(static_cast<int>(std::numeric_limits<local_limb_type>::digits * static_cast<int>(INT8_C(2))))>::exact_unsigned_type;

      using result_difference_type = typename std::iterator_traits<ResultIterator>::difference_type;
      using left_difference_type   = typename std::iterator_traits<InputIteratorLeft>::difference_type;
      using left_value_type        = typename std::iterator_traits<InputIteratorLeft>::value_type;
      using right_difference_type  = typename std::iterator_traits<InputIteratorRight>::difference_type;

      // The algorithm has been derived from the polynomial multiplication.
      // After the multiplication terms of equal order are grouped
      // together and retained up to order(3). The carries from the
      // multiplications are included when adding up the terms.
      // The results of the intermediate multiplications are stored
      // in local variables in memory.

      //   Column[CoefficientList[Expand[(a0 + a1 x + a2 x^2 + a3 x^3 + a4 x^4 + a5 x^5 + a6 x^6 + a7 x^7) (b0 + b1 x + b2 x^2 + b3 x^3 + b4 x^4 + b5 x^5 + b6 x^6 + b7 x^7)], x]]
      //   a0b0
      //   a1b0 + a0b1
      //   a2b0 + a1b1 + a0b2
      //   a3b0 + a2b1 + a1b2 + a0b3
      //   a4b0 + a3b1 + a2b2 + a1b3 + a0b4
      //   a5b0 + a4b1 + a3b2 + a2b3 + a1b4 + a0b5
      //   a6b0 + a5b1 + a4b2 + a3b3 + a2b4 + a1b5 + a0b6
      //   a7b0 + a6b1 + a5b2 + a4b3 + a3b4 + a2b5 + a1b6 + a0b7

      // See also Wolfram Alpha at:
      // https://www.wolframalpha.com/input/?i=Column%5BCoefficientList%5B+++Expand%5B%28a0+%2B+a1+x+%2B+a2+x%5E2+%2B+a3+x%5E3%29+%28b0+%2B+b1+x+%2B+b2+x%5E2+%2B+b3+x%5E3%29%5D%2C++++x%5D%5D
      // ... and take the upper half of the pyramid.

      const local_double_limb_type a0b0 = *detail::advance_and_point(a, static_cast<left_difference_type>(INT8_C(0))) * static_cast<local_double_limb_type>(*detail::advance_and_point(b, static_cast<right_difference_type>(INT8_C(0))));

      const local_double_limb_type a1b0 = *detail::advance_and_point(a, static_cast<left_difference_type>(INT8_C(1))) * static_cast<local_double_limb_type>(*detail::advance_and_point(b, static_cast<right_difference_type>(INT8_C(0))));
      const local_double_limb_type a0b1 = *detail::advance_and_point(a, static_cast<left_difference_type>(INT8_C(0))) * static_cast<local_double_limb_type>(*detail::advance_and_point(b, static_cast<right_difference_type>(INT8_C(1))));

      const local_double_limb_type a2b0 = *detail::advance_and_point(a, static_cast<left_difference_type>(INT8_C(2))) * static_cast<local_double_limb_type>(*detail::advance_and_point(b, static_cast<right_difference_type>(INT8_C(0))));
      const local_double_limb_type a1b1 = *detail::advance_and_point(a, static_cast<left_difference_type>(INT8_C(1))) * static_cast<local_double_limb_type>(*detail::advance_and_point(b, static_cast<right_difference_type>(INT8_C(1))));
      const local_double_limb_type a0b2 = *detail::advance_and_point(a, static_cast<left_difference_type>(INT8_C(0))) * static_cast<local_double_limb_type>(*detail::advance_and_point(b, static_cast<right_difference_type>(INT8_C(2))));

      const local_double_limb_type a3b0 = *detail::advance_and_point(a, static_cast<left_difference_type>(INT8_C(3))) * static_cast<local_double_limb_type>(*detail::advance_and_point(b, static_cast<right_difference_type>(INT8_C(0))));
      const local_double_limb_type a2b1 = *detail::advance_and_point(a, static_cast<left_difference_type>(INT8_C(2))) * static_cast<local_double_limb_type>(*detail::advance_and_point(b, static_cast<right_difference_type>(INT8_C(1))));
      const local_double_limb_type a1b2 = *detail::advance_and_point(a, static_cast<left_difference_type>(INT8_C(1))) * static_cast<local_double_limb_type>(*detail::advance_and_point(b, static_cast<right_difference_type>(INT8_C(2))));
      const local_double_limb_type a0b3 = *detail::advance_and_point(a, static_cast<left_difference_type>(INT8_C(0))) * static_cast<local_double_limb_type>(*detail::advance_and_point(b, static_cast<right_difference_type>(INT8_C(3))));

      const local_double_limb_type a3b1 = *detail::advance_and_point(a, static_cast<left_difference_type>(INT8_C(3))) * static_cast<local_double_limb_type>(*detail::advance_and_point(b, static_cast<right_difference_type>(INT8_C(1))));
      const local_double_limb_type a2b2 = *detail::advance_and_point(a, static_cast<left_difference_type>(INT8_C(2))) * static_cast<local_double_limb_type>(*detail::advance_and_point(b, static_cast<right_difference_type>(INT8_C(2))));
      const local_double_limb_type a1b3 = *detail::advance_and_point(a, static_cast<left_difference_type>(INT8_C(1))) * static_cast<local_double_limb_type>(*detail::advance_and_point(b, static_cast<right_difference_type>(INT8_C(3))));

      const local_double_limb_type a3b2 = *detail::advance_and_point(a, static_cast<left_difference_type>(INT8_C(3))) * static_cast<local_double_limb_type>(*detail::advance_and_point(b, static_cast<right_difference_type>(INT8_C(2))));
      const local_double_limb_type a2b3 = *detail::advance_and_point(a, static_cast<left_difference_type>(INT8_C(2))) * static_cast<local_double_limb_type>(*detail::advance_and_point(b, static_cast<right_difference_type>(INT8_C(3))));

      const local_double_limb_type a3b3 = *detail::advance_and_point(a, static_cast<left_difference_type>(INT8_C(3))) * static_cast<local_double_limb_type>(*detail::advance_and_point(b, static_cast<right_difference_type>(INT8_C(3))));

            local_double_limb_type rd1;
            local_double_limb_type rd2;
            local_double_limb_type rd3;
            local_double_limb_type rd4;
            local_double_limb_type rd5;
            local_double_limb_type rd6;

      // One special case is considered, the case of multiplication
      // of the form BITS/2 * BITS/2 = BITS. In this case, the algorithm
      // can be significantly simplified by using only the 'lower-halves'
      // of the data.
      if(    (*detail::advance_and_point(a, static_cast<left_difference_type>(INT8_C(7))) == static_cast<left_value_type>(UINT8_C(0))) && (*detail::advance_and_point(b, static_cast<right_difference_type>(INT8_C(7))) == static_cast<left_value_type>(UINT8_C(0)))
          && (*detail::advance_and_point(a, static_cast<left_difference_type>(INT8_C(6))) == static_cast<left_value_type>(UINT8_C(0))) && (*detail::advance_and_point(b, static_cast<right_difference_type>(INT8_C(6))) == static_cast<left_value_type>(UINT8_C(0)))
          && (*detail::advance_and_point(a, static_cast<left_difference_type>(INT8_C(5))) == static_cast<left_value_type>(UINT8_C(0))) && (*detail::advance_and_point(b, static_cast<right_difference_type>(INT8_C(5))) == static_cast<left_value_type>(UINT8_C(0)))
          && (*detail::advance_and_point(a, static_cast<left_difference_type>(INT8_C(4))) == static_cast<left_value_type>(UINT8_C(0))) && (*detail::advance_and_point(b, static_cast<right_difference_type>(INT8_C(4))) == static_cast<left_value_type>(UINT8_C(0))))
      {
        rd1   = static_cast<local_double_limb_type>
                (
                  detail::make_hi<local_limb_type>(a0b0)
                )
                + detail::make_lo<local_limb_type>(a1b0)
                + detail::make_lo<local_limb_type>(a0b1)
                ;

        rd2   = static_cast<local_double_limb_type>
                (
                  detail::make_hi<local_limb_type>(rd1)
                )
                + detail::make_lo<local_limb_type>(a2b0)
                + detail::make_lo<local_limb_type>(a1b1)
                + detail::make_lo<local_limb_type>(a0b2)
                + detail::make_hi<local_limb_type>(a1b0)
                + detail::make_hi<local_limb_type>(a0b1)
                ;

        rd3   = static_cast<local_double_limb_type>
                (
                  detail::make_hi<local_limb_type>(rd2)
                )
                + detail::make_lo<local_limb_type>(a3b0)
                + detail::make_lo<local_limb_type>(a2b1)
                + detail::make_lo<local_limb_type>(a1b2)
                + detail::make_lo<local_limb_type>(a0b3)
                + detail::make_hi<local_limb_type>(a2b0)
                + detail::make_hi<local_limb_type>(a1b1)
                + detail::make_hi<local_limb_type>(a0b2)
                ;

        rd4   = static_cast<local_double_limb_type>
                (
                  detail::make_hi<local_limb_type>(rd3)
                )
                + detail::make_lo<local_limb_type>(a3b1)
                + detail::make_lo<local_limb_type>(a2b2)
                + detail::make_lo<local_limb_type>(a1b3)
                + detail::make_hi<local_limb_type>(a3b0)
                + detail::make_hi<local_limb_type>(a2b1)
                + detail::make_hi<local_limb_type>(a1b2)
                + detail::make_hi<local_limb_type>(a0b3)
                ;

        rd5   = static_cast<local_double_limb_type>
                (
                  detail::make_hi<local_limb_type>(rd4)
                )
                + detail::make_lo<local_limb_type>(a3b2)
                + detail::make_lo<local_limb_type>(a2b3)
                + detail::make_hi<local_limb_type>(a3b1)
                + detail::make_hi<local_limb_type>(a2b2)
                + detail::make_hi<local_limb_type>(a1b3)
                ;

        rd6   = static_cast<local_double_limb_type>
                (
                  detail::make_hi<local_limb_type>(rd5)
                )
                + detail::make_lo<local_limb_type>(a3b3)
                + detail::make_hi<local_limb_type>(a3b2)
                + detail::make_hi<local_limb_type>(a2b3)
                ;

        *detail::advance_and_point(r, static_cast<result_difference_type>(INT8_C(7)))
              = static_cast<local_limb_type>
                (
                    detail::make_hi<local_limb_type>(rd6)
                  + detail::make_hi<local_limb_type>(a3b3)
                )
                ;
      }
      else
      {
        const local_double_limb_type a4b0 = *detail::advance_and_point(a, static_cast<left_difference_type>(INT8_C(4))) * static_cast<local_double_limb_type>(*detail::advance_and_point(b, static_cast<right_difference_type>(INT8_C(0))));
        const local_double_limb_type a0b4 = *detail::advance_and_point(a, static_cast<left_difference_type>(INT8_C(0))) * static_cast<local_double_limb_type>(*detail::advance_and_point(b, static_cast<right_difference_type>(INT8_C(4))));

        const local_double_limb_type a5b0 = *detail::advance_and_point(a, static_cast<left_difference_type>(INT8_C(5))) * static_cast<local_double_limb_type>(*detail::advance_and_point(b, static_cast<right_difference_type>(INT8_C(0))));
        const local_double_limb_type a4b1 = *detail::advance_and_point(a, static_cast<left_difference_type>(INT8_C(4))) * static_cast<local_double_limb_type>(*detail::advance_and_point(b, static_cast<right_difference_type>(INT8_C(1))));

        const local_double_limb_type a1b4 = *detail::advance_and_point(a, static_cast<left_difference_type>(INT8_C(1))) * static_cast<local_double_limb_type>(*detail::advance_and_point(b, static_cast<right_difference_type>(INT8_C(4))));
        const local_double_limb_type a0b5 = *detail::advance_and_point(a, static_cast<left_difference_type>(INT8_C(0))) * static_cast<local_double_limb_type>(*detail::advance_and_point(b, static_cast<right_difference_type>(INT8_C(5))));

        const local_double_limb_type a6b0 = *detail::advance_and_point(a, static_cast<left_difference_type>(INT8_C(6))) * static_cast<local_double_limb_type>(*detail::advance_and_point(b, static_cast<right_difference_type>(INT8_C(0))));
        const local_double_limb_type a5b1 = *detail::advance_and_point(a, static_cast<left_difference_type>(INT8_C(5))) * static_cast<local_double_limb_type>(*detail::advance_and_point(b, static_cast<right_difference_type>(INT8_C(1))));

        const local_double_limb_type a4b2 = *detail::advance_and_point(a, static_cast<left_difference_type>(INT8_C(4))) * static_cast<local_double_limb_type>(*detail::advance_and_point(b, static_cast<right_difference_type>(INT8_C(2))));
        const local_double_limb_type a2b4 = *detail::advance_and_point(a, static_cast<left_difference_type>(INT8_C(2))) * static_cast<local_double_limb_type>(*detail::advance_and_point(b, static_cast<right_difference_type>(INT8_C(4))));

        const local_double_limb_type a1b5 = *detail::advance_and_point(a, static_cast<left_difference_type>(INT8_C(1))) * static_cast<local_double_limb_type>(*detail::advance_and_point(b, static_cast<right_difference_type>(INT8_C(5))));
        const local_double_limb_type a0b6 = *detail::advance_and_point(a, static_cast<left_difference_type>(INT8_C(0))) * static_cast<local_double_limb_type>(*detail::advance_and_point(b, static_cast<right_difference_type>(INT8_C(6))));

        rd1   = static_cast<local_double_limb_type>
                (
                  detail::make_hi<local_limb_type>(a0b0)
                )
                + detail::make_lo<local_limb_type>(a1b0)
                + detail::make_lo<local_limb_type>(a0b1)
                ;

        rd2   = static_cast<local_double_limb_type>
                (
                  detail::make_hi<local_limb_type>(rd1)
                )
                + detail::make_lo<local_limb_type>(a2b0)
                + detail::make_lo<local_limb_type>(a1b1)
                + detail::make_lo<local_limb_type>(a0b2)
                + detail::make_hi<local_limb_type>(a1b0)
                + detail::make_hi<local_limb_type>(a0b1)
                ;

        rd3   = static_cast<local_double_limb_type>
                (
                  detail::make_hi<local_limb_type>(rd2)
                )
                + detail::make_lo<local_limb_type>(a3b0)
                + detail::make_lo<local_limb_type>(a2b1)
                + detail::make_lo<local_limb_type>(a1b2)
                + detail::make_lo<local_limb_type>(a0b3)
                + detail::make_hi<local_limb_type>(a2b0)
                + detail::make_hi<local_limb_type>(a1b1)
                + detail::make_hi<local_limb_type>(a0b2)
                ;

        rd4   = static_cast<local_double_limb_type>
                (
                  detail::make_hi<local_limb_type>(rd3)
                )
                + detail::make_lo<local_limb_type>(a4b0)
                + detail::make_lo<local_limb_type>(a3b1)
                + detail::make_lo<local_limb_type>(a2b2)
                + detail::make_lo<local_limb_type>(a1b3)
                + detail::make_lo<local_limb_type>(a0b4)
                + detail::make_hi<local_limb_type>(a3b0)
                + detail::make_hi<local_limb_type>(a2b1)
                + detail::make_hi<local_limb_type>(a1b2)
                + detail::make_hi<local_limb_type>(a0b3)
                ;

        rd5   = static_cast<local_double_limb_type>
                (
                  detail::make_hi<local_limb_type>(rd4)
                )
                + detail::make_lo<local_limb_type>(a5b0)
                + detail::make_lo<local_limb_type>(a4b1)
                + detail::make_lo<local_limb_type>(a3b2)
                + detail::make_lo<local_limb_type>(a2b3)
                + detail::make_lo<local_limb_type>(a1b4)
                + detail::make_lo<local_limb_type>(a0b5)
                + detail::make_hi<local_limb_type>(a4b0)
                + detail::make_hi<local_limb_type>(a3b1)
                + detail::make_hi<local_limb_type>(a2b2)
                + detail::make_hi<local_limb_type>(a1b3)
                + detail::make_hi<local_limb_type>(a0b4)
                ;

        rd6   = static_cast<local_double_limb_type>
                (
                  detail::make_hi<local_limb_type>(rd5)
                )
                + detail::make_lo<local_limb_type>(a6b0)
                + detail::make_lo<local_limb_type>(a5b1)
                + detail::make_lo<local_limb_type>(a4b2)
                + detail::make_lo<local_limb_type>(a3b3)
                + detail::make_lo<local_limb_type>(a2b4)
                + detail::make_lo<local_limb_type>(a1b5)
                + detail::make_lo<local_limb_type>(a0b6)
                + detail::make_hi<local_limb_type>(a5b0)
                + detail::make_hi<local_limb_type>(a4b1)
                + detail::make_hi<local_limb_type>(a3b2)
                + detail::make_hi<local_limb_type>(a2b3)
                + detail::make_hi<local_limb_type>(a1b4)
                + detail::make_hi<local_limb_type>(a0b5)
                ;

        *detail::advance_and_point(r, static_cast<result_difference_type>(INT8_C(7)))
              = static_cast<local_limb_type>
                (
                    detail::make_hi<local_limb_type>(rd6)
                  + static_cast<local_limb_type>    (*detail::advance_and_point(a, static_cast<left_difference_type>(INT8_C(7))) * static_cast<local_double_limb_type>(*detail::advance_and_point(b, static_cast<right_difference_type>(INT8_C(0)))))
                  + static_cast<local_limb_type>    (*detail::advance_and_point(a, static_cast<left_difference_type>(INT8_C(6))) * static_cast<local_double_limb_type>(*detail::advance_and_point(b, static_cast<right_difference_type>(INT8_C(1)))))
                  + static_cast<local_limb_type>    (*detail::advance_and_point(a, static_cast<left_difference_type>(INT8_C(5))) * static_cast<local_double_limb_type>(*detail::advance_and_point(b, static_cast<right_difference_type>(INT8_C(2)))))
                  + static_cast<local_limb_type>    (*detail::advance_and_point(a, static_cast<left_difference_type>(INT8_C(4))) * static_cast<local_double_limb_type>(*detail::advance_and_point(b, static_cast<right_difference_type>(INT8_C(3)))))
                  + static_cast<local_limb_type>    (*detail::advance_and_point(a, static_cast<left_difference_type>(INT8_C(3))) * static_cast<local_double_limb_type>(*detail::advance_and_point(b, static_cast<right_difference_type>(INT8_C(4)))))
                  + static_cast<local_limb_type>    (*detail::advance_and_point(a, static_cast<left_difference_type>(INT8_C(2))) * static_cast<local_double_limb_type>(*detail::advance_and_point(b, static_cast<right_difference_type>(INT8_C(5)))))
                  + static_cast<local_limb_type>    (*detail::advance_and_point(a, static_cast<left_difference_type>(INT8_C(1))) * static_cast<local_double_limb_type>(*detail::advance_and_point(b, static_cast<right_difference_type>(INT8_C(6)))))
                  + static_cast<local_limb_type>    (*detail::advance_and_point(a, static_cast<left_difference_type>(INT8_C(0))) * static_cast<local_double_limb_type>(*detail::advance_and_point(b, static_cast<right_difference_type>(INT8_C(7)))))
                  + detail::make_hi<local_limb_type>(a6b0)
                  + detail::make_hi<local_limb_type>(a5b1)
                  + detail::make_hi<local_limb_type>(a4b2)
                  + detail::make_hi<local_limb_type>(a3b3)
                  + detail::make_hi<local_limb_type>(a2b4)
                  + detail::make_hi<local_limb_type>(a1b5)
                  + detail::make_hi<local_limb_type>(a0b6)
                )
                ;
      }

      *detail::advance_and_point(r, static_cast<result_difference_type>(INT8_C(0))) = static_cast<local_limb_type>(a0b0);
      *detail::advance_and_point(r, static_cast<result_difference_type>(INT8_C(1))) = static_cast<local_limb_type>(rd1);
      *detail::advance_and_point(r, static_cast<result_difference_type>(INT8_C(2))) = static_cast<local_limb_type>(rd2);
      *detail::advance_and_point(r, static_cast<result_difference_type>(INT8_C(3))) = static_cast<local_limb_type>(rd3);
      *detail::advance_and_point(r, static_cast<result_difference_type>(INT8_C(4))) = static_cast<local_limb_type>(rd4);
      *detail::advance_and_point(r, static_cast<result_difference_type>(INT8_C(5))) = static_cast<local_limb_type>(rd5);
      *detail::advance_and_point(r, static_cast<result_difference_type>(INT8_C(6))) = static_cast<local_limb_type>(rd6);
    }
    #endif

    template<typename ResultIterator,
             typename InputIteratorLeft,
             typename InputIteratorRight,
             const size_t RePhraseWidth2 = Width2,
             std::enable_if_t<(   (uintwide_t<RePhraseWidth2, LimbType, AllocatorType>::number_of_limbs != static_cast<size_t>(UINT32_C(4)))
    #if defined(WIDE_INTEGER_HAS_MUL_8_BY_8_UNROLL)
                               && (uintwide_t<RePhraseWidth2, LimbType, AllocatorType>::number_of_limbs != static_cast<size_t>(UINT32_C(8)))
    #endif
                              )> const* = nullptr>
    static WIDE_INTEGER_CONSTEXPR auto eval_multiply_n_by_n_to_lo_part(      ResultIterator     r,
                                                                             InputIteratorLeft  a,
                                                                             InputIteratorRight b,
                                                                       const unsigned_fast_type count) -> void
    {
      static_assert
      (
           (std::numeric_limits<typename std::iterator_traits<ResultIterator>::value_type>::digits == std::numeric_limits<typename std::iterator_traits<InputIteratorLeft>::value_type>::digits)
        && (std::numeric_limits<typename std::iterator_traits<ResultIterator>::value_type>::digits == std::numeric_limits<typename std::iterator_traits<InputIteratorRight>::value_type>::digits),
        "Error: Internals require same widths for left-right-result limb_types at the moment"
      );

      using local_limb_type = typename std::iterator_traits<ResultIterator>::value_type;

      using local_double_limb_type =
        typename detail::uint_type_helper<static_cast<size_t>(std::numeric_limits<local_limb_type>::digits * 2)>::exact_unsigned_type;

      detail::fill_unsafe(r, detail::advance_and_point(r, count), static_cast<local_limb_type>(UINT8_C(0)));

      #if defined(WIDE_INTEGER_HAS_CLZ_LIMB_OPTIMIZATIONS)

      auto clz_a = static_cast<unsigned_fast_type>(UINT8_C(0));
      auto clz_b = static_cast<unsigned_fast_type>(UINT8_C(0));

      if(count > static_cast<unsigned_fast_type>(UINT8_C(0)))
      {
        {
          using input_left_value_type  = typename std::iterator_traits<InputIteratorLeft>::value_type;

          auto it_leading_zeros_a = detail::advance_and_point(a, static_cast<unsigned_fast_type>(count - static_cast<unsigned_fast_type>(UINT8_C(1)))); // NOLINT(llvm-qualified-auto,readability-qualified-auto)

          while(   (it_leading_zeros_a != a) // NOLINT(altera-id-dependent-backward-branch)
                && (*it_leading_zeros_a == static_cast<input_left_value_type>(UINT8_C(0))))
          {
            --it_leading_zeros_a;

            ++clz_a;
          }
        }

        {
          using input_right_value_type = typename std::iterator_traits<InputIteratorRight>::value_type;

          auto it_leading_zeros_b = detail::advance_and_point(b, static_cast<unsigned_fast_type>(count - static_cast<unsigned_fast_type>(UINT8_C(1)))); // NOLINT(llvm-qualified-auto,readability-qualified-auto)

          while(   (it_leading_zeros_b != b) // NOLINT(altera-id-dependent-backward-branch)
                && (*it_leading_zeros_b == static_cast<input_right_value_type>(UINT8_C(0))))
          {
            --it_leading_zeros_b;

            ++clz_b;
          }
        }
      }

      const auto count_b = static_cast<unsigned_fast_type>(count - clz_b);

      const auto imax = static_cast<unsigned_fast_type>(count - clz_a);

      #else

      const auto imax = count;

      #endif

      for(auto i = static_cast<unsigned_fast_type>(UINT8_C(0)); i < imax; ++i) // NOLINT(altera-id-dependent-backward-branch)
      {
        if(*a != static_cast<local_limb_type>(UINT8_C(0)))
        {
          auto carry = static_cast<local_double_limb_type>(UINT8_C(0));

          auto r_i_plus_j = detail::advance_and_point(r, i); // NOLINT(llvm-qualified-auto,readability-qualified-auto)
          auto bj         = b;                               // NOLINT(llvm-qualified-auto,readability-qualified-auto)

          #if defined(WIDE_INTEGER_HAS_CLZ_LIMB_OPTIMIZATIONS)
          const auto jmax =
            (std::min)(static_cast<unsigned_fast_type>(count - i),
                       static_cast<unsigned_fast_type>(count_b + static_cast<unsigned_fast_type>(UINT8_C(1))));
          #else
          const auto jmax = static_cast<unsigned_fast_type>(count - i);
          #endif

          for(auto j = static_cast<unsigned_fast_type>(UINT8_C(0)); j < jmax; ++j) // NOLINT(altera-id-dependent-backward-branch)
          {
            carry = static_cast<local_double_limb_type>(carry + static_cast<local_double_limb_type>(static_cast<local_double_limb_type>(*a) * *bj++));
            carry = static_cast<local_double_limb_type>(carry + *r_i_plus_j);

            *r_i_plus_j++ = static_cast<local_limb_type>(carry);
            carry         = detail::make_hi<local_limb_type>(carry);
          }
        }

        ++a;
      }
    }

    template<typename ResultIterator,
             typename InputIteratorLeft,
             typename InputIteratorRight>
    static WIDE_INTEGER_CONSTEXPR auto eval_multiply_n_by_n_to_2n(      ResultIterator     r,
                                                                        InputIteratorLeft  a,
                                                                        InputIteratorRight b,
                                                                  const unsigned_fast_type count) -> void
    {
      static_assert
      (
           (std::numeric_limits<typename std::iterator_traits<ResultIterator>::value_type>::digits == std::numeric_limits<typename std::iterator_traits<InputIteratorLeft>::value_type>::digits)
        && (std::numeric_limits<typename std::iterator_traits<ResultIterator>::value_type>::digits == std::numeric_limits<typename std::iterator_traits<InputIteratorRight>::value_type>::digits),
        "Error: Internals require same widths for left-right-result limb_types at the moment"
      );

      using local_limb_type = typename std::iterator_traits<ResultIterator>::value_type;

      using local_double_limb_type =
        typename detail::uint_type_helper<static_cast<size_t>(std::numeric_limits<local_limb_type>::digits * 2)>::exact_unsigned_type;

      detail::fill_unsafe(r, detail::advance_and_point(r, static_cast<size_t>(count * 2U)), static_cast<local_limb_type>(UINT8_C(0)));

      for(auto i = static_cast<unsigned_fast_type>(UINT8_C(0)); i < count; ++i)
      {
        if(*a != static_cast<local_limb_type>(UINT8_C(0)))
        {
          auto carry = static_cast<local_double_limb_type>(UINT8_C(0));

          auto r_i_plus_j = detail::advance_and_point(r, i); // NOLINT(llvm-qualified-auto,readability-qualified-auto)
          auto bj         = b;                               // NOLINT(llvm-qualified-auto,readability-qualified-auto)

          for(auto j = static_cast<unsigned_fast_type>(UINT8_C(0)); j < count; ++j)
          {
            carry =
              static_cast<local_double_limb_type>
              (
                  static_cast<local_double_limb_type>
                  (
                      carry
                    + static_cast<local_double_limb_type>(static_cast<local_double_limb_type>(*a) * *bj++)
                  )
                + *r_i_plus_j
              );

            *r_i_plus_j++ = static_cast<local_limb_type>(carry);
            carry         = detail::make_hi<local_limb_type>(carry);
          }

          *r_i_plus_j = static_cast<local_limb_type>(carry);
        }

        ++a;
      }
    }

    template<typename ResultIterator,
             typename InputIteratorLeft>
    static WIDE_INTEGER_CONSTEXPR auto eval_multiply_1d(      ResultIterator                                               r,
                                                              InputIteratorLeft                                            a,
                                                        const typename std::iterator_traits<InputIteratorLeft>::value_type b,
                                                        const unsigned_fast_type                                           count) -> limb_type
    {
      using local_limb_type = typename std::iterator_traits<ResultIterator>::value_type;
      using left_value_type = typename std::iterator_traits<InputIteratorLeft>::value_type;

      static_assert
      (
        (std::numeric_limits<local_limb_type>::digits == std::numeric_limits<left_value_type>::digits),
        "Error: Internals require same widths for left-right-result limb_types at the moment"
      );

      using local_double_limb_type =
        typename detail::uint_type_helper<static_cast<size_t>(std::numeric_limits<local_limb_type>::digits * 2)>::exact_unsigned_type;

      auto carry = static_cast<local_double_limb_type>(UINT8_C(0));

      if(b == static_cast<left_value_type>(UINT8_C(0)))
      {
        detail::fill_unsafe(r, detail::advance_and_point(r, count), static_cast<limb_type>(UINT8_C(0)));
      }
      else
      {
        #if defined(WIDE_INTEGER_HAS_CLZ_LIMB_OPTIMIZATIONS)
        auto clz_a = static_cast<unsigned_fast_type>(UINT8_C(0));

        if(count > static_cast<unsigned_fast_type>(UINT8_C(0)))
        {
          using input_left_value_type  = typename std::iterator_traits<InputIteratorLeft>::value_type;

          auto it_leading_zeros_a = detail::advance_and_point(a, static_cast<unsigned_fast_type>(count - static_cast<unsigned_fast_type>(UINT8_C(1)))); // NOLINT(llvm-qualified-auto,readability-qualified-auto)

          while(   (it_leading_zeros_a != a) // NOLINT(altera-id-dependent-backward-branch)
                && (*it_leading_zeros_a == static_cast<input_left_value_type>(UINT8_C(0))))
          {
            --it_leading_zeros_a;

            ++clz_a;
          }
        }

        const auto imax = static_cast<unsigned_fast_type>(count - clz_a);

        #else

        const auto imax = count;

        #endif

        auto i = static_cast<unsigned_fast_type>(UINT8_C(0));

        for( ; i < imax; ++i) // NOLINT(altera-id-dependent-backward-branch)
        {
          carry =
            static_cast<local_double_limb_type>
            (
                carry
              + static_cast<local_double_limb_type>(static_cast<local_double_limb_type>(*a++) * b)
            );

          *r++  = static_cast<local_limb_type>(carry);
          carry = detail::make_hi<local_limb_type>(carry);
        }

        #if defined(WIDE_INTEGER_HAS_CLZ_LIMB_OPTIMIZATIONS)
        for( ; i < count; ++i)
        {
          *r++  = static_cast<local_limb_type>(carry);
          carry = detail::make_hi<local_limb_type>(static_cast<local_double_limb_type>(UINT8_C(0)));
        }
        #endif
      }

      return static_cast<local_limb_type>(carry);
    }

    template<typename InputIteratorLeft>
    static WIDE_INTEGER_CONSTEXPR
    auto eval_multiply_kara_propagate_carry(      InputIteratorLeft                                            t,
                                            const unsigned_fast_type                                           n,
                                            const typename std::iterator_traits<InputIteratorLeft>::value_type carry) -> void
    {
      using local_limb_type = typename std::iterator_traits<InputIteratorLeft>::value_type;

      using local_double_limb_type =
        typename detail::uint_type_helper<static_cast<size_t>(static_cast<int>(std::numeric_limits<local_limb_type>::digits * static_cast<int>(INT8_C(2))))>::exact_unsigned_type;

      auto i = static_cast<unsigned_fast_type>(UINT8_C(0));

      auto carry_out = carry;

      while((i < n) && (carry_out != static_cast<local_limb_type>(UINT8_C(0)))) // NOLINT(altera-id-dependent-backward-branch)
      {
        const auto uv_as_ularge =
          static_cast<local_double_limb_type>
          (
            static_cast<local_double_limb_type>(*t) + carry_out
          );

        carry_out = detail::make_hi<local_limb_type>(uv_as_ularge);

        *t++ = static_cast<local_limb_type>(uv_as_ularge);

        ++i;
      }
    }

    template<typename InputIteratorLeft>
    static WIDE_INTEGER_CONSTEXPR
    auto eval_multiply_kara_propagate_borrow(      InputIteratorLeft  t,
                                             const unsigned_fast_type n,
                                             const bool               has_borrow) -> void
    {
      using local_limb_type = typename std::iterator_traits<InputIteratorLeft>::value_type;

      using local_double_limb_type =
        typename detail::uint_type_helper<static_cast<size_t>(static_cast<int>(std::numeric_limits<local_limb_type>::digits * static_cast<int>(INT8_C(2))))>::exact_unsigned_type;

      auto i = static_cast<unsigned_fast_type>(UINT8_C(0));

      auto has_borrow_out = has_borrow;

      while((i < n) && has_borrow_out) // NOLINT(altera-id-dependent-backward-branch)
      {
        auto uv_as_ularge = static_cast<local_double_limb_type>(*t);

        if(has_borrow_out)
        {
          --uv_as_ularge;
        }

        has_borrow_out =
        (
          detail::make_hi<local_limb_type>(uv_as_ularge) != static_cast<local_limb_type>(UINT8_C(0))
        );

        *t++ = static_cast<local_limb_type>(uv_as_ularge);

        ++i;
      }
    }

    template<typename ResultIterator,
             typename InputIteratorLeft,
             typename InputIteratorRight,
             typename InputIteratorTemp>
    static WIDE_INTEGER_CONSTEXPR
    auto eval_multiply_kara_n_by_n_to_2n(      ResultIterator     r, // NOLINT(misc-no-recursion)
                                         const InputIteratorLeft  a,
                                         const InputIteratorRight b,
                                         const unsigned_fast_type n,
                                               InputIteratorTemp  t) -> void
    {
      if(n <= static_cast<unsigned_fast_type>(UINT32_C(48)))
      {
        static_cast<void>(t);

        eval_multiply_n_by_n_to_2n(r, a, b, n);
      }
      else
      {
        static_assert
        (
             (std::numeric_limits<typename std::iterator_traits<ResultIterator>::value_type>::digits == std::numeric_limits<typename std::iterator_traits<InputIteratorLeft>::value_type>::digits)
          && (std::numeric_limits<typename std::iterator_traits<ResultIterator>::value_type>::digits == std::numeric_limits<typename std::iterator_traits<InputIteratorRight>::value_type>::digits)
          && (std::numeric_limits<typename std::iterator_traits<ResultIterator>::value_type>::digits == std::numeric_limits<typename std::iterator_traits<InputIteratorTemp>::value_type>::digits),
          "Error: Internals require same widths for left-right-result limb_types at the moment"
        );

        using local_limb_type = typename std::iterator_traits<ResultIterator>::value_type;

        using result_difference_type = typename std::iterator_traits<ResultIterator>::difference_type;
        using left_difference_type   = typename std::iterator_traits<InputIteratorLeft>::difference_type;
        using right_difference_type  = typename std::iterator_traits<InputIteratorRight>::difference_type;
        using temp_difference_type   = typename std::iterator_traits<InputIteratorTemp>::difference_type;

        // Based on "Algorithm 1.3 KaratsubaMultiply", Sect. 1.3.2, page 5
        // of R.P. Brent and P. Zimmermann, "Modern Computer Arithmetic",
        // Cambridge University Press (2011).

        // The Karatsuba multipliation computes the product of u*v as:
        // [b^N + b^(N/2)] a1*b1 + [b^(N/2)](a1 - a0)(b0 - b1) + [b^(N/2) + 1] a0*b0

        // Here we visualize u and v in two components 0,1 corresponding
        // to the high and low order parts, respectively.

        // Step 1
        // Calculate a1*b1 and store it in the upper part of r.
        // Calculate a0*b0 and store it in the lower part of r.
        // copy r to t0.

        // Step 2
        // Add a1*b1 (which is t2) to the middle two-quarters of r (which is r1)
        // Add a0*b0 (which is t0) to the middle two-quarters of r (which is r1)

        // Step 3
        // Calculate |a1-a0| in t0 and note the sign (i.e., the borrow flag)

        // Step 4
        // Calculate |b0-b1| in t1 and note the sign (i.e., the borrow flag)

        // Step 5
        // Call kara mul to calculate |a1-a0|*|b0-b1| in (t2),
        // while using temporary storage in t4 along the way.

        // Step 6
        // Check the borrow signs. If a1-a0 and b0-b1 have the same signs,
        // then add |a1-a0|*|b0-b1| to r1, otherwise subtract it from r1.

        const auto nh = static_cast<unsigned_fast_type>(n / 2U);

        const InputIteratorLeft   a0 = detail::advance_and_point(a, static_cast<left_difference_type>(0));
        const InputIteratorLeft   a1 = detail::advance_and_point(a, static_cast<left_difference_type>(nh));

        const InputIteratorRight  b0 = detail::advance_and_point(b, static_cast<right_difference_type>(0));
        const InputIteratorRight  b1 = detail::advance_and_point(b, static_cast<right_difference_type>(nh));

              ResultIterator      r0 = detail::advance_and_point(r, static_cast<result_difference_type>(0));
              ResultIterator      r1 = detail::advance_and_point(r, static_cast<result_difference_type>(nh));
              ResultIterator      r2 = detail::advance_and_point(r, static_cast<result_difference_type>(n));
              ResultIterator      r3 = detail::advance_and_point(r, static_cast<result_difference_type>(static_cast<result_difference_type>(n) + static_cast<result_difference_type>(nh)));

              InputIteratorTemp   t0 = detail::advance_and_point(t, static_cast<temp_difference_type>(0));
              InputIteratorTemp   t1 = detail::advance_and_point(t, static_cast<temp_difference_type>(nh));
              InputIteratorTemp   t2 = detail::advance_and_point(t, static_cast<temp_difference_type>(n));
              InputIteratorTemp   t4 = detail::advance_and_point(t, static_cast<temp_difference_type>(static_cast<result_difference_type>(n) + static_cast<result_difference_type>(n)));

        // Step 1
        //   a1*b1 -> r2
        //   a0*b0 -> r0
        //   r -> t0
        eval_multiply_kara_n_by_n_to_2n(r2, a1, b1, nh, t0);
        eval_multiply_kara_n_by_n_to_2n(r0, a0, b0, nh, t0);
        detail::copy_unsafe(r0, detail::advance_and_point(r0, static_cast<result_difference_type>(static_cast<result_difference_type>(n) * static_cast<result_difference_type>(2U))), t0);

        // Step 2
        //   r1 += a1*b1
        //   r1 += a0*b0
        auto carry = static_cast<local_limb_type>(eval_add_n(r1, r1, t2, n));
        eval_multiply_kara_propagate_carry(r3, nh, carry);
        carry = static_cast<local_limb_type>(eval_add_n(r1, r1, t0, n));
        eval_multiply_kara_propagate_carry(r3, nh, carry);

        // Step 3
        //   |a1-a0| -> t0
        const auto cmp_result_a1a0 = compare_ranges(a1, a0, nh);

        if(cmp_result_a1a0 == static_cast<std::int_fast8_t>(INT8_C(1)))
        {
          static_cast<void>(eval_subtract_n(t0, a1, a0, nh));
        }
        else if(cmp_result_a1a0 == static_cast<std::int_fast8_t>(INT8_C(-1)))
        {
          static_cast<void>(eval_subtract_n(t0, a0, a1, nh));
        }

        // Step 4
        //   |b0-b1| -> t1
        const auto cmp_result_b0b1 = compare_ranges(b0, b1, nh);

        if(cmp_result_b0b1 == static_cast<std::int_fast8_t>(INT8_C(1)))
        {
          static_cast<void>(eval_subtract_n(t1, b0, b1, nh));
        }
        else if(cmp_result_b0b1 == static_cast<std::int_fast8_t>(INT8_C(-1)))
        {
          static_cast<void>(eval_subtract_n(t1, b1, b0, nh));
        }

        // Step 5
        //   |a1-a0|*|b0-b1| -> t2
        eval_multiply_kara_n_by_n_to_2n(t2, t0, t1, nh, t4);

        // Step 6
        //   either r1 += |a1-a0|*|b0-b1|
        //   or     r1 -= |a1-a0|*|b0-b1|
        if(static_cast<std::int_fast8_t>(cmp_result_a1a0 * cmp_result_b0b1) == static_cast<std::int_fast8_t>(INT8_C(1)))
        {
          carry = eval_add_n(r1, r1, t2, n);

          eval_multiply_kara_propagate_carry(r3, nh, carry);
        }
        else if(static_cast<std::int_fast8_t>(cmp_result_a1a0 * cmp_result_b0b1) == static_cast<std::int_fast8_t>(INT8_C(-1)))
        {
          const auto has_borrow = eval_subtract_n(r1, r1, t2, n);

          eval_multiply_kara_propagate_borrow(r3, nh, has_borrow);
        }
      }
    }

    WIDE_INTEGER_CONSTEXPR auto eval_divide_knuth(const uintwide_t& other,
                                                        uintwide_t* remainder = nullptr) -> void
    {
      // Use Knuth's long division algorithm.
      // The loop-ordering of indices in Knuth's original
      // algorithm has been reversed due to the data format
      // used here. Several optimizations and combinations
      // of logic have been carried out in the source code.

      // See also:
      // D.E. Knuth, "The Art of Computer Programming, Volume 2:
      // Seminumerical Algorithms", Addison-Wesley (1998),
      // Section 4.3.1 Algorithm D and Exercise 16.

      using local_uint_index_type = unsigned_fast_type;

      const auto u_offset =
        static_cast<local_uint_index_type>
        (
          std::distance(values.crbegin(), std::find_if(values.crbegin(), values.crend(), [](const limb_type& elem) { return (elem != static_cast<limb_type>(UINT8_C(0))); }))
        );

      const auto v_offset =
        static_cast<local_uint_index_type>
        (
          std::distance(other.values.crbegin(), std::find_if(other.values.crbegin(), other.values.crend(), [](const limb_type& elem) { return (elem != static_cast<limb_type>(UINT8_C(0))); }))
        );

      if(v_offset == static_cast<local_uint_index_type>(number_of_limbs))
      {
        // The denominator is zero. Set the maximum value and return.
        // This also catches (0 / 0) and sets the maximum value for it.
        static_cast<void>(operator=(limits_helper_max(IsSigned))); // LCOV_EXCL_LINE

        if(remainder != nullptr) // LCOV_EXCL_LINE
        {
          detail::fill_unsafe(remainder->values.begin(), remainder->values.end(), static_cast<limb_type>(UINT8_C(0))); // LCOV_EXCL_LINE
        }
      }
      else if(u_offset == static_cast<local_uint_index_type>(number_of_limbs))
      {
        // The numerator is zero. Do nothing and return.

        if(remainder != nullptr)
        {
          *remainder = uintwide_t(static_cast<std::uint8_t>(UINT8_C(0)));
        }
      }
      else
      {
        const auto result_of_compare_left_with_right = compare(other);

        const auto left_is_less_than_right = (result_of_compare_left_with_right == INT8_C(-1));
        const auto left_is_equal_to_right  = (result_of_compare_left_with_right == INT8_C( 0));

        if(left_is_less_than_right)
        {
          // If the denominator is larger than the numerator,
          // then the result of the division is zero.
          if(remainder != nullptr)
          {
            *remainder = *this;
          }

          operator=(static_cast<std::uint8_t>(UINT8_C(0)));
        }
        else if(left_is_equal_to_right)
        {
          // If the denominator is equal to the numerator,
          // then the result of the division is one.
          operator=(static_cast<std::uint8_t>(UINT8_C(1)));

          if(remainder != nullptr)
          {
            *remainder = uintwide_t(static_cast<std::uint8_t>(UINT8_C(0)));
          }
        }
        else
        {
          eval_divide_knuth_core(u_offset, v_offset, other, remainder);
        }
      }
    }

    template<const size_t RePhraseWidth2 = Width2,
             std::enable_if_t<(RePhraseWidth2 > static_cast<size_t>(std::numeric_limits<limb_type>::digits))> const* = nullptr>
    WIDE_INTEGER_CONSTEXPR auto eval_divide_knuth_core(const unsigned_fast_type u_offset, // NOLINT(readability-function-cognitive-complexity)
                                                       const unsigned_fast_type v_offset,
                                                       const uintwide_t& other,
                                                             uintwide_t* remainder) -> void
    {
      using local_uint_index_type = unsigned_fast_type;

      if(static_cast<local_uint_index_type>(v_offset + static_cast<local_uint_index_type>(1U)) == static_cast<local_uint_index_type>(number_of_limbs))
      {
        // The denominator has one single limb.
        // Use a one-dimensional division algorithm.
        const limb_type short_denominator = *other.values.cbegin();

        eval_divide_by_single_limb(short_denominator, u_offset, remainder);
      }
      else
      {
        // We will now use the Knuth long division algorithm.

        // Compute the normalization factor d.
        const auto d =
          static_cast<limb_type>
          (
              static_cast<double_limb_type>(static_cast<double_limb_type>(1U) << static_cast<unsigned>(std::numeric_limits<limb_type>::digits))
            / static_cast<double_limb_type>(static_cast<double_limb_type>(*detail::advance_and_point(other.values.cbegin(), static_cast<size_t>(static_cast<local_uint_index_type>(number_of_limbs - 1U) - v_offset))) + static_cast<limb_type>(1U))
          );

        // Step D1(b), normalize u -> u * d = uu.
        // Step D1(c): normalize v -> v * d = vv.

        using uu_array_type =
          std::conditional_t<std::is_same<AllocatorType, void>::value,
                             detail::fixed_static_array <limb_type, number_of_limbs + 1U>,
                             detail::fixed_dynamic_array<limb_type,
                                                         number_of_limbs + 1U,
                                                         typename std::allocator_traits<std::conditional_t<std::is_same<AllocatorType, void>::value,
                                                                                                           std::allocator<void>,
                                                                                                           AllocatorType>>::template rebind_alloc<limb_type>>>;

        uu_array_type uu;

        representation_type
          vv
          {
            static_cast<typename representation_type::size_type>(number_of_limbs),
            static_cast<typename representation_type::value_type>(UINT8_C(0)),
            typename representation_type::allocator_type() // LCOV_EXCL_LINE
          };

        if(d > static_cast<limb_type>(UINT8_C(1)))
        {
          const auto num_limbs_minus_u_ofs =
            static_cast<size_t>
            (
              static_cast<local_uint_index_type>(number_of_limbs) - u_offset
            );

          *(uu.begin() + num_limbs_minus_u_ofs) =
            eval_multiply_1d
            (
              uu.begin(),
              values.cbegin(),
              d,
              static_cast<unsigned_fast_type>(num_limbs_minus_u_ofs)
            );

          static_cast<void>
          (
            eval_multiply_1d
            (
              vv.begin(),
              other.values.cbegin(),
              d,
              static_cast<unsigned_fast_type>(number_of_limbs - v_offset)
            )
          );
        }
        else
        {
          detail::copy_unsafe(values.cbegin(), values.cend(), uu.begin());

          *(uu.begin() + static_cast<size_t>(static_cast<local_uint_index_type>(number_of_limbs) - u_offset)) = static_cast<limb_type>(UINT8_C(0));

          vv = other.values;
        }

        // Step D2: Initialize j.
        // Step D7: Loop on j from m to 0.

        const auto n   = static_cast<local_uint_index_type>                                   (number_of_limbs - v_offset);
        const auto m   = static_cast<local_uint_index_type>(static_cast<local_uint_index_type>(number_of_limbs - u_offset) - n);
        const auto vj0 = static_cast<local_uint_index_type>(static_cast<local_uint_index_type>(n - static_cast<local_uint_index_type>(UINT8_C(1))));

        auto vv_at_vj0_it = detail::advance_and_point(vv.cbegin(), static_cast<size_t>(vj0)); // NOLINT(llvm-qualified-auto,readability-qualified-auto)

        const auto vv_at_vj0           = *vv_at_vj0_it--;
        const auto vv_at_vj0_minus_one = *vv_at_vj0_it;

        auto values_at_m_minus_j_it = detail::advance_and_point(values.begin(), static_cast<size_t>(m)); // NOLINT(llvm-qualified-auto,readability-qualified-auto)

        for(auto j = static_cast<local_uint_index_type>(UINT8_C(0)); j <= m; ++j) // NOLINT(altera-id-dependent-backward-branch)
        {
          // Step D3 [Calculate q_hat].
          //   if u[j] == v[j0]
          //     set q_hat = b - 1
          //   else
          //     set q_hat = (u[j] * b + u[j + 1]) / v[1]

          const auto uj     = static_cast<local_uint_index_type>(static_cast<local_uint_index_type>(static_cast<local_uint_index_type>(static_cast<local_uint_index_type>(number_of_limbs + 1U) - 1U) - u_offset) - j);
          const auto u_j_j1 = static_cast<double_limb_type>(static_cast<double_limb_type>(static_cast<double_limb_type>(*(uu.cbegin() + static_cast<size_t>(uj))) << static_cast<unsigned>(std::numeric_limits<limb_type>::digits)) + *(uu.cbegin() + static_cast<size_t>(uj - 1U)));

          auto q_hat =
            static_cast<limb_type>
            (
              (*(uu.cbegin() + static_cast<size_t>(uj)) == vv_at_vj0)
                ? (std::numeric_limits<limb_type>::max)()
                : static_cast<limb_type>(u_j_j1 / vv_at_vj0)
            );

          // Decrease q_hat if necessary.
          // This means that q_hat must be decreased if the
          // expression [(u[uj] * b + u[uj - 1] - q_hat * v[vj0 - 1]) * b]
          // exceeds the range of uintwide_t.

          for(auto t = static_cast<double_limb_type>(u_j_j1 - static_cast<double_limb_type>(q_hat * static_cast<double_limb_type>(vv_at_vj0)));
                     ;
                       --q_hat, t = static_cast<double_limb_type>(t + vv_at_vj0))
          {
            if(   (detail::make_hi<limb_type>(t) != static_cast<limb_type>(UINT8_C(0)))
               || (   static_cast<double_limb_type>(static_cast<double_limb_type>(vv_at_vj0_minus_one) * q_hat)
                   <= static_cast<double_limb_type>(static_cast<double_limb_type>(t << static_cast<unsigned>(std::numeric_limits<limb_type>::digits)) + *detail::advance_and_point(uu.cbegin(), static_cast<size_t>(uj - 2U)))))
            {
              break;
            }
          }

          {
            // Step D4: Multiply and subtract.
            // Replace u[j, ... j + n] by u[j, ... j + n] - q_hat * v[1, ... n].

            // Set nv = q_hat * (v[1, ... n]).
            uu_array_type nv { };

            *(nv.begin() + static_cast<size_t>(n)) = eval_multiply_1d(nv.begin(), vv.cbegin(), q_hat, n);

            const auto has_borrow =
              eval_subtract_n
              (
                detail::advance_and_point(uu.begin(),  static_cast<size_t>(static_cast<local_uint_index_type>(uj - n))),
                detail::advance_and_point(uu.cbegin(), static_cast<size_t>(static_cast<local_uint_index_type>(uj - n))),
                nv.cbegin(),
                static_cast<unsigned_fast_type>
                (
                  static_cast<local_uint_index_type>(n + static_cast<local_uint_index_type>(UINT8_C(1)))
                )
              );

            // Step D5: Test the remainder.
            // Set the result value: Set result.m_data[m - j] = q_hat.
            // Use the condition (u[j] < 0), in other words if the borrow
            // is non-zero, then step D6 needs to be carried out.

            if(has_borrow)
            {
              --q_hat;

              // Step D6: Add back.
              // Add v[1, ... n] back to u[j, ... j + n],
              // and decrease the result by 1.

              static_cast<void>
              (
                eval_add_n(uu.begin() + static_cast<size_t>(static_cast<local_uint_index_type>(uj - n)),
                           detail::advance_and_point(uu.cbegin(), static_cast<size_t>(static_cast<local_uint_index_type>(uj - n))),
                           vv.cbegin(),
                           static_cast<unsigned_fast_type>(n))
              );
            }
          }

          // Get the result data.
          *values_at_m_minus_j_it = static_cast<limb_type>(q_hat);

          if(j < m)
          {
            --values_at_m_minus_j_it;
          }
        }

        // Clear the data elements that have not
        // been computed in the division algorithm.
        {
          const auto m_plus_one =
            static_cast<local_uint_index_type>
            (
              static_cast<local_uint_index_type>(m) + static_cast<local_uint_index_type>(UINT8_C(1))
            );

          detail::fill_unsafe(detail::advance_and_point(values.begin(), m_plus_one), values.end(), static_cast<limb_type>(UINT8_C(0)));
        }

        if(remainder != nullptr)
        {
          auto rl_it_fwd = // NOLINT(llvm-qualified-auto,readability-qualified-auto)
            detail::advance_and_point(remainder->values.begin(), static_cast<signed_fast_type>(n));

          if(d == static_cast<limb_type>(UINT8_C(1)))
          {
            detail::copy_unsafe(uu.cbegin(),
                                detail::advance_and_point(uu.cbegin(), static_cast<size_t>(static_cast<local_uint_index_type>(number_of_limbs - v_offset))),
                                remainder->values.begin());
          }
          else
          {
            auto previous_u = static_cast<limb_type>(UINT8_C(0));

            auto rl_it_rev = static_cast<typename representation_type::reverse_iterator>(rl_it_fwd);

            auto ul =
              static_cast<signed_fast_type>
              (
                static_cast<size_t>
                (
                    number_of_limbs
                  - static_cast<size_t>(v_offset + static_cast<size_t>(UINT8_C(1)))
                )
              );

            for( ; rl_it_rev != remainder->values.rend(); ++rl_it_rev, --ul) // NOLINT(altera-id-dependent-backward-branch)
            {
              const auto t =
                static_cast<double_limb_type>
                (
                    *(uu.cbegin() + static_cast<size_t>(ul))
                  + static_cast<double_limb_type>
                    (
                      static_cast<double_limb_type>(previous_u) << static_cast<unsigned>(std::numeric_limits<limb_type>::digits)
                    )
                );

              *rl_it_rev = static_cast<limb_type>(static_cast<double_limb_type>(t / d));
              previous_u = static_cast<limb_type>(static_cast<double_limb_type>(t - static_cast<double_limb_type>(static_cast<double_limb_type>(d) * *rl_it_rev)));
            }
          }

          detail::fill_unsafe(rl_it_fwd, remainder->values.end(), static_cast<limb_type>(UINT8_C(0)));
        }
      }
    }

    template<const size_t RePhraseWidth2 = Width2,
             std::enable_if_t<(RePhraseWidth2 <= static_cast<size_t>(std::numeric_limits<limb_type>::digits))> const* = nullptr>
    WIDE_INTEGER_CONSTEXPR auto eval_divide_knuth_core(const unsigned_fast_type u_offset,
                                                       const unsigned_fast_type v_offset,
                                                       const uintwide_t& other,
                                                             uintwide_t* remainder) -> void
    {
      static_cast<void>(v_offset);

      // The denominator has one single limb.
      // Use a one-dimensional division algorithm.
      const auto short_denominator = static_cast<limb_type>(*other.values.cbegin());

      eval_divide_by_single_limb(short_denominator, u_offset, remainder);
    }

    template<typename IntegralType>
    static constexpr auto exceeds_width(IntegralType n) -> bool
    {
      return (static_cast<size_t>(n) >= uintwide_t::my_width2);
    }

    WIDE_INTEGER_NODISCARD constexpr auto right_shift_fill_value() const -> limb_type
    {
      return
        static_cast<limb_type>
        (
          (!is_neg(*this)) ? static_cast<limb_type>(UINT8_C(0))
                           : (std::numeric_limits<limb_type>::max)()
        );
    }

    template<typename IntegralType>
    WIDE_INTEGER_CONSTEXPR auto shl(IntegralType n) -> void
    {
      const auto offset =
        (std::min)(static_cast<unsigned_fast_type>(static_cast<unsigned_fast_type>(n) / static_cast<unsigned_fast_type>(std::numeric_limits<limb_type>::digits)),
                   static_cast<unsigned_fast_type>(number_of_limbs));

      const auto left_shift_amount = static_cast<unsigned_fast_type>(static_cast<unsigned_fast_type>(n) % static_cast<unsigned_fast_type>(std::numeric_limits<limb_type>::digits));

      if(offset > static_cast<unsigned_fast_type>(UINT8_C(0)))
      {
        std::copy_backward(values.cbegin(),
                           detail::advance_and_point(values.cbegin(), static_cast<size_t>(number_of_limbs - offset)),
                           detail::advance_and_point(values.begin(), static_cast<size_t>(number_of_limbs)));

        detail::fill_unsafe(values.begin(), detail::advance_and_point(values.begin(), static_cast<size_t>(offset)), static_cast<limb_type>(UINT8_C(0)));
      }

      using local_integral_type = unsigned_fast_type;

      if(left_shift_amount != static_cast<local_integral_type>(UINT8_C(0)))
      {
        auto part_from_previous_value = static_cast<limb_type>(UINT8_C(0));

        auto ai = detail::advance_and_point(values.begin(), offset); // NOLINT(llvm-qualified-auto,readability-qualified-auto)

        while(ai != values.end()) // NOLINT(altera-id-dependent-backward-branch)
        {
          const auto t = *ai;

          *ai++ =
            static_cast<limb_type>
            (
                static_cast<limb_type>(t << static_cast<local_integral_type>(left_shift_amount))
              | part_from_previous_value
            );

          const auto right_shift_previous_value =
            static_cast<local_integral_type>
            (
              static_cast<unsigned_fast_type>
              (
                  static_cast<std::int_fast32_t>(std::numeric_limits<limb_type>::digits)
                - static_cast<std::int_fast32_t>(left_shift_amount)
              )
            );

          part_from_previous_value = static_cast<limb_type>(t >> right_shift_previous_value);
        }
      }
    }

    template<typename IntegralType>
    WIDE_INTEGER_CONSTEXPR auto shr(IntegralType n) -> void
    {
      const auto offset =
        (std::min)(static_cast<unsigned_fast_type>(static_cast<unsigned_fast_type>(n) / static_cast<unsigned_fast_type>(std::numeric_limits<limb_type>::digits)),
                   static_cast<unsigned_fast_type>(number_of_limbs));

      const auto right_shift_amount = static_cast<std::uint_fast16_t>(static_cast<unsigned_fast_type>(n) % static_cast<unsigned_fast_type>(std::numeric_limits<limb_type>::digits));

      if(static_cast<size_t>(offset) > static_cast<size_t>(UINT8_C(0)))
      {
        detail::copy_unsafe(detail::advance_and_point(values.cbegin(), static_cast<size_t>(offset)),
                            detail::advance_and_point(values.cbegin(), static_cast<size_t>(number_of_limbs)),
                            values.begin());

        detail::fill_unsafe(detail::advance_and_point(values.begin(), static_cast<size_t>(static_cast<size_t>(number_of_limbs) - static_cast<size_t>(offset))),
                            values.end(),
                            right_shift_fill_value());
      }

      using local_integral_type = unsigned_fast_type;

      if(right_shift_amount != static_cast<local_integral_type>(UINT8_C(0)))
      {
        auto part_from_previous_value =
          static_cast<limb_type>
          (
            (!is_neg(*this))
              ? static_cast<limb_type>(UINT8_C(0))
              : static_cast<limb_type>((std::numeric_limits<limb_type>::max)() << static_cast<std::uint_fast16_t>(static_cast<std::uint_fast16_t>(std::numeric_limits<limb_type>::digits) - right_shift_amount))
          );

        auto r_ai =
          static_cast<typename representation_type::reverse_iterator>
          (
            detail::advance_and_point
            (
              values.begin(),
              static_cast<signed_fast_type>
              (
                static_cast<unsigned_fast_type>(number_of_limbs) - offset
              )
            )
          );

        while(r_ai != values.rend()) // NOLINT(altera-id-dependent-backward-branch)
        {
          const auto t = *r_ai;

          *r_ai++ = static_cast<limb_type>(static_cast<limb_type>(t >> static_cast<local_integral_type>(right_shift_amount)) | part_from_previous_value);

          const auto left_shift_previous_value =
            static_cast<local_integral_type>
            (
              static_cast<unsigned_fast_type>
              (
                  static_cast<std::int_fast32_t>(std::numeric_limits<limb_type>::digits)
                - static_cast<std::int_fast32_t>(right_shift_amount)
              )
            );

          part_from_previous_value = static_cast<limb_type>(t << left_shift_previous_value);
        }
      }
    }

    // Read string function.
    WIDE_INTEGER_CONSTEXPR auto rd_string(const char* str_input) -> bool // NOLINT(readability-function-cognitive-complexity)
    {
      detail::fill_unsafe(values.begin(), values.end(), static_cast<limb_type>(UINT8_C(0)));

      const auto str_length = detail::strlen_unsafe(str_input);

      auto base = static_cast<std::uint_fast8_t>(UINT8_C(10));

      auto pos = static_cast<unsigned_fast_type>(UINT8_C(0));

      // Detect: Is there a plus sign?
      // And if there is a plus sign, skip over the plus sign.
      if((str_length > static_cast<unsigned_fast_type>(UINT8_C(0))) && (str_input[static_cast<std::size_t>(UINT8_C(0))] == '+')) // NOLINT(cppcoreguidelines-pro-bounds-pointer-arithmetic)
      {
        ++pos;
      }

      auto str_has_neg_sign = false;

      // Detect: Is there a minus sign?
      // And if there is a minus sign, skip over the minus sign.
      if((str_length > static_cast<unsigned_fast_type>(UINT8_C(0))) && (str_input[static_cast<std::size_t>(UINT8_C(0))] == '-')) // NOLINT(cppcoreguidelines-pro-bounds-pointer-arithmetic)
      {
        str_has_neg_sign = true;

        ++pos;
      }

      // Perform a dynamic detection of the base.
      if(str_length > static_cast<unsigned_fast_type>(static_cast<std::size_t>(pos) + static_cast<std::size_t>(UINT8_C(0))))
      {
        const auto might_be_oct_or_hex = ((str_input[static_cast<std::size_t>(static_cast<std::size_t>(pos) + static_cast<std::size_t>(UINT8_C(0)))] == '0') && (str_length > static_cast<unsigned_fast_type>(static_cast<std::size_t>(pos) + static_cast<std::size_t>(UINT8_C(0))))); // NOLINT(cppcoreguidelines-pro-bounds-pointer-arithmetic)

        if(might_be_oct_or_hex)
        {
          if((str_input[static_cast<std::size_t>(static_cast<std::size_t>(pos) + static_cast<std::size_t>(UINT8_C(1)))] >= '0') && (str_input[static_cast<std::size_t>(static_cast<std::size_t>(pos) + static_cast<std::size_t>(UINT8_C(1)))] <= '8')) // NOLINT(cppcoreguidelines-pro-bounds-pointer-arithmetic)
          {
            // The input format is octal.
            base = static_cast<std::uint_fast8_t>(UINT8_C(8));

            pos = static_cast<unsigned_fast_type>(static_cast<std::size_t>(pos) + static_cast<std::size_t>(UINT8_C(1)));
          }
          else if((str_input[static_cast<std::size_t>(static_cast<std::size_t>(pos) + static_cast<std::size_t>(UINT8_C(1)))] == 'x') || (str_input[static_cast<std::size_t>(static_cast<std::size_t>(pos) + static_cast<std::size_t>(UINT8_C(1)))] == 'X')) // NOLINT(cppcoreguidelines-pro-bounds-pointer-arithmetic)
          {
            // The input format is hexadecimal.
            base = static_cast<std::uint_fast8_t>(UINT8_C(16));

            pos = static_cast<unsigned_fast_type>(static_cast<std::size_t>(pos) + static_cast<std::size_t>(UINT8_C(2)));
          }
        }
        else if((str_input[static_cast<std::size_t>(static_cast<std::size_t>(pos) + static_cast<std::size_t>(UINT8_C(0)))] >= '0') && (str_input[static_cast<std::size_t>(static_cast<std::size_t>(pos) + static_cast<std::size_t>(UINT8_C(0)))] <= '9')) // NOLINT(cppcoreguidelines-pro-bounds-pointer-arithmetic)
        {
          // The input format is decimal.
          ;
        }
      }

      auto char_is_valid = true;

      while((pos < str_length) && char_is_valid) // NOLINT(altera-id-dependent-backward-branch)
      {
        const auto c = str_input[pos++]; // NOLINT(cppcoreguidelines-pro-bounds-pointer-arithmetic)

        const auto char_is_apostrophe = (c == static_cast<char>(39));

        if(!char_is_apostrophe)
        {
          if(base == static_cast<std::uint_fast8_t>(UINT8_C(8)))
          {
            char_is_valid = ((c >= '0') && (c <= '8'));

            if(char_is_valid)
            {
              const auto uc_oct = static_cast<std::uint8_t>(c - static_cast<char>(UINT8_C(0x30)));

              static_cast<void>(operator<<=(static_cast<unsigned>(UINT8_C(3))));

              *values.begin() = static_cast<limb_type>(*values.begin() | uc_oct);
            }
          }
          else if(base == static_cast<std::uint_fast8_t>(UINT8_C(10)))
          {
            char_is_valid = ((c >= '0') && (c <= '9'));

            if(char_is_valid)
            {
              const auto uc_dec = static_cast<std::uint8_t>(c - static_cast<char>(UINT8_C(0x30)));

              static_cast<void>(mul_by_limb(static_cast<limb_type>(UINT8_C(10))));

              static_cast<void>(operator+=(uc_dec));
            }
          }
          else if(base == static_cast<std::uint_fast8_t>(UINT8_C(16)))
          {
            const auto char_is_a_to_f_lo((c >= 'a') && (c <= 'f'));
            const auto char_is_a_to_f_hi((c >= 'A') && (c <= 'F'));
            const auto char_is_0_to_9   ((c >= '0') && (c <= '9'));

            char_is_valid = (char_is_a_to_f_lo || char_is_a_to_f_hi || char_is_0_to_9);

            if(char_is_valid)
            {
              auto uc_hex = static_cast<std::uint8_t>(UINT8_C(0));

              if     (char_is_a_to_f_lo) { uc_hex = static_cast<std::uint8_t>(c - static_cast<char>(UINT8_C(  87))); }
              else if(char_is_a_to_f_hi) { uc_hex = static_cast<std::uint8_t>(c - static_cast<char>(UINT8_C(  55))); }
              else if(char_is_0_to_9)    { uc_hex = static_cast<std::uint8_t>(c - static_cast<char>(UINT8_C(0x30))); }

              static_cast<void>(operator<<=(static_cast<unsigned>(UINT8_C(4))));

              *values.begin() = static_cast<limb_type>(*values.begin() | uc_hex);
            }
          }
        }
      }

      if(str_has_neg_sign)
      {
        // Exclude this line from code coverage, even though explicit
        // test cases (search for "result_overshift_is_ok") are known
        // to cover this line.
        negate(); // LCOV_EXCL_LINE
      }

      return char_is_valid;
    }

    WIDE_INTEGER_CONSTEXPR auto bitwise_not() -> void // LCOV_EXCL_LINE
    {
      for(auto it = values.begin(); it != values.end(); ++it) // NOLINT(llvm-qualified-auto,readability-qualified-auto,altera-id-dependent-backward-branch)
      {
        *it = static_cast<limb_type>(~(*it));
      }
    } // LCOV_EXCL_LINE

    WIDE_INTEGER_CONSTEXPR auto preincrement() -> void
    {
      // Implement self-increment.

      auto it = values.begin(); // NOLINT(llvm-qualified-auto,readability-qualified-auto) // LCOV_EXCL_LINE

      do
      {
        ++(*it);
      }
      while((*it++ == static_cast<limb_type>(UINT8_C(0))) && (it != values.end())); // NOLINT(altera-id-dependent-backward-branch)
    }

    WIDE_INTEGER_CONSTEXPR auto predecrement() -> void
    {
      // Implement self-decrement.

      auto it = values.begin(); // NOLINT(llvm-qualified-auto,readability-qualified-auto)

      do
      {
        --(*it);
      }
      while((*it++ == (std::numeric_limits<limb_type>::max)()) && (it != values.end())); // NOLINT(altera-id-dependent-backward-branch)
    }
  };

  // Define some convenient unsigned wide integer types.
  using uint64_t    = uintwide_t<static_cast<size_t>(UINT32_C(   64)), std::uint16_t>;
  using uint128_t   = uintwide_t<static_cast<size_t>(UINT32_C(  128)), std::uint32_t>;
  using uint256_t   = uintwide_t<static_cast<size_t>(UINT32_C(  256)), std::uint32_t>;
  using uint512_t   = uintwide_t<static_cast<size_t>(UINT32_C(  512)), std::uint32_t>;
  using uint1024_t  = uintwide_t<static_cast<size_t>(UINT32_C( 1024)), std::uint32_t>;
  using uint2048_t  = uintwide_t<static_cast<size_t>(UINT32_C( 2048)), std::uint32_t>;
  using uint4096_t  = uintwide_t<static_cast<size_t>(UINT32_C( 4096)), std::uint32_t>;
  using uint8192_t  = uintwide_t<static_cast<size_t>(UINT32_C( 8192)), std::uint32_t>;
  using uint16384_t = uintwide_t<static_cast<size_t>(UINT32_C(16384)), std::uint32_t>;
  using uint32768_t = uintwide_t<static_cast<size_t>(UINT32_C(32768)), std::uint32_t>;
  using uint65536_t = uintwide_t<static_cast<size_t>(UINT32_C(65536)), std::uint32_t>;

  #if !defined(WIDE_INTEGER_DISABLE_TRIVIAL_COPY_AND_STD_LAYOUT_CHECKS)
  static_assert(std::is_trivially_copyable<uint64_t   >::value, "uintwide_t must be trivially copyable.");
  static_assert(std::is_trivially_copyable<uint128_t  >::value, "uintwide_t must be trivially copyable.");
  static_assert(std::is_trivially_copyable<uint256_t  >::value, "uintwide_t must be trivially copyable.");
  static_assert(std::is_trivially_copyable<uint512_t  >::value, "uintwide_t must be trivially copyable.");
  static_assert(std::is_trivially_copyable<uint1024_t >::value, "uintwide_t must be trivially copyable.");
  static_assert(std::is_trivially_copyable<uint2048_t >::value, "uintwide_t must be trivially copyable.");
  static_assert(std::is_trivially_copyable<uint4096_t >::value, "uintwide_t must be trivially copyable.");
  static_assert(std::is_trivially_copyable<uint8192_t >::value, "uintwide_t must be trivially copyable.");
  static_assert(std::is_trivially_copyable<uint16384_t>::value, "uintwide_t must be trivially copyable.");
  static_assert(std::is_trivially_copyable<uint32768_t>::value, "uintwide_t must be trivially copyable.");
  static_assert(std::is_trivially_copyable<uint65536_t>::value, "uintwide_t must be trivially copyable.");

  static_assert(std::is_standard_layout<uint64_t   >::value, "uintwide_t must have standard layout.");
  static_assert(std::is_standard_layout<uint128_t  >::value, "uintwide_t must have standard layout.");
  static_assert(std::is_standard_layout<uint256_t  >::value, "uintwide_t must have standard layout.");
  static_assert(std::is_standard_layout<uint512_t  >::value, "uintwide_t must have standard layout.");
  static_assert(std::is_standard_layout<uint1024_t >::value, "uintwide_t must have standard layout.");
  static_assert(std::is_standard_layout<uint2048_t >::value, "uintwide_t must have standard layout.");
  static_assert(std::is_standard_layout<uint4096_t >::value, "uintwide_t must have standard layout.");
  static_assert(std::is_standard_layout<uint8192_t >::value, "uintwide_t must have standard layout.");
  static_assert(std::is_standard_layout<uint16384_t>::value, "uintwide_t must have standard layout.");
  static_assert(std::is_standard_layout<uint32768_t>::value, "uintwide_t must have standard layout.");
  static_assert(std::is_standard_layout<uint65536_t>::value, "uintwide_t must have standard layout.");
  #endif

  using  int64_t    = uintwide_t<static_cast<size_t>(UINT32_C(   64)), std::uint16_t, void, true>;
  using  int128_t   = uintwide_t<static_cast<size_t>(UINT32_C(  128)), std::uint32_t, void, true>;
  using  int256_t   = uintwide_t<static_cast<size_t>(UINT32_C(  256)), std::uint32_t, void, true>;
  using  int512_t   = uintwide_t<static_cast<size_t>(UINT32_C(  512)), std::uint32_t, void, true>;
  using  int1024_t  = uintwide_t<static_cast<size_t>(UINT32_C( 1024)), std::uint32_t, void, true>;
  using  int2048_t  = uintwide_t<static_cast<size_t>(UINT32_C( 2048)), std::uint32_t, void, true>;
  using  int4096_t  = uintwide_t<static_cast<size_t>(UINT32_C( 4096)), std::uint32_t, void, true>;
  using  int8192_t  = uintwide_t<static_cast<size_t>(UINT32_C( 8192)), std::uint32_t, void, true>;
  using  int16384_t = uintwide_t<static_cast<size_t>(UINT32_C(16384)), std::uint32_t, void, true>;
  using  int32768_t = uintwide_t<static_cast<size_t>(UINT32_C(32768)), std::uint32_t, void, true>;
  using  int65536_t = uintwide_t<static_cast<size_t>(UINT32_C(65536)), std::uint32_t, void, true>;

  #if !defined(WIDE_INTEGER_DISABLE_TRIVIAL_COPY_AND_STD_LAYOUT_CHECKS)
  static_assert(std::is_trivially_copyable<int64_t   >::value, "uintwide_t must be trivially copyable.");
  static_assert(std::is_trivially_copyable<int128_t  >::value, "uintwide_t must be trivially copyable.");
  static_assert(std::is_trivially_copyable<int256_t  >::value, "uintwide_t must be trivially copyable.");
  static_assert(std::is_trivially_copyable<int512_t  >::value, "uintwide_t must be trivially copyable.");
  static_assert(std::is_trivially_copyable<int1024_t >::value, "uintwide_t must be trivially copyable.");
  static_assert(std::is_trivially_copyable<int2048_t >::value, "uintwide_t must be trivially copyable.");
  static_assert(std::is_trivially_copyable<int4096_t >::value, "uintwide_t must be trivially copyable.");
  static_assert(std::is_trivially_copyable<int8192_t >::value, "uintwide_t must be trivially copyable.");
  static_assert(std::is_trivially_copyable<int16384_t>::value, "uintwide_t must be trivially copyable.");
  static_assert(std::is_trivially_copyable<int32768_t>::value, "uintwide_t must be trivially copyable.");
  static_assert(std::is_trivially_copyable<int65536_t>::value, "uintwide_t must be trivially copyable.");

  static_assert(std::is_standard_layout<int64_t   >::value, "uintwide_t must have standard layout.");
  static_assert(std::is_standard_layout<int128_t  >::value, "uintwide_t must have standard layout.");
  static_assert(std::is_standard_layout<int256_t  >::value, "uintwide_t must have standard layout.");
  static_assert(std::is_standard_layout<int512_t  >::value, "uintwide_t must have standard layout.");
  static_assert(std::is_standard_layout<int1024_t >::value, "uintwide_t must have standard layout.");
  static_assert(std::is_standard_layout<int2048_t >::value, "uintwide_t must have standard layout.");
  static_assert(std::is_standard_layout<int4096_t >::value, "uintwide_t must have standard layout.");
  static_assert(std::is_standard_layout<int8192_t >::value, "uintwide_t must have standard layout.");
  static_assert(std::is_standard_layout<int16384_t>::value, "uintwide_t must have standard layout.");
  static_assert(std::is_standard_layout<int32768_t>::value, "uintwide_t must have standard layout.");
  static_assert(std::is_standard_layout<int65536_t>::value, "uintwide_t must have standard layout.");
  #endif

  // Insert a base class for numeric_limits<> support.
  // This class inherits from std::numeric_limits<unsigned int>
  // in order to provide limits for a non-specific unsigned type.

  template<const size_t Width2,
           typename LimbType,
           typename AllocatorType,
           const bool IsSigned>
  WIDE_INTEGER_NUM_LIMITS_CLASS_TYPE numeric_limits_uintwide_t_base
    : public std::numeric_limits<std::conditional_t<(!IsSigned), unsigned int, signed int>>
  {
  private:
    using local_wide_integer_type = uintwide_t<Width2, LimbType, AllocatorType, IsSigned>;

  public:
    static constexpr int digits          = (!IsSigned)
                                             ? static_cast<int>(local_wide_integer_type::my_width2)
                                             : static_cast<int>(local_wide_integer_type::my_width2 - static_cast<size_t>(UINT8_C(1)));

    static constexpr int digits10        = static_cast<int>((static_cast<std::uintmax_t>(digits)       * UINTMAX_C(75257499)) / UINTMAX_C(250000000));
    static constexpr int max_digits10    = digits10;
    static constexpr int max_exponent    = digits;
    static constexpr int max_exponent10  = static_cast<int>((static_cast<std::uintmax_t>(max_exponent) * UINTMAX_C(75257499)) / UINTMAX_C(250000000));

    static constexpr auto (max) () -> local_wide_integer_type { return local_wide_integer_type::limits_helper_max   (IsSigned); }
    static constexpr auto (min) () -> local_wide_integer_type { return local_wide_integer_type::limits_helper_min   (IsSigned); }
    static constexpr auto lowest() -> local_wide_integer_type { return local_wide_integer_type::limits_helper_lowest(IsSigned); }
  };

  template<class T>
  struct is_integral : public std::is_integral<T> { };

  template<const size_t Width2,
           typename LimbType,
           typename AllocatorType,
           const bool IsSigned>
  struct is_integral<math::wide_integer::uintwide_t<Width2, LimbType, AllocatorType, IsSigned>>
    : public std::integral_constant<bool, true> { };

  #if(__cplusplus >= 201703L)
  } // namespace math::wide_integer
  #else
  } // namespace wide_integer
  } // namespace math
  #endif

  WIDE_INTEGER_NAMESPACE_END

  namespace std
  {
    // Specialization of std::numeric_limits<uintwide_t>.
    #if defined(WIDE_INTEGER_NAMESPACE)
    template<const WIDE_INTEGER_NAMESPACE::math::wide_integer::size_t Width2,
             typename LimbType,
             typename AllocatorType,
             const bool IsSigned>
    WIDE_INTEGER_NUM_LIMITS_CLASS_TYPE numeric_limits<WIDE_INTEGER_NAMESPACE::math::wide_integer::uintwide_t<Width2, LimbType, AllocatorType, IsSigned>>
      : public WIDE_INTEGER_NAMESPACE::math::wide_integer::numeric_limits_uintwide_t_base<Width2, LimbType, AllocatorType, IsSigned> { };
    #else
    template<const ::math::wide_integer::size_t Width2,
             typename LimbType,
             typename AllocatorType,
             const bool IsSigned>
    WIDE_INTEGER_NUM_LIMITS_CLASS_TYPE numeric_limits<::math::wide_integer::uintwide_t<Width2, LimbType, AllocatorType, IsSigned>>
      : public ::math::wide_integer::numeric_limits_uintwide_t_base<Width2, LimbType, AllocatorType, IsSigned> { };
    #endif
  } // namespace std

  WIDE_INTEGER_NAMESPACE_BEGIN

  #if(__cplusplus >= 201703L)
  namespace math::wide_integer {
  #else
  namespace math { namespace wide_integer { // NOLINT(modernize-concat-nested-namespaces)
  #endif

  // Non-member binary add, sub, mul, div, mod of (uintwide_t op uintwide_t).
  template<const size_t Width2, typename LimbType, typename AllocatorType, const bool IsSigned> constexpr auto operator+ (const uintwide_t<Width2, LimbType, AllocatorType, IsSigned>& u, const uintwide_t<Width2, LimbType, AllocatorType, IsSigned>& v) -> uintwide_t<Width2, LimbType, AllocatorType, IsSigned> { return uintwide_t<Width2, LimbType, AllocatorType, IsSigned>(u).operator+=(v); }
  template<const size_t Width2, typename LimbType, typename AllocatorType, const bool IsSigned> constexpr auto operator- (const uintwide_t<Width2, LimbType, AllocatorType, IsSigned>& u, const uintwide_t<Width2, LimbType, AllocatorType, IsSigned>& v) -> uintwide_t<Width2, LimbType, AllocatorType, IsSigned> { return uintwide_t<Width2, LimbType, AllocatorType, IsSigned>(u).operator-=(v); }
  template<const size_t Width2, typename LimbType, typename AllocatorType, const bool IsSigned> constexpr auto operator* (const uintwide_t<Width2, LimbType, AllocatorType, IsSigned>& u, const uintwide_t<Width2, LimbType, AllocatorType, IsSigned>& v) -> uintwide_t<Width2, LimbType, AllocatorType, IsSigned> { return uintwide_t<Width2, LimbType, AllocatorType, IsSigned>(u).operator*=(v); }
  template<const size_t Width2, typename LimbType, typename AllocatorType, const bool IsSigned> constexpr auto operator/ (const uintwide_t<Width2, LimbType, AllocatorType, IsSigned>& u, const uintwide_t<Width2, LimbType, AllocatorType, IsSigned>& v) -> uintwide_t<Width2, LimbType, AllocatorType, IsSigned> { return uintwide_t<Width2, LimbType, AllocatorType, IsSigned>(u).operator/=(v); }
  template<const size_t Width2, typename LimbType, typename AllocatorType, const bool IsSigned> constexpr auto operator% (const uintwide_t<Width2, LimbType, AllocatorType, IsSigned>& u, const uintwide_t<Width2, LimbType, AllocatorType, IsSigned>& v) -> uintwide_t<Width2, LimbType, AllocatorType, IsSigned> { return uintwide_t<Width2, LimbType, AllocatorType, IsSigned>(u).operator%=(v); }

  // Non-member binary logic operations of (uintwide_t op uintwide_t).
  template<const size_t Width2, typename LimbType, typename AllocatorType, const bool IsSigned> constexpr auto operator| (const uintwide_t<Width2, LimbType, AllocatorType, IsSigned>& u, const uintwide_t<Width2, LimbType, AllocatorType, IsSigned>& v) -> uintwide_t<Width2, LimbType, AllocatorType, IsSigned> { return uintwide_t<Width2, LimbType, AllocatorType, IsSigned>(u).operator|=(v); }
  template<const size_t Width2, typename LimbType, typename AllocatorType, const bool IsSigned> constexpr auto operator^ (const uintwide_t<Width2, LimbType, AllocatorType, IsSigned>& u, const uintwide_t<Width2, LimbType, AllocatorType, IsSigned>& v) -> uintwide_t<Width2, LimbType, AllocatorType, IsSigned> { return uintwide_t<Width2, LimbType, AllocatorType, IsSigned>(u).operator^=(v); }
  template<const size_t Width2, typename LimbType, typename AllocatorType, const bool IsSigned> constexpr auto operator& (const uintwide_t<Width2, LimbType, AllocatorType, IsSigned>& u, const uintwide_t<Width2, LimbType, AllocatorType, IsSigned>& v) -> uintwide_t<Width2, LimbType, AllocatorType, IsSigned> { return uintwide_t<Width2, LimbType, AllocatorType, IsSigned>(u).operator&=(v); }

  // Non-member binary add, sub, mul, div, mod of (uintwide_t op IntegralType).
  template<typename IntegralType, const size_t Width2, typename LimbType, typename AllocatorType, const bool IsSigned> constexpr auto operator+(const uintwide_t<Width2, LimbType, AllocatorType, IsSigned>& u, const IntegralType& v) -> std::enable_if_t<std::is_integral<IntegralType>::value, uintwide_t<Width2, LimbType, AllocatorType, IsSigned>> { return uintwide_t<Width2, LimbType, AllocatorType, IsSigned>(u).operator+=(uintwide_t<Width2, LimbType, AllocatorType, IsSigned>(v)); }
  template<typename IntegralType, const size_t Width2, typename LimbType, typename AllocatorType, const bool IsSigned> constexpr auto operator-(const uintwide_t<Width2, LimbType, AllocatorType, IsSigned>& u, const IntegralType& v) -> std::enable_if_t<std::is_integral<IntegralType>::value, uintwide_t<Width2, LimbType, AllocatorType, IsSigned>> { return uintwide_t<Width2, LimbType, AllocatorType, IsSigned>(u).operator-=(uintwide_t<Width2, LimbType, AllocatorType, IsSigned>(v)); }
  template<typename IntegralType, const size_t Width2, typename LimbType, typename AllocatorType, const bool IsSigned> constexpr auto operator*(const uintwide_t<Width2, LimbType, AllocatorType, IsSigned>& u, const IntegralType& v) -> std::enable_if_t<std::is_integral<IntegralType>::value, uintwide_t<Width2, LimbType, AllocatorType, IsSigned>> { return uintwide_t<Width2, LimbType, AllocatorType, IsSigned>(u).operator*=(uintwide_t<Width2, LimbType, AllocatorType, IsSigned>(v)); }
  template<typename IntegralType, const size_t Width2, typename LimbType, typename AllocatorType, const bool IsSigned> constexpr auto operator/(const uintwide_t<Width2, LimbType, AllocatorType, IsSigned>& u, const IntegralType& v) -> std::enable_if_t<std::is_integral<IntegralType>::value, uintwide_t<Width2, LimbType, AllocatorType, IsSigned>> { return uintwide_t<Width2, LimbType, AllocatorType, IsSigned>(u).operator/=(uintwide_t<Width2, LimbType, AllocatorType, IsSigned>(v)); }

  template<typename IntegralType, const size_t Width2, typename LimbType, typename AllocatorType, const bool IsSigned>
  constexpr auto operator%(const uintwide_t<Width2, LimbType, AllocatorType, IsSigned>& u, const IntegralType& v) -> std::enable_if_t<(   std::is_integral<IntegralType>::value
                                                                                                                                       && std::is_signed<IntegralType>::value),
                                                                                                                                          uintwide_t<Width2, LimbType, AllocatorType, IsSigned>>
  {
    using local_wide_integer_type = uintwide_t<Width2, LimbType, AllocatorType, IsSigned>;

    return local_wide_integer_type(u).operator%=(local_wide_integer_type(v));
  }

  template<typename IntegralType, const size_t Width2, typename LimbType, typename AllocatorType, const bool IsSigned>
  WIDE_INTEGER_CONSTEXPR auto operator%(const uintwide_t<Width2, LimbType, AllocatorType, IsSigned>& u, const IntegralType& v) -> std::enable_if_t<(    std::is_integral<IntegralType>::value
                                                                                                                                                    &&  std::is_unsigned<IntegralType>::value
                                                                                                                                                    && (std::numeric_limits<IntegralType>::digits <= std::numeric_limits<LimbType>::digits)),
                                                                                                                                                    typename uintwide_t<Width2, LimbType, AllocatorType, IsSigned>::limb_type>
  {
    using local_wide_integer_type = uintwide_t<Width2, LimbType, AllocatorType, IsSigned>;

    const auto u_is_neg = local_wide_integer_type::is_neg(u);

    local_wide_integer_type remainder;

    local_wide_integer_type((!u_is_neg) ? u : -u).eval_divide_by_single_limb(v, static_cast<unsigned_fast_type>(UINT8_C(0)), &remainder);

    using local_limb_type = typename local_wide_integer_type::limb_type;

    auto u_rem = static_cast<local_limb_type>(remainder);

    return ((!u_is_neg) ? u_rem : static_cast<local_limb_type>(static_cast<local_limb_type>(~u_rem) + static_cast<local_limb_type>(UINT8_C(1))));
  }

  template<typename IntegralType, const size_t Width2, typename LimbType, typename AllocatorType, const bool IsSigned>
  constexpr auto operator%(const uintwide_t<Width2, LimbType, AllocatorType, IsSigned>& u, const IntegralType& v) -> std::enable_if_t<(    std::is_integral<IntegralType>::value
                                                                                                                                       &&  std::is_unsigned<IntegralType>::value
                                                                                                                                       && (std::numeric_limits<IntegralType>::digits > std::numeric_limits<LimbType>::digits)),
                                                                                                                                       uintwide_t<Width2, LimbType, AllocatorType, IsSigned>>
  {
    using local_wide_integer_type = uintwide_t<Width2, LimbType, AllocatorType, IsSigned>;

    return local_wide_integer_type(u).operator%=(local_wide_integer_type(v));
  }

  // Non-member binary add, sub, mul, div, mod of (IntegralType op uintwide_t).
  template<typename IntegralType, const size_t Width2, typename LimbType, typename AllocatorType, const bool IsSigned> constexpr auto operator+(const IntegralType& u, const uintwide_t<Width2, LimbType, AllocatorType, IsSigned>& v) -> std::enable_if_t<std::is_integral<IntegralType>::value, uintwide_t<Width2, LimbType, AllocatorType, IsSigned>> { return uintwide_t<Width2, LimbType, AllocatorType, IsSigned>(u).operator+=(v); }
  template<typename IntegralType, const size_t Width2, typename LimbType, typename AllocatorType, const bool IsSigned> constexpr auto operator-(const IntegralType& u, const uintwide_t<Width2, LimbType, AllocatorType, IsSigned>& v) -> std::enable_if_t<std::is_integral<IntegralType>::value, uintwide_t<Width2, LimbType, AllocatorType, IsSigned>> { return uintwide_t<Width2, LimbType, AllocatorType, IsSigned>(u).operator-=(v); }
  template<typename IntegralType, const size_t Width2, typename LimbType, typename AllocatorType, const bool IsSigned> constexpr auto operator*(const IntegralType& u, const uintwide_t<Width2, LimbType, AllocatorType, IsSigned>& v) -> std::enable_if_t<std::is_integral<IntegralType>::value, uintwide_t<Width2, LimbType, AllocatorType, IsSigned>> { return uintwide_t<Width2, LimbType, AllocatorType, IsSigned>(u).operator*=(v); }
  template<typename IntegralType, const size_t Width2, typename LimbType, typename AllocatorType, const bool IsSigned> constexpr auto operator/(const IntegralType& u, const uintwide_t<Width2, LimbType, AllocatorType, IsSigned>& v) -> std::enable_if_t<std::is_integral<IntegralType>::value, uintwide_t<Width2, LimbType, AllocatorType, IsSigned>> { return uintwide_t<Width2, LimbType, AllocatorType, IsSigned>(u).operator/=(v); }
  template<typename IntegralType, const size_t Width2, typename LimbType, typename AllocatorType, const bool IsSigned> constexpr auto operator%(const IntegralType& u, const uintwide_t<Width2, LimbType, AllocatorType, IsSigned>& v) -> std::enable_if_t<std::is_integral<IntegralType>::value, uintwide_t<Width2, LimbType, AllocatorType, IsSigned>> { return uintwide_t<Width2, LimbType, AllocatorType, IsSigned>(u).operator%=(v); }

  #if !defined(WIDE_INTEGER_DISABLE_FLOAT_INTEROP)
  // Non-member binary add, sub, mul, div, mod of (uintwide_t op FloatingPointType).
  template<typename FloatingPointType, const size_t Width2, typename LimbType, typename AllocatorType, const bool IsSigned> constexpr auto operator+(const uintwide_t<Width2, LimbType, AllocatorType, IsSigned>& u, const FloatingPointType& f) -> std::enable_if_t<std::is_floating_point<FloatingPointType>::value, uintwide_t<Width2, LimbType, AllocatorType, IsSigned>> { return uintwide_t<Width2, LimbType, AllocatorType, IsSigned>(u).operator+=(uintwide_t<Width2, LimbType, AllocatorType, IsSigned>(f)); }
  template<typename FloatingPointType, const size_t Width2, typename LimbType, typename AllocatorType, const bool IsSigned> constexpr auto operator-(const uintwide_t<Width2, LimbType, AllocatorType, IsSigned>& u, const FloatingPointType& f) -> std::enable_if_t<std::is_floating_point<FloatingPointType>::value, uintwide_t<Width2, LimbType, AllocatorType, IsSigned>> { return uintwide_t<Width2, LimbType, AllocatorType, IsSigned>(u).operator-=(uintwide_t<Width2, LimbType, AllocatorType, IsSigned>(f)); }
  template<typename FloatingPointType, const size_t Width2, typename LimbType, typename AllocatorType, const bool IsSigned> constexpr auto operator*(const uintwide_t<Width2, LimbType, AllocatorType, IsSigned>& u, const FloatingPointType& f) -> std::enable_if_t<std::is_floating_point<FloatingPointType>::value, uintwide_t<Width2, LimbType, AllocatorType, IsSigned>> { return uintwide_t<Width2, LimbType, AllocatorType, IsSigned>(u).operator*=(uintwide_t<Width2, LimbType, AllocatorType, IsSigned>(f)); }
  template<typename FloatingPointType, const size_t Width2, typename LimbType, typename AllocatorType, const bool IsSigned> constexpr auto operator/(const uintwide_t<Width2, LimbType, AllocatorType, IsSigned>& u, const FloatingPointType& f) -> std::enable_if_t<std::is_floating_point<FloatingPointType>::value, uintwide_t<Width2, LimbType, AllocatorType, IsSigned>> { return uintwide_t<Width2, LimbType, AllocatorType, IsSigned>(u).operator/=(uintwide_t<Width2, LimbType, AllocatorType, IsSigned>(f)); }
  template<typename FloatingPointType, const size_t Width2, typename LimbType, typename AllocatorType, const bool IsSigned> constexpr auto operator%(const uintwide_t<Width2, LimbType, AllocatorType, IsSigned>& u, const FloatingPointType& f) -> std::enable_if_t<std::is_floating_point<FloatingPointType>::value, uintwide_t<Width2, LimbType, AllocatorType, IsSigned>> { return uintwide_t<Width2, LimbType, AllocatorType, IsSigned>(u).operator%=(uintwide_t<Width2, LimbType, AllocatorType, IsSigned>(f)); }

  // Non-member binary add, sub, mul, div, mod of (FloatingPointType op uintwide_t).
  template<typename FloatingPointType, const size_t Width2, typename LimbType, typename AllocatorType, const bool IsSigned> constexpr auto operator+(const FloatingPointType& f, const uintwide_t<Width2, LimbType, AllocatorType, IsSigned>& v) -> std::enable_if_t<std::is_floating_point<FloatingPointType>::value, uintwide_t<Width2, LimbType, AllocatorType, IsSigned>> { return uintwide_t<Width2, LimbType, AllocatorType, IsSigned>(f).operator+=(v); }
  template<typename FloatingPointType, const size_t Width2, typename LimbType, typename AllocatorType, const bool IsSigned> constexpr auto operator-(const FloatingPointType& f, const uintwide_t<Width2, LimbType, AllocatorType, IsSigned>& v) -> std::enable_if_t<std::is_floating_point<FloatingPointType>::value, uintwide_t<Width2, LimbType, AllocatorType, IsSigned>> { return uintwide_t<Width2, LimbType, AllocatorType, IsSigned>(f).operator-=(v); }
  template<typename FloatingPointType, const size_t Width2, typename LimbType, typename AllocatorType, const bool IsSigned> constexpr auto operator*(const FloatingPointType& f, const uintwide_t<Width2, LimbType, AllocatorType, IsSigned>& v) -> std::enable_if_t<std::is_floating_point<FloatingPointType>::value, uintwide_t<Width2, LimbType, AllocatorType, IsSigned>> { return uintwide_t<Width2, LimbType, AllocatorType, IsSigned>(f).operator*=(v); }
  template<typename FloatingPointType, const size_t Width2, typename LimbType, typename AllocatorType, const bool IsSigned> constexpr auto operator/(const FloatingPointType& f, const uintwide_t<Width2, LimbType, AllocatorType, IsSigned>& v) -> std::enable_if_t<std::is_floating_point<FloatingPointType>::value, uintwide_t<Width2, LimbType, AllocatorType, IsSigned>> { return uintwide_t<Width2, LimbType, AllocatorType, IsSigned>(f).operator/=(v); }
  template<typename FloatingPointType, const size_t Width2, typename LimbType, typename AllocatorType, const bool IsSigned> constexpr auto operator%(const FloatingPointType& f, const uintwide_t<Width2, LimbType, AllocatorType, IsSigned>& v) -> std::enable_if_t<std::is_floating_point<FloatingPointType>::value, uintwide_t<Width2, LimbType, AllocatorType, IsSigned>> { return uintwide_t<Width2, LimbType, AllocatorType, IsSigned>(f).operator%=(v); }
  #endif

  // Non-member binary logic operations of (uintwide_t op IntegralType).
  template<typename IntegralType, const size_t Width2, typename LimbType, typename AllocatorType, const bool IsSigned> constexpr auto operator|(const uintwide_t<Width2, LimbType, AllocatorType, IsSigned>& u, const IntegralType& v) -> std::enable_if_t<std::is_integral<IntegralType>::value, uintwide_t<Width2, LimbType, AllocatorType, IsSigned>> { return uintwide_t<Width2, LimbType, AllocatorType, IsSigned>(u).operator|=(uintwide_t<Width2, LimbType, AllocatorType, IsSigned>(v)); }
  template<typename IntegralType, const size_t Width2, typename LimbType, typename AllocatorType, const bool IsSigned> constexpr auto operator^(const uintwide_t<Width2, LimbType, AllocatorType, IsSigned>& u, const IntegralType& v) -> std::enable_if_t<std::is_integral<IntegralType>::value, uintwide_t<Width2, LimbType, AllocatorType, IsSigned>> { return uintwide_t<Width2, LimbType, AllocatorType, IsSigned>(u).operator^=(uintwide_t<Width2, LimbType, AllocatorType, IsSigned>(v)); }
  template<typename IntegralType, const size_t Width2, typename LimbType, typename AllocatorType, const bool IsSigned> constexpr auto operator&(const uintwide_t<Width2, LimbType, AllocatorType, IsSigned>& u, const IntegralType& v) -> std::enable_if_t<std::is_integral<IntegralType>::value, uintwide_t<Width2, LimbType, AllocatorType, IsSigned>> { return uintwide_t<Width2, LimbType, AllocatorType, IsSigned>(u).operator&=(uintwide_t<Width2, LimbType, AllocatorType, IsSigned>(v)); }

  // Non-member binary binary logic operations of (IntegralType op uintwide_t).
  template<typename IntegralType, const size_t Width2, typename LimbType, typename AllocatorType, const bool IsSigned> constexpr auto operator|(const IntegralType& u, const uintwide_t<Width2, LimbType, AllocatorType, IsSigned>& v) -> std::enable_if_t<std::is_integral<IntegralType>::value, uintwide_t<Width2, LimbType, AllocatorType, IsSigned>> { return uintwide_t<Width2, LimbType, AllocatorType, IsSigned>(u).operator|=(v); }
  template<typename IntegralType, const size_t Width2, typename LimbType, typename AllocatorType, const bool IsSigned> constexpr auto operator^(const IntegralType& u, const uintwide_t<Width2, LimbType, AllocatorType, IsSigned>& v) -> std::enable_if_t<std::is_integral<IntegralType>::value, uintwide_t<Width2, LimbType, AllocatorType, IsSigned>> { return uintwide_t<Width2, LimbType, AllocatorType, IsSigned>(u).operator^=(v); }
  template<typename IntegralType, const size_t Width2, typename LimbType, typename AllocatorType, const bool IsSigned> constexpr auto operator&(const IntegralType& u, const uintwide_t<Width2, LimbType, AllocatorType, IsSigned>& v) -> std::enable_if_t<std::is_integral<IntegralType>::value, uintwide_t<Width2, LimbType, AllocatorType, IsSigned>> { return uintwide_t<Width2, LimbType, AllocatorType, IsSigned>(u).operator&=(v); }

  // Non-member shift functions of (uintwide_t shift IntegralType).
  template<typename IntegralType, const size_t Width2, typename LimbType, typename AllocatorType, const bool IsSigned> constexpr auto operator<<(const uintwide_t<Width2, LimbType, AllocatorType, IsSigned>& u, const IntegralType n) -> std::enable_if_t<std::is_integral<IntegralType>::value, uintwide_t<Width2, LimbType, AllocatorType, IsSigned>> { return uintwide_t<Width2, LimbType, AllocatorType, IsSigned>(u).operator<<=(n); }
  template<typename IntegralType, const size_t Width2, typename LimbType, typename AllocatorType, const bool IsSigned> constexpr auto operator>>(const uintwide_t<Width2, LimbType, AllocatorType, IsSigned>& u, const IntegralType n) -> std::enable_if_t<std::is_integral<IntegralType>::value, uintwide_t<Width2, LimbType, AllocatorType, IsSigned>> { return uintwide_t<Width2, LimbType, AllocatorType, IsSigned>(u).operator>>=(n); }

  // Non-member comparison functions of (uintwide_t cmp uintwide_t).
  template<const size_t Width2, typename LimbType, typename AllocatorType, const bool IsSigned> constexpr auto operator==(const uintwide_t<Width2, LimbType, AllocatorType, IsSigned>& u, const uintwide_t<Width2, LimbType, AllocatorType, IsSigned>& v) -> bool { return u.operator==(v); }
  template<const size_t Width2, typename LimbType, typename AllocatorType, const bool IsSigned> constexpr auto operator!=(const uintwide_t<Width2, LimbType, AllocatorType, IsSigned>& u, const uintwide_t<Width2, LimbType, AllocatorType, IsSigned>& v) -> bool { return u.operator!=(v); }
  template<const size_t Width2, typename LimbType, typename AllocatorType, const bool IsSigned> constexpr auto operator> (const uintwide_t<Width2, LimbType, AllocatorType, IsSigned>& u, const uintwide_t<Width2, LimbType, AllocatorType, IsSigned>& v) -> bool { return u.operator> (v); }
  template<const size_t Width2, typename LimbType, typename AllocatorType, const bool IsSigned> constexpr auto operator< (const uintwide_t<Width2, LimbType, AllocatorType, IsSigned>& u, const uintwide_t<Width2, LimbType, AllocatorType, IsSigned>& v) -> bool { return u.operator< (v); }
  template<const size_t Width2, typename LimbType, typename AllocatorType, const bool IsSigned> constexpr auto operator>=(const uintwide_t<Width2, LimbType, AllocatorType, IsSigned>& u, const uintwide_t<Width2, LimbType, AllocatorType, IsSigned>& v) -> bool { return u.operator>=(v); }
  template<const size_t Width2, typename LimbType, typename AllocatorType, const bool IsSigned> constexpr auto operator<=(const uintwide_t<Width2, LimbType, AllocatorType, IsSigned>& u, const uintwide_t<Width2, LimbType, AllocatorType, IsSigned>& v) -> bool { return u.operator<=(v); }

  // Non-member comparison functions of (uintwide_t cmp IntegralType).
  template<typename IntegralType, const size_t Width2, typename LimbType, typename AllocatorType, const bool IsSigned> constexpr auto operator==(const uintwide_t<Width2, LimbType, AllocatorType, IsSigned>& u, const IntegralType& v) -> std::enable_if_t<std::is_integral<IntegralType>::value, bool> { return u.operator==(uintwide_t<Width2, LimbType, AllocatorType, IsSigned>(v)); }
  template<typename IntegralType, const size_t Width2, typename LimbType, typename AllocatorType, const bool IsSigned> constexpr auto operator!=(const uintwide_t<Width2, LimbType, AllocatorType, IsSigned>& u, const IntegralType& v) -> std::enable_if_t<std::is_integral<IntegralType>::value, bool> { return u.operator!=(uintwide_t<Width2, LimbType, AllocatorType, IsSigned>(v)); }
  template<typename IntegralType, const size_t Width2, typename LimbType, typename AllocatorType, const bool IsSigned> constexpr auto operator> (const uintwide_t<Width2, LimbType, AllocatorType, IsSigned>& u, const IntegralType& v) -> std::enable_if_t<std::is_integral<IntegralType>::value, bool> { return u.operator> (uintwide_t<Width2, LimbType, AllocatorType, IsSigned>(v)); }
  template<typename IntegralType, const size_t Width2, typename LimbType, typename AllocatorType, const bool IsSigned> constexpr auto operator< (const uintwide_t<Width2, LimbType, AllocatorType, IsSigned>& u, const IntegralType& v) -> std::enable_if_t<std::is_integral<IntegralType>::value, bool> { return u.operator< (uintwide_t<Width2, LimbType, AllocatorType, IsSigned>(v)); }
  template<typename IntegralType, const size_t Width2, typename LimbType, typename AllocatorType, const bool IsSigned> constexpr auto operator>=(const uintwide_t<Width2, LimbType, AllocatorType, IsSigned>& u, const IntegralType& v) -> std::enable_if_t<std::is_integral<IntegralType>::value, bool> { return u.operator>=(uintwide_t<Width2, LimbType, AllocatorType, IsSigned>(v)); }
  template<typename IntegralType, const size_t Width2, typename LimbType, typename AllocatorType, const bool IsSigned> constexpr auto operator<=(const uintwide_t<Width2, LimbType, AllocatorType, IsSigned>& u, const IntegralType& v) -> std::enable_if_t<std::is_integral<IntegralType>::value, bool> { return u.operator<=(uintwide_t<Width2, LimbType, AllocatorType, IsSigned>(v)); }

  // Non-member comparison functions of (IntegralType cmp uintwide_t).
  template<typename IntegralType, const size_t Width2, typename LimbType, typename AllocatorType, const bool IsSigned> constexpr auto operator==(const IntegralType& u, const uintwide_t<Width2, LimbType, AllocatorType, IsSigned>& v) -> std::enable_if_t<std::is_integral<IntegralType>::value, bool> { return uintwide_t<Width2, LimbType, AllocatorType, IsSigned>(u).operator==(v); }
  template<typename IntegralType, const size_t Width2, typename LimbType, typename AllocatorType, const bool IsSigned> constexpr auto operator!=(const IntegralType& u, const uintwide_t<Width2, LimbType, AllocatorType, IsSigned>& v) -> std::enable_if_t<std::is_integral<IntegralType>::value, bool> { return uintwide_t<Width2, LimbType, AllocatorType, IsSigned>(u).operator!=(v); }
  template<typename IntegralType, const size_t Width2, typename LimbType, typename AllocatorType, const bool IsSigned> constexpr auto operator> (const IntegralType& u, const uintwide_t<Width2, LimbType, AllocatorType, IsSigned>& v) -> std::enable_if_t<std::is_integral<IntegralType>::value, bool> { return uintwide_t<Width2, LimbType, AllocatorType, IsSigned>(u).operator> (v); }
  template<typename IntegralType, const size_t Width2, typename LimbType, typename AllocatorType, const bool IsSigned> constexpr auto operator< (const IntegralType& u, const uintwide_t<Width2, LimbType, AllocatorType, IsSigned>& v) -> std::enable_if_t<std::is_integral<IntegralType>::value, bool> { return uintwide_t<Width2, LimbType, AllocatorType, IsSigned>(u).operator< (v); }
  template<typename IntegralType, const size_t Width2, typename LimbType, typename AllocatorType, const bool IsSigned> constexpr auto operator>=(const IntegralType& u, const uintwide_t<Width2, LimbType, AllocatorType, IsSigned>& v) -> std::enable_if_t<std::is_integral<IntegralType>::value, bool> { return uintwide_t<Width2, LimbType, AllocatorType, IsSigned>(u).operator>=(v); }
  template<typename IntegralType, const size_t Width2, typename LimbType, typename AllocatorType, const bool IsSigned> constexpr auto operator<=(const IntegralType& u, const uintwide_t<Width2, LimbType, AllocatorType, IsSigned>& v) -> std::enable_if_t<std::is_integral<IntegralType>::value, bool> { return uintwide_t<Width2, LimbType, AllocatorType, IsSigned>(u).operator<=(v); }

  #if !defined(WIDE_INTEGER_DISABLE_FLOAT_INTEROP)
  // Non-member comparison functions of (uintwide_t cmp FloatingPointType).
  template<typename FloatingPointType, const size_t Width2, typename LimbType, typename AllocatorType, const bool IsSigned> constexpr auto operator==(const uintwide_t<Width2, LimbType, AllocatorType, IsSigned>& u, const FloatingPointType& f) -> std::enable_if_t<std::is_floating_point<FloatingPointType>::value, bool> { return u.operator==(uintwide_t<Width2, LimbType, AllocatorType, IsSigned>(f)); }
  template<typename FloatingPointType, const size_t Width2, typename LimbType, typename AllocatorType, const bool IsSigned> constexpr auto operator!=(const uintwide_t<Width2, LimbType, AllocatorType, IsSigned>& u, const FloatingPointType& f) -> std::enable_if_t<std::is_floating_point<FloatingPointType>::value, bool> { return u.operator!=(uintwide_t<Width2, LimbType, AllocatorType, IsSigned>(f)); }
  template<typename FloatingPointType, const size_t Width2, typename LimbType, typename AllocatorType, const bool IsSigned> constexpr auto operator> (const uintwide_t<Width2, LimbType, AllocatorType, IsSigned>& u, const FloatingPointType& f) -> std::enable_if_t<std::is_floating_point<FloatingPointType>::value, bool> { return u.operator> (uintwide_t<Width2, LimbType, AllocatorType, IsSigned>(f)); }
  template<typename FloatingPointType, const size_t Width2, typename LimbType, typename AllocatorType, const bool IsSigned> constexpr auto operator< (const uintwide_t<Width2, LimbType, AllocatorType, IsSigned>& u, const FloatingPointType& f) -> std::enable_if_t<std::is_floating_point<FloatingPointType>::value, bool> { return u.operator< (uintwide_t<Width2, LimbType, AllocatorType, IsSigned>(f)); }
  template<typename FloatingPointType, const size_t Width2, typename LimbType, typename AllocatorType, const bool IsSigned> constexpr auto operator>=(const uintwide_t<Width2, LimbType, AllocatorType, IsSigned>& u, const FloatingPointType& f) -> std::enable_if_t<std::is_floating_point<FloatingPointType>::value, bool> { return u.operator>=(uintwide_t<Width2, LimbType, AllocatorType, IsSigned>(f)); }
  template<typename FloatingPointType, const size_t Width2, typename LimbType, typename AllocatorType, const bool IsSigned> constexpr auto operator<=(const uintwide_t<Width2, LimbType, AllocatorType, IsSigned>& u, const FloatingPointType& f) -> std::enable_if_t<std::is_floating_point<FloatingPointType>::value, bool> { return u.operator<=(uintwide_t<Width2, LimbType, AllocatorType, IsSigned>(f)); }

  // Non-member comparison functions of (FloatingPointType cmp uintwide_t).
  template<typename FloatingPointType, const size_t Width2, typename LimbType, typename AllocatorType, const bool IsSigned> constexpr auto operator==(const FloatingPointType& f, const uintwide_t<Width2, LimbType, AllocatorType, IsSigned>& v) -> std::enable_if_t<std::is_floating_point<FloatingPointType>::value, bool> { return uintwide_t<Width2, LimbType, AllocatorType, IsSigned>(f).operator==(v); }
  template<typename FloatingPointType, const size_t Width2, typename LimbType, typename AllocatorType, const bool IsSigned> constexpr auto operator!=(const FloatingPointType& f, const uintwide_t<Width2, LimbType, AllocatorType, IsSigned>& v) -> std::enable_if_t<std::is_floating_point<FloatingPointType>::value, bool> { return uintwide_t<Width2, LimbType, AllocatorType, IsSigned>(f).operator!=(v); }
  template<typename FloatingPointType, const size_t Width2, typename LimbType, typename AllocatorType, const bool IsSigned> constexpr auto operator> (const FloatingPointType& f, const uintwide_t<Width2, LimbType, AllocatorType, IsSigned>& v) -> std::enable_if_t<std::is_floating_point<FloatingPointType>::value, bool> { return uintwide_t<Width2, LimbType, AllocatorType, IsSigned>(f).operator> (v); }
  template<typename FloatingPointType, const size_t Width2, typename LimbType, typename AllocatorType, const bool IsSigned> constexpr auto operator< (const FloatingPointType& f, const uintwide_t<Width2, LimbType, AllocatorType, IsSigned>& v) -> std::enable_if_t<std::is_floating_point<FloatingPointType>::value, bool> { return uintwide_t<Width2, LimbType, AllocatorType, IsSigned>(f).operator< (v); }
  template<typename FloatingPointType, const size_t Width2, typename LimbType, typename AllocatorType, const bool IsSigned> constexpr auto operator>=(const FloatingPointType& f, const uintwide_t<Width2, LimbType, AllocatorType, IsSigned>& v) -> std::enable_if_t<std::is_floating_point<FloatingPointType>::value, bool> { return uintwide_t<Width2, LimbType, AllocatorType, IsSigned>(f).operator>=(v); }
  template<typename FloatingPointType, const size_t Width2, typename LimbType, typename AllocatorType, const bool IsSigned> constexpr auto operator<=(const FloatingPointType& f, const uintwide_t<Width2, LimbType, AllocatorType, IsSigned>& v) -> std::enable_if_t<std::is_floating_point<FloatingPointType>::value, bool> { return uintwide_t<Width2, LimbType, AllocatorType, IsSigned>(f).operator<=(v); }
  #endif // !defined(WIDE_INTEGER_DISABLE_FLOAT_INTEROP)

  #if !defined(WIDE_INTEGER_DISABLE_IOSTREAM)

  // I/O streaming functions.
  template<typename char_type,
           typename traits_type,
           const size_t Width2,
           typename LimbType,
           typename AllocatorType,
           const bool IsSigned>
  auto operator<<(std::basic_ostream<char_type, traits_type>& out,
                  const uintwide_t<Width2, LimbType, AllocatorType, IsSigned>& x) -> std::basic_ostream<char_type, traits_type>&
  {
    std::basic_ostringstream<char_type, traits_type> ostr;

    const std::ios::fmtflags my_flags = out.flags();

    const auto show_pos     = ((my_flags & std::ios::showpos)   == std::ios::showpos);
    const auto show_base    = ((my_flags & std::ios::showbase)  == std::ios::showbase);
    const auto is_uppercase = ((my_flags & std::ios::uppercase) == std::ios::uppercase);

    auto base_rep = std::uint_fast8_t { };

    if     ((my_flags & std::ios::oct) == std::ios::oct) { base_rep = static_cast<std::uint_fast8_t>(UINT8_C( 8)); }
    else if((my_flags & std::ios::hex) == std::ios::hex) { base_rep = static_cast<std::uint_fast8_t>(UINT8_C(16)); }
    else                                                 { base_rep = static_cast<std::uint_fast8_t>(UINT8_C(10)); }

    const auto field_width   = static_cast<unsigned_fast_type>(out.width());
    const auto fill_char_out = static_cast<char>(out.fill());

    using local_wide_integer_type = uintwide_t<Width2, LimbType, AllocatorType, IsSigned>;

    if(base_rep == static_cast<std::uint_fast8_t>(UINT8_C(8)))
    {
      using string_storage_oct_type =
        std::conditional_t
          <local_wide_integer_type::my_width2 <= static_cast<size_t>(UINT32_C(2048)),
            detail::fixed_static_array <char,
                                        local_wide_integer_type::wr_string_max_buffer_size_oct()>,
            detail::fixed_dynamic_array<char,
                                        local_wide_integer_type::wr_string_max_buffer_size_oct(),
                                        typename std::allocator_traits<std::conditional_t<std::is_same<AllocatorType, void>::value,
                                                                                         std::allocator<void>,
                                                                                         AllocatorType>>::template rebind_alloc<typename local_wide_integer_type::limb_type>>>;

      // TBD: There is redundant storage of this kind both here
      // in this subroutine as well as in the wr_string method.
      string_storage_oct_type str_result; // LCOV_EXCL_LINE

      str_result.fill('\0');

      x.wr_string(str_result.begin(), base_rep, show_base, show_pos, is_uppercase, field_width, fill_char_out);

      static_cast<void>(ostr << str_result.data());
    }
    else if(base_rep == static_cast<std::uint_fast8_t>(UINT8_C(10)))
    {
      using string_storage_dec_type =
        std::conditional_t
          <local_wide_integer_type::my_width2 <= static_cast<size_t>(UINT32_C(2048)),
            detail::fixed_static_array <char,
                                        local_wide_integer_type::wr_string_max_buffer_size_dec()>,
            detail::fixed_dynamic_array<char,
                                        local_wide_integer_type::wr_string_max_buffer_size_dec(),
                                        typename std::allocator_traits<std::conditional_t<std::is_same<AllocatorType, void>::value,
                                                                                          std::allocator<void>,
                                                                                          AllocatorType>>::template rebind_alloc<typename local_wide_integer_type::limb_type>>>;

      // TBD: There is redundant storage of this kind both here
      // in this subroutine as well as in the wr_string method.
      string_storage_dec_type str_result;

      str_result.fill('\0');

      x.wr_string(str_result.begin(), base_rep, show_base, show_pos, is_uppercase, field_width, fill_char_out);

      static_cast<void>(ostr << str_result.data());
    }
    else if(base_rep == static_cast<std::uint_fast8_t>(UINT8_C(16)))
    {
      using string_storage_hex_type =
        std::conditional_t
          <local_wide_integer_type::my_width2 <= static_cast<size_t>(UINT32_C(2048)),
            detail::fixed_static_array <char,
                                        local_wide_integer_type::wr_string_max_buffer_size_hex()>,
            detail::fixed_dynamic_array<char,
                                        local_wide_integer_type::wr_string_max_buffer_size_hex(),
                                        typename std::allocator_traits<std::conditional_t<std::is_same<AllocatorType, void>::value,
                                                                                         std::allocator<void>,
                                                                                         AllocatorType>>::template rebind_alloc<typename local_wide_integer_type::limb_type>>>;

      // TBD: There is redundant storage of this kind both here
      // in this subroutine as well as in the wr_string method.
      string_storage_hex_type str_result;

      str_result.fill('\0');

      x.wr_string(str_result.begin(), base_rep, show_base, show_pos, is_uppercase, field_width, fill_char_out);

      static_cast<void>(ostr << str_result.data());
    }

    return (out << ostr.str());
  }

  template<typename char_type,
           typename traits_type,
           const size_t Width2,
           typename LimbType,
           typename AllocatorType,
           const bool IsSigned>
  auto operator>>(std::basic_istream<char_type, traits_type>& in,
                  uintwide_t<Width2, LimbType, AllocatorType, IsSigned>& x) -> std::basic_istream<char_type, traits_type>&
  {
    std::string str_in;

    in >> str_in;

    using local_wide_integer_type = uintwide_t<Width2, LimbType, AllocatorType, IsSigned>;

    x = local_wide_integer_type(str_in.c_str());

    return in;
  }

  #endif // !defined(WIDE_INTEGER_DISABLE_IOSTREAM)

  #if(__cplusplus >= 201703L)
  } // namespace math::wide_integer
  #else
  } // namespace wide_integer
  } // namespace math
  #endif

  // Implement various number-theoretical tools.

  #if(__cplusplus >= 201703L)
  namespace math::wide_integer {
  #else
  namespace math { namespace wide_integer { // NOLINT(modernize-concat-nested-namespaces)
  #endif

  namespace detail {

  #if !defined(WIDE_INTEGER_DISABLE_FLOAT_INTEROP)
  namespace my_own {

  template<typename FloatingPointType> WIDE_INTEGER_CONSTEXPR auto frexp(FloatingPointType x, int* expptr) -> std::enable_if_t<(std::is_floating_point<FloatingPointType>::value &&   std::numeric_limits<FloatingPointType>::is_iec559 ), FloatingPointType>
  {
    using local_floating_point_type = FloatingPointType;

    const auto x_is_neg = (x < static_cast<local_floating_point_type>(0.0L));

    local_floating_point_type f = (x_is_neg ? -x : x); // NOLINT(altera-id-dependent-backward-branch)

    auto e2 = static_cast<int>(INT8_C(0));

    constexpr auto two_pow32 =
      static_cast<long double>
      (
        static_cast<long double>(0x10000) * static_cast<long double>(0x10000)
      );

    while(f >= static_cast<local_floating_point_type>(two_pow32)) // NOLINT(altera-id-dependent-backward-branch)
    {
      // TBD: Maybe optimize this exponent reduction
      // with a more clever kind of binary searching.

      f   = static_cast<local_floating_point_type>(f / static_cast<local_floating_point_type>(two_pow32));
      e2 += static_cast<int>(INT32_C(32));
    }

    constexpr auto one_ldbl = static_cast<long double>(1.0L);

    while(f >= static_cast<local_floating_point_type>(one_ldbl)) // NOLINT(altera-id-dependent-backward-branch)
    {
      constexpr auto two_ldbl = static_cast<long double>(2.0L);

      f = static_cast<local_floating_point_type>(f / static_cast<local_floating_point_type>(two_ldbl));

      ++e2;
    }

    if(expptr != nullptr)
    {
      *expptr = e2;
    }

    return ((!x_is_neg) ? f : -f);
  }

  template<typename FloatingPointType> WIDE_INTEGER_CONSTEXPR auto frexp(FloatingPointType x, int* expptr) -> std::enable_if_t<(std::is_floating_point<FloatingPointType>::value && (!std::numeric_limits<FloatingPointType>::is_iec559)), FloatingPointType>
  {
    using std::frexp;

    return frexp(x, expptr);
  }

  template<typename FloatingPointType> WIDE_INTEGER_CONSTEXPR auto (isfinite)(FloatingPointType x) -> std::enable_if_t<(std::is_floating_point<FloatingPointType>::value &&   std::numeric_limits<FloatingPointType>::is_iec559 ), bool>
  {
    using local_floating_point_type = FloatingPointType;

    auto x_is_finite = true;

    const auto x_is_nan = (x != x);

    if(x_is_nan)
    {
      x_is_finite = false;
    }
    else
    {
      const auto x_is_inf_pos = (x > (std::numeric_limits<local_floating_point_type>::max)());
      const auto x_is_inf_neg = (x <  std::numeric_limits<local_floating_point_type>::lowest());

      if(x_is_inf_pos || x_is_inf_neg)
      {
        x_is_finite = false;
      }
    }

    return x_is_finite;
  }

  template<typename FloatingPointType> WIDE_INTEGER_CONSTEXPR auto (isfinite)(FloatingPointType x) -> std::enable_if_t<(std::is_floating_point<FloatingPointType>::value && (!std::numeric_limits<FloatingPointType>::is_iec559)), bool>
  {
    using std::isfinite;

    return (isfinite)(x);
  }
  } // namespace my_own
  #endif // !defined(WIDE_INTEGER_DISABLE_FLOAT_INTEROP)

  template<typename UnsignedIntegralType>
  inline WIDE_INTEGER_CONSTEXPR auto lsb_helper(const UnsignedIntegralType& u) -> unsigned_fast_type
  {
    // Compile-time checks.
    static_assert((   std::is_integral<UnsignedIntegralType>::value
                   && std::is_unsigned<UnsignedIntegralType>::value),
                   "Error: Please check the characteristics of UnsignedIntegralType");

    auto result = static_cast<unsigned_fast_type>(UINT8_C(0));

    using local_unsigned_integral_type = UnsignedIntegralType;

    auto mask = static_cast<local_unsigned_integral_type>(u); // NOLINT(altera-id-dependent-backward-branch)

    // This assumes that at least one bit is set.
    // Otherwise saturation of the index will occur.

    // Naive and basic LSB search.
    // TBD: This could be improved with a binary search
    // on the lowest bit position of the fundamental type.
    while(static_cast<std::uint_fast8_t>(static_cast<std::uint_fast8_t>(mask) & static_cast<std::uint_fast8_t>(UINT8_C(1))) == static_cast<std::uint_fast8_t>(UINT8_C(0))) // NOLINT(hicpp-signed-bitwise,altera-id-dependent-backward-branch)
    {
      mask = static_cast<local_unsigned_integral_type>(mask >> static_cast<unsigned>(UINT8_C(1)));

      ++result;
    }

    return result;
  }

  template<typename UnsignedIntegralType>
  inline WIDE_INTEGER_CONSTEXPR auto msb_helper(const UnsignedIntegralType& u) -> unsigned_fast_type
  {
    // Compile-time checks.
    static_assert((   std::is_integral<UnsignedIntegralType>::value
                   && std::is_unsigned<UnsignedIntegralType>::value),
                   "Error: Please check the characteristics of UnsignedIntegralType");

    using local_unsigned_integral_type = UnsignedIntegralType;

    auto i = signed_fast_type { };

    // TBD: This could potentially be improved with a binary
    // search for the highest bit position in the type.

    for(i = static_cast<signed_fast_type>(std::numeric_limits<local_unsigned_integral_type>::digits - 1); i >= static_cast<signed_fast_type>(INT8_C(0)); --i)
    {
      const auto bit_is_set =
      (
        static_cast<local_unsigned_integral_type>
        (
          u & static_cast<local_unsigned_integral_type>(static_cast<local_unsigned_integral_type>(1U) << static_cast<unsigned_fast_type>(i))
        )
        != static_cast<local_unsigned_integral_type>(UINT8_C(0))
      );

      if(bit_is_set)
      {
        break;
      }
    }

    return static_cast<unsigned_fast_type>((std::max)(static_cast<signed_fast_type>(INT8_C(0)), i));
  }

  template<>
  inline WIDE_INTEGER_CONSTEXPR auto msb_helper<std::uint32_t>(const std::uint32_t& u) -> unsigned_fast_type
  {
    auto r = static_cast<unsigned_fast_type>(UINT8_C(0));
    auto x = static_cast<std::uint_fast32_t>(u);

    // Use O(log2[N]) binary-halving in an unrolled loop to find the msb.
    if(static_cast<std::uint_fast32_t>(x & static_cast<std::uint_fast32_t>(UINT32_C(0xFFFF0000))) != static_cast<std::uint_fast32_t>(UINT8_C(0))) { x = static_cast<std::uint_fast32_t>(x >> static_cast<unsigned>(UINT8_C(16))); r = static_cast<unsigned_fast_type>(r | static_cast<unsigned_fast_type>(UINT8_C(16))); }
    if(static_cast<std::uint_fast32_t>(x & static_cast<std::uint_fast32_t>(UINT32_C(0x0000FF00))) != static_cast<std::uint_fast32_t>(UINT8_C(0))) { x = static_cast<std::uint_fast32_t>(x >> static_cast<unsigned>(UINT8_C( 8))); r = static_cast<unsigned_fast_type>(r | static_cast<unsigned_fast_type>(UINT8_C( 8))); }
    if(static_cast<std::uint_fast32_t>(x & static_cast<std::uint_fast32_t>(UINT32_C(0x000000F0))) != static_cast<std::uint_fast32_t>(UINT8_C(0))) { x = static_cast<std::uint_fast32_t>(x >> static_cast<unsigned>(UINT8_C( 4))); r = static_cast<unsigned_fast_type>(r | static_cast<unsigned_fast_type>(UINT8_C( 4))); }
    if(static_cast<std::uint_fast32_t>(x & static_cast<std::uint_fast32_t>(UINT32_C(0x0000000C))) != static_cast<std::uint_fast32_t>(UINT8_C(0))) { x = static_cast<std::uint_fast32_t>(x >> static_cast<unsigned>(UINT8_C( 2))); r = static_cast<unsigned_fast_type>(r | static_cast<unsigned_fast_type>(UINT8_C( 2))); }
    if(static_cast<std::uint_fast32_t>(x & static_cast<std::uint_fast32_t>(UINT32_C(0x00000002))) != static_cast<std::uint_fast32_t>(UINT8_C(0))) {                                                                               r = static_cast<unsigned_fast_type>(r | static_cast<unsigned_fast_type>(UINT8_C( 1))); }

    return r;
  }

  template<>
  inline WIDE_INTEGER_CONSTEXPR auto msb_helper<std::uint16_t>(const std::uint16_t& u) -> unsigned_fast_type
  {
    auto r = static_cast<unsigned_fast_type>(UINT8_C(0));
    auto x = static_cast<std::uint_fast16_t>(u);

    // Use O(log2[N]) binary-halving in an unrolled loop to find the msb.
    if(static_cast<std::uint_fast16_t>(static_cast<std::uint_fast32_t>(x) & UINT32_C(0xFF00)) != UINT16_C(0)) { x = static_cast<std::uint_fast16_t>(x >> static_cast<unsigned>(UINT8_C(8))); r = static_cast<unsigned_fast_type>(r | UINT32_C(8)); }
    if(static_cast<std::uint_fast16_t>(static_cast<std::uint_fast32_t>(x) & UINT32_C(0x00F0)) != UINT16_C(0)) { x = static_cast<std::uint_fast16_t>(x >> static_cast<unsigned>(UINT8_C(4))); r = static_cast<unsigned_fast_type>(r | UINT32_C(4)); }
    if(static_cast<std::uint_fast16_t>(static_cast<std::uint_fast32_t>(x) & UINT32_C(0x000C)) != UINT16_C(0)) { x = static_cast<std::uint_fast16_t>(x >> static_cast<unsigned>(UINT8_C(2))); r = static_cast<unsigned_fast_type>(r | UINT32_C(2)); }
    if(static_cast<std::uint_fast16_t>(static_cast<std::uint_fast32_t>(x) & UINT32_C(0x0002)) != UINT16_C(0)) {                                                                              r = static_cast<unsigned_fast_type>(r | UINT32_C(1)); }

    return r;
  }

  template<>
  inline WIDE_INTEGER_CONSTEXPR auto msb_helper<std::uint8_t>(const std::uint8_t& u) -> unsigned_fast_type
  {
    auto r = static_cast<unsigned_fast_type>(UINT8_C(0));
    auto x = static_cast<std::uint_fast8_t>(u);

    // Use O(log2[N]) binary-halving in an unrolled loop to find the msb.
    if(static_cast<std::uint_fast8_t>(static_cast<std::uint_fast32_t>(x) & UINT32_C(0xF0)) != UINT8_C(0)) { x = static_cast<std::uint_fast8_t>(x >> static_cast<unsigned>(UINT8_C(4))); r = static_cast<unsigned_fast_type>(r | UINT32_C(4)); }
    if(static_cast<std::uint_fast8_t>(static_cast<std::uint_fast32_t>(x) & UINT32_C(0x0C)) != UINT8_C(0)) { x = static_cast<std::uint_fast8_t>(x >> static_cast<unsigned>(UINT8_C(2))); r = static_cast<unsigned_fast_type>(r | UINT32_C(2)); }
    if(static_cast<std::uint_fast8_t>(static_cast<std::uint_fast32_t>(x) & UINT32_C(0x02)) != UINT8_C(0)) {                                                                             r = static_cast<unsigned_fast_type>(r | UINT32_C(1)); }

    return r;
  }

  } // namespace detail

  template<const size_t Width2,
           typename LimbType,
           typename AllocatorType,
           const bool IsSigned>
  WIDE_INTEGER_CONSTEXPR auto swap(uintwide_t<Width2, LimbType, AllocatorType, IsSigned>& x,
                                   uintwide_t<Width2, LimbType, AllocatorType, IsSigned>& y) noexcept -> void
  {
    if(&x != &y)
    {
      std::swap(x, y);
    }
  }

  template<const size_t Width2,
           typename LimbType,
           typename AllocatorType,
           const bool IsSigned>
  inline WIDE_INTEGER_CONSTEXPR auto lsb(const uintwide_t<Width2, LimbType, AllocatorType, IsSigned>& x) -> unsigned_fast_type
  {
    // Calculate the position of the least-significant bit.
    // Use a linear search starting from the least significant limbs.

    using local_wide_integer_type = uintwide_t<Width2, LimbType, AllocatorType, IsSigned>;
    using local_value_type        = typename local_wide_integer_type::representation_type::value_type;

    auto bpos   = static_cast<unsigned_fast_type>(UINT8_C(0));
    auto offset = static_cast<unsigned_fast_type>(UINT8_C(0));

    for(auto it = x.crepresentation().cbegin(); it != x.crepresentation().cend(); ++it, ++offset) // NOLINT(llvm-qualified-auto,readability-qualified-auto,altera-id-dependent-backward-branch)
    {
      const auto vi = static_cast<local_value_type>(*it & (std::numeric_limits<local_value_type>::max)());

      if(vi != static_cast<local_value_type>(UINT8_C(0)))
      {
        bpos =
          static_cast<unsigned_fast_type>
          (
              detail::lsb_helper(*it)
            + static_cast<unsigned_fast_type>
              (
                static_cast<unsigned_fast_type>(std::numeric_limits<local_value_type>::digits) * offset
              )
          );

        break;
      }
    }

    return bpos;
  }

  template<const size_t Width2,
           typename LimbType,
           typename AllocatorType,
           const bool IsSigned>
  WIDE_INTEGER_CONSTEXPR auto msb(const uintwide_t<Width2, LimbType, AllocatorType, IsSigned>& x) -> unsigned_fast_type
  {
    // Calculate the position of the most-significant bit.
    // Use a linear search starting from the most significant limbs.

    using local_wide_integer_type = uintwide_t<Width2, LimbType, AllocatorType, IsSigned>;
    using local_value_type        = typename local_wide_integer_type::representation_type::value_type;

    auto bpos   = static_cast<unsigned_fast_type>(UINT8_C(0));
    auto offset = static_cast<unsigned_fast_type>(x.crepresentation().size() - 1U);

    for(auto ri = x.crepresentation().crbegin(); ri != x.crepresentation().crend(); ++ri, --offset) // NOLINT(altera-id-dependent-backward-branch)
    {
      const auto vr = static_cast<local_value_type>(*ri & (std::numeric_limits<local_value_type>::max)());

      if(vr != static_cast<local_value_type>(UINT8_C(0)))
      {
        bpos =
          static_cast<unsigned_fast_type>
          (
              detail::msb_helper(*ri)
            + static_cast<unsigned_fast_type>
              (
                static_cast<unsigned_fast_type>(std::numeric_limits<local_value_type>::digits) * offset
              )
          );

        break;
      }
    }

    return bpos;
  }

  template<const size_t Width2,
           typename LimbType,
           typename AllocatorType,
           const bool IsSigned>
  constexpr auto abs(const uintwide_t<Width2, LimbType, AllocatorType, IsSigned>& x) -> uintwide_t<Width2, LimbType, AllocatorType, IsSigned>
  {
    using local_wide_integer_type = uintwide_t<Width2, LimbType, AllocatorType, IsSigned>;

    return ((!local_wide_integer_type::is_neg(x)) ? x : -x);
  }

  template<const size_t Width2,
           typename LimbType,
           typename AllocatorType,
           const bool IsSigned>
  WIDE_INTEGER_CONSTEXPR auto sqrt(const uintwide_t<Width2, LimbType, AllocatorType, IsSigned>& m) -> uintwide_t<Width2, LimbType, AllocatorType, IsSigned>
  {
    // Calculate the square root.

    using local_wide_integer_type = uintwide_t<Width2, LimbType, AllocatorType, IsSigned>;

    local_wide_integer_type s;

    if(m.is_zero() || local_wide_integer_type::is_neg(m))
    {
      s = local_wide_integer_type(static_cast<std::uint_fast8_t>(UINT8_C(0)));
    }
    else
    {
      // Obtain the initial guess via algorithms
      // involving the position of the msb.
      const auto msb_pos = msb(m);

      // Obtain the initial value.
      const auto left_shift_amount =
        static_cast<unsigned_fast_type>
        (
          ((static_cast<unsigned_fast_type>(msb_pos % static_cast<unsigned_fast_type>(UINT8_C(2))) == static_cast<unsigned_fast_type>(UINT8_C(0)))
            ? static_cast<unsigned_fast_type>(1U + static_cast<unsigned_fast_type>((msb_pos + static_cast<unsigned_fast_type>(UINT8_C(0))) / 2U))
            : static_cast<unsigned_fast_type>(1U + static_cast<unsigned_fast_type>((msb_pos + static_cast<unsigned_fast_type>(UINT8_C(1))) / 2U)))
        );

      auto u = static_cast<local_wide_integer_type>(static_cast<std::uint_fast8_t>(UINT8_C(1))) << left_shift_amount;

      // Perform the iteration for the square root.
      // See Algorithm 1.13 SqrtInt, Sect. 1.5.1
      // in R.P. Brent and Paul Zimmermann, "Modern Computer Arithmetic",
      // Cambridge University Press, 2011.

      for(auto i = static_cast<unsigned_fast_type>(UINT8_C(0)); i < static_cast<unsigned_fast_type>(UINT8_C(64)); ++i)
      {
        s = u;

        u = (s + (m / s)) >> static_cast<unsigned>(UINT8_C(1));

        if(u >= s) { break; } // LCOV_EXCL_LINE
      }
    }

    return s;
  }

  template<const size_t Width2,
           typename LimbType,
           typename AllocatorType,
           const bool IsSigned>
  WIDE_INTEGER_CONSTEXPR auto cbrt(const uintwide_t<Width2, LimbType, AllocatorType, IsSigned>& m) -> uintwide_t<Width2, LimbType, AllocatorType, IsSigned> // NOLINT(misc-no-recursion)
  {
    // Calculate the cube root.

    using local_wide_integer_type = uintwide_t<Width2, LimbType, AllocatorType, IsSigned>;

    local_wide_integer_type s;

    if(local_wide_integer_type::is_neg(m))
    {
      s = -cbrt(-m);
    }
    else if(m.is_zero())
    {
      s = local_wide_integer_type(static_cast<std::uint_fast8_t>(UINT8_C(0)));
    }
    else
    {
      // Obtain the initial guess via algorithms
      // involving the position of the msb.
      const auto msb_pos = msb(m);

      // Obtain the initial value.
      const auto msb_pos_mod_3 = static_cast<unsigned_fast_type>(msb_pos % static_cast<unsigned_fast_type>(UINT8_C(3)));

      const auto left_shift_amount =
        static_cast<unsigned_fast_type>
        (
          (msb_pos_mod_3 == static_cast<unsigned_fast_type>(UINT8_C(0)))
            ? static_cast<unsigned_fast_type>(static_cast<unsigned_fast_type>(UINT8_C(1)) + static_cast<unsigned_fast_type>(static_cast<unsigned_fast_type>(msb_pos + static_cast<unsigned_fast_type>(UINT8_C(0))) / 3U))
            : static_cast<unsigned_fast_type>(static_cast<unsigned_fast_type>(UINT8_C(1)) + static_cast<unsigned_fast_type>(static_cast<unsigned_fast_type>(msb_pos + static_cast<unsigned_fast_type>(static_cast<unsigned_fast_type>(UINT8_C(3)) - msb_pos_mod_3)) / 3U))
        );

      auto u = static_cast<local_wide_integer_type>(static_cast<std::uint_fast8_t>(UINT8_C(1))) << left_shift_amount;

      // Perform the iteration for the k'th root.
      // See Algorithm 1.14 RootInt, Sect. 1.5.2
      // in R.P. Brent and Paul Zimmermann, "Modern Computer Arithmetic",
      // Cambridge University Press, 2011.

      const auto three_minus_one = static_cast<unsigned_fast_type>(static_cast<unsigned>(3U - 1U));

      for(auto i = static_cast<unsigned_fast_type>(UINT8_C(0)); i < static_cast<unsigned_fast_type>(UINT8_C(64)); ++i)
      {
        s = u;

        local_wide_integer_type m_over_s_pow_3_minus_one = m;

        for(auto   j = static_cast<unsigned_fast_type>(UINT8_C(0));
                   j < static_cast<unsigned_fast_type>(static_cast<unsigned>(3U - 1U));
                 ++j)
        {
          // Use a loop here to divide by s^(3 - 1) because
          // without a loop, s^(3 - 1) is likely to overflow.

          m_over_s_pow_3_minus_one /= s;
        }

        u = ((s * three_minus_one) + m_over_s_pow_3_minus_one) / static_cast<unsigned>(UINT8_C(3));

        if(u >= s) { break; }
      }
    }

    return s;
  }

  template<const size_t Width2,
           typename LimbType,
           typename AllocatorType,
           const bool IsSigned>
  WIDE_INTEGER_CONSTEXPR auto rootk(const uintwide_t<Width2, LimbType, AllocatorType, IsSigned>& m, std::uint_fast8_t k) -> uintwide_t<Width2, LimbType, AllocatorType, IsSigned>
  {
    // Calculate the k'th root.

    using local_wide_integer_type = uintwide_t<Width2, LimbType, AllocatorType, IsSigned>;

    local_wide_integer_type s;

    if(k < static_cast<std::uint_fast8_t>(UINT8_C(2)))
    {
      s = m;
    }
    else if(k == static_cast<std::uint_fast8_t>(UINT8_C(2)))
    {
      s = sqrt(m);
    }
    else if(k == static_cast<std::uint_fast8_t>(UINT8_C(3)))
    {
      s = cbrt(m);
    }
    else
    {
      if(m.is_zero() || local_wide_integer_type::is_neg(m))
      {
        s = local_wide_integer_type(static_cast<std::uint_fast8_t>(UINT8_C(0)));
      }
      else
      {
        // Obtain the initial guess via algorithms
        // involving the position of the msb.
        const unsigned_fast_type msb_pos = msb(m);

        // Obtain the initial value.
        const unsigned_fast_type msb_pos_mod_k = msb_pos % k;

        const auto left_shift_amount =
          static_cast<unsigned_fast_type>
          (
            ((msb_pos_mod_k == static_cast<unsigned_fast_type>(UINT8_C(0)))
              ? static_cast<unsigned_fast_type>(UINT8_C(1)) + static_cast<unsigned_fast_type>(static_cast<unsigned_fast_type>(msb_pos + static_cast<unsigned_fast_type>(UINT8_C(0))) / k)
              : static_cast<unsigned_fast_type>(UINT8_C(1)) + static_cast<unsigned_fast_type>(static_cast<unsigned_fast_type>(msb_pos + static_cast<unsigned_fast_type>(k - msb_pos_mod_k)) / k))
          );

        auto u = static_cast<local_wide_integer_type>(static_cast<std::uint_fast8_t>(UINT8_C(1))) << left_shift_amount;

        // Perform the iteration for the k'th root.
        // See Algorithm 1.14 RootInt, Sect. 1.5.2
        // in R.P. Brent and Paul Zimmermann, "Modern Computer Arithmetic",
        // Cambridge University Press, 2011.

        const auto k_minus_one = static_cast<unsigned_fast_type>(k - 1U);

        for(auto   i = static_cast<unsigned_fast_type>(UINT8_C(0));
                   i < static_cast<unsigned_fast_type>(UINT8_C(64));
                 ++i)
        {
          s = u;

          local_wide_integer_type m_over_s_pow_k_minus_one = m;

          for(auto j = static_cast<unsigned_fast_type>(UINT8_C(0)); j < k_minus_one; ++j) // NOLINT(altera-id-dependent-backward-branch)
          {
            // Use a loop here to divide by s^(k - 1) because
            // without a loop, s^(k - 1) is likely to overflow.

            m_over_s_pow_k_minus_one /= s;
          }

          u = ((s * k_minus_one) + m_over_s_pow_k_minus_one) / k;

          if(u >= s) { break; } // LCOV_EXCL_LINE
        }
      }
    }

    return s;
  }

  template<typename OtherIntegralTypeP,
           const size_t Width2,
           typename LimbType,
           typename AllocatorType,
           const bool IsSigned>
  WIDE_INTEGER_CONSTEXPR auto pow(const uintwide_t<Width2, LimbType, AllocatorType, IsSigned>& b, const OtherIntegralTypeP& p) -> uintwide_t<Width2, LimbType, AllocatorType, IsSigned>
  {
    // Calculate (b ^ p).
    using local_wide_integer_type = uintwide_t<Width2, LimbType, AllocatorType, IsSigned>;
    using local_limb_type         = typename local_wide_integer_type::limb_type;

    local_wide_integer_type result;
    auto p0 = static_cast<local_limb_type>(p);

    if((p0 == static_cast<local_limb_type>(UINT8_C(0))) && (p == static_cast<OtherIntegralTypeP>(0)))
    {
      result = local_wide_integer_type(static_cast<std::uint8_t>(UINT8_C(1)));
    }
    else if((p0 == static_cast<local_limb_type>(UINT8_C(1))) && (p == static_cast<OtherIntegralTypeP>(UINT8_C(1))))
    {
      result = b;
    }
    else if((p0 == static_cast<local_limb_type>(UINT8_C(2))) && (p == static_cast<OtherIntegralTypeP>(UINT8_C(2))))
    {
      result  = b;
      result *= b;
    }
    else
    {
      result = local_wide_integer_type(static_cast<std::uint8_t>(UINT8_C(1)));

      local_wide_integer_type y      (b);
      local_wide_integer_type p_local(p);

      while(((p0 = static_cast<local_limb_type>(p_local)) != static_cast<local_limb_type>(UINT8_C(0))) || (p_local != static_cast<local_wide_integer_type>(UINT8_C(0)))) // NOLINT(altera-id-dependent-backward-branch)
      {
        if(static_cast<unsigned_fast_type>(p0 & static_cast<local_limb_type>(UINT8_C(1))) != static_cast<unsigned_fast_type>(UINT8_C(0)))
        {
          result *= y;
        }

        y *= y;

        p_local >>= 1U;
      }
    }

    return result;
  }

  template<typename OtherIntegralTypeP,
           typename OtherIntegralTypeM,
           const size_t Width2,
           typename LimbType,
           typename AllocatorType,
           const bool IsSigned>
  WIDE_INTEGER_CONSTEXPR auto powm(const uintwide_t<Width2, LimbType, AllocatorType, IsSigned>& b,
                                   const OtherIntegralTypeP& p,
                                   const OtherIntegralTypeM& m) -> uintwide_t<Width2, LimbType, AllocatorType, IsSigned>
  {
    // Calculate (b ^ p) % m.

    using local_normal_width_type = uintwide_t<Width2, LimbType, AllocatorType, IsSigned>;
    using local_double_width_type = typename local_normal_width_type::double_width_type;
    using local_limb_type         = typename local_normal_width_type::limb_type;

          local_normal_width_type result;
          local_double_width_type y      (b);
    const local_double_width_type m_local(m);

    auto p0 = static_cast<local_limb_type>(p); // NOLINT(altera-id-dependent-backward-branch)

    if((p0 == static_cast<local_limb_type>(UINT8_C(0))) && (p == static_cast<OtherIntegralTypeP>(0)))
    {
      result = local_normal_width_type((m != static_cast<unsigned>(UINT8_C(1))) ? static_cast<std::uint8_t>(UINT8_C(1)) : static_cast<std::uint8_t>(UINT8_C(0)));
    }
    else if((p0 == static_cast<local_limb_type>(UINT8_C(1))) && (p == static_cast<OtherIntegralTypeP>(1)))
    {
      result = b % m;
    }
    else if((p0 == static_cast<local_limb_type>(UINT8_C(2))) && (p == static_cast<OtherIntegralTypeP>(2)))
    {
      y *= y;
      y %= m_local;

      result = local_normal_width_type(y);
    }
    else
    {
      using local_other_integral_p_type = OtherIntegralTypeP;

      local_double_width_type     x      (static_cast<std::uint8_t>(UINT8_C(1)));
      local_other_integral_p_type p_local(p);

      while(((p0 = static_cast<local_limb_type>(p_local)) != static_cast<local_limb_type>(UINT8_C(0))) || (p_local != static_cast<local_other_integral_p_type>(0))) // NOLINT(altera-id-dependent-backward-branch)
      {
        if(static_cast<unsigned_fast_type>(p0 & static_cast<local_limb_type>(UINT8_C(1))) != static_cast<unsigned_fast_type>(UINT8_C(0)))
        {
          x *= y;
          x %= m_local;
        }

        y *= y;
        y %= m_local;

        p_local >>= 1U; // NOLINT(hicpp-signed-bitwise) // LCOV_EXCL_LINE
      }

      result = local_normal_width_type(x);
    }

    return result;
  }

  namespace detail {

  template<typename UnsignedShortType>
  WIDE_INTEGER_CONSTEXPR auto integer_gcd_reduce(UnsignedShortType u, UnsignedShortType v) -> UnsignedShortType
  {
    #if (defined(__cpp_lib_gcd_lcm) && (__cpp_lib_gcd_lcm >= 201606L))
    return std::gcd(u, v);
    #else
    return detail::gcd_unsafe(u, v);
    #endif
  }

  } // namespace detail

  template<const size_t Width2,
           typename LimbType,
           typename AllocatorType,
           const bool IsSigned>
  WIDE_INTEGER_CONSTEXPR auto gcd(const uintwide_t<Width2, LimbType, AllocatorType, IsSigned>& a, // NOLINT(readability-function-cognitive-complexity,bugprone-easily-swappable-parameters)
                                  const uintwide_t<Width2, LimbType, AllocatorType, IsSigned>& b) -> uintwide_t<Width2, LimbType, AllocatorType, IsSigned>
  {
    // This implementation of GCD is an adaptation
    // of existing code from Boost.Multiprecision.

    using local_wide_integer_type = uintwide_t<Width2, LimbType, AllocatorType, IsSigned>;
    using local_ushort_type       = typename local_wide_integer_type::limb_type;
    using local_ularge_type       = typename local_wide_integer_type::double_limb_type;

    const auto u_is_neg = local_wide_integer_type::is_neg(a);
    const auto v_is_neg = local_wide_integer_type::is_neg(b);

    local_wide_integer_type u((!u_is_neg) ? a : -a);
    local_wide_integer_type v((!v_is_neg) ? b : -b);

    local_wide_integer_type result;

    using local_size_type = typename local_wide_integer_type::representation_type::size_type;

    if(u == v)
    { // NOLINT(bugprone-branch-clone)
      // This handles cases having (u = v) and also (u = v = 0).
      result = u; // LCOV_EXCL_LINE
    }
    else if((static_cast<local_ushort_type>(v) == static_cast<local_ushort_type>(UINT8_C(0))) && (v == static_cast<unsigned>(UINT8_C(0))))
    {
      // This handles cases having (v = 0) with (u != 0).
      result = u; // LCOV_EXCL_LINE
    }
    else if((static_cast<local_ushort_type>(u) == static_cast<local_ushort_type>(UINT8_C(0))) && (u == static_cast<unsigned>(UINT8_C(0))))
    {
      // This handles cases having (u = 0) with (v != 0).
      result = v;
    }
    else
    {
      // Now we handle cases having (u != 0) and (v != 0).

      // Let shift := lg K, where K is the greatest
      // power of 2 dividing both u and v.

      const unsigned_fast_type u_shift = lsb(u);
      const unsigned_fast_type v_shift = lsb(v);

      const unsigned_fast_type left_shift_amount = (std::min)(u_shift, v_shift);

      u >>= u_shift;
      v >>= v_shift;

      for(;;)
      {
        // Now u and v are both odd, so diff(u, v) is even.
        // Let u = min(u, v), v = diff(u, v) / 2.

        if(u > v)
        {
          swap(u, v);
        }

        if(u == v)
        {
          break;
        }

        if(v <= (std::numeric_limits<local_ularge_type>::max)())
        {
          if(v <= (std::numeric_limits<local_ushort_type>::max)())
          {
            const auto vs = *v.crepresentation().cbegin();
            const auto us = *u.crepresentation().cbegin();

            u = detail::integer_gcd_reduce(vs, us);
          }
          else
          {
            const auto my_v_hi =
              static_cast<local_ushort_type>
              (
                (v.crepresentation().size() >= static_cast<typename local_wide_integer_type::representation_type::size_type>(UINT8_C(2)))
                  ? static_cast<local_ushort_type>(*detail::advance_and_point(v.crepresentation().cbegin(), static_cast<local_size_type>(UINT8_C(1))))
                  : static_cast<local_ushort_type>(UINT8_C(0))
              );

            const auto my_u_hi =
              static_cast<local_ushort_type>
              (
                (u.crepresentation().size() >= static_cast<typename local_wide_integer_type::representation_type::size_type>(UINT8_C(2)))
                  ? static_cast<local_ushort_type>(*detail::advance_and_point(u.crepresentation().cbegin(), static_cast<local_size_type>(UINT8_C(1))))
                  : static_cast<local_ushort_type>(UINT8_C(0))
              );

            const local_ularge_type v_large = detail::make_large(*v.crepresentation().cbegin(), my_v_hi);
            const local_ularge_type u_large = detail::make_large(*u.crepresentation().cbegin(), my_u_hi);

            u = detail::integer_gcd_reduce(v_large, u_large);
          }

          break;
        }

        v  -= u;
        v >>= lsb(v);
      }

      result = (u << left_shift_amount);
    }

    return ((u_is_neg == v_is_neg) ? result : -result);
  }

  template<typename UnsignedShortType>
  WIDE_INTEGER_CONSTEXPR auto gcd(const UnsignedShortType& u, const UnsignedShortType& v) -> std::enable_if_t<(   (std::is_integral<UnsignedShortType>::value)
                                                                                                               && (std::is_unsigned<UnsignedShortType>::value)), UnsignedShortType>
  {
    return detail::gcd_unsafe(u, v);
  }

  namespace detail {

  template<typename IntegerType>
  WIDE_INTEGER_CONSTEXPR auto lcm_impl(const IntegerType& a, const IntegerType& b) -> IntegerType
  {
    using local_integer_type = IntegerType;

    const local_integer_type ap = detail::abs_unsafe(a);
    const local_integer_type bp = detail::abs_unsafe(b);

    const auto a_is_greater_than_b = (ap > bp);

    const auto gcd_of_ab = gcd(a, b);

    return (a_is_greater_than_b ? ap * (bp / gcd_of_ab)
                                : bp * (ap / gcd_of_ab));
  }

  } // namespace detail

  template<const size_t Width2,
           typename LimbType,
           typename AllocatorType,
           const bool IsSigned>
  WIDE_INTEGER_CONSTEXPR auto lcm(const uintwide_t<Width2, LimbType, AllocatorType, IsSigned>& a,
                                  const uintwide_t<Width2, LimbType, AllocatorType, IsSigned>& b) -> uintwide_t<Width2, LimbType, AllocatorType, IsSigned>
  {
    return detail::lcm_impl(a, b);
  }

  template<typename UnsignedShortType>
  WIDE_INTEGER_CONSTEXPR auto lcm(const UnsignedShortType& a, const UnsignedShortType& b) -> std::enable_if_t<(   (std::is_integral<UnsignedShortType>::value)
                                                                                                               && (std::is_unsigned<UnsignedShortType>::value)), UnsignedShortType>
  {
    return detail::lcm_impl(a, b);
  }

  template<const size_t Width2,
           typename LimbType,
           typename AllocatorType,
           const bool IsSignedLeft,
           const bool IsSignedRight,
           std::enable_if_t<((!IsSignedLeft) && (!IsSignedRight))> const*>
  WIDE_INTEGER_CONSTEXPR
  auto divmod(const uintwide_t<Width2, LimbType, AllocatorType, IsSignedLeft >& a,
              const uintwide_t<Width2, LimbType, AllocatorType, IsSignedRight>& b) -> std::pair<uintwide_t<Width2, LimbType, AllocatorType, IsSignedLeft>, uintwide_t<Width2, LimbType, AllocatorType, IsSignedRight>>
  {
    using local_unsigned_wide_type = uintwide_t<Width2, LimbType, AllocatorType, false>;

          local_unsigned_wide_type ua(a);
    const local_unsigned_wide_type ub(b);

    local_unsigned_wide_type ur { };

    ua.eval_divide_knuth(ub, &ur);

    using divmod_result_pair_type = std::pair<local_unsigned_wide_type, local_unsigned_wide_type>;

    return divmod_result_pair_type { ua, ur };
  }

  template<const size_t Width2,
           typename LimbType,
           typename AllocatorType,
           const bool IsSignedLeft,
           const bool IsSignedRight,
           std::enable_if_t<(IsSignedLeft || IsSignedRight)> const*>
  WIDE_INTEGER_CONSTEXPR
  auto divmod(const uintwide_t<Width2, LimbType, AllocatorType, IsSignedLeft >& a,
              const uintwide_t<Width2, LimbType, AllocatorType, IsSignedRight>& b) -> std::pair<uintwide_t<Width2, LimbType, AllocatorType, IsSignedLeft>, uintwide_t<Width2, LimbType, AllocatorType, IsSignedRight>>
  {
    using local_unsigned_wide_type = uintwide_t<Width2, LimbType, AllocatorType, false>;

    using local_unknown_signedness_left_type  = uintwide_t<Width2, LimbType, AllocatorType, IsSignedLeft>;
    using local_unknown_signedness_right_type = uintwide_t<Width2, LimbType, AllocatorType, IsSignedRight>;

    const auto numer_was_neg = local_unknown_signedness_left_type::is_neg(a);
    const auto denom_was_neg = local_unknown_signedness_right_type::is_neg(b);

          local_unsigned_wide_type ua((!numer_was_neg) ? a : -a);
    const local_unsigned_wide_type ub((!denom_was_neg) ? b : -b);

    local_unsigned_wide_type ur { };

    ua.eval_divide_knuth(ub, &ur);

    using divmod_result_pair_type =
      std::pair<local_unknown_signedness_left_type, local_unknown_signedness_right_type>;

    auto result =
      divmod_result_pair_type
      {
        local_unknown_signedness_left_type  { },
        local_unknown_signedness_right_type { }
      };

    if(numer_was_neg == denom_was_neg)
    {
      result.first  = local_unknown_signedness_left_type(ua);
      result.second = (!numer_was_neg) ? local_unknown_signedness_right_type(ur) : -local_unknown_signedness_right_type(ur);
    }
    else
    {
      const auto division_is_exact = (ur == static_cast<unsigned>(UINT8_C(0)));

      if(!division_is_exact) { ++ua; }

      result.first = local_unknown_signedness_left_type(ua);

      result.first.negate();

      if(!division_is_exact) { ur -= ub; }

      result.second = local_unknown_signedness_right_type(ur);

      if(!denom_was_neg) { result.second.negate(); }
    }

    return result;
  }

  template<const size_t Width2,
           typename LimbType,
           typename AllocatorType,
           const bool IsSigned>
  class uniform_int_distribution
  {
  public:
    using result_type = uintwide_t<Width2, LimbType, AllocatorType, IsSigned>;

    struct param_type
    {
    public:
      explicit WIDE_INTEGER_CONSTEXPR
        param_type
        (
          const result_type& p_a = (std::numeric_limits<result_type>::min)(), // NOLINT(modernize-pass-by-value)
          const result_type& p_b = (std::numeric_limits<result_type>::max)()  // NOLINT(modernize-pass-by-value)
        ) : param_a(p_a),
            param_b(p_b) { }

      WIDE_INTEGER_CONSTEXPR param_type(const param_type& other) : param_a(other.param_a),
                                                                   param_b(other.param_b) { }

      WIDE_INTEGER_CONSTEXPR param_type(param_type&& other) noexcept
        : param_a(static_cast<result_type&&>(other.param_a)),
          param_b(static_cast<result_type&&>(other.param_b)) { }

      ~param_type() = default;

      WIDE_INTEGER_CONSTEXPR auto operator=(const param_type& other) -> param_type& // NOLINT(cert-oop54-cpp)
      {
        if(this != &other)
        {
          param_a = other.param_a;
          param_b = other.param_b;
        }

        return *this;
      }

      WIDE_INTEGER_CONSTEXPR auto operator=(param_type&& other) noexcept -> param_type&
      {
        param_a = other.param_a;
        param_b = other.param_b;

        return *this;
      }

      WIDE_INTEGER_NODISCARD constexpr auto get_a() const -> result_type { return param_a; }
      WIDE_INTEGER_NODISCARD constexpr auto get_b() const -> result_type { return param_b; }

      WIDE_INTEGER_CONSTEXPR auto set_a(const result_type& p_a) -> void { param_a = p_a; }
      WIDE_INTEGER_CONSTEXPR auto set_b(const result_type& p_b) -> void { param_b = p_b; }

    private:
      result_type param_a; // NOLINT(readability-identifier-naming)
      result_type param_b; // NOLINT(readability-identifier-naming)

      friend inline constexpr auto operator==(const param_type& lhs,
                                              const param_type& rhs) -> bool
      {
        return (   (lhs.param_a == rhs.param_a)
                && (lhs.param_b == rhs.param_b));
      }

      friend inline constexpr auto operator!=(const param_type& lhs,
                                              const param_type& rhs) -> bool
      {
        return (   (lhs.param_a != rhs.param_a)
                || (lhs.param_b != rhs.param_b));
      }
    };

    explicit WIDE_INTEGER_CONSTEXPR
      uniform_int_distribution
      (
        const result_type& p_a = (std::numeric_limits<result_type>::min)(),
        const result_type& p_b = (std::numeric_limits<result_type>::max)()
      ) : my_params(p_a, p_b) { }

    explicit WIDE_INTEGER_CONSTEXPR uniform_int_distribution(const param_type& other_params)
      : my_params(other_params) { }

    WIDE_INTEGER_CONSTEXPR uniform_int_distribution(const uniform_int_distribution& other_distribution) = delete;

    WIDE_INTEGER_CONSTEXPR uniform_int_distribution(uniform_int_distribution&& other) noexcept : my_params(other.my_params) { }

    ~uniform_int_distribution() = default;

    auto WIDE_INTEGER_CONSTEXPR operator=(const uniform_int_distribution& other) -> uniform_int_distribution& // NOLINT(cert-oop54-cpp)
    {
      if(this != &other)
      {
        my_params = other.my_params;
      }

      return *this;
    }

    auto WIDE_INTEGER_CONSTEXPR operator=(uniform_int_distribution&& other) noexcept -> uniform_int_distribution&
    {
      my_params = other.my_params;

      return *this;
    }

    auto WIDE_INTEGER_CONSTEXPR param(const param_type& new_params) -> void
    {
      my_params = new_params;
    }

    WIDE_INTEGER_NODISCARD auto param() const -> const param_type& { return my_params; }

    WIDE_INTEGER_NODISCARD auto a() const -> result_type { return my_params.get_a(); }
    WIDE_INTEGER_NODISCARD auto b() const -> result_type { return my_params.get_b(); }

    template<typename GeneratorType,
             const int GeneratorResultBits = std::numeric_limits<typename GeneratorType::result_type>::digits>
    WIDE_INTEGER_CONSTEXPR auto operator()(GeneratorType& generator) -> result_type
    {
      return
        generate<GeneratorType, GeneratorResultBits>
        (
          generator,
          my_params
        );
    }

    template<typename GeneratorType,
             const int GeneratorResultBits = std::numeric_limits<typename GeneratorType::result_type>::digits>
    WIDE_INTEGER_CONSTEXPR auto operator()(      GeneratorType& input_generator,
                                           const param_type&    input_params) -> result_type
    {
      return
        generate<GeneratorType, GeneratorResultBits>
        (
          input_generator,
          input_params
        );
    }

  private:
    param_type my_params; // NOLINT(readability-identifier-naming)

    template<typename GeneratorType,
             const int GeneratorResultBits = std::numeric_limits<typename GeneratorType::result_type>::digits>
    WIDE_INTEGER_CONSTEXPR auto generate(      GeneratorType& input_generator,
                                         const param_type&    input_params) const -> result_type
    {
      // Generate random numbers r, where a <= r <= b.

      auto result = static_cast<result_type>(static_cast<std::uint8_t>(UINT8_C(0)));

      using local_limb_type = typename result_type::limb_type;

      using generator_result_type = typename GeneratorType::result_type;

      constexpr auto digits_generator_result_type = static_cast<std::uint32_t>(GeneratorResultBits);

      static_assert(static_cast<std::uint32_t>(digits_generator_result_type % static_cast<std::uint32_t>(UINT8_C(8))) == static_cast<std::uint32_t>(UINT8_C(0)),
                    "Error: Generator result type must have a multiple of 8 bits.");

      constexpr auto digits_limb_ratio =
        static_cast<std::uint32_t>(std::numeric_limits<local_limb_type>::digits / static_cast<int>(INT8_C(8)));

      constexpr auto digits_gtor_ratio = static_cast<std::uint32_t>(digits_generator_result_type / static_cast<std::uint32_t>(UINT8_C(8)));

      generator_result_type value = generator_result_type();

      auto it = result.representation().begin(); // NOLINT(llvm-qualified-auto,readability-qualified-auto,altera-id-dependent-backward-branch)

      auto j = static_cast<unsigned_fast_type>(UINT8_C(0));

      while(it != result.representation().end()) // NOLINT(altera-id-dependent-backward-branch)
      {
        if(static_cast<unsigned_fast_type>(j % static_cast<unsigned_fast_type>(digits_gtor_ratio)) == static_cast<unsigned_fast_type>(UINT8_C(0)))
        {
          value = input_generator();
        }

        const auto right_shift_amount =
          static_cast<unsigned>
          (
              static_cast<unsigned_fast_type>(j % digits_gtor_ratio)
            * static_cast<unsigned_fast_type>(UINT8_C(8))
          );

        const auto next_byte = static_cast<std::uint8_t>(value >> right_shift_amount);

        *it =
          static_cast<typename result_type::limb_type>
          (
            *it | static_cast<local_limb_type>(static_cast<local_limb_type>(next_byte) << static_cast<unsigned>(static_cast<unsigned_fast_type>(j % digits_limb_ratio) * static_cast<unsigned_fast_type>(UINT8_C(8))))
          );

        ++j;

        if(static_cast<unsigned_fast_type>(j % digits_limb_ratio) == static_cast<unsigned_fast_type>(UINT8_C(0)))
        {
          ++it;
        }
      }

      if(   (input_params.get_a() != (std::numeric_limits<result_type>::min)())
         || (input_params.get_b() != (std::numeric_limits<result_type>::max)()))
      {
        // Note that this restricts the range r to:
        //   r = { [input_generator() % ((b - a) + 1)] + a }

        result_type range(input_params.get_b() - input_params.get_a());

        ++range;

        result %= range;
        result += input_params.get_a();
      }

      return result;
    }
  };

  template<const size_t Width2,
           typename LimbType,
           typename AllocatorType,
           const bool IsSigned>
  constexpr auto operator==(const uniform_int_distribution<Width2, LimbType, AllocatorType, IsSigned>& lhs,
                            const uniform_int_distribution<Width2, LimbType, AllocatorType, IsSigned>& rhs) -> bool
  {
    return (lhs.param() == rhs.param());
  }

  template<const size_t Width2,
           typename LimbType,
           typename AllocatorType,
           const bool IsSigned>
  constexpr auto operator!=(const uniform_int_distribution<Width2, LimbType, AllocatorType, IsSigned>& lhs,
                            const uniform_int_distribution<Width2, LimbType, AllocatorType, IsSigned>& rhs) -> bool
  {
    return (lhs.param() != rhs.param());
  }

  template<typename DistributionType,
           typename GeneratorType,
           const size_t Width2,
           typename LimbType,
           typename AllocatorType,
           const bool IsSigned>
  auto miller_rabin(const uintwide_t<Width2, LimbType, AllocatorType, IsSigned>& n, // NOLINT(readability-function-cognitive-complexity)
                    const unsigned_fast_type                                     number_of_trials,
                          DistributionType&                                      distribution,
                          GeneratorType&                                         generator) -> bool
  {
    // This Miller-Rabin primality test is loosely based on
    // an adaptation of some code from Boost.Multiprecision.
    // The Boost.Multiprecision code can be found here:
    // https://www.boost.org/doc/libs/1_76_0/libs/multiprecision/doc/html/boost_multiprecision/tut/primetest.html

    // Note: Some comments in this subroutine use the Wolfram Language(TM).
    // These can be exercised at the web links to WolframAlpha(R) provided

    using local_wide_integer_type = uintwide_t<Width2, LimbType, AllocatorType, IsSigned>;
    using local_limb_type         = typename local_wide_integer_type::limb_type;

    const local_wide_integer_type np((!local_wide_integer_type::is_neg(n)) ? n : -n);

    {
      const auto n0 = static_cast<local_limb_type>(np);

      const auto n_is_even =
        (static_cast<local_limb_type>(n0 & static_cast<local_limb_type>(UINT8_C(1))) == static_cast<local_limb_type>(UINT8_C(0)));

      if(n_is_even)
      {
        // The prime candidate is not prime because n is even.
        // Also handle the trivial special case of (n = 2).

        const auto n_is_two =
          ((n0 == static_cast<local_limb_type>(UINT8_C(2))) && (np == static_cast<local_limb_type>(UINT8_C(2))));

        return n_is_two;
      }

      if((n0 <= static_cast<local_limb_type>(UINT8_C(227))) && (np <= static_cast<local_limb_type>(UINT8_C(227))))
      {
        // This handles the trivial special case of the (non-primality) of 1.
        const auto n_is_one =
          ((n0 == static_cast<local_limb_type>(UINT8_C(1))) && (np == static_cast<local_limb_type>(UINT8_C(1))));

        if(n_is_one)
        {
          return false;
        }

        // Exclude pure small primes from 3...227.
        // Table[Prime[i], {i, 2, 49}] =
        // {
        //     3,   5,   7,  11,  13,  17,  19,  23,
        //    29,  31,  37,  41,  43,  47,  53,  59,
        //    61,  67,  71,  73,  79,  83,  89,  97,
        //   101, 103, 107, 109, 113, 127, 131, 137,
        //   139, 149, 151, 157, 163, 167, 173, 179,
        //   181, 191, 193, 197, 199, 211, 223, 227
        // }
        // See also:
        // https://www.wolframalpha.com/input/?i=Table%5BPrime%5Bi%5D%2C+%7Bi%2C+2%2C+49%7D%5D

        constexpr std::array<local_limb_type, static_cast<std::size_t>(UINT8_C(48))> small_primes =
        {{
          static_cast<local_limb_type>(UINT8_C(  3)), static_cast<local_limb_type>(UINT8_C(  5)), static_cast<local_limb_type>(UINT8_C(  7)), static_cast<local_limb_type>(UINT8_C( 11)), static_cast<local_limb_type>(UINT8_C( 13)), static_cast<local_limb_type>(UINT8_C( 17)), static_cast<local_limb_type>(UINT8_C( 19)), static_cast<local_limb_type>(UINT8_C( 23)),
          static_cast<local_limb_type>(UINT8_C( 29)), static_cast<local_limb_type>(UINT8_C( 31)), static_cast<local_limb_type>(UINT8_C( 37)), static_cast<local_limb_type>(UINT8_C( 41)), static_cast<local_limb_type>(UINT8_C( 43)), static_cast<local_limb_type>(UINT8_C( 47)), static_cast<local_limb_type>(UINT8_C( 53)), static_cast<local_limb_type>(UINT8_C( 59)),
          static_cast<local_limb_type>(UINT8_C( 61)), static_cast<local_limb_type>(UINT8_C( 67)), static_cast<local_limb_type>(UINT8_C( 71)), static_cast<local_limb_type>(UINT8_C( 73)), static_cast<local_limb_type>(UINT8_C( 79)), static_cast<local_limb_type>(UINT8_C( 83)), static_cast<local_limb_type>(UINT8_C( 89)), static_cast<local_limb_type>(UINT8_C( 97)),
          static_cast<local_limb_type>(UINT8_C(101)), static_cast<local_limb_type>(UINT8_C(103)), static_cast<local_limb_type>(UINT8_C(107)), static_cast<local_limb_type>(UINT8_C(109)), static_cast<local_limb_type>(UINT8_C(113)), static_cast<local_limb_type>(UINT8_C(127)), static_cast<local_limb_type>(UINT8_C(131)), static_cast<local_limb_type>(UINT8_C(137)),
          static_cast<local_limb_type>(UINT8_C(139)), static_cast<local_limb_type>(UINT8_C(149)), static_cast<local_limb_type>(UINT8_C(151)), static_cast<local_limb_type>(UINT8_C(157)), static_cast<local_limb_type>(UINT8_C(163)), static_cast<local_limb_type>(UINT8_C(167)), static_cast<local_limb_type>(UINT8_C(173)), static_cast<local_limb_type>(UINT8_C(179)),
          static_cast<local_limb_type>(UINT8_C(181)), static_cast<local_limb_type>(UINT8_C(191)), static_cast<local_limb_type>(UINT8_C(193)), static_cast<local_limb_type>(UINT8_C(197)), static_cast<local_limb_type>(UINT8_C(199)), static_cast<local_limb_type>(UINT8_C(211)), static_cast<local_limb_type>(UINT8_C(223)), static_cast<local_limb_type>(UINT8_C(227))
        }};

        return std::binary_search(small_primes.cbegin(),
                                  small_primes.cend(),
                                  n0);
      }
    }

    // Check small factors.

    // Exclude small prime factors from { 3 ...  53 }.
    // Product[Prime[i], {i, 2, 16}] = 16294579238595022365
    // See also: https://www.wolframalpha.com/input/?i=Product%5BPrime%5Bi%5D%2C+%7Bi%2C+2%2C+16%7D%5D
    {
      constexpr std::uint64_t pp0 = UINT64_C(16294579238595022365);

      const auto m0 = static_cast<std::uint64_t>(np % pp0);

      if((m0 == static_cast<std::uint64_t>(UINT8_C(0))) || (detail::integer_gcd_reduce(m0, pp0) != static_cast<std::uint64_t>(UINT8_C(1))))
      {
        return false;
      }
    }

    // Exclude small prime factors from { 59 ... 101 }.
    // Product[Prime[i], {i, 17, 26}] = 7145393598349078859
    // See also: https://www.wolframalpha.com/input/?i=Product%5BPrime%5Bi%5D%2C+%7Bi%2C+17%2C+26%7D%5D
    {
      constexpr std::uint64_t pp1 = UINT64_C(7145393598349078859);

      const auto m1 = static_cast<std::uint64_t>(np % pp1);

      if((m1 == static_cast<std::uint64_t>(UINT8_C(0))) || (detail::integer_gcd_reduce(m1, pp1) != static_cast<std::uint64_t>(UINT8_C(1))))
      {
        return false;
      }
    }

    // Exclude small prime factors from { 103 ... 149 }.
    // Product[Prime[i], {i, 27, 35}] = 6408001374760705163
    // See also: https://www.wolframalpha.com/input/?i=Product%5BPrime%5Bi%5D%2C+%7Bi%2C+27%2C+35%7D%5D
    {
      constexpr std::uint64_t pp2 = UINT64_C(6408001374760705163);

      const auto m2 = static_cast<std::uint64_t>(np % pp2);

      if((m2 == static_cast<std::uint64_t>(UINT8_C(0))) || (detail::integer_gcd_reduce(m2, pp2) != static_cast<std::uint64_t>(UINT8_C(1))))
      {
        return false;
      }
    }

    // Exclude small prime factors from { 151 ... 191 }.
    // Product[Prime[i], {i, 36, 43}] = 690862709424854779
    // See also: https://www.wolframalpha.com/input/?i=Product%5BPrime%5Bi%5D%2C+%7Bi%2C+36%2C+43%7D%5D
    {
      constexpr std::uint64_t pp3 = UINT64_C(690862709424854779);

      const auto m3 = static_cast<std::uint64_t>(np % pp3);

      if((m3 == static_cast<std::uint64_t>(UINT8_C(0))) || (detail::integer_gcd_reduce(m3, pp3) != static_cast<std::uint64_t>(UINT8_C(1))))
      {
        return false;
      }
    }

    // Exclude small prime factors from { 193 ... 227 }.
    // Product[Prime[i], {i, 44, 49}] = 80814592450549
    // See also: https://www.wolframalpha.com/input/?i=Product%5BPrime%5Bi%5D%2C+%7Bi%2C+44%2C+49%7D%5D
    {
      constexpr std::uint64_t pp4 = UINT64_C(80814592450549);

      const auto m4 = static_cast<std::uint64_t>(np % pp4);

      if((m4 == static_cast<std::uint64_t>(UINT8_C(0))) || (detail::integer_gcd_reduce(m4, pp4) != static_cast<std::uint64_t>(UINT8_C(1))))
      {
        return false;
      }
    }

    const auto nm1 = static_cast<local_wide_integer_type>(np - static_cast<unsigned>(UINT8_C(1)));

    // Since we have already excluded all small factors
    // up to and including 227, n is greater than 227.

    {
      // Perform a single Fermat test which will
      // exclude many non-prime candidates.

      const local_wide_integer_type fn = powm(local_wide_integer_type(static_cast<local_limb_type>(228U)), nm1, np);

      const auto fn0 = static_cast<local_limb_type>(fn);

      if((fn0 != static_cast<local_limb_type>(UINT8_C(1))) && (fn != 1U))
      {
        return false;
      }
    }

    const unsigned_fast_type k = lsb(nm1);

    const local_wide_integer_type q = nm1 >> k;

    using local_param_type = typename DistributionType::param_type;

    const local_param_type params(local_wide_integer_type(2U), np - 2U);

    auto is_probably_prime = true;

    auto i = static_cast<unsigned_fast_type>(UINT8_C(0));

    auto x = local_wide_integer_type { };
    auto y = local_wide_integer_type { };

    // Execute the random trials.
    do
    {
      x = distribution(generator, params);
      y = powm(x, q, np);

      auto j = static_cast<unsigned_fast_type>(UINT8_C(0));

      while(y != nm1) // NOLINT(altera-id-dependent-backward-branch)
      {
        const local_limb_type y0(y);

        if((y0 == static_cast<local_limb_type>(UINT8_C(1))) && (y == 1U))
        {
          if(j != static_cast<unsigned_fast_type>(UINT8_C(0)))
          {
            is_probably_prime = false;
          }
          else
          {
            break;
          }
        }
        else
        {
          ++j;

          if(j == k)
          {
            is_probably_prime = false;
          }
          else
          {
            y = powm(y, 2U, np);
          }
        }
      }

      ++i;
    }
    while((i < number_of_trials) && is_probably_prime);

    // The prime candidate is probably prime in the sense
    // of the very high probability resulting from Miller-Rabin.
    return is_probably_prime;
  }

  #if (defined(__cpp_lib_to_chars) && (__cpp_lib_to_chars >= 201611L))
  template<const size_t Width2,
           typename LimbType,
           typename AllocatorType,
           const bool IsSigned>
  auto to_chars(char* first,
                char* last,
                const uintwide_t<Width2, LimbType, AllocatorType, IsSigned>& x,
                int base) -> std::to_chars_result
  {
    using local_wide_integer_type = uintwide_t<Width2, LimbType, AllocatorType, IsSigned>;

    const auto base_rep     = static_cast<std::uint_fast8_t>(base);
    const auto show_base    = false;
    const auto show_pos     = false;
    const auto is_uppercase = false;

    std::to_chars_result result { last, std::errc::value_too_large };

    if(base_rep == static_cast<std::uint_fast8_t>(UINT8_C(8)))
    {
      using string_storage_oct_type =
        std::conditional_t
          <local_wide_integer_type::my_width2 <= static_cast<size_t>(UINT32_C(2048)),
            detail::fixed_static_array <char,
                                        local_wide_integer_type::wr_string_max_buffer_size_oct()>,
            detail::fixed_dynamic_array<char,
                                        local_wide_integer_type::wr_string_max_buffer_size_oct(),
                                        typename std::allocator_traits<std::conditional_t<std::is_same<AllocatorType, void>::value,
                                                                                         std::allocator<void>,
                                                                                         AllocatorType>>::template rebind_alloc<typename local_wide_integer_type::limb_type>>>;

      using local_size_type = typename string_storage_oct_type::size_type;

      string_storage_oct_type str_temp { };

      str_temp.fill(local_wide_integer_type::my_fill_char());

      const auto wr_string_is_ok = x.wr_string(str_temp.begin(), base_rep, show_base, show_pos, is_uppercase);

      auto rit_trim = std::find_if(str_temp.crbegin(),
                                   str_temp.crend(),
                                   local_wide_integer_type::is_not_fill_char);

      const auto wr_string_and_trim_is_ok =
      (
        (rit_trim != str_temp.crend()) && wr_string_is_ok
      );

      if(wr_string_and_trim_is_ok)
      {
        const auto chars_retrieved =
          static_cast<local_size_type>
          (
            str_temp.size() - static_cast<local_size_type>(std::distance(str_temp.crbegin(), rit_trim))
          );

        const auto chars_to_get = static_cast<local_size_type>(std::distance(first, last));

        result.ptr = detail::copy_unsafe(str_temp.cbegin(),
                                         str_temp.cbegin() + (std::min)(chars_retrieved, chars_to_get),
                                         first);

        result.ec = std::errc();
      }
    }
    else if(base_rep == static_cast<std::uint_fast8_t>(UINT8_C(16)))
    {
      using string_storage_hex_type =
        std::conditional_t
          <local_wide_integer_type::my_width2 <= static_cast<size_t>(UINT32_C(2048)),
            detail::fixed_static_array <char,
                                        local_wide_integer_type::wr_string_max_buffer_size_hex()>,
            detail::fixed_dynamic_array<char,
                                        local_wide_integer_type::wr_string_max_buffer_size_hex(),
                                        typename std::allocator_traits<std::conditional_t<std::is_same<AllocatorType, void>::value,
                                                                                         std::allocator<void>,
                                                                                         AllocatorType>>::template rebind_alloc<typename local_wide_integer_type::limb_type>>>;

      using local_size_type = typename string_storage_hex_type::size_type;

      string_storage_hex_type str_temp { };

      str_temp.fill(local_wide_integer_type::my_fill_char());

      const auto wr_string_is_ok = x.wr_string(str_temp.begin(), base_rep, show_base, show_pos, is_uppercase);

      auto rit_trim = std::find_if(str_temp.crbegin(),
                                   str_temp.crend(),
                                   local_wide_integer_type::is_not_fill_char);

      const auto wr_string_and_trim_is_ok =
      (
        (rit_trim != str_temp.crend()) && wr_string_is_ok
      );

      if(wr_string_and_trim_is_ok)
      {
        const auto chars_retrieved =
          static_cast<local_size_type>
          (
            str_temp.size() - static_cast<local_size_type>(std::distance(str_temp.crbegin(), rit_trim))
          );

        const auto chars_to_get = static_cast<local_size_type>(std::distance(first, last));

        result.ptr = detail::copy_unsafe(str_temp.cbegin(),
                                         str_temp.cbegin() + (std::min)(chars_retrieved, chars_to_get),
                                         first);

        result.ec = std::errc();
      }
    }
    else
    {
      using string_storage_dec_type =
        std::conditional_t
          <local_wide_integer_type::my_width2 <= static_cast<size_t>(UINT32_C(2048)),
            detail::fixed_static_array <char,
                                        local_wide_integer_type::wr_string_max_buffer_size_dec()>,
            detail::fixed_dynamic_array<char,
                                        local_wide_integer_type::wr_string_max_buffer_size_dec(),
                                        typename std::allocator_traits<std::conditional_t<std::is_same<AllocatorType, void>::value,
                                                                                         std::allocator<void>,
                                                                                         AllocatorType>>::template rebind_alloc<typename local_wide_integer_type::limb_type>>>;

      using local_size_type = typename string_storage_dec_type::size_type;

      string_storage_dec_type str_temp { };

      str_temp.fill(local_wide_integer_type::my_fill_char());

      const auto wr_string_is_ok = x.wr_string(str_temp.begin(), base_rep, show_base, show_pos, is_uppercase);

      auto rit_trim = std::find_if(str_temp.crbegin(),
                                   str_temp.crend(),
                                   local_wide_integer_type::is_not_fill_char);

      const auto wr_string_and_trim_is_ok =
      (
        (rit_trim != str_temp.crend()) &&  wr_string_is_ok
      );

      if(wr_string_and_trim_is_ok)
      {
        const auto chars_retrieved =
          static_cast<local_size_type>
          (
            str_temp.size() - static_cast<local_size_type>(std::distance(str_temp.crbegin(), rit_trim))
          );

        const auto chars_to_get = static_cast<local_size_type>(std::distance(first, last));

        result.ptr = detail::copy_unsafe(str_temp.cbegin(),
                                         str_temp.cbegin() + (std::min)(chars_retrieved, chars_to_get),
                                         first);

        result.ec = std::errc();
      }
    }

    return result;
  }
  #endif

  #if !defined(WIDE_INTEGER_DISABLE_TO_STRING)
  template<const size_t Width2,
           typename LimbType,
           typename AllocatorType,
           const bool IsSigned>
  auto to_string(const uintwide_t<Width2, LimbType, AllocatorType, IsSigned>& x) -> std::string
  {
    using local_wide_integer_type = uintwide_t<Width2, LimbType, AllocatorType, IsSigned>;

    using string_storage_dec_type =
      std::conditional_t
        <local_wide_integer_type::my_width2 <= static_cast<size_t>(UINT32_C(2048)),
          detail::fixed_static_array <char,
                                      local_wide_integer_type::wr_string_max_buffer_size_dec()>,
          detail::fixed_dynamic_array<char,
                                      local_wide_integer_type::wr_string_max_buffer_size_dec(),
                                      typename std::allocator_traits<std::conditional_t<std::is_same<AllocatorType, void>::value,
                                                                                       std::allocator<void>,
                                                                                       AllocatorType>>::template rebind_alloc<typename local_wide_integer_type::limb_type>>>;

    using local_size_type = typename string_storage_dec_type::size_type;

    string_storage_dec_type str_temp; // LCOV_EXCL_LINE

    str_temp.fill(local_wide_integer_type::my_fill_char());

    const auto base_rep     = static_cast<std::uint_fast8_t>(UINT8_C(10));
    const auto show_base    = false;
    const auto show_pos     = false;
    const auto is_uppercase = false;

    const auto wr_string_is_ok = x.wr_string(str_temp.begin(), base_rep, show_base, show_pos, is_uppercase);

    auto rit_trim = std::find_if(str_temp.crbegin(),
                                 str_temp.crend(),
                                 local_wide_integer_type::is_not_fill_char);

    const auto wr_string_and_trim_is_ok =
    (
      (rit_trim != str_temp.crend()) && wr_string_is_ok
    );

    std::string str_result { };

    if(wr_string_and_trim_is_ok)
    {
      const auto str_result_size =
        static_cast<local_size_type>
        (
            str_temp.size()
          - static_cast<local_size_type>(std::distance(str_temp.crbegin(), rit_trim))
        );

      detail::fill_unsafe(str_temp.begin() + str_result_size, str_temp.end(), '\0');

      str_result = std::string(str_temp.data());
    }

    return str_result;
  }
  #endif

  template<typename ForwardIterator,
           const size_t Width2,
           typename LimbType,
           typename AllocatorType,
           std::enable_if_t<std::numeric_limits<typename std::iterator_traits<ForwardIterator>::value_type>::digits == std::numeric_limits<LimbType>::digits> const*>
  WIDE_INTEGER_CONSTEXPR
  auto import_bits(uintwide_t<Width2, LimbType, AllocatorType, false>& val,
                   ForwardIterator first,
                   ForwardIterator last,
                   unsigned        chunk_size,
                   bool            msv_first) -> uintwide_t<Width2, LimbType, AllocatorType, false>&
  {
    // This subroutine implements limb-by-limb import of bit-chunks.
    // This template specialization is intended for full chunk sizes,
    // whereby the width of the chunk's value type equals the limb's width.
    // If, however, the chunk_size to import is not "full", then this
    // subroutine uses slow bit-by-bit methods.
    // The order of bit-chunks imported is set by msv_first.

    using local_unsigned_wide_integer_type = uintwide_t<Width2, LimbType, AllocatorType, false>;
    using local_result_iterator_type       = typename local_unsigned_wide_integer_type::representation_type::reverse_iterator;
    using local_result_value_type          = typename local_result_iterator_type::value_type;
    using local_input_iterator_type        = ForwardIterator;
    using local_input_value_type           = typename std::iterator_traits<local_input_iterator_type>::value_type;

    static_assert(std::numeric_limits<local_result_value_type>::digits == std::numeric_limits<local_input_value_type>::digits,
                  "Error: Mismatch for input element width and result uintwide_t limb width");

    if(chunk_size == static_cast<unsigned>(UINT8_C(0)))
    {
      chunk_size = static_cast<unsigned>(std::numeric_limits<local_input_value_type>::digits);
    }

    chunk_size = (std::min)(static_cast<unsigned>(std::numeric_limits<local_input_value_type>::digits), chunk_size);

    const auto chunk_is_whole =
      (chunk_size == static_cast<unsigned>(std::numeric_limits<local_result_value_type>::digits));

    const auto input_distance = static_cast<std::size_t>(std::distance(first, last));

    if(chunk_is_whole)
    {
      const auto copy_len =
        (std::min)(static_cast<std::size_t>(val.crepresentation().size()), input_distance);

      if(msv_first)
      {
        detail::copy_unsafe(first,
                            detail::advance_and_point(first, copy_len),
                            local_result_iterator_type(detail::advance_and_point(val.representation().begin(), copy_len)));
      }
      else
      {
        using local_input_reverse_iterator_type = std::reverse_iterator<local_input_iterator_type>;

        using local_difference_type = typename local_result_iterator_type::difference_type;

        detail::copy_unsafe(local_input_reverse_iterator_type(detail::advance_and_point(first, static_cast<local_difference_type>(input_distance))),
                            local_input_reverse_iterator_type(detail::advance_and_point(first, static_cast<local_difference_type>(static_cast<local_difference_type>(input_distance) - static_cast<local_difference_type>(copy_len)))),
                            local_result_iterator_type       (detail::advance_and_point(val.representation().begin(), copy_len)));
      }

      detail::fill_unsafe(detail::advance_and_point(val.representation().begin(), copy_len),
                          val.representation().end(),
                          static_cast<local_result_value_type>(UINT8_C(0)));
    }
    else
    {
      val = 0;

      const auto chunk_size_in  = static_cast<unsigned_fast_type>(chunk_size);
      const auto chunk_size_out = static_cast<unsigned_fast_type>(std::numeric_limits<local_result_value_type>::digits);

      const auto total_bits_input =
        static_cast<unsigned_fast_type>
        (
          static_cast<std::size_t>(chunk_size_in) * input_distance
        );

      const auto total_bits_to_use =
        (std::min)
        (
          static_cast<signed_fast_type>(total_bits_input),
          static_cast<signed_fast_type>(std::numeric_limits<local_unsigned_wide_integer_type>::digits)
        );

      const auto result_distance =
        static_cast<std::size_t>
        (
            static_cast<std::size_t>(static_cast<std::size_t>(total_bits_to_use) / chunk_size_out)
          + static_cast<std::size_t>
            (
              (static_cast<std::size_t>(static_cast<std::size_t>(total_bits_to_use) % chunk_size_out) != static_cast<std::size_t>(UINT8_C(0)))
                ? static_cast<std::size_t>(UINT8_C(1))
                : static_cast<std::size_t>(UINT8_C(0))
            )
        );

      auto it_result = local_result_iterator_type(detail::advance_and_point(val.representation().begin(), result_distance));

      if(msv_first)
      {
        detail::import_export_helper(first, it_result, total_bits_to_use, chunk_size_in, chunk_size_out);
      }
      else
      {
        using local_input_reverse_iterator_type = std::reverse_iterator<local_input_iterator_type>;

        detail::import_export_helper(local_input_reverse_iterator_type(last), it_result, total_bits_to_use, chunk_size_in, chunk_size_out);
      }
    }

    return val;
  }

  template<typename ForwardIterator,
           const size_t Width2,
           typename LimbType,
           typename AllocatorType,
           std::enable_if_t<!(std::numeric_limits<typename std::iterator_traits<ForwardIterator>::value_type>::digits == std::numeric_limits<LimbType>::digits)> const*>
  WIDE_INTEGER_CONSTEXPR
  auto import_bits(uintwide_t<Width2, LimbType, AllocatorType, false>& val,
                   ForwardIterator first,
                   ForwardIterator last,
                   unsigned        chunk_size,
                   bool            msv_first) -> uintwide_t<Width2, LimbType, AllocatorType, false>&
  {
    // This subroutine implements limb-by-limb import of bit-chunks.
    // This template specialization is intended for non-full chunk sizes,
    // whereby the width of the chunk's value type differs from the limb's width.
    // The order of bit-chunks imported is set by msv_first.

    using local_unsigned_wide_integer_type = uintwide_t<Width2, LimbType, AllocatorType, false>;
    using local_result_iterator_type       = typename local_unsigned_wide_integer_type::representation_type::reverse_iterator;
    using local_result_value_type          = typename local_result_iterator_type::value_type;
    using local_input_iterator_type        = ForwardIterator;
    using local_input_value_type           = typename std::iterator_traits<local_input_iterator_type>::value_type;

    static_assert(std::numeric_limits<local_result_value_type>::digits != std::numeric_limits<local_input_value_type>::digits,
                  "Error: Erroneous match for input element width and result uintwide_t limb width");

    const auto input_distance = static_cast<std::size_t>(std::distance(first, last));

    val = 0;

    if(chunk_size == static_cast<unsigned>(UINT8_C(0)))
    {
      chunk_size = static_cast<unsigned>(std::numeric_limits<local_input_value_type>::digits);
    }

    chunk_size = (std::min)(static_cast<unsigned>(std::numeric_limits<local_input_value_type>::digits), chunk_size);

    const auto chunk_size_in  = static_cast<unsigned_fast_type>(chunk_size);
    const auto chunk_size_out = static_cast<unsigned_fast_type>(std::numeric_limits<local_result_value_type>::digits);

    const auto total_bits_input =
      static_cast<unsigned_fast_type>
      (
        static_cast<std::size_t>(chunk_size_in) * input_distance
      );

    const auto total_bits_to_use =
      (std::min)
      (
        static_cast<signed_fast_type>(total_bits_input),
        static_cast<signed_fast_type>(std::numeric_limits<local_unsigned_wide_integer_type>::digits)
      );

    const auto result_distance =
      static_cast<std::size_t>
      (
          static_cast<std::size_t>(static_cast<unsigned_fast_type>(total_bits_to_use) / chunk_size_out)
        + static_cast<std::size_t>
          (
            (static_cast<std::size_t>(static_cast<unsigned_fast_type>(total_bits_to_use) % chunk_size_out) != static_cast<std::size_t>(UINT8_C(0)))
              ? static_cast<std::size_t>(UINT8_C(1))
              : static_cast<std::size_t>(UINT8_C(0))
          )
      );

    auto it_result = local_result_iterator_type(detail::advance_and_point(val.representation().begin(), result_distance));

    if(msv_first)
    {
      detail::import_export_helper(first, it_result, total_bits_to_use, chunk_size_in, chunk_size_out);
    }
    else
    {
      using local_input_reverse_iterator_type = std::reverse_iterator<local_input_iterator_type>;

      detail::import_export_helper(local_input_reverse_iterator_type(last), it_result, total_bits_to_use, chunk_size_in, chunk_size_out);
    }

    return val;
  }

  template<typename OutputIterator,
           const size_t Width2,
           typename LimbType,
           typename AllocatorType,
           const bool IsSigned,
           std::enable_if_t<std::numeric_limits<typename std::iterator_traits<OutputIterator>::value_type>::digits == std::numeric_limits<LimbType>::digits> const*>
  WIDE_INTEGER_CONSTEXPR
  auto export_bits(const uintwide_t<Width2, LimbType, AllocatorType, IsSigned>& val,
                         OutputIterator out,
                         unsigned       chunk_size,
                         bool           msv_first) -> OutputIterator
  {
    // This subroutine implements limb-by-limb export of bit-chunks.
    // This template specialization is intended for full chunk sizes,
    // whereby the width of the chunk's value type equals the limb's width.
    // If, however, the chunk_size to export is not "full", then this
    // subroutine uses slow bit-by-bit methods.
    // The order of bit-chunks exported is set by msv_first.

    using local_unsigned_wide_integer_type = uintwide_t<Width2, LimbType, AllocatorType, false>;
    using local_result_iterator_type       = OutputIterator;
    using local_result_value_type          = typename std::iterator_traits<local_result_iterator_type>::value_type;
    using local_input_value_type           = typename local_unsigned_wide_integer_type::representation_type::value_type;

    const auto val_unsigned =
    (
      (!uintwide_t<Width2, LimbType, AllocatorType, IsSigned>::is_neg(val))
        ? static_cast<local_unsigned_wide_integer_type>(val)
        : static_cast<local_unsigned_wide_integer_type>(-val)
    );

    static_assert(std::numeric_limits<local_result_value_type>::digits == std::numeric_limits<local_input_value_type>::digits,
                  "Error: Erroneous mismatch for input element width and result uintwide_t limb width");

    chunk_size = (std::min)(static_cast<unsigned>(std::numeric_limits<local_result_value_type>::digits), chunk_size);

    const auto chunk_size_in  = static_cast<unsigned_fast_type>(std::numeric_limits<local_input_value_type>::digits);
    const auto chunk_size_out = chunk_size;

    const auto msb_plus_one =
      static_cast<unsigned_fast_type>(msb(val_unsigned) + static_cast<unsigned_fast_type>(UINT8_C(1)));

    const auto input_distance_chunk_size_has_mod =
      (static_cast<unsigned_fast_type>(msb_plus_one % chunk_size_in) != static_cast<unsigned_fast_type>(UINT8_C(0)));

    const auto input_distance =
      static_cast<std::size_t>
      (
          static_cast<std::size_t>(msb_plus_one / chunk_size_in)
        + static_cast<std::size_t>
          (
            input_distance_chunk_size_has_mod ? static_cast<std::size_t>(UINT8_C(1))
                                              : static_cast<std::size_t>(UINT8_C(0))
          )
      );

    const auto chunk_is_whole =
      (chunk_size == static_cast<unsigned>(std::numeric_limits<local_result_value_type>::digits));

    if(chunk_is_whole)
    {
      if(msv_first)
      {
        using local_input_const_reverse_iterator_type =
          typename local_unsigned_wide_integer_type::representation_type::const_reverse_iterator;

        out = detail::copy_unsafe(local_input_const_reverse_iterator_type(detail::advance_and_point(val.representation().cbegin(), input_distance)),
                                  val.representation().crend(),
                                  out);
      }
      else
      {
        out = detail::copy_unsafe(val.representation().cbegin(),
                                  detail::advance_and_point(val.representation().cbegin(), input_distance),
                                  out);
      }
    }
    else
    {
      if(msv_first)
      {
        using local_input_reverse_iterator_type = std::reverse_iterator<typename local_unsigned_wide_integer_type::representation_type::const_iterator>;

        out =   detail::import_export_helper(local_input_reverse_iterator_type(detail::advance_and_point(val_unsigned.crepresentation().cbegin(), input_distance)), out, static_cast<signed_fast_type>(msb_plus_one), chunk_size_in, chunk_size_out)
              + static_cast<std::size_t>(UINT8_C(1));
      }
      else
      {
        const auto output_distance_chunk_size_has_mod =
          (static_cast<unsigned_fast_type>(msb_plus_one % chunk_size_out) != static_cast<unsigned_fast_type>(UINT8_C(0)));

        const auto output_distance =
          static_cast<std::size_t>
          (
              static_cast<std::size_t>(msb_plus_one / chunk_size_out)
            + static_cast<std::size_t>
              (
                output_distance_chunk_size_has_mod ? static_cast<std::size_t>(UINT8_C(1))
                                                   : static_cast<std::size_t>(UINT8_C(0))
              )
          );

        using local_input_reverse_iterator_type  = typename local_unsigned_wide_integer_type::representation_type::const_reverse_iterator;
        using local_result_reverse_iterator_type = std::reverse_iterator<local_result_iterator_type>;

        static_cast<void>(detail::import_export_helper(local_input_reverse_iterator_type (detail::advance_and_point(val_unsigned.crepresentation().cbegin(), input_distance)),
                                                       local_result_reverse_iterator_type(detail::advance_and_point(out, output_distance)), // LCOV_EXCL_LINE
                                                       static_cast<signed_fast_type>(msb_plus_one),
                                                       chunk_size_in,
                                                       chunk_size_out));

        using result_difference_type = typename std::iterator_traits<local_result_iterator_type>::difference_type;

        out += static_cast<result_difference_type>(output_distance);
      }
    }

    return out;
  }

  template<typename OutputIterator,
           const size_t Width2,
           typename LimbType,
           typename AllocatorType,
           const bool IsSigned,
           std::enable_if_t<!(std::numeric_limits<typename std::iterator_traits<OutputIterator>::value_type>::digits == std::numeric_limits<LimbType>::digits)> const*>
  WIDE_INTEGER_CONSTEXPR
  auto export_bits(const uintwide_t<Width2, LimbType, AllocatorType, IsSigned>& val,
                         OutputIterator out,
                         unsigned       chunk_size,
                         bool           msv_first) -> OutputIterator
  {
    // This subroutine implements limb-by-limb export of bit-chunks.
    // This template specialization is intended for non-full chunk sizes,
    // whereby the width of the chunk's value type differs from the limb's width.
    // The order of bit-chunks exported is set by msv_first.

    using local_unsigned_wide_integer_type = uintwide_t<Width2, LimbType, AllocatorType, false>;
    using local_result_iterator_type       = OutputIterator;
    using local_result_value_type          = typename std::iterator_traits<local_result_iterator_type>::value_type;
    using local_input_value_type           = typename local_unsigned_wide_integer_type::representation_type::value_type;

    const auto val_unsigned =
    (
      (!uintwide_t<Width2, LimbType, AllocatorType, IsSigned>::is_neg(val))
        ? static_cast<local_unsigned_wide_integer_type>(val)
        : static_cast<local_unsigned_wide_integer_type>(-val)
    );

    static_assert(std::numeric_limits<local_result_value_type>::digits != std::numeric_limits<local_input_value_type>::digits,
                  "Error: Erroneous match for input element width and result uintwide_t limb width");

    chunk_size = (std::min)(static_cast<unsigned>(std::numeric_limits<local_result_value_type>::digits), chunk_size);

    const auto chunk_size_in  = static_cast<unsigned_fast_type>(std::numeric_limits<local_input_value_type>::digits);
    const auto chunk_size_out = chunk_size;

    const auto msb_plus_one =
      static_cast<unsigned_fast_type>(msb(val_unsigned) + static_cast<unsigned_fast_type>(UINT8_C(1)));

    const auto input_distance_chunk_size_has_mod =
      (static_cast<unsigned_fast_type>(msb_plus_one % chunk_size_in) != static_cast<unsigned_fast_type>(UINT8_C(0)));

    const auto input_distance =
      static_cast<std::size_t>
      (
          static_cast<std::size_t>(msb_plus_one / chunk_size_in)
        + static_cast<std::size_t>
          (
            input_distance_chunk_size_has_mod ? static_cast<std::size_t>(UINT8_C(1))
                                              : static_cast<std::size_t>(UINT8_C(0))
          )
      );

    if(msv_first)
    {
      using local_input_reverse_iterator_type = typename local_unsigned_wide_integer_type::representation_type::const_reverse_iterator;

      out =
        detail::import_export_helper
        (
          local_input_reverse_iterator_type(detail::advance_and_point(val_unsigned.crepresentation().cbegin(), input_distance)),
          out,
          static_cast<signed_fast_type>(msb_plus_one),
          chunk_size_in,
          chunk_size_out
        );

      ++out;
    }
    else
    {
      const auto output_distance_chunk_size_has_mod =
        (static_cast<unsigned_fast_type>(msb_plus_one % chunk_size_out) != static_cast<unsigned_fast_type>(UINT8_C(0)));

      const auto output_distance =
        static_cast<std::size_t>
        (
            static_cast<std::size_t>(msb_plus_one / chunk_size_out)
          + static_cast<std::size_t>
            (
              output_distance_chunk_size_has_mod ? static_cast<std::size_t>(UINT8_C(1))
                                                 : static_cast<std::size_t>(UINT8_C(0))
            )
        );

      using local_input_reverse_iterator_type  = typename local_unsigned_wide_integer_type::representation_type::const_reverse_iterator;
      using local_result_reverse_iterator_type = std::reverse_iterator<local_result_iterator_type>;

      static_cast<void>
      (
        detail::import_export_helper
        (
          local_input_reverse_iterator_type (detail::advance_and_point(val_unsigned.crepresentation().cbegin(), input_distance)),
          local_result_reverse_iterator_type(detail::advance_and_point(out, output_distance)),
          static_cast<signed_fast_type>(msb_plus_one),
          chunk_size_in,
          chunk_size_out
        )
      );

      using result_difference_type = typename std::iterator_traits<local_result_iterator_type>::difference_type;

      out += static_cast<result_difference_type>(output_distance);
    }

    return out;
  }

  #if(__cplusplus >= 201703L)
  } // namespace math::wide_integer
  #else
  } // namespace wide_integer
  } // namespace math
  #endif

  WIDE_INTEGER_NAMESPACE_END

#endif // UINTWIDE_T_2018_10_02_H
