//
// Copyright (c) 2010-2017 Benjamin Kaufmann
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
#ifndef BK_LIB_LEFT_RIGHT_SEQUENCE_INCLUDED
#define BK_LIB_LEFT_RIGHT_SEQUENCE_INCLUDED
#ifdef _MSC_VER
#pragma warning( push )
#pragma warning (disable : 4200)
#endif
#if defined(__GNUC__) && __GNUC__ >= 8
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wclass-memaccess"
#endif

#include "type_manip.h"
#include <iterator>
#include <cassert>
#include <cstring>
namespace bk_lib { namespace detail {

// base class for left_right_sequence
// see below
template <class L, class R>
class left_right_rep {
public:
	typedef L left_type;
	typedef R right_type;
	typedef unsigned int size_type;
	typedef L*       left_iterator;
	typedef const L* const_left_iterator;
	typedef std::reverse_iterator<R*>       right_iterator;
	typedef std::reverse_iterator<const R*> const_right_iterator;

	typedef typename bk_lib::detail::align_of<left_type>::type  left_align_type;
	typedef typename bk_lib::detail::align_of<right_type>::type right_align_type;
///@cond
	typedef typename bk_lib::detail::if_then_else<
		sizeof(left_type) >= sizeof(right_type),
		left_type,
		right_type>::type max_type;
	typedef typename bk_lib::detail::if_then_else<
		sizeof(left_align_type) >= sizeof(right_align_type),
		left_align_type,
		right_align_type>::type align_type;
///@endcond
	left_right_rep() : buf_(0), cap_(0), free_(0), left_(0), right_(0) {}

	bool      empty()         const { return left_ == 0 && right_ == cap_; }
	size_type left_size()     const { return left_/sizeof(left_type); }
	size_type right_size()    const { return (cap_-right_)/sizeof(right_type); }
	size_type size()          const { return left_size() + right_size(); }
	size_type left_capacity() const { return (cap_ / sizeof(left_type)); }
	size_type right_capacity()const { return (cap_ / sizeof(right_type)); }
	const_left_iterator  left_begin() const { return const_left_iterator(reinterpret_cast<const left_type*>(begin())); }
	const_left_iterator  left_end()   const { return const_left_iterator(reinterpret_cast<const left_type*>(buf_+left_)); }
	      left_iterator  left_begin()       { return left_iterator(reinterpret_cast<left_type*>(begin())); }
	      left_iterator  left_end()         { return left_iterator(reinterpret_cast<left_type*>(buf_+left_)); }
	const_right_iterator right_begin()const { return const_right_iterator(reinterpret_cast<const right_type*>(end())); }
	const_right_iterator right_end()  const { return const_right_iterator(reinterpret_cast<const right_type*>(buf_+right_)); }
	      right_iterator right_begin()      { return right_iterator(reinterpret_cast<right_type*>(end())); }
	      right_iterator right_end()        { return right_iterator(reinterpret_cast<right_type*>(buf_+right_)); }
	const left_type&     left(size_type i)const { return *(left_begin()+i); }
	      left_type&     left(size_type i)      { return *(left_begin()+i); }

	void clear(bool releaseMem = false) {
		if (releaseMem) {
			release();
			buf_  = 0;
			cap_  = 0;
			free_ = 0;
		}
		left_ = 0;
		right_= cap_;
	}
	void push_left(const left_type& x) {
		if ((left_ + sizeof(left_type)) > right_) {
			realloc();
		}
		new (left())left_type(x);
		left_ += sizeof(left_type);
	}

	void push_right(const right_type& x) {
		if ( (left_ + sizeof(right_type)) > right_ ) {
			realloc();
		}
		right_ -= sizeof(right_type);
		new (right()) right_type(x);
	}
	void pop_left() {
		assert(left_size() != 0);
		left_ -= sizeof(left_type);
	}
	void pop_right() {
		assert(right_size() != 0);
		right_ += sizeof(right_type);
	}

	void erase_left(left_iterator it) {
		if (it != left_end()) {
			left_iterator x = it++;
			std::memmove(x, it, (left_end()-it)*sizeof(left_type));
			left_ -= sizeof(left_type);
		}
	}
	void erase_left_unordered(left_iterator it) {
		if (it != left_end()) {
			left_ -= sizeof(left_type);
			*it    = *reinterpret_cast<left_type*>(left());
		}
	}
	void erase_right(right_iterator it) {
		if (it != right_end()) {
			right_type* r = (++it).base();
			right_type* b = reinterpret_cast<right_type*>(right());
			assert(r >= b);
			std::memmove(b+1, b, (r-b)*sizeof(right_type));
			right_ += sizeof(right_type);
		}
	}
	void erase_right_unordered(right_iterator it) {
		if (it != right_end()) {
			*it    = *reinterpret_cast<right_type*>(right());
			right_+= sizeof(right_type);
		}
	}
	void shrink_left(left_iterator it) {
		left_ = static_cast<size_type>((it - left_begin())*sizeof(left_type));
	}
	void shrink_right(right_iterator it) {
		buf_type* x = reinterpret_cast<buf_type*>(it.base());
		right_ = static_cast<size_type>(x - begin());
	}
	enum { block_size      = ((sizeof(max_type)+(sizeof(align_type)-1)) / sizeof(align_type)) * sizeof(align_type) };
protected:
	left_right_rep(const left_right_rep&) { }
	left_right_rep& operator=(const left_right_rep&) { return *this; }
	typedef unsigned char buf_type;
	buf_type*       begin()      { return buf_; }
	const buf_type* begin()const { return buf_; }
	buf_type*       end()        { return buf_+cap_; }
	const buf_type* end()  const { return buf_+cap_; }
	buf_type*       left()       { return buf_+left_; }
	buf_type*       right()      { return buf_+right_; }
	size_type    capacity()const { return cap_ / block_size; }
	size_type    raw_size()const { return left_ + (cap_-right_); }
	void release()               { if (free_ != 0) { ::operator delete(buf_); } }
	void realloc();
	buf_type* buf_;
	size_type cap_ : 31;
	size_type free_:  1;
	size_type left_;
	size_type right_;
};

template <class L, class R>
void left_right_rep<L, R>::realloc() {
	size_type new_cap = ((capacity()*3)>>1) * block_size;
	size_type min_cap = 4 * block_size;
	if (new_cap < min_cap) new_cap = min_cap;
	buf_type* temp = (buf_type*)::operator new(new_cap*sizeof(buf_type));
	// copy left
	std::memcpy(temp, begin(), left_size()*sizeof(L));
	// copy right
	size_type r = cap_ - right_;
	std::memcpy(temp+(new_cap-r), right(), right_size() * sizeof(R));
	// swap
	release();
	buf_   = temp;
	cap_   = new_cap;
	free_  = 1;
	right_ = new_cap - r;
}

// always store sequence in heap-allocated buffer
template <class L, class R>
struct no_inline_buffer : public left_right_rep<L, R> {
	typedef typename left_right_rep<L, R>::buf_type buf_type;
	enum { inline_raw_cap = 0 };
	buf_type* extra()           { return 0; }
};

// store small sequences directly inside of object
template <class L, class R, unsigned cap>
struct with_inline_buffer : public left_right_rep<L, R> {
	typedef typename left_right_rep<L, R>::buf_type   buf_type;
	typedef typename left_right_rep<L, R>::align_type align_type;
	enum { inline_raw_cap = cap };
	buf_type* extra()           { return rep_.mem; }
	union X {
		align_type align;
		buf_type   mem[inline_raw_cap];
	} rep_;
};

// select proper base class for left_right_sequence based
// on parameter i
template <class L, class R, unsigned i>
struct select_base {
private:
	typedef unsigned char        buf_type;
	typedef left_right_rep<L, R> base_type;
	typedef typename base_type::size_type size_type;
	typedef typename base_type::align_type align_type;
	typedef no_inline_buffer<L, R> no_extra_type;
	typedef with_inline_buffer<L, R, sizeof(align_type)> with_extra_type;

	enum { padding         = sizeof(with_extra_type) - (sizeof(no_extra_type)+sizeof(align_type)) };
	enum { size_with_pad   = sizeof(no_extra_type) + padding };
	enum { store_extra     = (i > size_with_pad) && (i - size_with_pad) >= base_type::block_size };
	enum { inline_raw_cap  = store_extra ? ((i - size_with_pad)/base_type::block_size)*base_type::block_size : 0 };
public:
	typedef typename if_then_else<
		store_extra!=0,
		with_inline_buffer<L, R, inline_raw_cap>,
		no_inline_buffer<L, R> >::type type;
};


} // bk_lib::detail

//! Stores two sequences in one contiguous memory block
/*!
 * The left sequence grows from left to right, while the
 * right sequence grows from right to left. On overlap, the
 * memory block is automatically extended.
 *
 * \param L value type of left sequence
 * \param R value type of right sequence
 * \param i max size on stack
 * \pre L and R can be copied with memcpy (i.e. have trivial copy constructor and trivial destructor)
 */
template <class L, class R, unsigned i>
class left_right_sequence : public bk_lib::detail::select_base<L, R, i>::type {
public:
	typedef typename bk_lib::detail::select_base<L, R, i>::type base_type;
	typedef typename base_type::left_type  left_type;
	typedef typename base_type::right_type right_type;
	typedef typename base_type::size_type  size_type;
	typedef typename base_type::align_type align_type;
	typedef typename base_type::max_type   max_type;
	typedef typename base_type::buf_type   buf_type;

	typedef typename base_type::left_iterator       left_iterator;
	typedef typename base_type::const_left_iterator const_left_iterator;
	typedef typename base_type::right_iterator       right_iterator;
	typedef typename base_type::const_right_iterator const_right_iterator;

	left_right_sequence() {
		this->buf_  = this->extra();
		this->cap_  = base_type::inline_raw_cap;
		this->free_ = 0;
		this->left_ = 0;
		this->right_= base_type::inline_raw_cap;
	}
	left_right_sequence(const left_right_sequence& other);
	~left_right_sequence() { this->release(); }
	left_right_sequence& operator=(const left_right_sequence&);

	void try_shrink() {
		if (this->raw_size() <= base_type::inline_raw_cap && this->buf_ != this->extra()) {
			buf_type* e = this->extra();
			size_type c = base_type::inline_raw_cap;
			size_type r = c - (this->right_size()*sizeof(right_type));
			std::memcpy(e,   this->begin(), this->left_size() * sizeof(left_type));
			std::memcpy(e+r, this->right(), this->right_size()* sizeof(right_type));
			this->release();
			this->buf_  = e;
			this->cap_  = c;
			this->free_ = 0;
			this->right_= r;
		}
	}
	void move(left_right_sequence& other) {
		this->clear(true);
		if (other.raw_size() <= base_type::inline_raw_cap) {
			copy(other);
			other.clear(true);
		}
		else {
			this->buf_   = other.buf_;
			this->cap_   = other.cap_;
			this->free_  = other.free_;
			this->left_  = other.left_;
			this->right_ = other.right_;
			other.buf_  = other.extra();
			other.cap_  = base_type::inline_raw_cap;
			other.free_ = 0;
			other.left_ = 0;
			other.right_= base_type::inline_raw_cap;
		}
	}
private:
	void      copy(const left_right_sequence&);
};

template <class L, class R, unsigned i>
void left_right_sequence<L, R, i>::copy(const left_right_sequence& other) {
	size_type os = other.raw_size();
	if ( os <= base_type::inline_raw_cap ) {
		this->buf_ = this->extra();
		this->cap_ = base_type::inline_raw_cap;
		this->free_= 0;
	}
	else {
		os         = ((os + (base_type::block_size-1)) / base_type::block_size) * base_type::block_size;
		this->buf_ = (buf_type*)::operator new(os*sizeof(buf_type));
		this->cap_ = os;
		this->free_= 1;
	}
	this->left_ = other.left_;
	this->right_= this->cap_ - (other.right_size()*sizeof(right_type));
	std::memcpy(this->begin(), other.begin(), other.left_size()*sizeof(left_type));
	std::memcpy(this->right(), const_cast<left_right_sequence&>(other).right(), other.right_size()*sizeof(right_type));
}

template <class L, class R, unsigned i>
left_right_sequence<L, R, i>::left_right_sequence(const left_right_sequence& other) : base_type(other) {
	copy(other);
}

template <class L, class R, unsigned i>
left_right_sequence<L, R, i>& left_right_sequence<L, R, i>::operator=(const left_right_sequence& other) {
	if (this != &other) {
		this->release();
		copy(other);
	}
	return *this;
}


} // namespace bk_lib


#ifdef _MSC_VER
#pragma warning(pop)
#endif
#if defined(__GNUC__) && __GNUC__ >= 8
#pragma GCC diagnostic pop
#endif

#endif

