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

#ifndef CLASP_UTIL_MISC_TYPES_H_INCLUDED
#define CLASP_UTIL_MISC_TYPES_H_INCLUDED

#include <clasp/config.h>
#include <utility>    // std::pair
#include <functional> // std::unary_function, std::binary_function
#include <iterator>
#include <algorithm>
#if defined(__GNUC__) || defined(__clang__)
// Disable deprecation warnings from std::{unary,binary}_function
#pragma GCC system_header
#endif

/*!
 * \file
 * \brief Some utility types and functions.
 */
namespace Clasp {

/*!
 * \defgroup misc Miscellaneous
 * \brief Miscellaneous and Internal Stuff not specific to clasp.
 */
//@{

template <class T>
inline T bit_mask(unsigned n) { return static_cast<T>(1) << n; }
//! Returns whether bit n is set in x.
template <class T>
inline bool test_bit(T x, unsigned n) { return (x & bit_mask<T>(n)) != 0; }
template <class T>
inline T clear_bit(T x, unsigned n)   { return x & ~bit_mask<T>(n); }
template <class T>
inline T set_bit(T x, unsigned n)     { return x | bit_mask<T>(n); }
template <class T>
inline T toggle_bit(T x, unsigned n)  { return x ^ bit_mask<T>(n); }
template <class T>
inline T& store_clear_bit(T& x, unsigned n)  { return (x &= ~bit_mask<T>(n)); }
template <class T>
inline T& store_set_bit(T& x, unsigned n)  { return (x |= bit_mask<T>(n)); }
template <class T>
inline T& store_toggle_bit(T& x, unsigned n)  { return (x ^= bit_mask<T>(n)); }
template <class T>
inline T right_most_bit(T x) { return x & (-x); }

inline uint32 log2(uint32 x) {
	uint32 ln = 0;
	if (x & 0xFFFF0000u) { x >>= 16; ln |= 16; }
	if (x & 0xFF00u    ) { x >>=  8; ln |=  8; }
	if (x & 0xF0u      ) { x >>=  4; ln |=  4; }
	if (x & 0xCu       ) { x >>=  2; ln |=  2; }
	if (x & 0x2u       ) {/*x>>=1*/; ln |=  1; }
	return ln;
}

//! Computes n choose k.
inline uint64 choose(unsigned n, unsigned k) {
	if (k == 0) return 1;
	if (k > n) return 0;
	if (2 * k > n) { return choose(n, n-k);}
	uint64 res = n;
	for (unsigned i = 2 ; i <= k; ++i) {
		res *= (n + 1 - i);
		res /= i;
	}
	return res;
}
inline double ratio(uint64 x, uint64 y)   { return y ? static_cast<double>(x) / static_cast<double>(y) : 0; }
inline double percent(uint64 x, uint64 y) {	return ratio(x, y) * 100.0; }

//! A very simple but fast Pseudo-random number generator.
/*!
 * \note This class is a replacement for the standard rand-function. It is provided
 * in order to get reproducible random numbers among different compilers.
 */
class RNG {
public:
	explicit RNG(uint32 seed = 1) : seed_(seed) {}

	//! Sets the starting point for random-number generation.
	/*!
	 * The function sets the starting point for generating a series of pseudorandom integers.
	 * To reinitialize the generator, use 1 as the seed argument. Any other value for seed
	 * sets the generator to a random starting point. Calling rand() before any call to srand()
	 * generates the same sequence as calling srand() with seed passed as 1.
	 */
	void srand(uint32 seed) { seed_ = seed; }

	//! Generates a pseudorandom number
	/*!
	 * The rand function returns a pseudorandom integer in the range 0 to 32767
	 * Use the srand function to seed the pseudorandom-number generator before calling rand.
	 */
	uint32 rand() {
		return( ((seed_ = seed_ * 214013L + 2531011L) >> 16) & 0x7fff );
	}

	//! random floating point number in the range [0, 1.0)
	double drand() {
		return this->rand()/static_cast<double>(0x8000u);
	}

	//! random number in the range [0, max)
	unsigned irand(unsigned max) {
		return static_cast<unsigned>(drand() * max);
	}

	uint32 seed() const { return seed_; }

	uint32 operator()(unsigned max) { return irand(max); }
	uint32 operator()()             { return rand(); }
private:
	uint32 seed_;
};

//! Updates the given exponential moving average with the given sample.
/*!
 * Computes ema = currentEma + ((double(sample) - currentEma)*alpha);
 */
template <class T>
inline double exponentialMovingAverage(double currentEma, T sample, double alpha) {
	return (static_cast<double>(sample) * alpha) + (currentEma * (1.0 - alpha));
}
//! Updates the given moving average with the given sample.
template <class T>
inline double cumulativeMovingAverage(double currentAvg, T sample, uint64 numSeen) {
	return (static_cast<double>(sample) + (currentAvg * numSeen)) / static_cast<double>(numSeen + 1);
}

//! An unary operator function that calls p->destroy().
struct DestroyObject {
	template <class T> void operator()(T* p) const { if (p) p->destroy(); }
};
//! An unary operator function that calls delete p.
struct DeleteObject {
	template <class T> void operator()(T* p) const { delete p; }
};
//! An unary operator function that calls p->release().
struct ReleaseObject {
	template <class T> void operator()(T* p) const { if (p) p->release(); }
};
//! An unary operator function that returns whether its argument is 0.
struct IsNull {
	template <class T> bool operator()(const T& p) const { return p == 0; }
};

//! A predicate that checks whether a std::pair contains a certain value.
template <class T>
struct PairContains {
	PairContains(const T& p) : p_(p) {}
	bool operator()(const std::pair<T, T>& s) const {
		return s.first == p_ || s.second == p_;
	}
	T p_;
};

//! Removes from the container c the first occurrence of a value v for which p(v) returns true.
/*!
 * \pre C is a container that provides back() and pop_back()
 * \note Removal is implemented by replacing the element to be removed with
 * the back()-element followed by a call to pop_back().
 */
template <class C, class P>
void remove_first_if(C& cont, const P& p) {
	for (typename C::iterator it = cont.begin(), end = cont.end(); it != end; ++it) {
		if (p(*it)) {
			*it = cont.back();
			cont.pop_back();
			return;
		}
	}
}

//! An unary operator function that simply returns its argument.
template <class T>
struct identity : std::unary_function<T, T>{
	T&        operator()(T& x)      const { return x; }
	const T&  operator()(const T& x)  const { return x; }
};


//! An unary operator function that returns the first value of a std::pair.
template <class P>
struct select1st : std::unary_function<P, typename P::first_type> {
	typename P::first_type& operator()(P& x) const {
		return x.first;
	}
	const typename P::first_type& operator()(const P& x) const {
		return x.first;
	}
};

//! An unary operator function that returns the second value of a std::pair.
template <class P>
struct select2nd : std::unary_function<P, typename P::second_type> {
	typename P::second_type& operator()(P& x) const {
		return x.second;
	}
	const typename P::second_type& operator()(const P& x) const {
		return x.second;
	}
};

//! An unary operator function that returns Op1(Op2(x)).
template <class OP1, class OP2>
struct compose_1 : public std::unary_function<
                            typename OP2::argument_type,
                            typename OP1::result_type> {
	compose_1(const OP1& op1, const OP2& op2)
		: op1_(op1)
		, op2_(op2) {}

	typename OP1::result_type operator()(const typename OP2::argument_type& x) const {
		return op1_(op2_(x));
	}
protected:
	OP1 op1_;
	OP2 op2_;
};

/*!
 * A template helper function used to construct objects of type compose_1,
 * where the component types are based on the data types passed as parameters.
 */
template <class OP1, class OP2>
inline compose_1<OP1, OP2> compose1(const OP1& op1, const OP2& op2) {
	return compose_1<OP1, OP2>(op1, op2);
}

//! An unary operator function that returns OP1(OP2(x), OP3(x)).
template <class OP1, class OP2, class OP3>
struct compose_2_1 : public std::unary_function<
                            typename OP2::argument_type,
                            typename OP1::result_type> {
	compose_2_1(const OP1& op1, const OP2& op2, const OP3& op3)
		: op1_(op1)
		, op2_(op2)
		, op3_(op3) {}

	typename OP1::result_type operator()(const typename OP2::argument_type& x) const {
		return op1_(op2_(x), op3_(x));
	}
protected:
	OP1 op1_;
	OP2 op2_;
	OP3 op3_;
};

/*!
 * A template helper function used to construct objects of type compose_2_1,
 * where the component types are based on the data types passed as parameters.
 */
template <class OP1, class OP2, class OP3>
inline compose_2_1<OP1, OP2,OP3> compose2(const OP1& op1, const OP2& op2, const OP3& op3) {
	return compose_2_1<OP1, OP2, OP3>(op1, op2, op3);
}


//! A binary operator function that returns OP1(OP2(x), OP3(y)).
template <class OP1, class OP2, class OP3>
struct compose_2_2 : public std::binary_function<
                            typename OP2::argument_type,
                            typename OP3::argument_type,
                            typename OP1::result_type> {
	compose_2_2(const OP1& op1 = OP1(), const OP2& op2 = OP2(), const OP3& op3 = OP3())
		: op1_(op1)
		, op2_(op2)
		, op3_(op3) {}

	typename OP1::result_type operator()(const typename OP2::argument_type& x, const typename OP3::argument_type& y) const {
		return op1_(op2_(x), op3_(y));
	}
protected:
	OP1 op1_;
	OP2 op2_;
	OP3 op3_;
};

/*!
 * A template helper function used to construct objects of type compose_2_2,
 * where the component types are based on the data types passed as parameters.
 */
template <class OP1, class OP2, class OP3>
inline compose_2_2<OP1, OP2,OP3> compose22(const OP1& op1, const OP2& op2, const OP3& op3) {
	return compose_2_2<OP1, OP2, OP3>(op1, op2, op3);
}

//! TODO: replace with std::is_sorted once we switch to C++11
template <class ForwardIterator, class Compare>
bool isSorted(ForwardIterator first, ForwardIterator last, Compare comp) {
	if (first != last) {
		for (ForwardIterator n = first; ++n != last; ++first) {
			if (comp(*n, *first)) return false;
		}
	}
	return true;
}

//! Possible ownership operations.
struct Ownership_t {
	enum Type { Retain = 0, Acquire = 1 };
};
//! A smart pointer that optionally owns its pointee.
template <class T, class D = DeleteObject>
class SingleOwnerPtr {
public:
	SingleOwnerPtr() : ptr_(0) {}
	explicit SingleOwnerPtr(T* ptr, Ownership_t::Type t = Ownership_t::Acquire)
		: ptr_(uintp(ptr) | uintp(t == Ownership_t::Acquire)) {
	}
	~SingleOwnerPtr()       { *this = 0; }
	bool is_owner()   const { return test_bit(ptr_, 0); }
	T*   get()        const { return (T*)clear_bit(ptr_, 0); }
	T&   operator*()  const { return *get(); }
	T*   operator->() const { return  get(); }
	SingleOwnerPtr& operator=(T* ptr) { reset(ptr); return *this; }
	void swap(SingleOwnerPtr& o) { std::swap(ptr_, o.ptr_); }
	T*   release()               { store_clear_bit(ptr_, 0); return get();  }
	T*   acquire()               { store_set_bit(ptr_, 0);   return get();  }
	void reset(T* x)             {
		if (x != get() && is_owner()) { D deleter; deleter(release()); }
		ptr_ = set_bit(uintp(x),0);
	}
private:
	SingleOwnerPtr(const SingleOwnerPtr&);
	SingleOwnerPtr& operator=(const SingleOwnerPtr&);
	uintp ptr_;
};
template <class T>
class FlaggedPtr {
public:
	FlaggedPtr() : ptr_(0) {}
	explicit FlaggedPtr(T* ptr, bool sf = false) : ptr_(uintp(ptr)) { if (sf) flag(); }
	bool flagged()    const  { return test_bit(ptr_, 0); }
	T*   get()        const  { return (T*)clear_bit(ptr_, 0); }
	T&   operator*()  const  { return *get(); }
	T*   operator->() const  { return  get(); }
	void swap(FlaggedPtr& o) { std::swap(ptr_, o.ptr_); }
	void flag()   { store_set_bit(ptr_, 0); }
	void unflag() { store_clear_bit(ptr_, 0); }
private:
	uintp ptr_;
};
template <class T>
inline FlaggedPtr<T> make_flagged(T* ptr, bool setFlag) { return FlaggedPtr<T>(ptr, setFlag); }

//! A (numerical) range represented by a low and a high value.
template <class T>
struct Range {
	Range(T x, T y) : lo(x), hi(y) { if (x > y)  { hi = x;  lo = y; } }
	T clamp(T val) const {
		if (val < lo) return lo;
		if (val > hi) return hi;
		return val;
	}
	T lo;
	T hi;
};
template <class T>
inline bool operator==(const Range<T>& lhs, const Range<T>& rhs) {
	return lhs.lo == rhs.lo && lhs.hi == rhs.hi;
}
//! An iterator type for iterating over a range of numerical values.
template <class T>
struct num_iterator : std::iterator<std::random_access_iterator_tag, T> {
	explicit num_iterator(const T& val) : val_(val) {}
	typedef typename std::iterator<std::random_access_iterator_tag, T>::value_type      value_type;
	typedef typename std::iterator<std::random_access_iterator_tag, T>::difference_type difference_type;
	bool operator==(const num_iterator& rhs) const { return val_ == rhs.val_; }
	bool operator!=(const num_iterator& rhs) const { return val_ != rhs.val_; }
	bool operator<=(const num_iterator& rhs) const { return val_ <= rhs.val_; }
	bool operator< (const num_iterator& rhs) const { return val_  < rhs.val_; }
	bool operator>=(const num_iterator& rhs) const { return val_ >= rhs.val_; }
	bool operator> (const num_iterator& rhs) const { return val_  > rhs.val_; }
	value_type    operator*()  const { return val_; }
	T const*      operator->() const { return &val_; }
	num_iterator& operator++()       { ++val_; return *this; }
	num_iterator  operator++(int)    { num_iterator t(*this); ++*this; return t; }
	num_iterator& operator--()       { --val_; return *this; }
	num_iterator  operator--(int)    { num_iterator t(*this); --*this; return t; }
	num_iterator& operator+=(difference_type n) { val_ += n; return *this; }
	num_iterator& operator-=(difference_type n) { val_ -= n; return *this; }
	num_iterator  operator+(difference_type n) const { return num_iterator(static_cast<T>(val_ + n)); }
	num_iterator  operator-(difference_type n) const { return num_iterator(static_cast<T>(val_ - n)); }
	value_type    operator[](difference_type n)const { return val_ + n; }
	friend num_iterator operator+(difference_type n, num_iterator it) { return num_iterator(it.val_ + n); }
private:
	T val_;
};
//@}

//! Base class for library events.
struct Event {
	//! Set of known event sources.
	enum Subsystem { subsystem_facade = 0, subsystem_load = 1, subsystem_prepare = 2, subsystem_solve = 3 };
	//! Possible verbosity levels.
	enum Verbosity { verbosity_quiet  = 0, verbosity_low  = 1, verbosity_high    = 2, verbosity_max   = 3 };
	explicit Event(Subsystem sys, uint32 evId, Verbosity verbosity) : system(sys), verb(verbosity), op(0), id(evId) {}
	uint32 system : 2; //!< One of Event::Subsystem - subsystem that produced the event.
	uint32 verb   : 2; //!< One of Event::Verbosity - the verbosity level of this event.
	uint32 op     : 8; //!< Operation that triggered the event.
	uint32 id     : 16;//!< Type id of event.
	static uint32 nextId();
};
//! CRTP-base class for events of type T that registers an id for type T.
template <class T>
struct Event_t : Event {
	Event_t(Subsystem sys, Verbosity verb) : Event(sys, id_s, verb) {}
	static const uint32 id_s;
};
template <class T> const uint32 Event_t<T>::id_s = Event::nextId();

template <class ToType, class EvType> const ToType* event_cast(const EvType& ev) { return ev.id == ToType::id_s ? static_cast<const ToType*>(&ev) : 0; }

}
#endif
