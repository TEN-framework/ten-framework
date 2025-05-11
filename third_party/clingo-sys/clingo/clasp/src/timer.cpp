//
// Copyright (c) 2006-2017 Benjamin Kaufmann
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
#include <clasp/util/timer.h>

#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN // exclude APIs such as Cryptography, DDE, RPC, Shell, and Windows Sockets.
#define NOMINMAX            // do not let windows.h define macros min and max
#include <windows.h>        // GetProcessTimes, GetCurrentProcess, FILETIME
#define TICKS_PER_SEC 10000000

namespace Clasp {

double RealTime::getTime() {
	union Convert {
		FILETIME time;
		__int64  asUint;
	} now;
	GetSystemTimeAsFileTime(&now.time);
	return static_cast<double>(now.asUint/static_cast<double>(TICKS_PER_SEC));
}

double ProcessTime::getTime() {
	FILETIME ignoreStart, ignoreExit;
	union Convert {
		FILETIME time;
		__int64  asUint;
	} user, system;
	GetProcessTimes(GetCurrentProcess(), &ignoreStart, &ignoreExit, &user.time, &system.time);
	return (user.asUint + system.asUint) / double(TICKS_PER_SEC);
}

double ThreadTime::getTime() {
	FILETIME ignoreStart, ignoreExit;
	union Convert {
		FILETIME time;
		__int64  asUint;
	} user, system;
	GetThreadTimes(GetCurrentThread(), &ignoreStart, &ignoreExit, &user.time, &system.time);
	return (user.asUint + system.asUint) / double(TICKS_PER_SEC);
}

}
#else
#include <sys/times.h>   // times()
#include <sys/time.h>    // gettimeofday()
#include <sys/resource.h>// getrusage
#include <limits>
#ifdef __APPLE__
#include <mach/mach.h>
#include <mach/thread_info.h>
#endif
namespace Clasp {

double RealTime::getTime() {
	struct timeval now;
	return gettimeofday(&now, 0) == 0
		? static_cast<double>(now.tv_sec) + static_cast<double>(now.tv_usec / 1000000.0)
		: 0.0;
}
inline double rusageTime(int who) {
	struct rusage usage;
	getrusage(who, &usage);
	return(static_cast<double>(usage.ru_utime.tv_sec) + static_cast<double>(usage.ru_utime.tv_usec / 1000000.0))
		+ (static_cast<double>(usage.ru_stime.tv_sec) + static_cast<double>(usage.ru_stime.tv_usec / 1000000.0));
}
double ProcessTime::getTime() {
	return rusageTime(RUSAGE_SELF);
}
double ThreadTime::getTime() {
	double res = 0.0;
#if defined(RUSAGE_THREAD)
	res = rusageTime(RUSAGE_THREAD);
#elif __APPLE__
	struct thread_basic_info t_info;
	mach_msg_type_number_t t_info_count = TASK_BASIC_INFO_COUNT;
	if (thread_info(mach_thread_self(), THREAD_BASIC_INFO, (thread_info_t)&t_info, &t_info_count) == KERN_SUCCESS) {
		time_value_add(&t_info.user_time, &t_info.system_time);
		res = static_cast<double>(t_info.user_time.seconds) + static_cast<double>(t_info.user_time.microseconds / static_cast<double>(TIME_MICROS_MAX));
	}
#endif
	return res;
}

}
#endif

