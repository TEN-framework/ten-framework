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
#ifndef PROGRAM_OPTIONS_VALUE_STORE_H_INCLUDED
#define PROGRAM_OPTIONS_VALUE_STORE_H_INCLUDED
#include <typeinfo>
#include <new>
namespace Potassco { namespace ProgramOptions { namespace detail {
typedef void (*vcall_type)(const void* in, void** out);
typedef vcall_type vtable_type[4];
typedef vtable_type* vptr_type;
// workaround: some compilers don't support
// out-of-class definition of member templates.
template <class T>
inline vptr_type vtable(const T* = 0);
template <class T>
inline vptr_type base_vtable(const T* = 0);
} // namespace detail

//! A type that can hold any kind of value type
class ValueStore {
public:
	//! creates an empty object
	ValueStore();
	//! copies the value in other
	ValueStore(const ValueStore& other);
	//! stores a copy of obj
	template <class T>
	ValueStore(const T& obj)
		: vptr_(detail::vtable(static_cast<const T*>(0)))
		, value_(0) {
		clone(&obj, &value_);
	}
	//! releases any stored value
	~ValueStore();
	//! stores a copy of other releasing any previous value
	ValueStore& operator=(ValueStore other);
	//! stores a copy of obj releasing any previous value
	template <class T>
	ValueStore& operator=(const T& obj) {
		ValueStore(obj).swap(*this);
		return *this;
	}
	//! stores obj in this and takes over ownership of obj
	/*!
	 * \pre delete obj is valid
	 */
	template <class T>
	ValueStore& assimilate(T* obj) {
		clear();
		vptr_ = detail::base_vtable(static_cast<const T*>(0));
		value_= obj;
		return *this;
	}
	//! swaps this with other
	void   swap(ValueStore& other);
	//! Returns true if holder does not contain a value.
	bool   empty()       const { return vptr_ == 0; }
	//! Returns the type of the stored value.
	const  std::type_info&
		     type()        const;
	//! destroys and releases any stored value
	void   clear();
	//! surrenders any stored value without destroying it
	void   surrender();

	void* extract_raw() const {
		return !empty()
			? extract(const_cast<void**>(&value_))
			: 0;
	}
private:
	enum { call_extract = 0, vcall_clone = 1, vcall_destroy = 2, vcall_typeid = 3 };
	typedef detail::vptr_type vptr_type;
	void clone(const void* obj, void** out) const;
	void* extract(void** o) const;
	vptr_type vptr_;
	void*     value_;
};

struct bad_value_cast : std::bad_cast {
	const char * what() const throw() {
		return "value_cast: invalid conversion on ValueStore";
	}
};

//! Extracts a typed value from a ValueStore.
/*!
 * \throw bad_value_cast if value is not of type T
 */
template <class T>
const T& value_cast(const ValueStore& v, const T* = 0) {
	if (v.type() == typeid(T)) {
		return *static_cast<const T*>(const_cast<const void*>(v.extract_raw()));
	}
	throw bad_value_cast();
}
//! Returns 0 on error
template <class T>
const T* value_cast(const ValueStore* v, const T* = 0) {
	if (v->type() == typeid(T)) {
		return static_cast<const T*>(const_cast<const void*>(v->extract_raw()));
	}
	return 0;
}
/*!
 * \overload
 */
template <class T>
T&       value_cast(ValueStore& v, const T* p = 0) {
	return const_cast<T&>(value_cast(const_cast<const ValueStore&>(v), p));
}
template <class T>
T*       value_cast(ValueStore* v, const T* p = 0) {
	return const_cast<T*>(value_cast(const_cast<const ValueStore*>(v), p));
}

//! Extracts a typed value from a ValueStore *without* checking if the type matches
template <class T>
const T* unsafe_value_cast(const ValueStore* v, const T* = 0) {
	return static_cast<const T*>(const_cast<const void*>(v->extract_raw()));
}
/*!
 * \overload
 */
template <class T>
T*       unsafe_value_cast(ValueStore* v, const T* p = 0) {
	return const_cast<T*>(unsafe_value_cast(const_cast<const ValueStore*>(v), p));
}

#include "detail/value_store.h"

} // namespace ProgramOptions
} // namespace Potassco
#endif
