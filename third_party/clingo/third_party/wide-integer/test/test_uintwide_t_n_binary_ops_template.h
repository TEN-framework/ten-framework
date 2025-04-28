///////////////////////////////////////////////////////////////////////////////
//  Copyright Christopher Kormanyos 2019 - 2022.
//  Distributed under the Boost Software License,
//  Version 1.0. (See accompanying file LICENSE_1_0.txt
//  or copy at http://www.boost.org/LICENSE_1_0.txt)
//

#ifndef TEST_UINTWIDE_T_N_BINARY_OPS_TEMPLATE_2019_12_19_H // NOLINT(llvm-header-guard)
  #define TEST_UINTWIDE_T_N_BINARY_OPS_TEMPLATE_2019_12_19_H

  #include <algorithm>
  #include <atomic>
  #include <cstddef>
  #include <random>
  #include <vector>

  #include <test/test_uintwide_t_n_binary_ops_base.h>

  #if defined(WIDE_INTEGER_NAMESPACE)
  template<const WIDE_INTEGER_NAMESPACE::math::wide_integer::size_t MyDigits2,
           typename MyLimbType = std::uint32_t,
           typename AllocatorType = void>
  #else
  template<const ::math::wide_integer::size_t MyDigits2,
           typename MyLimbType = std::uint32_t,
           typename AllocatorType = void>
  #endif
  class test_uintwide_t_n_binary_ops_template : public test_uintwide_t_n_binary_ops_base // NOLINT(cppcoreguidelines-special-member-functions,hicpp-special-member-functions)
  {
  private:
    #if defined(WIDE_INTEGER_NAMESPACE)
    static constexpr auto digits2 = static_cast<WIDE_INTEGER_NAMESPACE::math::wide_integer::size_t>(MyDigits2);
    #else
    static constexpr auto digits2 = static_cast<math::wide_integer::size_t>(MyDigits2);
    #endif

    using boost_uint_backend_allocator_type = void;

    using boost_uint_backend_type =
      boost::multiprecision::cpp_int_backend<digits2,
                                             digits2,
                                             boost::multiprecision::unsigned_magnitude,
                                             boost::multiprecision::unchecked,
                                             boost_uint_backend_allocator_type>;

    using boost_uint_type = boost::multiprecision::number<boost_uint_backend_type, boost::multiprecision::et_on>;

    using local_limb_type = MyLimbType;

    #if defined(WIDE_INTEGER_NAMESPACE)
    using local_uint_type = WIDE_INTEGER_NAMESPACE::math::wide_integer::uintwide_t<digits2, local_limb_type, AllocatorType>;
    #else
    using local_uint_type = ::math::wide_integer::uintwide_t<digits2, local_limb_type, AllocatorType>;
    #endif

  public:
    explicit test_uintwide_t_n_binary_ops_template(const std::size_t count)
      : test_uintwide_t_n_binary_ops_base(count) { }

    ~test_uintwide_t_n_binary_ops_template() override = default;

    #if defined(WIDE_INTEGER_NAMESPACE)
    WIDE_INTEGER_NODISCARD auto get_digits2() const -> WIDE_INTEGER_NAMESPACE::math::wide_integer::size_t override { return digits2; }
    #else
    WIDE_INTEGER_NODISCARD auto get_digits2() const -> ::math::wide_integer::size_t override { return digits2; }
    #endif

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

      get_equal_random_test_values_boost_and_local_n<local_uint_type, boost_uint_type, AllocatorType>(a_local.data(), a_boost.data(), size());
      get_equal_random_test_values_boost_and_local_n<local_uint_type, boost_uint_type, AllocatorType>(b_local.data(), b_boost.data(), size());
    }

    auto do_test(std::size_t rounds) -> bool override
    {
      bool result_is_ok = true;

      for(std::size_t i = 0U; i < rounds; ++i)
      {
        std::cout << "initialize()       boost compare with uintwide_t: round " << i << ",  digits2: " << this->get_digits2() << std::endl;
        this->initialize();

        std::cout << "test_binary_add()  boost compare with uintwide_t: round " << i << ",  digits2: " << this->get_digits2() << std::endl;
        result_is_ok = (test_binary_add() && result_is_ok);

        std::cout << "test_binary_sub()  boost compare with uintwide_t: round " << i << ",  digits2: " << this->get_digits2() << std::endl;
        result_is_ok = (test_binary_sub() && result_is_ok);

        std::cout << "test_binary_mul()  boost compare with uintwide_t: round " << i << ",  digits2: " << this->get_digits2() << std::endl;
        result_is_ok = (test_binary_mul() && result_is_ok);

        std::cout << "test_binary_div()  boost compare with uintwide_t: round " << i << ",  digits2: " << this->get_digits2() << std::endl;
        result_is_ok = (test_binary_div() && result_is_ok);

        std::cout << "test_binary_mod()  boost compare with uintwide_t: round " << i << ",  digits2: " << this->get_digits2() << std::endl;
        result_is_ok = (test_binary_mod() && result_is_ok);

        std::cout << "test_binary_sqrt() boost compare with uintwide_t: round " << i << ",  digits2: " << this->get_digits2() << std::endl;
        result_is_ok = (test_binary_sqrt() && result_is_ok);
      }

      return result_is_ok;
    }

    WIDE_INTEGER_NODISCARD auto test_binary_add() const -> bool
    {
      bool result_is_ok = true;

      std::atomic_flag test_lock = ATOMIC_FLAG_INIT;

      my_concurrency::parallel_for
      (
        static_cast<std::size_t>(0U),
        size(),
        [&test_lock, &result_is_ok, this](std::size_t i)
        {
          const boost_uint_type c_boost = a_boost[i] + b_boost[i];
          const local_uint_type c_local = a_local[i] + b_local[i];

          const std::string str_boost = hexlexical_cast(c_boost);
          const std::string str_local = hexlexical_cast(c_local);

          while(test_lock.test_and_set()) { ; }
          result_is_ok = ((str_boost == str_local) && result_is_ok);
          test_lock.clear();
        }
      );

      return result_is_ok;
    }

    WIDE_INTEGER_NODISCARD auto test_binary_sub() const -> bool
    {
      bool result_is_ok = true;

      std::atomic_flag test_lock = ATOMIC_FLAG_INIT;

      my_concurrency::parallel_for
      (
        static_cast<std::size_t>(0U),
        size(),
        [&test_lock, &result_is_ok, this](std::size_t i)
        {
          const boost_uint_type c_boost = a_boost[i] - b_boost[i];
          const local_uint_type c_local = a_local[i] - b_local[i];

          const std::string str_boost = hexlexical_cast(c_boost);
          const std::string str_local = hexlexical_cast(c_local);

          while(test_lock.test_and_set()) { ; }
          result_is_ok = ((str_boost == str_local) && result_is_ok);
          test_lock.clear();
        }
      );

      return result_is_ok;
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
          const boost_uint_type c_boost = a_boost[i] * b_boost[i];
          const local_uint_type c_local = a_local[i] * b_local[i];

          const std::string str_boost = hexlexical_cast(c_boost);
          const std::string str_local = hexlexical_cast(c_local);

          while(test_lock.test_and_set()) { ; }
          result_is_ok = ((str_boost == str_local) && result_is_ok);
          test_lock.clear();
        }
      );

      return result_is_ok;
    }

    WIDE_INTEGER_NODISCARD auto test_binary_div() const -> bool
    {
      std::atomic_flag test_lock = ATOMIC_FLAG_INIT;

      my_gen().seed(static_cast<typename random_generator_type::result_type>(std::clock()));
      std::uniform_int_distribution<> dis(1, static_cast<int>(digits2 - 1U));

      bool result_is_ok = true;

      my_concurrency::parallel_for
      (
        static_cast<std::size_t>(0U),
        size(),
        [&result_is_ok, this, &dis, &test_lock](std::size_t i)
        {
          while(test_lock.test_and_set()) { ; }
          const auto right_shift_amount = static_cast<std::size_t>(dis(my_gen()));
          test_lock.clear();

          const boost_uint_type c_boost = a_boost[i] / (std::max)(boost_uint_type(1U), boost_uint_type(b_boost[i] >> right_shift_amount));
          const local_uint_type c_local = a_local[i] / (std::max)(local_uint_type(1U),                (b_local[i] >> right_shift_amount));

          const std::string str_boost = hexlexical_cast(c_boost);
          const std::string str_local = hexlexical_cast(c_local);

          while(test_lock.test_and_set()) { ; }
          result_is_ok = ((str_boost == str_local) && result_is_ok);
          test_lock.clear();
        }
      );

      return result_is_ok;
    }

    WIDE_INTEGER_NODISCARD auto test_binary_mod() const -> bool
    {
      std::atomic_flag test_lock = ATOMIC_FLAG_INIT;

      my_gen().seed(static_cast<typename random_generator_type::result_type>(std::clock()));
      std::uniform_int_distribution<> dis(1, static_cast<int>(digits2 - 1U));

      bool result_is_ok = true;

      my_concurrency::parallel_for
      (
        static_cast<std::size_t>(0U),
        size(),
        [&result_is_ok, this, &dis, &test_lock](std::size_t i)
        {
          while(test_lock.test_and_set()) { ; }
          const auto right_shift_amount = static_cast<std::size_t>(dis(my_gen()));
          test_lock.clear();

          const boost_uint_type c_boost = a_boost[i] % (std::max)(boost_uint_type(1U), boost_uint_type(b_boost[i] >> right_shift_amount));
          const local_uint_type c_local = a_local[i] % (std::max)(local_uint_type(1U), (b_local[i] >> right_shift_amount));

          const std::string str_boost = hexlexical_cast(c_boost);
          const std::string str_local = hexlexical_cast(c_local);

          while(test_lock.test_and_set()) { ; }
          result_is_ok = ((str_boost == str_local) && result_is_ok);
          test_lock.clear();
        }
      );

      return result_is_ok;
    }

    WIDE_INTEGER_NODISCARD auto test_binary_sqrt() const -> bool
    {
      bool result_is_ok = true;

      std::atomic_flag test_lock = ATOMIC_FLAG_INIT;

      my_concurrency::parallel_for
      (
        static_cast<std::size_t>(0U),
        size(),
        [&test_lock, &result_is_ok, this](std::size_t i)
        {
          const boost_uint_type c_boost = sqrt(a_boost[i]);
          const local_uint_type c_local = sqrt(a_local[i]);

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
    std::vector<local_uint_type> a_local { }; // NOLINT(readability-identifier-naming)
    std::vector<local_uint_type> b_local { }; // NOLINT(readability-identifier-naming)

    std::vector<boost_uint_type> a_boost { }; // NOLINT(readability-identifier-naming)
    std::vector<boost_uint_type> b_boost { }; // NOLINT(readability-identifier-naming)
  };

#endif // TEST_UINTWIDE_T_N_BINARY_OPS_TEMPLATE_2019_12_19_H
