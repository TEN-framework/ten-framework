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
#include <potassco/program_opts/value_store.h>
#include <algorithm>
namespace Potassco { namespace ProgramOptions {

ValueStore::ValueStore()
	: vptr_(0)
	, value_(0) {
}
ValueStore::ValueStore(const ValueStore& other)
	: vptr_(other.vptr_)
	, value_(0) {
	if (!other.empty()) {
		clone(extract(const_cast<void**>(&other.value_)), &value_);
	}
}
ValueStore::~ValueStore() {
	clear();
}
ValueStore& ValueStore::operator=(ValueStore other) {
	other.swap(*this);
	return *this;
}
void ValueStore::swap(ValueStore& other) {
	std::swap(vptr_ , other.vptr_);
	std::swap(value_, other.value_);
}

const  std::type_info& ValueStore::type() const {
	if (!empty()) {
		void* x;
		(*vptr_)[vcall_typeid](0, &x);
		return *static_cast<const std::type_info*>( x );
	}
	struct internal_empty_type {};
	return typeid(internal_empty_type);
}

void ValueStore::clear() {
	if (!empty()) {
		(*vptr_)[vcall_destroy](extract(&value_), &value_);
		vptr_  = 0;
	}
}

void ValueStore::surrender() {
	vptr_ = 0;
}

void ValueStore::clone(const void* obj, void** out) const {
	(*vptr_)[vcall_clone](obj, out);
}

void* ValueStore::extract(void** v) const {
	if ((*vptr_)[call_extract] == 0) {
		return *v;
	}
	return reinterpret_cast<void*>(v);
}
} // namespace ProgramOptions
} // namespace Potassco

