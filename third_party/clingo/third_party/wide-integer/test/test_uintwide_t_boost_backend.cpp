///////////////////////////////////////////////////////////////////////////////
//  Copyright Christopher Kormanyos 2019 - 2022.
//  Distributed under the Boost Software License,
//  Version 1.0. (See accompanying file LICENSE_1_0.txt
//  or copy at http://www.boost.org/LICENSE_1_0.txt)
//

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

#include <boost/multiprecision/cpp_int.hpp>
#include <boost/multiprecision/uintwide_t_backend.hpp>

#include <test/test_uintwide_t.h>

using local_uint_type =
  #if defined(WIDE_INTEGER_NAMESPACE)
  boost::multiprecision::number<boost::multiprecision::uintwide_t_backend<static_cast<WIDE_INTEGER_NAMESPACE::math::wide_integer::size_t>(UINT32_C(1024))>,
                                boost::multiprecision::et_off>;
  #else
  boost::multiprecision::number<boost::multiprecision::uintwide_t_backend<static_cast<::math::wide_integer::size_t>(UINT32_C(1024))>,
                                boost::multiprecision::et_off>;
  #endif

using boost_uint_backend_type =
  boost::multiprecision::cpp_int_backend<static_cast<unsigned>(UINT32_C(1024)),
                                         static_cast<unsigned>(UINT32_C(1024)),
                                         boost::multiprecision::unsigned_magnitude>;

using boost_uint_type = boost::multiprecision::number<boost_uint_backend_type,
                                                      boost::multiprecision::et_off>;

#if defined(WIDE_INTEGER_NAMESPACE)
auto WIDE_INTEGER_NAMESPACE::math::wide_integer::test_uintwide_t_boost_backend() -> bool
#else
auto ::math::wide_integer::test_uintwide_t_boost_backend() -> bool
#endif
{
  bool result_is_ok = true;

  // Test a non-trivial calculation. A naive algorithm for calculating
  // a factorial (in this case 100!) has been selected.
  {
    local_uint_type u = 1U;

    for(auto i = static_cast<std::size_t>(UINT32_C(2)); i <= static_cast<std::size_t>(UINT32_C(100)); ++i)
    {
      u *= i;
    }

    const local_uint_type local_control("93326215443944152681699238856266700490715968264381621468592963895217599993229915608941463976156518286253697920827223758251185210916864000000000000000000000000");
    const boost_uint_type boost_control("93326215443944152681699238856266700490715968264381621468592963895217599993229915608941463976156518286253697920827223758251185210916864000000000000000000000000");

    const bool local_control_is_ok = (u == local_control);

    {
      std::stringstream strm_lhs;
      strm_lhs << u;

      std::stringstream strm_rhs;
      strm_rhs << boost_control;

      const bool boost_control_is_ok = (strm_lhs.str() == strm_rhs.str());

      result_is_ok = ((local_control_is_ok && boost_control_is_ok) && result_is_ok);
    }

    // Test divide-by-limb.
    u /= static_cast<std::uint8_t>(UINT8_C(10));

    result_is_ok = ((u == local_uint_type("9332621544394415268169923885626670049071596826438162146859296389521759999322991560894146397615651828625369792082722375825118521091686400000000000000000000000")) && result_is_ok);

    // Test full multiplication.
    u *= u;

    result_is_ok = ((u == local_uint_type("87097824890894800794165901619444858655697206439408401342159325362433799963465833258779670963327549206446903807622196074763642894114359201905739606775078813946074899053317297580134329929871847646073758894343134833829668015151562808541626917661957374931734536035195944960000000000000000000000000000000000000000000000")) && result_is_ok);
  }

  // Test a very simple constexpr example.
  {
    WIDE_INTEGER_CONSTEXPR local_uint_type cu("123");

    WIDE_INTEGER_CONSTEXPR bool result_cu_is_ok = (cu == 123U); // NOLINT(cppcoreguidelines-avoid-magic-numbers,readability-magic-numbers)

    result_is_ok = (result_cu_is_ok && result_is_ok);

    #if defined(WIDE_INTEGER_CONSTEXPR_IS_COMPILE_TIME_CONST) && (WIDE_INTEGER_CONSTEXPR_IS_COMPILE_TIME_CONST != 0)
    static_assert(result_cu_is_ok, "Error: test_uintwide_t_boost_backend not OK!");
    #endif
  }

  return result_is_ok;
}

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

#if (((BOOST_VERSION == 108000) || (BOOST_VERSION == 108100)) && defined(BOOST_NO_EXCEPTIONS))
#if defined(__clang__)
#pragma GCC diagnostic pop
#endif
#if defined(_MSC_VER)
#pragma warning(pop)
#endif
#endif
