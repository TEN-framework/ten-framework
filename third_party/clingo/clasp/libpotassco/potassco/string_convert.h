//
// Copyright (c) 2017 Benjamin Kaufmann
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
#ifndef POTASSCO_STRING_CONVERT_H_INCLUDED
#define POTASSCO_STRING_CONVERT_H_INCLUDED
#include <potassco/platform.h>
#include <potassco/basic_types.h>
#include <cstring>
#include <string>
#include <vector>
#include <utility>
#include <stdexcept>
#include <typeinfo>
#include <istream>
#include <iterator>
#include <climits>
#include <cstdarg>
#if !defined(_MSC_VER)
#include <strings.h>
#else
inline int strcasecmp(const char* lhs, const char* rhs)            { return _stricmp(lhs, rhs); }
inline int strncasecmp(const char* lhs, const char* rhs, size_t n) { return _strnicmp(lhs, rhs, n); }
#endif
namespace Potassco { namespace detail {
// A primitive input stream buffer for fast extraction from a given string
// NOTE: The input string is NOT COPIED, hence it
//       MUST NOT CHANGE during extraction
template<class T, class Traits = std::char_traits<T> >
class input_from_string : public std::basic_streambuf<T, Traits> {
	typedef std::basic_streambuf<T, Traits>   base_type;
	typedef typename Traits::char_type*       pointer_type;
	typedef const typename Traits::char_type* const_pointer_type;
	typedef typename base_type::pos_type      pos_type;
	typedef typename base_type::off_type      off_type;
public:
	explicit input_from_string(const_pointer_type p, size_t size)
		: buffer_(const_cast<pointer_type>(p))
		, size_(size) {
		base_type::setp(0, 0); // no write buffer
		base_type::setg(buffer_, buffer_, buffer_+size_); // read buffer
	}
	pos_type seekoff(off_type offset, std::ios_base::seekdir dir, std::ios_base::openmode which) {
		if(which & std::ios_base::out) {
			// not supported!
			return base_type::seekoff(offset, dir, which);
		}
		if(dir == std::ios_base::cur) {
			offset += static_cast<off_type>(base_type::gptr() - base_type::eback());
		}
		else if(dir == std::ios_base::end) {
			offset = static_cast<off_type>(size_) - offset;
		}
		return seekpos(offset, which);
	}
	pos_type seekpos(pos_type offset, std::ios_base::openmode which) {
		if((which & std::ios_base::out) == 0 && offset >= pos_type(0) && ((size_t)offset) <= size_) {
			base_type::setg(buffer_, buffer_+(size_t)offset, buffer_+size_);
			return offset;
		}
		return base_type::seekpos(offset, which);
	}
private:
	input_from_string(const input_from_string&);
	input_from_string& operator=(const input_from_string&);
protected:
	pointer_type buffer_;
	size_t       size_;
};

template<class T, class Traits = std::char_traits<T> >
class input_stream : public std::basic_istream<T, Traits> {
public:
	input_stream(const std::string& str)
		: std::basic_istream<T, Traits>(0)
		, buffer_(str.data(), str.size()) {
		std::basic_istream<T, Traits>::rdbuf(&buffer_);
	}
	input_stream(const char* x, size_t size)
		: std::basic_istream<T, Traits>(0)
		, buffer_(x, size) {
		std::basic_istream<T, Traits>::rdbuf(&buffer_);
	}
private:
	input_from_string<T, Traits> buffer_;
};
struct no_stream_support { template <class T> no_stream_support(const T&) {} };
no_stream_support& operator >> (std::istream&, const no_stream_support&);
template <bool b, class T>  struct enable_if;
template <class T>          struct enable_if<true, T> { typedef T type; };
template <class T, T>       struct type_check { enum { value = 1 }; };
template <class T, class U> struct is_same       { enum { value = 0 }; };
template <class T>          struct is_same<T, T> { enum { value = 1 }; };
} // namespace detail
///////////////////////////////////////////////////////////////////////////////
// primitive parser
///////////////////////////////////////////////////////////////////////////////
template <class T>
int xconvert(const char* x, T& out, const char** errPos = 0, double = 0);
int xconvert(const char* x, bool& out, const char** errPos = 0, int = 0);
int xconvert(const char* x, char& out, const char** errPos = 0, int = 0);
int xconvert(const char* x, unsigned& out, const char** errPos = 0, int = 0);
int xconvert(const char* x, int& out, const char** errPos = 0, int = 0);
int xconvert(const char* x, long& out, const char** errPos = 0, int = 0);
int xconvert(const char* x, unsigned long& out, const char** errPos = 0, int = 0);
int xconvert(const char* x, double& out, const char** errPos = 0, int = 0);
int xconvert(const char* x, const char*& out, const char** errPos = 0, int = 0);
int xconvert(const char* x, std::string& out, const char** errPos = 0, int sep = 0);
template <class T>
typename detail::enable_if<detail::type_check<EnumClass(*)(), &T::enumClass>::value, int>::type
xconvert(const char* x, T& out, const char** errPos, int e = 0) {
	size_t len = T::enumClass().convert(x, e);
	if (errPos) { *errPos = x + len; }
	if (len)    { out = static_cast<T>(e); }
	return int(len > 0u);
}
std::string& xconvert(std::string&, bool);
std::string& xconvert(std::string&, char);
std::string& xconvert(std::string&, int);
std::string& xconvert(std::string&, unsigned int);
std::string& xconvert(std::string&, long);
std::string& xconvert(std::string&, unsigned long);
std::string& xconvert(std::string&, double);
inline std::string& xconvert(std::string& out, const std::string& s) { return out.append(s); }
inline std::string& xconvert(std::string& out, const char* s)        { return out.append(s ? s : ""); }
template <class T>
typename detail::enable_if<detail::type_check<EnumClass(*)(), &T::enumClass>::value, std::string>::type&
xconvert(std::string& out, T x) {
	const char* key;
	size_t len = T::enumClass().convert(static_cast<int>(x), key);
	return out.append(key, len);
}
#if defined(LLONG_MAX)
int xconvert(const char* x, long long& out, const char** errPos = 0, int = 0);
int xconvert(const char* x, unsigned long long& out, const char** errPos = 0, int = 0);
std::string& xconvert(std::string&, long long x);
std::string& xconvert(std::string&, unsigned long long x);
#endif
///////////////////////////////////////////////////////////////////////////////
// composite parser
///////////////////////////////////////////////////////////////////////////////
const int def_sep = int(',');
template <class T>
int xconvert(const char* x, std::vector<T>& out, const char** errPos = 0, int sep = def_sep);

// parses T[,U] optionally enclosed in parentheses
template <class T, class U>
int xconvert(const char* x, std::pair<T, U>& out, const char** errPos = 0, int sep = def_sep) {
	if (!x) { return 0; }
	if (sep == 0) { sep = def_sep; }
	std::pair<T, U> temp(out);
	const char* n = x;
	int ps = 0;
	if (*n == '(') { ++ps; ++n; }
	int tokT = xconvert(n, temp.first, &n, sep);
	int tokU = tokT && *n == (char)sep && n[1] ? xconvert(n+1, temp.second, &n, sep) : 0;
	int sum  = 0;
	if (!ps || *n == ')') {
		n += ps;
		if (tokU)        { out.second= temp.second; ++sum; }
		if (tokU || !*n) { out.first = temp.first; ++sum; }
	}
	if (!sum) { n = x; }
	if (errPos) *errPos = n;
	return sum;
}
// parses T1 [, ..., Tn] optionally enclosed in brackets
template <class T, class OutIt>
std::size_t convert_seq(const char* x, std::size_t maxLen, OutIt out, char sep, const char** errPos = 0) {
	if (!x) { return 0; }
	const char* n = x;
	std::size_t t = 0;
	std::size_t b = 0;
	if (*n == '[') { ++b; ++n; }
	while (t != maxLen) {
		T temp;
		if (!xconvert(n, temp, &n, sep)) break;
		*out++ = temp;
		++t;
		if (!*n || *n != (char)sep || !n[1]) break;
		n = n+1;
	}
	if (!b || *n == ']') { n += b; }
	else                 { n  = x; }
	if (errPos) *errPos = n;
	return t;
}
// parses T1 [, ..., Tn] optionally enclosed in brackets
template <class T>
int xconvert(const char* x, std::vector<T>& out, const char** errPos, int sep) {
	if (sep == 0) { sep = def_sep; }
	std::size_t sz = out.size();
	std::size_t t  = convert_seq<T>(x, out.max_size() - sz, std::back_inserter(out), static_cast<char>(sep), errPos);
	if (!t) { out.resize(sz); }
	return static_cast<int>(t);
}
template <class T, int sz>
int xconvert(const char* x, T(&out)[sz], const char** errPos = 0, int sep = 0) {
	return static_cast<int>(convert_seq<T>(x, sz, out, static_cast<char>(sep ? sep : def_sep), errPos));
}
template <class T, class U>
std::string& xconvert(std::string& out, const std::pair<T, U>& in, char sep = static_cast<char>(def_sep)) {
	xconvert(out, in.first).append(1, sep);
	return xconvert(out, in.second);
}
template <class IT>
std::string& xconvert(std::string& accu, IT begin, IT end, char sep = static_cast<char>(def_sep)) {
	for (bool first = true; begin != end; first = false) {
		if (!first) { accu += sep; }
		xconvert(accu, *begin++);
	}
	return accu;
}
template <class T>
std::string& xconvert(std::string& out, const std::vector<T>& in, char sep = static_cast<char>(def_sep)) {
	return xconvert(out, in.begin(), in.end(), sep);
}
///////////////////////////////////////////////////////////////////////////////
// fall back parser
///////////////////////////////////////////////////////////////////////////////
template <class T>
int xconvert(const char* x, T& out, const char** errPos, double) {
	std::size_t xLen = std::strlen(x);
	const char* err  = x;
	detail::input_stream<char> str(x, xLen);
	if (str >> out) {
		if (str.eof()){ err += xLen; }
		else          { err += static_cast<std::size_t>(str.tellg()); }
	}
	if (errPos) { *errPos = err; }
	return int(err != x);
}
///////////////////////////////////////////////////////////////////////////////
// string -> T
///////////////////////////////////////////////////////////////////////////////
class bad_string_cast : public std::bad_cast {
public:
	~bad_string_cast() throw();
	virtual const char* what() const throw();
};
template <class T>
bool string_cast(const char* arg, T& to) {
	const char* end;
	return xconvert(arg, to, &end, 0) != 0 && !*end;
}
template <class T>
T string_cast(const char* s) {
	T to;
	if (string_cast<T>(s, to)) { return to; }
	throw bad_string_cast();
}
template <class T>
T string_cast(const std::string& s) { return string_cast<T>(s.c_str()); }
template <class T>
bool string_cast(const std::string& from, T& to) { return string_cast<T>(from.c_str(), to); }

template <class T>
bool stringTo(const char* str, T& x) {
	return string_cast(str, x);
}
///////////////////////////////////////////////////////////////////////////////
// T -> string
///////////////////////////////////////////////////////////////////////////////
template <class U>
std::string string_cast(const U& num) {
	std::string out;
	xconvert(out, num);
	return out;
}
template <class T>
inline std::string toString(const T& x) {
	return string_cast(x);
}
template <class T, class U>
inline std::string toString(const T& x, const U& y) {
	std::string res;
	xconvert(res, x).append(1, ',');
	return xconvert(res, y);
}
template <class T, class U, class V>
std::string toString(const T& x, const U& y, const V& z) {
	std::string res;
	xconvert(res, x).append(1, ',');
	xconvert(res, y).append(1, ',');
	return xconvert(res, z);
}

#define POTASSCO_FORMAT_S(str, fmt, ...) (Potassco::StringBuilder(str).appendFormat((fmt), __VA_ARGS__), (str))
#define POTASSCO_FORMAT(fmt, ...) (Potassco::StringBuilder().appendFormat((fmt), __VA_ARGS__).c_str())

int vsnprintf(char* s, size_t n, const char* format, va_list arg);
//! A class for creating a sequence of characters.
class StringBuilder {
public:
	typedef StringBuilder ThisType;
	enum Mode {Fixed, Dynamic};
	//! Constructs an empty object with an initial capacity of 63 characters.
	explicit StringBuilder();
	//! Constructs an object that appends new characters to the given string s.
	explicit StringBuilder(std::string& s);
	//! Constrcuts an object that stores up to n characters in the given array.
	/*!
	 * If m is Fixed, the maximum size of this sequence is fixed to n.
	 * Otherwise, the sequence may switch to an internal buffer once size()
	 * exceeds n characters.
	 */
	explicit StringBuilder(char* buf, std::size_t n, Mode m = Fixed);
	~StringBuilder();

	//! Returns the character sequence as a null-terminated string.
	const char* c_str() const;
	//! Returns the length of this character sequence, i.e. std::strlen(c_str()).
	std::size_t size()  const;
	//! Returns if character sequence is empty, i.e. std::strlen(c_str()) == 0.
	bool        empty() const { return size() == std::size_t(0); }
	Span<char>  toSpan() const;
	//! Resizes this character sequence to a length of n characters.
	/*!
	 * \throw std::logic_error if n > maxSize()
	 * \throw std::bad_alloc if the function needs to allocate storage and fails.
	 */
	ThisType&   resize(std::size_t n, char c = '\0');
	//! Returns the maximum size of this sequence.
	std::size_t maxSize() const;
	//! Clears this character sequence.
	ThisType&   clear() { return resize(std::size_t(0)); }

	//! Returns the character at the specified position.
	char        operator[](std::size_t p) const { return buffer().head[p]; }
	char&       operator[](std::size_t p)       { return buffer().head[p]; }

	//! Appends the given null-terminated string.
	ThisType&   append(const char* str);
	//! Appends the first n characters in str.
	ThisType&   append(const char* str, std::size_t n);
	//! Appends n consecutive copies of character c.
	ThisType&   append(std::size_t n, char c);
	//! Appends the given number.
	ThisType&   append(int n)                { return append_(static_cast<uint64_t>(static_cast<int64_t>(n)), n >= 0); }
	ThisType&   append(long n)               { return append_(static_cast<uint64_t>(static_cast<int64_t>(n)), n >= 0); }
	ThisType&   append(long long n)          { return append_(static_cast<uint64_t>(static_cast<int64_t>(n)), n >= 0); }
	ThisType&   append(unsigned int n)       { return append_(static_cast<uint64_t>(n), true); }
	ThisType&   append(unsigned long n)      { return append_(static_cast<uint64_t>(n), true); }
	ThisType&   append(unsigned long long n) { return append_(static_cast<uint64_t>(n), true); }
	ThisType&   append(float x)              { return append(static_cast<double>(x)); }
	ThisType&   append(double x);
	//! Appends the null-terminated string fmt, replacing any format specifier in the same way as printf does.
	ThisType&   appendFormat(const char* fmt, ...);
private:
	StringBuilder(const StringBuilder&);
	StringBuilder& operator=(const StringBuilder&);
	ThisType& append_(uint64_t n, bool pos);
	enum Type { Sbo = 0u, Str = 64u, Buf = 128u };
	enum { Own = 1u, SboCap = 63u };
	struct Buffer {
		std::size_t free() const { return size - used; }
		char*       pos()  const { return head + used; }
		char*       head;
		std::size_t used;
		std::size_t size;
	};
	void setTag(uint8_t t) { reinterpret_cast<uint8_t&>(sbo_[63]) = t; }
	uint8_t  tag()  const  { return static_cast<uint8_t>(sbo_[63]); }
	Type     type() const  { return static_cast<Type>(tag() & uint8_t(Str|Buf)); }
	Buffer   grow(std::size_t n);
	Buffer   buffer() const;
	union {
		std::string* str_;
		Buffer       buf_;
		char         sbo_[64];
	};
};
inline Span<char> toSpan(StringBuilder& b) { return b.toSpan(); }

} // namespace Potassco

#endif
