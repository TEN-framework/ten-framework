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
#ifndef PROGRAM_OPTIONS_NOTIFIER_H_INCLUDED
#define PROGRAM_OPTIONS_NOTIFIER_H_INCLUDED

namespace Potassco { namespace ProgramOptions { namespace detail {

///////////////////////////////////////////////////////////////////////////////
// Notifier
///////////////////////////////////////////////////////////////////////////////
// HACK:
// This should actually be replaced with a proper and typesafe delegate class.
// At least, it should be parametrized on the actual parameter type since
// the invocation of f via the generic func is not strictly conforming.
// Yet, it should work on all major compilers - we do not mess with
// the address and the alignment of void* should be compatible with that of T*.
template <class ParamT>
struct Notifier {
	typedef bool (*notify_func_type)(void*, const std::string& name, ParamT);
	Notifier() : obj(0), func(0) {}
	template <class O>
	Notifier(O* o, bool (*f)(O*, const std::string&, ParamT)) {
		obj = o;
		func= reinterpret_cast<notify_func_type>(f);
	}
	void*            obj;
	notify_func_type func;
	bool notify(const std::string& name, ParamT val) const {
		return func(obj, name, val);
	}
	bool empty() const { return func == 0; }
};

template <class ParamT, class ObjT>
struct Notify {
	typedef bool (*type)(ObjT*, const std::string& name, ParamT);
};

} // namespace detail
} // namespace ProgramOptions
} // namespace Potassco
#endif
