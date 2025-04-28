///////////////////////////////////////////////////////////////////////////////
//  Copyright Christopher Kormanyos 2019 - 2022.
//  Distributed under the Boost Software License,
//  Version 1.0. (See accompanying file LICENSE_1_0.txt
//  or copy at http://www.boost.org/LICENSE_1_0.txt)
//

#ifndef TEST_UINTWIDE_T_N_NUMBER_THEORY_FUNCS_TEMPLATE_2019_12_29_H // NOLINT(llvm-header-guard)
  #define TEST_UINTWIDE_T_N_NUMBER_THEORY_FUNCS_TEMPLATE_2019_12_29_H

  #include <test/test_uintwide_t_n_base.h>

  #if defined(WIDE_INTEGER_NAMESPACE)
  template<const WIDE_INTEGER_NAMESPACE::math::wide_integer::size_t MyWidth2,
           typename MyLimbType = std::uint32_t>
  #else
  template<const ::math::wide_integer::size_t MyWidth2,
           typename MyLimbType = std::uint32_t>
  #endif
  class test_uintwide_t_n_number_theory_funcs_template : public test_uintwide_t_n_base
  {
  public:
    #if defined(WIDE_INTEGER_NAMESPACE)
    static constexpr auto digits2 = static_cast<WIDE_INTEGER_NAMESPACE::math::wide_integer::size_t>(MyWidth2);
    #else
    static constexpr auto digits2 = static_cast<math::wide_integer::size_t>(MyWidth2);
    #endif

    test_uintwide_t_n_number_theory_funcs_template(const std::size_t count)
      : test_uintwide_t_n_base(count) { }

    virtual ~test_uintwide_t_n_number_theory_funcs_template() = default;

    #if defined(WIDE_INTEGER_NAMESPACE)
    virtual auto get_digits2() const -> WIDE_INTEGER_NAMESPACE::math::wide_integer::size_t { return digits2; }
    #else
    virtual auto get_digits2() const -> ::math::wide_integer::size_t { return digits2; }
    #endif

    virtual auto initialize() -> void { }
  };

#endif // TEST_UINTWIDE_T_N_NUMBER_THEORY_FUNCS_TEMPLATE_2019_12_29_H
