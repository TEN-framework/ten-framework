///////////////////////////////////////////////////////////////////////////////
//  Copyright Christopher Kormanyos 2019 - 2022.
//  Distributed under the Boost Software License,
//  Version 1.0. (See accompanying file LICENSE_1_0.txt
//  or copy at http://www.boost.org/LICENSE_1_0.txt)
//

#ifndef TEST_UINTWIDE_T_N_BINARY_OPS_MUL_N_BY_M_TEMPLATE_2019_12_26_H // NOLINT(llvm-header-guard)
  #define TEST_UINTWIDE_T_N_BINARY_OPS_MUL_N_BY_M_TEMPLATE_2019_12_26_H

  #include <atomic>

  #include <test/test_uintwide_t_n_base.h>
  #include <test/test_uintwide_t_n_binary_ops_base.h>

  #if defined(WIDE_INTEGER_NAMESPACE)
  template<const WIDE_INTEGER_NAMESPACE::math::wide_integer::size_t MyDigits2A,
           const WIDE_INTEGER_NAMESPACE::math::wide_integer::size_t MyDigits2B,
           typename MyLimbType = std::uint32_t>
  #else
  template<const ::math::wide_integer::size_t MyDigits2A,
           const ::math::wide_integer::size_t MyDigits2B,
           typename MyLimbType = std::uint32_t>
  #endif
  class test_uintwide_t_n_binary_ops_mul_n_by_m_template : public test_uintwide_t_n_binary_ops_base // NOLINT(cppcoreguidelines-special-member-functions,hicpp-special-member-functions)
  {
  private:
    #if defined(WIDE_INTEGER_NAMESPACE)
    static constexpr auto digits2a = static_cast<WIDE_INTEGER_NAMESPACE::math::wide_integer::size_t>(MyDigits2A);
    static constexpr auto digits2b = static_cast<WIDE_INTEGER_NAMESPACE::math::wide_integer::size_t>(MyDigits2B);
    #else
    static constexpr auto digits2a = static_cast<math::wide_integer::size_t>(MyDigits2A);
    static constexpr auto digits2b = static_cast<math::wide_integer::size_t>(MyDigits2B);
    #endif

    #if defined(WIDE_INTEGER_NAMESPACE)
    WIDE_INTEGER_NODISCARD auto get_digits2a() const -> WIDE_INTEGER_NAMESPACE::math::wide_integer::size_t          { return digits2a; }
    WIDE_INTEGER_NODISCARD auto get_digits2b() const -> WIDE_INTEGER_NAMESPACE::math::wide_integer::size_t          { return digits2b; }
    WIDE_INTEGER_NODISCARD auto get_digits2 () const -> WIDE_INTEGER_NAMESPACE::math::wide_integer::size_t override { return digits2a + digits2b; }
    #else
    WIDE_INTEGER_NODISCARD auto get_digits2a() const -> ::math::wide_integer::size_t          { return digits2a; }
    WIDE_INTEGER_NODISCARD auto get_digits2b() const -> ::math::wide_integer::size_t          { return digits2b; }
    WIDE_INTEGER_NODISCARD auto get_digits2 () const -> ::math::wide_integer::size_t override { return digits2a + digits2b; }
    #endif

    using boost_uint_backend_a_allocator_type = void;

    using boost_uint_backend_a_type =
      boost::multiprecision::cpp_int_backend<digits2a,
                                             digits2a,
                                             boost::multiprecision::unsigned_magnitude,
                                             boost::multiprecision::unchecked,
                                             boost_uint_backend_a_allocator_type>;

    using boost_uint_backend_b_allocator_type = void;

    using boost_uint_backend_b_type =
      boost::multiprecision::cpp_int_backend<digits2b,
                                             digits2b,
                                             boost::multiprecision::unsigned_magnitude,
                                             boost::multiprecision::unchecked,
                                             boost_uint_backend_b_allocator_type>;

    using boost_uint_backend_c_allocator_type = void;

    using boost_uint_backend_c_type =
      boost::multiprecision::cpp_int_backend<digits2a + digits2b,
                                             digits2a + digits2b,
                                             boost::multiprecision::unsigned_magnitude,
                                             boost::multiprecision::unchecked,
                                             boost_uint_backend_c_allocator_type>;

    using boost_uint_a_type = boost::multiprecision::number<boost_uint_backend_a_type, boost::multiprecision::et_off>;
    using boost_uint_b_type = boost::multiprecision::number<boost_uint_backend_b_type, boost::multiprecision::et_off>;
    using boost_uint_c_type = boost::multiprecision::number<boost_uint_backend_c_type, boost::multiprecision::et_off>;

    using local_limb_type = MyLimbType;

    #if defined(WIDE_INTEGER_NAMESPACE)
    using local_uint_a_type = WIDE_INTEGER_NAMESPACE::math::wide_integer::uintwide_t<digits2a, local_limb_type>;
    using local_uint_b_type = WIDE_INTEGER_NAMESPACE::math::wide_integer::uintwide_t<digits2b, local_limb_type>;
    using local_uint_c_type = WIDE_INTEGER_NAMESPACE::math::wide_integer::uintwide_t<digits2a + digits2b, local_limb_type>;
    #else
    using local_uint_a_type = ::math::wide_integer::uintwide_t<digits2a, local_limb_type>;
    using local_uint_b_type = ::math::wide_integer::uintwide_t<digits2b, local_limb_type>;
    using local_uint_c_type = ::math::wide_integer::uintwide_t<digits2a + digits2b, local_limb_type>;
    #endif

  public:
    explicit test_uintwide_t_n_binary_ops_mul_n_by_m_template(const std::size_t count)
      : test_uintwide_t_n_binary_ops_base(count) { }

    ~test_uintwide_t_n_binary_ops_mul_n_by_m_template() override = default;

    auto do_test(std::size_t rounds) -> bool override
    {
      bool result_is_ok = true;

      for(std::size_t i = 0U; i < rounds; ++i)
      {
        std::cout << "initialize()       boost compare with uintwide_t: round " << i << ",  digits2: " << this->get_digits2() << std::endl;
        this->initialize();

        std::cout << "test_binary_mul()  boost compare with uintwide_t: round " << i << ",  digits2: " << this->get_digits2() << std::endl;
        result_is_ok = (test_binary_mul() && result_is_ok);
      }

      return result_is_ok;
    }

    auto initialize() -> void override
    {
      a_local.clear();
      b_local.clear();

      a_boost.clear();
      b_boost.clear();

      a_local.resize(size());
      b_local.resize(size());

      a_boost.resize(size());
      b_boost.resize(size());

      get_equal_random_test_values_boost_and_local_n(a_local.data(), a_boost.data(), size());
      get_equal_random_test_values_boost_and_local_n(b_local.data(), b_boost.data(), size());
    }

    WIDE_INTEGER_NODISCARD auto test_binary_mul() const -> bool
    {
      bool result_is_ok = true;

      std::atomic_flag test_lock = ATOMIC_FLAG_INIT;

      my_concurrency::parallel_for
      (
        static_cast<std::size_t>(0U),
        size(),
        [&test_lock, &result_is_ok, this](std::size_t i)
        {
          const boost_uint_c_type c_boost =   boost_uint_c_type(a_boost[i])
                                            * b_boost[i];

          const local_uint_c_type c_local =   static_cast<local_uint_c_type>(a_local[i])
                                            * static_cast<local_uint_c_type>(b_local[i]);

          const std::string str_boost = hexlexical_cast(c_boost);
          const std::string str_local = hexlexical_cast(c_local);

          while(test_lock.test_and_set()) { ; }
          result_is_ok = ((str_boost == str_local) && result_is_ok);
          test_lock.clear();
        }
      );

      return result_is_ok;
    }

  private:
    std::vector<local_uint_a_type> a_local { }; // NOLINT(readability-identifier-naming)
    std::vector<local_uint_b_type> b_local { }; // NOLINT(readability-identifier-naming)

    std::vector<boost_uint_a_type> a_boost { }; // NOLINT(readability-identifier-naming)
    std::vector<boost_uint_b_type> b_boost { }; // NOLINT(readability-identifier-naming)
  };

#endif // TEST_UINTWIDE_T_N_BINARY_OPS_MUL_N_BY_M_TEMPLATE_2019_12_26_H
