//
// Copyright (c) 2016-2017 Benjamin Kaufmann
//
// This file is part of Clasp. See http://www.cs.uni-potsdam.de/clasp/
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
#ifndef CLASP_HASH_H_INCLUDED
#define CLASP_HASH_H_INCLUDED
#include <potassco/platform.h>
#include <cstring>
namespace Clasp {
//! Hasher for strings.
/*!
 * \see http://research.microsoft.com/en-us/people/palarson/
 */
struct StrHash {
	std::size_t operator()(const char* str) const {
		std::size_t h = 0;
		for (const char* s = str; *s; ++s) {
			h = h * 101 + static_cast<std::size_t>(*s);
		}
		return h;
	}
};
//! Comparison function for C-strings to be used with hash map/set.
struct StrEq {
	bool operator()(const char* lhs, const char* rhs) const { return std::strcmp(lhs, rhs) == 0; }
};

}
#endif
