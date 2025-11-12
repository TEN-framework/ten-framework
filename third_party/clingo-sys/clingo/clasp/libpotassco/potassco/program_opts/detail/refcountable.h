//
// Copyright (c) 2010-2017 Benjamin Kaufmann
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
//
// NOTE: ProgramOptions is inspired by Boost.Program_options
//       see: www.boost.org/libs/program_options
//
#ifndef PROGRAM_OPTIONS_REFCOUNTABLE_H_INCLUDED
#define PROGRAM_OPTIONS_REFCOUNTABLE_H_INCLUDED
namespace Potassco { namespace ProgramOptions { namespace detail {

class RefCountable {
public:
	RefCountable() : refCount_(1) {}
	int  addRef() { return ++refCount_; }
	int  release() { return --refCount_; }
	int  refCount() const { return refCount_; }
private:
	int refCount_;
};

template <class T>
class IntrusiveSharedPtr {
public:
	typedef T element_type;
	explicit IntrusiveSharedPtr(T* p = 0) throw()
		: ptr_(p) { /* NO add ref */
	}
	IntrusiveSharedPtr(const IntrusiveSharedPtr& o) throw()
		: ptr_(o.ptr_) {
		addRef();
	}
	~IntrusiveSharedPtr() throw () { release(); }
	IntrusiveSharedPtr& operator=(const IntrusiveSharedPtr& other) {
		other.addRef();
		this->release();
		this->ptr_ = other.ptr_;
		return *this;
	}
	T&     operator*() const throw() { return *ptr_; }
	T*     operator->() const throw() { return ptr_; }
	T*     get() const throw() { return ptr_; }
	void   reset() throw() { release(); ptr_ = 0; }
	bool   unique() const throw() { return  !ptr_ || ptr_->refCount() == 1; }
	int    count() const  throw() { return ptr_ ? ptr_->refCount() : 0; }
	void   swap(IntrusiveSharedPtr& b) {
		T* temp = ptr_;
		ptr_ = b.ptr_;
		b.ptr_ = temp;
	}
private:
	T* ptr_;
	void addRef()  const { if (ptr_) ptr_->addRef(); }
	void release() const {
		if (ptr_ && ptr_->release() == 0) {
			delete ptr_;
		}
	}
};

} // namespace detail
} // namespace ProgramOptions
} // namespace Potassco
#endif
