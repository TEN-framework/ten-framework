//
// Copyright (c) 2012-2017 Benjamin Kaufmann
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

#ifndef CLASP_UTIL_MUTEX_H_INCLUDED
#define CLASP_UTIL_MUTEX_H_INCLUDED

#include <mutex>
#include <condition_variable>

namespace Clasp { namespace mt {
using std::mutex;
using std::lock_guard;
using std::unique_lock;
using std::swap;
using std::defer_lock_t;
struct condition_variable : private std::condition_variable {
	typedef std::condition_variable base_type;
	using base_type::notify_one;
	using base_type::notify_all;
	using base_type::wait;

	template <typename X = std::condition_variable>
	inline auto native_handle() -> typename X::native_handle_type {
		return X::native_handle();
	}

	inline bool wait_for(unique_lock<mutex>& lock, double timeInSecs) {
		return base_type::wait_for(lock, std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::duration<double>(timeInSecs)))
			== std::cv_status::no_timeout;
	}
};
}}
#endif
