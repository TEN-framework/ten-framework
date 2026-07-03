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

#ifndef CLASP_TIMER_H_INCLUDED
#define CLASP_TIMER_H_INCLUDED

#ifdef _MSC_VER
#pragma once
#endif
/*!
 * \file
 * \brief Defines various types for getting absolute times.
 */
namespace Clasp {

//! A type for getting the current process time.
struct ProcessTime {
	static double getTime();
};
//! A type for getting the current thread time.
struct ThreadTime {
	static double getTime();
};
//! A tpe for getting the current wall-clock time.
struct RealTime {
	static double getTime();
};

inline double diffTime(double tEnd, double tStart) {
	double diff = tEnd - tStart;
	return diff >= 0 ? diff : 0.0;
}

//! A class for measuring elapsed time.
/*!
 * \tparam TimeType must provide a single static function
 * TimeType::getTime() returning an absolute time.
 */
template <class TimeType>
class Timer {
public:
	Timer() : start_(0), split_(0), total_(0) {}

	void   start()   { start_ = TimeType::getTime(); }
	void   stop()    { split(TimeType::getTime()); }
	void   reset()   { *this  = Timer(); }
	//! Same as stop(), start();
	void   lap()     { double t; split(t = TimeType::getTime()); start_ = t; }
	//! Returns the elapsed time (in seconds) for last start-stop cycle.
	double elapsed() const { return split_; }
	//! Returns the total elapsed time for all start-stop cycles.
	double total()   const { return total_; }
private:
	void split(double t) { total_ += (split_ = diffTime(t, start_)); }
	double start_;
	double split_;
	double total_;
};

}
#endif
