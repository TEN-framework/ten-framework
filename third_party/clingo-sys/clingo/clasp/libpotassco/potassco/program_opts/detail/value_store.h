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
#ifndef PROGRAM_OPTIONS_VALUE_STORE_IMPL_H_INCLUDED
#define PROGRAM_OPTIONS_VALUE_STORE_IMPL_H_INCLUDED

namespace detail {
template <class T>
struct VTable {
	static void clone(const void* o, void** out) {
		*out = new T(*static_cast<const T*>(o));
	}
	static void destroy(const void* o, void** out) {
		delete static_cast<const T*>(o);
		*out = 0;
	}
	static void typeinfo(const void*, void** out)  {
		*out = const_cast<void*>( static_cast<const void*>(&typeid(T)) );
	}
	static vtable_type vtable_s;
};
template <class T>
struct OptVTable {
	static void clone(const void* o, void** out) {
		new (&*out) T(*static_cast<const T*>(o));
	}
	static void destroy(const void* o, void** out) {
		static_cast<const T*>(o)->~T();
		*out = 0;
	}
	static vtable_type vtable_s;
};
template <class T>
vtable_type VTable<T>::vtable_s = {
	  0
	, &VTable<T>::clone
	, &VTable<T>::destroy
	, &VTable<T>::typeinfo
};
template <class T>
vtable_type OptVTable<T>::vtable_s = {
	(vcall_type)0x1
	, &OptVTable<T>::clone
	, &OptVTable<T>::destroy
	, &VTable<T>::typeinfo
};
template <bool> struct bool2type {};
template <class T>
inline vptr_type vtable_select(bool2type<0>, const T* = 0) {
	return &VTable<T>::vtable_s;
}
template <class T>
inline vptr_type vtable_select(bool2type<1>, const T* = 0) {
	return &OptVTable<T>::vtable_s;
}
template <class T>
inline vptr_type vtable(const T* x) {
	return vtable_select(bool2type<sizeof(T)<=sizeof(void*)>(), x);
}

template <class T>
inline vptr_type base_vtable(const T* x) {
	return vtable_select(bool2type<0>(), x);
}

} // namespace detail

#endif
