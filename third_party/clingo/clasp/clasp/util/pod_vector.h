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
#ifndef BK_LIB_POD_VECTOR_H_INCLUDED
#define BK_LIB_POD_VECTOR_H_INCLUDED
#include "type_manip.h"
#include <iterator>
#include <memory>
#include <cstring>
#include <algorithm>
#include <cassert>
#include <stdexcept>

#if defined(__GNUC__)
#pragma GCC system_header
#endif

namespace bk_lib { namespace detail {
	template <class T>
	void fill(T* first, T* last, const T& x) {
		assert(first <= last);
		switch ((last - first) & 7u)
		{
		case 0:
				while (first != last)
				{
				new(first++) T(x);
		case 7: new(first++) T(x);
		case 6: new(first++) T(x);
		case 5: new(first++) T(x);
		case 4: new(first++) T(x);
		case 3: new(first++) T(x);
		case 2: new(first++) T(x);
		case 1: new(first++) T(x);
				assert(first <= last);
				}
		}
	}
	template <class Iter, class T>
	void copy(Iter first, Iter last, std::size_t s, T* out) {
		switch (s & 7u)
		{
		case 0:
				while (first != last)
				{
				new(out++) T(*first++);
		case 7: new(out++) T(*first++);
		case 6: new(out++) T(*first++);
		case 5: new(out++) T(*first++);
		case 4: new(out++) T(*first++);
		case 3: new(out++) T(*first++);
		case 2: new(out++) T(*first++);
		case 1: new(out++) T(*first++);
				}
		}
	}
	template <class T>
	struct Fill {
		Fill(const T& val) : val_(val) {}
		void operator()(T* first, std::size_t n) const { detail::fill(first, first + n, val_); }
		const T& val_;
	private: Fill& operator=(const Fill&);
	};
	template <class Iter>
	struct Copy {
		Copy(Iter first, Iter last) : first_(first), last_(last) {}
		template <class T>
		void operator()(T* out, std::size_t n) const { detail::copy(first_, last_, n, out); }
		Iter first_;
		Iter last_;
	};
	template <class T>
	struct Memcpy {
		Memcpy(const T* first) : first_(first) {}
		void operator()(T* out, std::size_t n) const {
			std::memcpy(out, first_, n*sizeof(T));
		}
		const T* first_;
	};
	typedef char yes_type;
	typedef char (&no_type)[2];
	template <class T>
	struct IterType {
		static yes_type isPtr(const volatile void*);
		static no_type isPtr(...);
		static yes_type isLong(long long);
		static no_type  isLong(...);
		static T& makeT();
		enum { ptr = sizeof(isPtr(makeT())) == sizeof(yes_type) };
		enum { num = sizeof(isLong(makeT())) == sizeof(yes_type) };
		enum { value = ptr ? 1 : num ? 2 : 0 };
	};

} // end namespace bk_lib::detail

//! A std::vector-replacement for POD-Types.
/*!
 * \pre T is a POD-Type
 * \see http://www.comeaucomputing.com/techtalk/#pod for a description of POD-Types.
 * \note Does not call any destructors and uses std::memcpy to copy/move elements
 * \note On LP64-machines size and capacity are represented as unsigned integers (instead of e.g. std::size_t)
 */
template <class T, class Allocator = std::allocator<T> >
class pod_vector {
public:
	// types:
	typedef          pod_vector<T,Allocator>          this_type;//not standard
	typedef          Allocator                        allocator_type;
	typedef typename Allocator::reference             reference;
	typedef typename Allocator::const_reference       const_reference;
	typedef typename Allocator::pointer               iterator;
	typedef typename Allocator::const_pointer         const_iterator;
	typedef typename Allocator::pointer               pointer;
	typedef typename Allocator::const_pointer         const_pointer;
	typedef std::reverse_iterator<iterator>           reverse_iterator;
	typedef std::reverse_iterator<const_iterator>     const_reverse_iterator;
	typedef          T                                value_type;
	typedef typename detail::if_then_else<
		sizeof(typename Allocator::size_type)<=sizeof(unsigned int),
		typename Allocator::size_type,
		unsigned int>::type                           size_type;
	typedef typename detail::if_then_else<
		sizeof(typename Allocator::difference_type)<=sizeof(int),
		typename Allocator::difference_type,
		int>::type                                    difference_type;
	// ctors
	//! constructs an empty pod_vector.
	/*!
	 * \post size() == capacity() == 0
	 */
	pod_vector() : ebo_(0, allocator_type()) { }

	//! constructs an empty pod_vector that uses a copy of a for memory allocations.
	/*!
	 * \post size() == capacity() == 0
	 */
	explicit pod_vector(const allocator_type& a) : ebo_(0, a) { }

	//! constructs a pod_vector containing n copies of value.
	/*!
	 * \post size() == n
	 */
	explicit pod_vector(size_type n, const T& value = T(), const allocator_type& a = allocator_type())
		: ebo_(n, a) {
		detail::fill(ebo_.buf, ebo_.buf + n, value);
		ebo_.size = n;
	}

	//! constructs a pod_vector equal to the range [first, last).
	/*!
	 * \post size() = distance between first and last.
	 */
	template <class Iter>
	pod_vector(Iter first, Iter last, const allocator_type& a = allocator_type(), typename detail::disable_if<detail::IterType<Iter>::num>::type* = 0)
		: ebo_(0, a) {
		insert_range(end(), first, last, typename std::iterator_traits<Iter>::iterator_category());
	}

	//! creates a copy of other
	/*!
	 * \post size() == other.size() && capacity() == other.size()
	 */
	pod_vector(const pod_vector& other) : ebo_(other.size(), other.get_allocator()) {
		std::memcpy(ebo_.buf, other.begin(), other.size()*sizeof(T));
		ebo_.size = other.size();
	}

	pod_vector& operator=(const pod_vector& other) {
		if (this != &other) {
			assign(other.begin(), other.end());
		}
		return *this;
	}

	//! frees all memory allocated by this pod_vector.
	/*!
	 * \note Won't call any destructors, because PODs don't have those.
	 */
	~pod_vector() { }

	/** @name inspectors
	 * inspector-functions
	 */
	//@{

	//! returns the number of elements currently stored in this pod_vector.
	size_type size()     const { return ebo_.size; }
	//! size of the largest possible pod_vector
	size_type max_size() const {
		typename allocator_type::size_type x = get_allocator().max_size();
		std::size_t                        y = size_type(-1)/sizeof(T);
		return static_cast<size_type>(std::min(std::size_t(x), y));
	}
	//! returns the total number of elements this pod_vector can hold without requiring reallocation.
	size_type capacity() const  { return ebo_.cap; }
	//! returns size() == 0
	bool empty() const { return ebo_.size == 0;  }

	const_iterator begin() const { return ebo_.buf; }
	const_iterator end()   const { return ebo_.buf+ebo_.size;}
	const_reverse_iterator rbegin() const { return const_reverse_iterator(end()); }
	const_reverse_iterator rend()   const { return const_reverse_iterator(begin()); }

	iterator       begin()  { return ebo_.buf; }
	iterator       end()    { return ebo_.buf+ebo_.size; }
	reverse_iterator rbegin() { return reverse_iterator(end()); }
	reverse_iterator rend()   { return reverse_iterator(begin()); }

	//! returns a copy of the allocator used by this pod_vector
	allocator_type get_allocator() const { return ebo_; }

	//@}
	/** @name elemacc
	 * element access
	 */
	//@{

	//! returns a reference to the element at position n
	/*!
	 * \pre n < size()
	 */
	reference operator[](size_type n) {
		assert(n < size());
		return ebo_.buf[n];
	}

	//! returns a reference-to-const to the element at position n
	/*!
	 * \pre n < size()
	 */
	const_reference operator[](size_type n) const {
		assert(n < size());
		return ebo_.buf[n];
	}

	//! same as operator[] but throws std::out_of_range if pre-condition is not met.
	const_reference at(size_type n) const {
		if (n < size()) return ebo_.buf[n];
		throw std::out_of_range("pod_vector::at");
	}
	//! same as operator[] but throws std::out_of_range if pre-condition is not met.
	reference at(size_type n) {
		if (n < size()) return ebo_.buf[n];
		throw std::out_of_range("pod_vector::at");
	}

	//! equivalent to *begin()
	reference front() { assert(!empty()); return *ebo_.buf; }
	//! equivalent to *begin()
	const_reference front() const { assert(!empty()); return *ebo_.buf; }

	//! equivalent to *--end()
	reference back() { assert(!empty()); return ebo_.buf[ebo_.size-1]; }

	//! equivalent to *--end()
	const_reference back() const { assert(!empty()); return ebo_.buf[ebo_.size-1]; }

	//@}
	/** @name mutators
	 * mutator functions
	 */
	//@{

	//! erases all elements in the range [begin(), end)
	/*!
	 * \post size() == 0
	 */
	void clear() { ebo_.size = 0; }

	void assign(size_type n, const T& val) {
		clear();
		insert(end(), n, val);
	}

	template <class Iter>
	void assign(Iter first, Iter last) {
		clear();
		insert(end(), first, last);
	}

	//! erases the element pointed to by pos.
	/*!
	 * \pre pos != end() && !empty()
	 * \return an iterator pointing to the element following pos (before that element was erased)
	 * of end() if no such element exists.
	 *
	 * \note invalidates all iterators and references referring to elements after pos.
	 */
	iterator erase(iterator pos) {
		assert(!empty() && pos != end());
		erase(pos, pos + 1);
		return pos;
	}

	//! erases the elements in the range [first, last)
	/*!
	 * \pre [first, last) must be a valid range.
	 */
	iterator erase(iterator first, iterator last) {
		if (end() - last > 0) {
			std::memmove(first, last, (end() - last) * sizeof(T));
		}
		ebo_.size -= static_cast<size_type>(last - first);
		return first;
	}

	//! adjusts the size of this pod_vector to ns.
	/*!
	 * resize is equivalent to:
	 * if ns > size insert(end(), ns - size(), val)
	 * if ns < size erase(begin() + ns, end())
	 *
	 * \post size() == ns
	 */
	void resize(size_type ns, const T& val = T()) {
		if (ns > size()) {
			ns <= capacity() ? detail::fill(end(), end()+(ns-size()), val) : append_realloc(ns-size(), val);
		}
		ebo_.size = ns;
	}

	//! reallocates storage if necessary but never changes the size() of this pod_vector.
	/*!
	 * \note if n is <= capacity() reserve is a noop. Otherwise a reallocation takes place
	 * and capacity() >= n after reserve returned.
	 * \note reallocation invalidates all references, pointers and iterators referring to
	 * elements in this pod_vectror.
	 *
	 * \note when reallocation occurs elements are copied from the old storage using memcpy.
	 */
	void reserve(size_type n) {
		if (n > capacity()) {
			T* temp = ebo_.allocate(n);
			std::memcpy(temp, ebo_.buf, size()*sizeof(T));
			ebo_.release();
			ebo_.buf = temp;
			ebo_.cap = n;
		}
	}

	void swap(pod_vector& other) {
		std::swap(ebo_.buf, other.ebo_.buf);
		std::swap(ebo_.size, other.ebo_.size);
		std::swap(ebo_.cap, other.ebo_.cap);
	}

	//! equivalent to insert(end(), x);
	void push_back(const T& x) {
		if (size() < capacity()) {
			new ((ebo_.buf+ebo_.size++)) T(x);
		}
		else {
			append_realloc(1, x);
		}
	}

	//! equivalent to erase(--end());
	/*!
	 * \pre !empty()
	 */
	void pop_back() {
		assert(!empty());
		--ebo_.size;
	}

	//! inserts a copy of val before pos.
	/*!
	 * \pre pos is a valid iterator.
	 * \return an iterator pointing to the copy of val that was inserted.
	 * \note if size() + 1 > capacity() reallocation occurs. Otherwise iterators and
	 * references referring to elements before pos remain valid.
	 *
	 */
	iterator insert(iterator pos, const T& val) {
		return insert(pos, (size_type)1, val);
	}

	//! inserts n copies of val before pos.
	/*!
	 * \pre pos is a valid iterator.
	 */
	iterator insert(iterator pos, size_type n, const T& val) {
		size_type off = static_cast<size_type>(pos-begin());
		insert_impl(pos, n, detail::Fill<T>(val));
		return ebo_.buf + off;
	}

	//! inserts copies of elements in the range [first, last) before pos.
	/*!
	 * \pre first and last are not iterators into this pod_vector.
	 * \pre pos is a valid iterator.
	 * \note if first and last are pointers, memcpy is used to insert the elements
	 * in the range [first, last) into this container.
	 *
	 */
	template <class Iter>
	void insert(iterator pos, Iter first, Iter last, typename detail::disable_if<detail::IterType<Iter>::num>::type* = 0) {
		insert_range(pos, first, last, typename std::iterator_traits<Iter>::iterator_category());
	}


	/** @name nonstd
	 * Non-standard interface
	 */
	//@{

	//! adjusts the size of this pod_vector to ns.
	/*!
	 * In contrast to pod_vector::resize this function does not
	 * initializes new elements in case ns > size().
	 * This reflects the behaviour of built-in arrays of pod-types.
	 * \note
	 *  Any access to an unitialized element is illegal unless it is accessed
	 *  in order to assign a new value.
	 */
	void resize_no_init(size_type ns) {
		reserve(ns);
		ebo_.size = ns;
	}
	//@}
private:
	size_type grow_size(size_type n) {
		size_type new_cap = size() + n;
		assert(new_cap > size() && "pod_vector: max size exceeded!");
		assert(new_cap > capacity());
		if (new_cap < 4) new_cap = 1 << (new_cap+1);
		size_type x = (capacity()*3)>>1;
		if (new_cap < x) new_cap = x;
		return new_cap;
	}
	void append_realloc(size_type n, const T& x) {
		size_type new_cap = grow_size(n);
		pointer temp      = ebo_.allocate(new_cap);
		std::memcpy(temp, ebo_.buf, size()*sizeof(T));
		detail::fill(temp+size(), temp+size()+n, x);
		ebo_.release();
		ebo_.buf  = temp;
		ebo_.cap  = new_cap;
		ebo_.size+= n;
	}
	void move_right(iterator pos, size_type n) {
		assert( (pos || n == 0) && (ebo_.eos() - pos) >= (int)n);
		std::memmove(pos + n, pos, (end() - pos) * sizeof(T));
	}
	template <class It>
	void insert_range(iterator pos, It first, It last,  std::random_access_iterator_tag,
		typename detail::disable_if<detail::same_type<pointer, It>::value == 0 && detail::same_type<const_pointer, It>::value == 0>::type* = 0) {
		assert( (first < begin() || first >= end()) && "pod_vec::insert(): Precondition violated!");
		typename allocator_type::difference_type diff = std::distance(first, last);
		assert(diff == 0 || (static_cast<size_type>(size()+diff) > size() && "pod_vector: max size exceeded!"));
		insert_impl(pos, static_cast<size_type>(diff), detail::Memcpy<T>(first));
	}
	template <class It>
	void insert_range(iterator pos, It first, It last,  std::forward_iterator_tag) {
		typename allocator_type::difference_type diff = std::distance(first, last);
		assert(diff == 0 || (static_cast<size_type>(size()+diff) > size() && "pod_vector: max size exceeded!"));
		insert_impl(pos, static_cast<size_type>(diff), detail::Copy<It>(first, last));
	}
	template <class Iter>
	void insert_range(iterator pos, Iter first, Iter last, std::input_iterator_tag) {
		pod_vector<T> temp;
		while (first != last) temp.push_back(*first++);
		insert(pos, temp.begin(), temp.end());
	}

	// NOTE: template parameter ST should always equal size_type
	// and is only needed to workaround an internal compiler error
	// in gcc 3.4.3
	template <class ST, class P>
	void insert_impl(iterator pos, ST n, const P& pred) {
		assert(n == 0 || (size()+n) > size() );
		if (size()+n <= capacity()) {
			move_right(pos, n);
			pred(pos, n);
			ebo_.size += n;
		}
		else {
			size_type new_cap = grow_size(n);
			pointer temp      = ebo_.allocate(new_cap);
			size_type prefix  = static_cast<size_type>(pos-begin());
			// copy prefix
			std::memcpy(temp, begin(), prefix*sizeof(T));
			// insert new stuff
			pred(temp+prefix, n);
			// copy suffix
			std::memcpy(temp+prefix+n, pos, (end()-pos)*sizeof(T));
			ebo_.release();
			ebo_.buf  = temp;
			ebo_.size+= n;
			ebo_.cap  = new_cap;
		}
	}
	struct ebo : public Allocator { // empty-base-optimization
		typedef typename this_type::size_type size_type;
		typedef typename this_type::allocator_type A;
		pointer    buf;  // pointer to array
		size_type  size; // current size (used elements)
		size_type  cap;  // max size before regrow
		ebo(size_type n, const Allocator& a) : Allocator(a), buf(0), size(0), cap(n) {
			if (n > 0) {  buf = A::allocate(n); }
		}
		~ebo()            { release(); }
		void release()    { if (buf) A::deallocate(buf, cap); }
		T*   eos() const  { return buf + cap; }
	}      ebo_;
};

template <class T, class A>
inline bool operator==(const pod_vector<T, A>& lhs, const pod_vector<T, A>& rhs) {
	return lhs.size() == rhs.size()
		&& std::equal(lhs.begin(), lhs.end(), rhs.begin());
}

template <class T, class A>
inline bool operator!=(const pod_vector<T, A>& lhs, const pod_vector<T, A>& rhs) {
	return ! (lhs == rhs);
}

template <class T, class A>
inline bool operator<(const pod_vector<T, A>& lhs, const pod_vector<T, A>& rhs) {
	return std::lexicographical_compare(lhs.begin(), lhs.end(), rhs.begin(), rhs.end());
}

template <class T, class A>
inline bool operator>(const pod_vector<T, A>& lhs, const pod_vector<T, A>& rhs) {
	return rhs < lhs;
}

template <class T, class A>
inline bool operator<=(const pod_vector<T, A>& lhs, const pod_vector<T, A>& rhs) {
	return !(rhs < lhs);
}

template <class T, class A>
inline bool operator>=(const pod_vector<T, A>& lhs, const pod_vector<T, A>& rhs) {
	return !(lhs < rhs);
}

template <class T, class A>
inline void swap(pod_vector<T, A>& lhs, pod_vector<T, A>& rhs) {
	lhs.swap(rhs);
}

}

#endif

