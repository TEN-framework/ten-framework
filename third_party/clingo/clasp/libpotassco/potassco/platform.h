//
// Copyright (c) 2016-2017 Benjamin Kaufmann
//
// This file is part of Potassco.
//
// Permission is hereby granted, free of charge, to any person obtaining a copy
// of this software and associated documentation files (the "Software"), to
// deal in the Software without restriction, including without limitation the
// rights to use, copy, modify, merge, publish, distribute, sublicense, and/or
// sell copies of the Software, and to permit persons to whom the Software is
// furnished to do so, subject to the following conditions:
//
// The above copyright notice and this permission notice shall be included in
// all copies or substantial portions of the Software.
//
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
// IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
// FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
// AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
// LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
// FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS
// IN THE SOFTWARE.
//
#ifndef POTASSCO_PLATFORM_H_INCLUDED
#define POTASSCO_PLATFORM_H_INCLUDED
#include <cstddef>
#include <cassert>
#include <cstdlib>
#include <cerrno>
#if !defined(POTASSCO_HAS_STATIC_ASSERT)
#	if (defined(__cplusplus) && __cplusplus >= 201103L) || (defined(_MSC_VER) && _MSC_VER >= 1600) || (defined(static_assert) && !defined(_LIBCPP_VERSION))
#		define POTASSCO_HAS_STATIC_ASSERT 1
#	else
#		define POTASSCO_HAS_STATIC_ASSERT 0
#	endif
#endif

#define POTASSCO_STRING2(x) #x
#define POTASSCO_STRING(x) POTASSCO_STRING2(x)
#define POTASSCO_CONCAT2(X, Y) X ## Y
#define POTASSCO_CONCAT(X, Y) POTASSCO_CONCAT2(X, Y)

#if defined(_MSC_VER)
#define POTASSCO_ATTR_UNUSED
#define POTASSCO_ATTR_NORETURN __declspec(noreturn)
#define POTASSCO_PRAGMA_TODO(X) __pragma(message(__FILE__ "(" POTASSCO_STRING(__LINE__) ") : TODO: " X))
#define POTASSCO_FUNC_NAME __FUNCTION__
#define POTASSCO_WARNING_BEGIN_RELAXED \
	__pragma(warning(push))\
	__pragma(warning (disable : 4200))

#define POTASSCO_WARNING_END_RELAXED \
	__pragma(warning(pop))

#elif defined(__GNUC__) || defined(__clang__)
#if !defined(__STDC_FORMAT_MACROS)
#define __STDC_FORMAT_MACROS
#endif
#if !defined(__STDC_LIMIT_MACROS)
#define __STDC_LIMIT_MACROS
#endif
#define POTASSCO_ATTR_UNUSED __attribute__((unused))
#define POTASSCO_ATTR_NORETURN __attribute__ ((noreturn))
#define POTASSCO_FUNC_NAME __PRETTY_FUNCTION__
#define POTASSCO_APPLY_PRAGMA(x) _Pragma (#x)
#define POTASSCO_PRAGMA_TODO(x) POTASSCO_APPLY_PRAGMA(message ("TODO: " #x))
#	if defined(__clang__)
#		pragma clang diagnostic push
#		pragma clang diagnostic ignored "-Wgnu-zero-variadic-macro-arguments"
#		pragma clang diagnostic ignored "-Wvariadic-macros"
#		define POTASSCO_WARNING_BEGIN_RELAXED \
		_Pragma("clang diagnostic push") \
		_Pragma("clang diagnostic ignored \"-Wzero-length-array\"")
#		define POTASSCO_WARNING_END_RELAXED _Pragma("clang diagnostic pop")
#	else
#		pragma GCC diagnostic push
#		pragma GCC system_header
#		define POTASSCO_WARNING_BEGIN_RELAXED \
		_Pragma("GCC diagnostic push")\
		_Pragma("GCC diagnostic ignored \"-Wpragmas\"")\
		_Pragma("GCC diagnostic ignored \"-Wpedantic\"")\
		_Pragma("GCC diagnostic ignored \"-pedantic\"")
#		define POTASSCO_WARNING_END_RELAXED _Pragma("GCC diagnostic pop")
#endif
#else
#define POTASSCO_ATTR_UNUSED
#define POTASSCO_ATTR_NORETURN
#define POTASSCO_FUNC_NAME __FILE__
#define POTASSCO_WARNING_BEGIN_RELAXED
#define POTASSCO_WARNING_END_RELAXED
#endif
#include <inttypes.h>

#if !defined(POTASSCO_ENABLE_PRAGMA_TODO) || POTASSCO_ENABLE_PRAGMA_TODO==0
#undef POTASSCO_PRAGMA_TODO
#define POTASSCO_PRAGMA_TODO(X)
#endif

#if POTASSCO_HAS_STATIC_ASSERT == 0
template <bool> struct static_assertion;
template <>     struct static_assertion<true> {};
#undef static_assert
#define static_assert(x, message) typedef bool POTASSCO_CONCAT(potassco_static_assertion,__LINE__)[sizeof(static_assertion< (x) >)] POTASSCO_ATTR_UNUSED
#endif

#if UINTPTR_MAX > UINT64_MAX
#error Unsupported platform!
#endif

namespace Potassco {
struct EnumClass {
	const char *name, *rep;
	int  min, max;
	bool   isValid(int v)             const;
	size_t convert(const char*, int&) const;
	size_t convert(int, const char*&) const;
};
enum FailType {
	error_assert   = -1,
	error_logic    = -2,
	error_runtime  = -3
};
POTASSCO_ATTR_NORETURN extern void fail(int ec, const char* file, unsigned line, const char* exp, const char* fmt, ...);
} // namespace Potassco

/*!
 * \addtogroup BasicTypes
 */
///@{

//! Macro for defining a set of constants similar to a C++11 strong enum.
#define POTASSCO_ENUM_CONSTANTS_T(TypeName, BaseType, minVal, ...) \
	enum E { __VA_ARGS__, __eEnd, eMin = minVal, eMax = __eEnd - 1 };\
	TypeName(E x = eMin) : val_(x) {}\
	explicit TypeName(BaseType x) : val_(static_cast<E>(x)) {assert(x <= eMax);}\
	operator BaseType() const { return static_cast<BaseType>(val_); } \
	static Potassco::EnumClass enumClass() { Potassco::EnumClass r = {#TypeName, #__VA_ARGS__, eMin, eMax}; return r;}\
	E val_

//! Macro for defining a set of constants starting at 0.
#define POTASSCO_ENUM_CONSTANTS(TypeName, ...) \
	POTASSCO_ENUM_CONSTANTS_T(TypeName, unsigned, 0u, __VA_ARGS__)

//! Executes the given expression and calls Potassco::fail() with the given error code if it evaluates to false.
#define POTASSCO_CHECK(exp, ec, ...) \
	(void)( (!!(exp)) || (Potassco::fail(ec, POTASSCO_FUNC_NAME, unsigned(__LINE__), #exp, ## __VA_ARGS__, 0),0))

//! Shorthand for POTASSCO_CHECK(exp, Potassco::error_logic, args...).
#define POTASSCO_REQUIRE(exp, ...) POTASSCO_CHECK(exp, Potassco::error_logic, ## __VA_ARGS__)
//! Shorthand for POTASSCO_CHECK(exp, Potassco::error_assert, args...).
#define POTASSCO_ASSERT(exp, ...)  POTASSCO_CHECK(exp, Potassco::error_assert, ## __VA_ARGS__)
//! Shorthand for POTASSCO_CHECK(exp, Potassco::error_runtime, args...).
#define POTASSCO_EXPECT(exp, ...)  POTASSCO_CHECK(exp, Potassco::error_runtime, ## __VA_ARGS__)
///@}

#if (defined(__cplusplus) && __cplusplus >= 201103L) || defined(_MSC_VER) || defined(_LIBCPP_VERSION)
#define POTASSCO_EXT_INCLUDE(x) <x>
#if !defined(_MSC_VER) || _MSC_VER > 1500
#define POTASSCO_EXT_NS std
#else
#define POTASSCO_EXT_NS std::tr1
#endif
#else
#define POTASSCO_EXT_INCLUDE(x) <tr1/x>
#define POTASSCO_EXT_NS std::tr1
#endif

#if defined(__GNUC__)
#pragma GCC diagnostic pop
#elif defined(__clang__)
#pragma clang diagnostic pop
#endif

#endif
