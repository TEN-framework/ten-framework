Wide-integer
==================

<p align="center">
    <a href="https://github.com/ckormanyos/wide-integer/actions">
        <img src="https://github.com/ckormanyos/wide-integer/actions/workflows/wide_integer.yml/badge.svg" alt="Build Status"></a>
    <a href="https://github.com/ckormanyos/wide-integer/issues?q=is%3Aissue+is%3Aopen+sort%3Aupdated-desc">
        <img src="https://custom-icon-badges.herokuapp.com/github/issues-raw/ckormanyos/wide-integer?logo=github" alt="Issues" /></a>
    <a href="https://github.com/ckormanyos/wide-integer/actions?query=workflow%3ACodeQL">
        <img src="https://github.com/ckormanyos/wide-integer/actions/workflows/CodeQL.yml/badge.svg" alt="CodeQL"></a>
    <a href="https://scan.coverity.com/projects/ckormanyos-wide-integer">
        <img src="https://scan.coverity.com/projects/24742/badge.svg" alt="Coverity Scan"></a>
    <a href="https://sonarcloud.io/summary/new_code?id=ckormanyos_wide-integer">
        <img src="https://sonarcloud.io/api/project_badges/measure?project=ckormanyos_wide-integer&metric=alert_status" alt="Quality Gate Status"></a>
    <a href="https://www.codacy.com/gh/ckormanyos/wide-integer/dashboard?utm_source=github.com&amp;utm_medium=referral&amp;utm_content=ckormanyos/wide-integer&amp;utm_campaign=Badge_Grade">
        <img src="https://app.codacy.com/project/badge/Grade/40d129959071441b9780e68998514226"/></a>
    <a href="https://codecov.io/gh/ckormanyos/wide-integer">
        <img src="https://codecov.io/gh/ckormanyos/wide-integer/branch/master/graph/badge.svg?token=kiBP4MAjdV" alt="code coverage"></a>
    <a href="https://github.com/ckormanyos/wide-integer/blob/master/LICENSE_1_0.txt">
        <img src="https://img.shields.io/badge/license-BSL%201.0-blue.svg" alt="Boost Software License 1.0"></a>
    <a href="https://img.shields.io/github/commit-activity/y/ckormanyos/wide-integer">
        <img src="https://img.shields.io/github/commit-activity/y/ckormanyos/wide-integer" alt="GitHub commit activity" /></a>
    <a href="https://github.com/ckormanyos/wide-integer">
        <img src="https://img.shields.io/github/languages/code-size/ckormanyos/wide-integer" alt="GitHub code size in bytes" /></a>
    <a href="https://godbolt.org/z/b6qoaMf4e" alt="godbolt">
        <img src="https://img.shields.io/badge/try%20it%20on-godbolt-green" /></a>
</p>

Wide-integer implements a generic C++ template for extended width
unsigned and signed integral types.

This C++ template header-only library implements drop-in big integer types
such as `uint128_t`, `uint256_t`, `uint384_t`, `uint512_t`, `uint1024_t`, `uint1536_t`, etc.
These can be used essentially like regular built-in integers.
Corresponding _signed_ integer types such as `int128_t`, `int256_t`, and the like
can also be used.

The big integer class is called `math::wide_integer::uintwide_t`
(i.e., `uintwide_t` residing in the `namespace` `math::wide_integer`),
as shown in greater detail below.

Wide-integer supports both unsigned as well as
signed integral types having width of
$1 {\ldots} 63 {\times} 2^N$
while being $16$, $24$, $32$ or larger.
In addition, small integer types such as software-synthesized versions of
`uint24_t`, `uint48_t`, `uint64_t`, `uint96_t`, `uint128_t`, etc.
(or signed counterparts of these) can also be created with wide-integer.

We also emphasize here that less-common types (i.e., those less common than, say,
`uint128_t`, `uint256_t`, `uint512_t`, etc.) can also be synthesized.
Types such as `uint80_t` made from five 16-bit limbs,
or `uint96_t` composed of three 32-bit limbs, or
other similar types can be readily synthesized with wide-integer.

Wide-integer also features basic realizations of several
elementary and number theoretical functions such as root finding,
random distribution, Miller-Rabin primality testing,
greatest common denominator (GCD), least common multiplier (LCM),
integer division (i.e., `divmod()`) and more.

Inclusion of a single C++14 header file
is all that is needed for using wide-integer,
as shown in the [examples](./examples).

## Implementation goals

  - Signed and unsigned versions of `uintwide_t` should behave as closely as possible to the behaviors of signed and unsigned versions of built-in `int`.
  - Relatively wide precision range from $24$, $32$, $64$ bits up to tens of thousands of bits.
  - Moderately good efficiency over the entire wide precision range.
  - Clean header-only C++14 design.
  - Seamless portability to any modern C++14, 17, 20, 23 compiler and beyond.
  - Scalability with small memory footprint and efficiency suitable for both PC/workstation systems as well as _bare-metal_ embedded systems.
  - C++20 `constexpr`-_ness_ for construction, cast to built-in types, binary arithmetic, comparison operations, some elementary functions and more.

## Quick start

When working in your own project with wide-integer,
using the [`uintwide_t.h` header](./math/wide_integer/uintwide_t.h)
is straightforward. Identify the header within
its directory. Include this header path to the compiler's set
of include paths or in your project.
Then simply `#include <uintwide_t.h>` the normal C++ way.

Easy application follows via traditional C-style typedef or alias
such as `uint512_t`. An instance of the defined type can be used very much
like a built-in integral type.

In the following code, for example,
the static `uint512_t` variable `x` is initialized with unsigned, integral value `3U`.
The `main` subroutine subsequently computes $3^{301}$ with the specialized
wide-integer, namespace-specific function `pow`, which is found via ADL.

The approximate result is

$$3^{301}~{\approx}~4.10674{\times}~10^{143}\text{.}$$

See also the following informative links to Wolfram Alpha(R).

  - Query the approximate value of $3^{301}$ with [`N[3^301]`](https://www.wolframalpha.com/input?i=N%5B3%5E301%5D)
  - Verify the exact value of $3^{301}$ with [`3^301`](https://www.wolframalpha.com/input?i=3%5E301)

This example, compiled with successful output result,
is shown in its entirety in the following
[short link](https://godbolt.org/z/eWKzqx77G) to [godbolt](https://godbolt.org).

In particular,

```cpp
#include <iostream>

#include <math/wide_integer/uintwide_t.h>

using uint512_t = ::math::wide_integer::uintwide_t<512U, std::uint32_t>;

static const uint512_t x = 3U;

auto main() -> int
{
  // Compute x^301, i.e., 3^301.
  const auto p3 = pow(x, 301);

  // 410674437175765127973978082146264947899391086876012309414440570235106991532497229781400618467066824164751453321793982128440538198297087323698003
  std::cout << p3 << std::endl;
}
```

The code sequence above defines the local data type `uint512_t` with
an alias. The first template parameter `512U` sets the binary width
(or bit count) while the second optional template parameter `std::uint32_t`
sets the internal _limb_ _type_. The limb type must be unsigned and one of
`std::uint8_t`, `std::uint16_t`, `std::uint32_t` or on some systems
`std::uint64_t`. If the second template parameter `LimbType` is left blank,
the default limb type is thirty-two bits in width and unsigned.

The complete template signature of the `uintwide_t` class is shown below.

```cpp
namespace math::wide_integer {

namespace detail { using size_t = std::uint32_t; }

using detail::size_t;

// Forward declaration of the uintwide_t template class.
template<const size_t Width2,
         typename LimbType      = std::uint32_t,
         typename AllocatorType = void,
         const bool IsSigned    = false>
class uintwide_t;

} // namespace math::wide_integer
```

`uintwide_t` also has a third optional template paramter that
is used to set the _allocator_ _type_ employed for internal storage of the
big integer's data. The default allocator type is `void`
and `uintwide_t` uses stack allocation with an `std::array`-like internal representation.
Setting the allocator type to an actual allocator such as,
for instance, `std::allocator<limb_type>` activates allocator-based
internal storage for `uintwide_t`.
Using allocator-based storage reduces stack consumption and
can be especially beneficial for higher digit counts.
For low digit counts, the allocator type can
simply be left blank (thus defaulting to `void`)
or explicitly be set to `void` and stack allocation
will be used in either case.

If an allocator is supplied with any granularity other than `limb_type`
(in other words `LimbType`) such as `std::allocator<void>`, `custom_allocator_type<char>`, etc.,
then the `uintwide_t` class will internally _rebind_ the allocator
to the granularity and `unsigned`-ness of `limb_type` using `rebind_alloc`
from `std::allocator_traits`.

The fourth template parameter `IsSigned` can be set to `true`
to activate a signed integer type. If left blank,
the default value of `IsSigned` is  `false`
and the integer type will be unsigned.

## Examples

Various interesting and algorithmically challenging
[examples](./examples) have been implemented.
It is hoped that the examples provide inspiration and guidance
on how to use wide-integer.

  - ![`example000_numeric_limits.cpp`](./examples/example000_numeric_limits.cpp) verifies parts of the specializations of `std::numeric_limits` for (unsigned) `uint256_t`and (signed) `int256_t`.
  - ![`example000a_builtin_convert.cpp`](./examples/example000a_builtin_convert.cpp) exercises some conversions to/from built-in types/`uintwide_t`.
  - ![`example001_mul_div.cpp`](./examples/example001_mul_div.cpp) performs multiplication and division.
  - ![`example001a_div_mod.cpp`](./examples/example001a_div_mod.cpp) exercises division and modulus calculations.
  - ![`example002_shl_shr.cpp`](./examples/example002_shl_shr.cpp) does a few left and right shift operations.
  - ![`example003_sqrt.cpp`](./examples/example003_sqrt.cpp) computes a square root.
  - ![`example003a_cbrt`](./examples/example003a_cbrt.cpp) computes a cube root.
  - ![`example004_rootk_pow.cpp`](./examples/example004_rootk_pow.cpp) computes an integral seventh root and its corresponding power. A negative-valued cube root is also tested.
  - ![`example005_powm.cpp`](./examples/example005_powm.cpp) tests the power-modulus function `powm`.
  - ![`example005a_pow_factors_of_p99.cpp`](./examples/example005a_pow_factors_of_p99.cpp) verifies a beautiful, known prime factorization result from a classic tabulated value.
  - ![`example006_gcd.cpp`](./examples/example006_gcd.cpp) tests several computations of greatest common divisor using the `gcd` function.
  - ![`example007_random_generator.cpp`](./examples/example007_random_generator.cpp) computes some large pseudo-random integers.
  - ![`example008_miller_rabin_prime.cpp`](./examples/example008_miller_rabin_prime.cpp) implements primality testing via Miller-Rabin.
  - ![`example008a_miller_rabin_prime.cpp`](./examples/example008a_miller_rabin_prime.cpp) verifies Boost's interpretation of Miller-Rabin primality testing using `uintwide_t`-based types.
  - ![`example009_timed_mul.cpp`](./examples/example009_timed_mul.cpp) measures multiplication timings.
  - ![`example009a_timed_mul_4_by_4.cpp`](./examples/example009a_timed_mul_4_by_4.cpp) also measures multiplication timings for the special case of wide integers having four limbs.
  - ![`example009b_timed_mul_8_by_8.cpp`](./examples/example009b_timed_mul_8_by_8.cpp) measures, yet again, multiplication timings for the special case of wide integers having eight limbs.
  - ![`example010_uint48_t.cpp`](./examples/example010_uint48_t.cpp) verifies 48-bit integer caluclations.
  - ![`example011_uint24_t.cpp`](./examples/example011_uint24_t.cpp) performs calculations with 24-bits, which is definitely on the small side of the range of wide-integer.
  - ![`example012_rsa_crypto.cpp`](./examples/example012_rsa_crypto.cpp) performs cryptographic calculations with 2048-bits, exploring a standardized test case.
  - ![`example013_ecdsa_sign_verify.cpp`](./examples/example013_ecdsa_sign_verify.cpp) provides an intuitive view on elliptic-curve algebra, depicting a well-known cryptographic key-gen/sign/verify method.
  - ![`example014_pi_spigot_wide.cpp`](./examples/example014_pi_spigot_wide.cpp) calculates $10,001$ decimal digits of the mathematical constant $\pi$ using a `uintwide_t`-based template spigot algorithm.

## Building

### Build Status

[![Build Status](https://github.com/ckormanyos/wide-integer/actions/workflows/wide_integer.yml/badge.svg)](https://github.com/ckormanyos/wide-integer/actions)

The recent status of building and executing the tests and examples
in Continuous Integration (CI) is always shown in the Build Status banner.
Additional banners from other syntax checks and builds may also be visible.

It is also possible, if desired, to build and execute
the tests and examples using various different OS/compiler
combinations.

### Build with Microsoft Visual Studio

Building and running the tests and examples can be accomplished
using the Microsoft VisualStudio solution workspace provided
in `wide_integer.sln`, `wide_integer_vs2022.sln`, etc.
The MSVC solution file(s) are located in the project's root directory.

### Build with CMake

You can also build and run tests and examples from an empty directory
using CMake. Follow the CMake pattern:

```sh
cmake /path/to/wide-integer
cmake --build .
ctest --verbose
```

### Build on the *nix command line

Alternatively building the tests and examples with native GCC (i.e., on *nix)
can be executed with a simple, but rather lengthy, command line
entered manually from the command shell.
Consider, for instance, building in Linux with GCC in the presence of `unsigned` `__int128`.
Furthermore, the Boost.Multiprecision library is used for some examples and tests.
In this build example, Boost is intended to be located
in the made-up directory `../boost-root`,
which needs to be adapted according to the actual location of Boost.
The command line below illustrates how to build all
of the wide_integer tests and examples directly
from the *nix command line.

```sh
cd wide_integer
g++                                         \
-finline-functions                          \
-finline-limit=32                           \
-march=native                               \
-mtune=native                               \
-O3                                         \
-Wall                                       \
-Wextra                                     \
-Wpedantic                                  \
-Wconversion                                \
-Wsign-conversion                           \
-Wno-maybe-uninitialized                    \
-Wno-cast-function-type                     \
-std=c++14                                  \
-DWIDE_INTEGER_HAS_LIMB_TYPE_UINT64         \
-I.                                         \
-I../boost-root                             \
-pthread                                    \
-lpthread                                   \
test/test.cpp                               \
test/test_uintwide_t_boost_backend.cpp      \
test/test_uintwide_t_edge_cases.cpp         \
test/test_uintwide_t_examples.cpp           \
test/test_uintwide_t_float_convert.cpp      \
test/test_uintwide_t_int_convert.cpp        \
test/test_uintwide_t_n_base.cpp             \
test/test_uintwide_t_n_binary_ops_base.cpp  \
test/test_uintwide_t_spot_values.cpp        \
examples/example000_numeric_limits.cpp      \
examples/example000a_builtin_convert.cpp    \
examples/example001_mul_div.cpp             \
examples/example001a_div_mod.cpp            \
examples/example002_shl_shr.cpp             \
examples/example003_sqrt.cpp                \
examples/example003a_cbrt.cpp               \
examples/example004_rootk_pow.cpp           \
examples/example005_powm.cpp                \
examples/example005a_pow_factors_of_p99     \
examples/example006_gcd.cpp                 \
examples/example007_random_generator.cpp    \
examples/example008_miller_rabin_prime.cpp  \
examples/example008a_miller_rabin_prime.cpp \
examples/example009_timed_mul.cpp           \
examples/example009a_timed_mul_4_by_4.cpp   \
examples/example009b_timed_mul_8_by_8.cpp   \
examples/example010_uint48_t.cpp            \
examples/example011_uint24_t.cpp            \
examples/example012_rsa_crypto.cpp          \
examples/example013_ecdsa_sign_verify.cpp   \
examples/example014_pi_spigot_wide.cpp      \
-o wide_integer.exe
```

## Testing, CI and Quality Checks

### Testing

Testing is definitely a big issue. A growing, supported
test suite improves confidence in the library.
It provides for tested, efficient functionality on the PC and workstation.
The GitHub code is, as mentioned above, delivered with an affiliated MSVC
project or a variety of other build/make options that use easy-to-understand
subroutines called from `main()`. These exercise the various
examples and the full suite of test cases.

If an issue is reported, reproduced and verified, an attempt
is made to correct it without breaking any other
code. Upon successful correction, specific test cases
exercising the reported issue are usually added as part
of the issue resolution process.

### CI and Quality checks

CI runs on both push-to-branch as well as pull request using GitHub Actions.
Various compilers, operating systems, and C++ standards
ranging from C++14, 17, 20, 23 are included in CI.

In CI, we use both elevated GCC/clang compiler warnings
as well as MSVC level 4 warnings active on the correspondoing platforms.
For additional in-depth syntax checking, clang-tidy is used both in CI
as well as in offline checks to improve static code quality.

Both GCC's run-time
[sanitizers](https://gcc.gnu.org/onlinedocs/gcc/Instrumentation-Options.html)
as well as [_valgrind_](https://valgrind.org) (see also [1] and [2] in the References below)
are used in CI in order to help assure dynamic quality.

Additional quality checks are performed on pull-request
and merge to master using modern third party open-source services.
These include
[CodeQL](https://github.com/ckormanyos/wide-integer/actions?query=workflow%3ACodeQL),
[Synopsis Coverity](https://scan.coverity.com/projects/ckormanyos-wide-integer),
and [CodeSonar](https://sonarcloud.io/summary/new_code?id=ckormanyos_wide-integer).
At the moment, the Coverity check is run with manual report submission.
Automation of this is, however, planned.

Code coverage uses GCC/gcov/lcov and has a
quality-gate with comparison/baseline-check provided by
[Codecov](https://app.codecov.io/gh/ckormanyos/wide-integer).

Quality badges are displayed at the top of this repository's
readme page.

## Detailed examples

We will now present various straightforward detailed examples.

The code below performs some elementary algebraic calculations
with a simple mixture of 256-bit and 512-bit unsigned integral types.

This example, compiled with successful output result,
is shown in its entirety in the following
[short link](https://godbolt.org/z/fq9E5aW9v) to [godbolt](https://godbolt.org).

```cpp
#include <iomanip>
#include <iostream>

#include <math/wide_integer/uintwide_t.h>

auto main() -> int
{
  using ::math::wide_integer::uint256_t;
  using ::math::wide_integer::uint512_t;

  // Construction from string. Additional constructors
  // are available from other built-in types.

  const uint256_t a("0xF4DF741DE58BCB2F37F18372026EF9CBCFC456CB80AF54D53BDEED78410065DE");
  const uint256_t b("0x166D63E0202B3D90ECCEAA046341AB504658F55B974A7FD63733ECF89DD0DF75");

  // Elementary arithmetic operations.
  const auto c = uint512_t(a) * uint512_t(b);
  const auto d = (a / b);

  // Logical comparison.
  const auto result_is_ok = (   (c == "0x1573D6A7CEA734D99865C4F428184983CDB018B80E9CC44B83C773FBE11993E7E491A360C57EB4306C61F9A04F7F7D99BE3676AAD2D71C5592D5AE70F84AF076")
                             && (d == "0xA"));

  // Print the hexadecimal representation string output.
  const auto flg = std::cout.flags();

  std::cout << "0x" << std::hex << std::uppercase << c << std::endl;
  std::cout << "0x" << std::hex << std::uppercase << d << std::endl;

  // Visualize if the result is OK.

  std::cout << "result_is_ok: " << std::boolalpha << result_is_ok << std::endl;

  std::cout.flags(flg);
}
```

Wide-integer also supports a small selection of number-theoretical
functions such as least and most significant bit,
square root, $k^{th}$ root,
power, power-modulus, greatest common denominator
and random number generation.
These functions are found via ADL.

The example below calculates an integer square root.

This example, compiled with successful output result,
is shown in its entirety in the following
[short link](https://godbolt.org/z/Gf4sfeP48) to [godbolt](https://godbolt.org).

```cpp
#include <iomanip>
#include <iostream>

#include <math/wide_integer/uintwide_t.h>

auto main() -> int
{
  using uint256_t = ::math::wide_integer::uint256_t;

  const uint256_t a("0xF4DF741DE58BCB2F37F18372026EF9CBCFC456CB80AF54D53BDEED78410065DE");

  const uint256_t s = sqrt(a);

  const auto result_is_ok = (s == "0xFA5FE7853F1D4AD92BDF244179CA178B");

  std::cout << "result_is_ok: " << std::boolalpha << result_is_ok << std::endl;
}
```

The following sample performs add, subtract, multiply and divide of `uint48_t`.
See this example also in the following
[short link](https://godbolt.org/z/vMqWav5P6) to [godbolt](https://godbolt.org).

```cpp
#include <iomanip>
#include <iostream>
#include <random>

#include <math/wide_integer/uintwide_t.h>

auto main() -> int
{
  using uint48_t = ::math::wide_integer::uintwide_t<static_cast<math::wide_integer::size_t>(UINT32_C(48)), std::uint8_t>;

  using distribution_type  = ::math::wide_integer::uniform_int_distribution<static_cast<math::wide_integer::size_t>(UINT32_C(48)), typename uint48_t::limb_type>;

  using random_engine_type = std::linear_congruential_engine<std::uint32_t, UINT32_C(48271), UINT32_C(0), UINT32_C(2147483647)>;

  random_engine_type generator(static_cast<std::uint32_t>(UINT32_C(0xF00DCAFE))); // NOLINT(cert-msc32-c,cert-msc51-cpp,cppcoreguidelines-avoid-magic-numbers,readability-magic-numbers)

  distribution_type distribution;

  const auto a64 = static_cast<std::uint64_t>(distribution(generator));
  const auto b64 = static_cast<std::uint64_t>(distribution(generator));

  const uint48_t a(a64);
  const uint48_t b(b64);

  const uint48_t c_add = (a + b);
  const uint48_t c_sub = (a - b);
  const uint48_t c_mul = (a * b);
  const uint48_t c_div = (a / b);

  const auto result_is_ok = (   (   (c_add == static_cast<std::uint64_t>((a64 + b64) & static_cast<std::uint64_t>(UINT64_C(0x0000FFFFFFFFFFFF))))
                                 && (c_sub == static_cast<std::uint64_t>((a64 - b64) & static_cast<std::uint64_t>(UINT64_C(0x0000FFFFFFFFFFFF))))
                                 && (c_mul == static_cast<std::uint64_t>((a64 * b64) & static_cast<std::uint64_t>(UINT64_C(0x0000FFFFFFFFFFFF))))
                                 && (c_div == static_cast<std::uint64_t>((a64 / b64) & static_cast<std::uint64_t>(UINT64_C(0x0000FFFFFFFFFFFF)))))
                             &&
                                (   (static_cast<std::uint64_t>(c_add) == static_cast<std::uint64_t>((a64 + b64) & static_cast<std::uint64_t>(UINT64_C(0x0000FFFFFFFFFFFF))))
                                 && (static_cast<std::uint64_t>(c_sub) == static_cast<std::uint64_t>((a64 - b64) & static_cast<std::uint64_t>(UINT64_C(0x0000FFFFFFFFFFFF))))
                                 && (static_cast<std::uint64_t>(c_mul) == static_cast<std::uint64_t>((a64 * b64) & static_cast<std::uint64_t>(UINT64_C(0x0000FFFFFFFFFFFF))))
                                 && (static_cast<std::uint64_t>(c_div) == static_cast<std::uint64_t>((a64 / b64) & static_cast<std::uint64_t>(UINT64_C(0x0000FFFFFFFFFFFF))))));

  std::cout << "result_is_ok: " << std::boolalpha << result_is_ok << std::endl;
}
```

The next example computes the real-valued cube root of $10^{3,333}$.
The real-valued cube root of this very large unsigned integer is $10^{1,111}$.
We will use the (somewhat uncommon) integral data type `uint11264_t`.
Since `uint11264_t` has approximately $3,390$ decimal digits of precision,
it is large enough to hold the value of $10^{3,333}$
prior to (and following) the cube root operation.

See this example fully worked out at the following
[short link](https://godbolt.org/z/aKKesPK36) to [godbolt](https://godbolt.org)

```cpp
#include <iomanip>
#include <iostream>

#include <math/wide_integer/uintwide_t.h>

auto main() -> int
{
  using uint11264_t = ::math::wide_integer::uintwide_t<11264U, std::uint32_t>;

  // Create the string '1' + 3,333 times '0', which is
  // equivalent to the decimal integral value 10^3333.

  const std::string str_a = "1" + std::string(3333U, '0');

  const uint11264_t a = str_a.data();

  const uint11264_t s = cbrt(a);

  // Create the string '1' + 1,111 times '0', which is
  // equivalent to the decimal integral value 10^1111.
  // (This is the cube root of 10^3333.)

  const std::string str_control = "1" + std::string(1111U, '0');

  const auto result_is_ok = (s == uint11264_t(str_control.data()));

  std::cout << s << std::endl;

  std::cout << "result_is_ok: " << std::boolalpha << result_is_ok << std::endl;
}
```

## Additional details

Wide-Integer has been tested with numerous compilers, for target systems ranging from eight to sixty-four bits.
The library is specifically designed for efficiency with small to medium bit counts.
Supported bit counts include integers
$1 {\ldots} 63 {\times} 2^N$
while being $16$, $24$, $32$ or larger such as
$256$, $384$, $512$, $768$, $1024$,
or other less common bit counts such as $11,264$, etc.

Small, medium and large bit counts are supported.
Common applications might use the range of `uint128_t`, `uint256_t` or `uint512_t`.
It is also possible to make
software-synthesized (not very efficient) versions of `uint24_t`, `uint32_t` or `uint48_t`,
which might useful for hardware prototyping or other simulation and verification needs.
On the high-digit end, Karatsuba multiplication extends the high performance range
to many thousands of bits. Fast long division, however, relies on a classical algorithm
and sub-quadratic high-precision division is not yet implemented.

Portability of the code is another key point of focus. Special care
has been taken to test in certain high-performance embedded real-time
programming environments.

### Configuration macros (compile-time)

Various configuration features can optionally be
enabled or disabled at compile time with the compiler switches:

```cpp
#define WIDE_INTEGER_DISABLE_IOSTREAM
#define WIDE_INTEGER_DISABLE_TO_STRING
#define WIDE_INTEGER_DISABLE_FLOAT_INTEROP
#define WIDE_INTEGER_DISABLE_IMPLEMENT_UTIL_DYNAMIC_ARRAY
#define WIDE_INTEGER_HAS_LIMB_TYPE_UINT64
#define WIDE_INTEGER_HAS_MUL_8_BY_8_UNROLL
#define WIDE_INTEGER_DISABLE_TRIVIAL_COPY_AND_STD_LAYOUT_CHECKS
#define WIDE_INTEGER_NAMESPACE
#define WIDE_INTEGER_DISABLE_WIDE_INTEGER_CONSTEXPR
#define WIDE_INTEGER_TEST_REPRESENTATION_AS_STD_LIST
#define WIDE_INTEGER_DISABLE_PRIVATE_CLASS_DATA_MEMBERS
#define WIDE_INTEGER_HAS_CLZ_LIMB_OPTIMIZATIONS
```

When working with even the most tiny microcontroller systems,
I/O streaming can optionally be disabled with the compiler switch:

```cpp
#define WIDE_INTEGER_DISABLE_IOSTREAM
```

The default setting is `WIDE_INTEGER_DISABLE_IOSTREAM` not set
and I/O streaming operations are enabled.

Conversion to `std::string` is supported with the specialized wide-integer, namespace-specific function
`to_string`. This analagous to the standard library's `std::to_string` function,
but implemented specifically for instances of `uintwide_t`.
Wide-integer's local, namespace-specific `to_string`
function (and the inclusion of the necessary `<string>` header)
are both deactivated with:

```cpp
#define WIDE_INTEGER_DISABLE_TO_STRING
```

Interoperability with built-in floating-point types
such as construct-from, cast-to, binary arithmetic with
built-in floating-point types can be
optionally disabled by defining:

```cpp
#define WIDE_INTEGER_DISABLE_FLOAT_INTEROP
```

The default setting is `WIDE_INTEGER_DISABLE_FLOAT_INTEROP` not set
and all available functions implementing construction-from,
cast-to, binary arithmetic with built-in floating-point types
are enabled.

```cpp
#define WIDE_INTEGER_DISABLE_IMPLEMENT_UTIL_DYNAMIC_ARRAY
```

This macro disables `uintwide_t.h`'s own local implementation
of the `util::dynamic_array` template class.
The logic of this macro is negated. Its default setting
(of being disabled itself) ensures that standalone `uintwide_t.h`
is free from any additional header dependencies.

The template utility class `util::dynamic_array` is used
as a storage container for certain instantiations of `uintwide_t`.
This macro is disabled by default and `uintwide_t.h`
does actually provide its own local implementation
of the `util::dynamic_array` template class.
Otherwise, the header file `<util/utility/util_dynamic_array.h>`
must be found in the include path.

When working on high-performance systems having `unsigned __int128`
(an extended-width, yet non-standard data type),
a 64-bit limb of type `uint64_t` can be used.
Enable the 64-bit limb type on such systems
with the compiler switch:

```cpp
#define WIDE_INTEGER_HAS_LIMB_TYPE_UINT64
```

or (when using GCC, clang or similar) on the compiler
command line with:

```cpp
-DWIDE_INTEGER_HAS_LIMB_TYPE_UINT64
```

This macro is disabled by default.

The example below, for instance, uses a 64-bit limb type
on GCC or clang.

```cpp
#define WIDE_INTEGER_HAS_LIMB_TYPE_UINT64

#include <math/wide_integer/uintwide_t.h>

using uint_fast256_t = ::math::wide_integer::uintwide_t<256U, std::uint64_t>;

static uint_fast256_t x = 42U;
```

Another potential optimization macro can be activated with:

```cpp
#define WIDE_INTEGER_HAS_MUL_8_BY_8_UNROLL
```

This macro might improve performance on some target/compiler systems
by manually unrolling the multiplication loop(s) for
`uintwide_t` instances having eight limbs. This macro is disabled
by default.

```cpp
#define WIDE_INTEGER_DISABLE_TRIVIAL_COPY_AND_STD_LAYOUT_CHECKS
```

This macro disables compile-time checks for `std::is_trivially_copyable`
and `std::is_standard_layout`. These checks provide assurance
(among other attributes) that `uintwide_t`'s constructors
satisfy rules needed for mixed-language C/C++ usage.
Some older legacy target/compiler systems might have non-standard
or incomplete STL implementations that lack these compile-time
templates. For such compilers, it makes sense to deactivate
these compile-time checks via activation of this macro.
This macro is disabled by default and both the trivially-copyable
as well as the standard-layout compile-time checks are active.

```cpp
#define WIDE_INTEGER_NAMESPACE something_unique
```

This is an advanced macro intended to be used in strict, exacting applications for which
using the unqualified, global namespace `math` (i.e., `namespace` `::math`) is undesired or inacceptable.
We recall that all parts of the wide-integer implementation,
such as the `uintwide_t` class and its associated implementation
details reside within `namespace` `::math::wide_integer`

Defining the macro `WIDE_INTEGER_NAMESPACE` to be something like,
for instance,

```sh
-DWIDE_INTEGER_NAMESPACE=something_unique
```

places all parts of the wide-integer implementation and its details
within the prepended outer namespace `something_unique` -
as in

```cpp
namespace something_unique::math::wide_integer
{
  // ...
}
```

When utilizing the `WIDE_INTEGER_NAMESPACE` option,
the actual name or nesting depth of the desired prepended
outer namespace can be varied if (or as) needed
for the particular project.

By default the macro `WIDE_INTEGER_NAMESPACE` is not defined.
In this default state, `namespace` `::math::wide_integer` is used
and the `uintwide_t` class and its associated implementation
details reside therein.

Domain-specific, non-supported prototyping can be (partially)
enabled by defining the macro

```cpp
#define WIDE_INTEGER_DISABLE_WIDE_INTEGER_CONSTEXPR
```

This advanced macro disables most C++20 `constexpr` features.
It also disables standard layout and trivially constructable
attributes.This macro can be used (if needed)
when progressive prototyping or other non-standard investigations
require disabling most of wide-integer's default-supplied
C++20 `constexpr`-handling.

This might be useful when _manually_ substituting
non-standard, alternate containers instead of using
wide-integer's default-supplied containers.
Note: During verification steps of wide-integer,
for instance, this macro has been used when performing
algorithmic proof-of-concept via use of `std::list`
for storage containters (instead of the default-supplied
dynamic/static array-like containers).

```cpp
#define WIDE_INTEGER_TEST_REPRESENTATION_AS_STD_LIST
```

This macro is used only for testing the correct functionality
of wide-integer when the internal storage representation is
`std::list<limb_type>`. This feature is useful for verifying
that the internal algorithms used in wide-integer remain entirely
iterator-based and do not require any use of random-access
iteration whatsoever.

When defining the macro `WIDE_INTEGER_TEST_REPRESENTATION_AS_STD_LIST`,
the macro `WIDE_INTEGER_DISABLE_WIDE_INTEGER_CONSTEXPR` will also
be defined automatically in order to _disable_
wide-integer's C++20 `constexpr`-ness. This is necessary
because the `std::list` container is incompatible with some or most
of wide-integer's C++20 `constexpr`-ness.

```cpp
#define WIDE_INTEGER_DISABLE_PRIVATE_CLASS_DATA_MEMBERS
```

This optional macro can be used to switch `uintwide_t`'s
data member access from _private_ to _public_. This allows the
`uintwide_t` class to be used as a so-called _structured_ class,
such as is needed for constant-valued template parameters
in the sense of C++20's `constexpr`-ness.
This preprocessor switch was invented based on the discussion in
[issue 335](https://github.com/ckormanyos/wide-integer/issues/335)

Making private data members public is unusual for some designs.
So the preprocessor switch `WIDE_INTEGER_DISABLE_PRIVATE_CLASS_DATA_MEMBERS`
is not defined (i.e., not set) by default. This ensures that
`uintwide_t`'s data members remain private by default.

```cpp
#define WIDE_INTEGER_HAS_CLZ_LIMB_OPTIMIZATIONS
```

This optional macro activates certain optimizations
that count leading zero-limbs prior to classical quadratic
multiplication. This may offer performance advantages on some systems
by avoiding some potentially costly zero-valued limb-multiplication steps.

This preprocessor switch was motivated by the discussion in
[issue 362](https://github.com/ckormanyos/wide-integer/issues/362)

By default, the preprocessor switch `WIDE_INTEGER_HAS_CLZ_LIMB_OPTIMIZATIONS`
is not defined and CLZ-limb optimizations are default-_disabled_.

### C++14, 17, 20 `constexpr` support

When using C++20 `uintwide_t` supports compile-time
`constexpr` construction and evaluation of results
of binary arithmetic, comparison operators
and various elementary functions.
The following code, for instance, shows compile-time instantiations
of `uintwide_t` from character strings with subsequent `constexpr` evaluations
of binary operations multiply, divide, intergal cast and comparison.

See this example fully worked out at the following
[short link](https://godbolt.org/z/vYsfWYhe4) to [godbolt](https://godbolt.org).
The generated assembly includes nothing other than the call to `main()`
and its subsequent `return` of the value zero
(i.e., `main()`'s successful return-value in this example).

```cpp
#include <math/wide_integer/uintwide_t.h>

// Use (at least) a C++20 compiler for this example.

using uint256_t = ::math::wide_integer::uintwide_t<256U>;
using uint512_t = ::math::wide_integer::uintwide_t<512U>;

// Compile-time construction from string.
constexpr auto a = uint256_t("0xF4DF741DE58BCB2F37F18372026EF9CBCFC456CB80AF54D53BDEED78410065DE");
constexpr auto b = uint256_t("0x166D63E0202B3D90ECCEAA046341AB504658F55B974A7FD63733ECF89DD0DF75");

// Compile time binary arithmetic operations.
constexpr auto c = uint512_t(a) * uint512_t(b);
constexpr auto d = uint256_t(a / b);

// Compile-time comparison.
constexpr auto result_is_ok = (   (c == "0x1573D6A7CEA734D99865C4F428184983CDB018B80E9CC44B83C773FBE11993E7E491A360C57EB4306C61F9A04F7F7D99BE3676AAD2D71C5592D5AE70F84AF076")
                               && (static_cast<std::uint_fast8_t>(d) == static_cast<std::uint_fast8_t>(UINT8_C(10))));

// constexpr verification.
static_assert(result_is_ok, "Error: example is not OK!");

auto main() -> int
{
  return (result_is_ok ? 0 : -1);
}
```

The so-called `constexpr`-_ness_ of `uintwide_t` has been checked on GCC 10 and up,
clang 10 and up (with `-std=c++20`) and VC 14.2 (with `/std:c++latest`),
also for various embedded compilers such as `avr-gcc` 10 and up,
`arm-non-eabi-gcc` 10 and up, and more. In addition,
some compilations using compilers having less modern standards
such as C++14, 17, 2a have also been checked
for `constexpr` usage of `uintwide_t`. If you have an older
compiler, you might have to check the compiler's
ability to obtain the entire benefit of `constexpr` with `uintwide_t`.

If full `constexpr` compliance is not available or its
availability is unknown, the preprocessor symbols below can be useful.
These symbols are defined or set directly within the header(s)
of the wide_integer library.

```cpp
WIDE_INTEGER_CONSTEXPR
WIDE_INTEGER_CONSTEXPR_IS_COMPILE_TIME_CONST
```

The preprocessor symbol `WIDE_INTEGER_CONSTEXPR` acts as either
a synonym for `constexpr` or expands to nothing depending on
whether the availability of `constexpr` support has been automatically
detected or not.
The preprocessor symbol `WIDE_INTEGER_CONSTEXPR_IS_COMPILE_TIME_CONST`
has the value of `0` or `1`, where `1` indicates that `uintwide_t`
values qualified with `WIDE_INTEGER_CONSTEXPR` are actually
compile-time constant (i.e., `constexpr`).

Detection of availability of `constexpr` support is implemented
[with preprocessor queries in uintwide_t.h](https://github.com/ckormanyos/wide-integer/blob/4ad2cb5e96acc0b326c8fc2bbb74546dc90053ef/math/wide_integer/uintwide_t.h#L36).
These complicated proprocessor queries are not complete (in the sense of
detecting all world-wide compiler/target systems). If you have
a specific compiler/target system needing `constexpr` detection,
please feel free to contact me directly so that this can be implemented.

### Signed integer support

Signed big integers are also supported in the wide_integer library.
Use the fourth template partameter `IsSigned` to indicate the
signed-_ness_ (or unsigned-_ness_) of `uintwide_t`.
The code below, for instance, uses an aliased version of
signed `int256_t`.

```cpp
#include <math/wide_integer/uintwide_t.h>

using int256_t = ::math::wide_integer::uintwide_t<256U, std::uint32_t, void, true>;

const int256_t n1(-3);
const int256_t n2(-3);

// 9
const int256_t n3 = n1 * n2;
```

### Negative arguments in number theoretical functions

The following design choices have been implemented when handling
negative arguments in number theoretical functions.

  - Right shift by `n` bits via `operator>>(n)` performs a so-called _arithmetic_ right shift (ASHR). For signed integers having negative value, right-shift continually fills the sign bit with 1 while shifting right. The result is similar to signed division and closely mimics common compiler behavior for right-shift of negative-valued built-in signed `int`.
  - `sqrt` of `x` negative returns zero.
  - `cbrt` of `x` nexative integer returns `-cbrt(-x)`.
  - $k^{th}$ root of `x` negative returns zero unless the cube root is being computed, in which case `-cbrt(-x)` is returned.
  - GCD and LCM of `a`, `b` signed convert both arguments to positive and negate the result for `a`, `b` having opposite signs.
  - Miller-Rabin primality testing treats negative inetegers as positive when testing for prime, thus extending the set of primes to negative integers.
  - MSB/LSB (most/least significant bit) do not differentiate between positive or negative argument such that MSB of a negative integer will be the highest bit of the corresponding unsigned type.
  - Printing both positive-valued and negative-valued signed integers in hexadecimal format is supported. When printing negative-valued, signed  `uintwide_t` in hexadecimal format, the sign bit and all other bits are treated as if the integer were unsigned. The negative sign is not explicitly shown when using hexadecimal format, even if the underlying integer is signed and negative-valued. A potential positive sign, however, will be shown for positive-valued signed integers in hexadecimal form in the presence of `std::showpos`.
  - Signed integer division and modulus results obtained from the `divmod()` function follow established number-theoretical rounding conventions, which are the same as those used by the `//`-operator in Python-3 (i.e., the same as Python-3's built-in `divmod()` function). These conventions also match those used by Mathematica(R)'s `QuotientRemainder[]` function.

## Further details

### Notable construction/conversion rules

The following notable construction/conversion rules have been implemented
in the wide-integer project.

  - Constructions-from built-in types are implicit. These are considered widening conversions.
  - Casts to built-in types are explicit and considered narrowing, regardless of the widths of left-and-right hand sides of the conversion.
  - All of both constructions-from as well as casts-to wider/less-wide and signed/unsigned wide-integer types are implicit (even if the conversion at hand is narrowing via having fewer bits). Casts such as `int128_t` to/from `uint160_t` and similar, for instance, are implicit.
  - All wide-integer-types are move constructable.
  - All wide-integer types having the same widths and having the same limb-type (but possibly different sign) are move-assignable and `std::move()`-capable.

### Importing and exporting bits

Support for importing and exporting bits is granted by the subroutines
`import_bits()` and `export_bits()`. Their interfaces, input/output forms
and constraints are intended to be identical with those used in
[Boost's import/export-bits functions](https://www.boost.org/doc/libs/1_80_0/libs/multiprecision/doc/html/boost_multiprecision/tut/import_export.html).

### Alternatives and limitations

Alternative libraries for big integral types include,
among others, most notably
[GMP](https://gmplib.org/)
and
[`Boost.Multiprecision`](https://www.boost.org/doc/libs/1_79_0/libs/multiprecision/doc/html/index.html).

At the moment, the digit range of wide-integer is limited
to the granularity of the full limb type.
This means that less-common bit counts
requiring the use of non-full limbs
are not supported. It is **not** possible with this library,
for instance, to synthesize, let's say, a 61-bit integral type.

This can have performance impact. If you would like
to synthesize an 80-bit integral type, for example, this can
be done, but at the cost of using five 16-bit limbs.
This degrades performance due to the higher limb count.
This phenomenon was discussed in
[issue 234](https://github.com/ckormanyos/wide-integer/issues/234)

## References

A original publications on [_valgrind_](https://valgrind.org)
and its relation to memory errors can be found in [1] and [2].

[1] Nicholas Nethercote and Julian Seward,
_Valgrind:_ _A_ _Framework_ _for_ _Heavyweight_ _Dynamic_ _Binary_ _Instrumentation_,
Proceedings of ACM SIGPLAN 2007 Conference on Programming Language Design and Implementation (PLDI 2007),
San Diego, California, USA, June 2007.

[2] Nicholas Nethercote and Julian Seward,
_How_ _to_ _Shadow_ _Every_ _Byte_ _of_ _Memory_ _Used_ _by_ _a_ _Program_.
Proceedings of the Third International ACM SIGPLAN/SIGOPS Conference on Virtual Execution Environments (VEE 2007),
San Diego, California, USA, June 2007.
