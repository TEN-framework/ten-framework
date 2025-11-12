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
#ifndef BK_LIB_INDEXED_PRIORITY_QUEUE_H_INCLUDED
#define BK_LIB_INDEXED_PRIORITY_QUEUE_H_INCLUDED

#ifdef _MSC_VER
#pragma warning (disable : 4267)
#pragma warning (disable : 4244)
#pragma once
#endif

#include <cstddef>
#include "pod_vector.h"
namespace bk_lib { namespace detail {

typedef std::size_t key_type;
const key_type noKey = static_cast<key_type>(-1);
inline key_type heap_root() { return 0; }
inline key_type heap_left(std::size_t i) { return (i<<1)+1; }
inline key_type heap_right(std::size_t i) { return (i+1)<<1; }
inline key_type heap_parent(std::size_t i) { return (i-1)>>1; }

}

// Note: Uses a Max-Heap!
template <
	class Cmp // sort-predicate - if Cmp(k1, k2) == true, n1 has higher priority than n2
>
class indexed_priority_queue {
public:
	typedef detail::key_type key_type;
	typedef pod_vector<key_type> index_container_type;
	typedef std::size_t     size_type;
	typedef Cmp             compare_type;

	explicit indexed_priority_queue( const compare_type& c = compare_type() );
	indexed_priority_queue(const indexed_priority_queue& other);

	indexed_priority_queue& operator=(const indexed_priority_queue& other) {
		indices_  = other.indices_;
		heap_     = other.heap_;
		compare_  = other.compare_;
		return *this;
	}

	const compare_type& key_compare() const {
		return compare_;
	}

	bool empty() const {
		return heap_.empty();
	}
	void reserve(size_type n) {
		indices_.reserve(n);
	}

	void push(key_type k) {
		assert( !is_in_queue(k) );
		if ((key_type)indices_.size() <= k) {
			if (indices_.capacity() <= k) { indices_.reserve(((k+1)*3)>>1); }
			indices_.resize(k+1, detail::noKey);
		}
		indices_[k] = (key_type)heap_.size();
		heap_.push_back(k);
		siftup(indices_[k]);
	}

	void pop() {
		assert(!empty());
		key_type x  = heap_[0];
		heap_[0]    = heap_.back();
		indices_[heap_[0]] = 0;
		indices_[x]   = detail::noKey;
		heap_.pop_back();
		if (heap_.size() > 1) {siftdown(0);}
	}

	void clear() {
		heap_.clear();
		indices_.clear();
	}

	template <class C>
	void swapMem(indexed_priority_queue<C>& o) {
		clear();
		o.clear();
		heap_.swap(o.heap_);
		indices_.swap(o.indices_);
	}
	size_type size( ) const {
		return heap_.size();
	}

	key_type top() const {
		assert(!empty());
		return heap_[0];
	}

	void update(key_type k) {
		if (!is_in_queue(k)) {
			push(k);
		}
		else {
			siftup(indices_[k]);
			siftdown(indices_[k]);
		}
	}
	// call if priority of k has increased
	void increase(key_type k) {
		assert(is_in_queue(k));
		siftup(indices_[k]);
	}
	// call if priority of k has decreased
	void decrease(key_type k) {
		assert(is_in_queue(k));
		siftdown(indices_[k]);
	}

	bool is_in_queue(key_type k) const {
		assert(valid_key(k));
		return k < (key_type)indices_.size() && indices_[k] != detail::noKey;
	}

	void remove(key_type k) {
		if (is_in_queue(k)) {
			key_type kInHeap  = indices_[k];
			heap_[kInHeap]    = heap_.back();
			indices_[heap_.back()]  = kInHeap;
			heap_.pop_back();
			indices_[k] = detail::noKey;
			if (heap_.size() > 1 && kInHeap != (key_type)heap_.size()) {
				siftup(kInHeap);
				siftdown(kInHeap);
			}
		}
	}
private:
	template <class X>
	friend class indexed_priority_queue;
	bool valid_key(key_type k) const {
		return k != detail::noKey;
	}
	index_container_type  indices_;
	index_container_type  heap_;
	compare_type          compare_;
	void siftup(key_type n) {
		using namespace detail;
		key_type x = heap_[n];
		key_type p = heap_parent(n);
		while (n != 0 && compare_(x, heap_[p])){
			heap_[n] = heap_[p];
			indices_[heap_[n]] = n;
			n = p;
			p = heap_parent(n);
		}
		heap_[n] = x;
		indices_[x] = n;
	}

	void siftdown(key_type n) {
		using namespace detail;
		key_type x = heap_[n];
		while (heap_left(n) < (key_type)heap_.size()){
			key_type child = smaller_child(n);
			if (!compare_(heap_[child], x)) {
				break;
			}
			heap_[n] = heap_[child];
			indices_[heap_[n]] = n;
			n = child;
		}
		heap_[n] = x;
		indices_[x] = n;
	}

	key_type smaller_child(size_type n) const {
		using namespace detail;
		return heap_right(n) < (key_type)heap_.size() && compare_(heap_[heap_right(n)], heap_[heap_left(n)])
			? heap_right(n)
			: heap_left(n);
	}
};

template <class C>
indexed_priority_queue<C>::indexed_priority_queue( const compare_type& c )
	: indices_()
	, heap_()
	, compare_(c) {
}

template <class C>
indexed_priority_queue<C>::indexed_priority_queue(const indexed_priority_queue& other)
	: indices_(other.indices_)
	, heap_(other.heap_)
	, compare_(other.compare_) {
}

}
#endif
