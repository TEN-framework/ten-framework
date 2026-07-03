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
#ifndef PROGRAM_OPTIONS_MAPPED_VALUE_H_INCLUDED
#define PROGRAM_OPTIONS_MAPPED_VALUE_H_INCLUDED
#ifdef _MSC_VER
#pragma warning (disable : 4786)
#pragma warning (disable : 4503)
#endif
#include <potassco/program_opts/typed_value.h>
#include <potassco/program_opts/value_store.h>
#include <string>
#include <cstddef>
#include <map>
#if defined(_MSC_VER) && _MSC_VER <= 1200
namespace std { using ::size_t; }
#endif

namespace Potassco { namespace ProgramOptions {
///////////////////////////////////////////////////////////////////////////////
// ValueMap
///////////////////////////////////////////////////////////////////////////////
//! Type for storing anonymous values.
/*!
 * Maps option names to their values.
 */
class ValueMap {
public:
	ValueMap() {}
	~ValueMap() {}
	bool               empty() const { return map_.empty(); }
	size_t             size()  const { return map_.size(); }
	size_t             count(const std::string& name) const { return map_.count(name); }
	void               clear() { map_.clear(); }
	const ValueStore& operator[](const std::string& name) const {
		MapType::const_iterator it = map_.find(name);
		if (it == map_.end()) {
			throw UnknownOption("ValueMap", name);
		}
		return it->second;
	}
	template <class T>
	static bool add(ValueMap* this_, const std::string& name, const T* value) {
		MapType::iterator it = this_->map_.find(name);
		if (it == this_->map_.end()) {
			it = this_->map_.insert(it, MapType::value_type(name, ValueStore()));
		}
		if (it->second.extract_raw() != value) {
			it->second.assimilate(const_cast<T*>(value));
		}
		return true;
	}
private:
	ValueMap(const ValueMap&);
	ValueMap& operator=(const ValueMap&);
	typedef std::map<std::string, ValueStore> MapType;
	MapType map_;
};

/*!
 * Creates a value that is created on demand and stored in a given value map.
 *
 * \see OptionGroup::addOptions()
 */
template <class T>
inline NotifiedValue<T>* store(ValueMap& map, typename detail::Parser<T>::type p = &string_cast<T>) {
	return notify<T>(&map, &ValueMap::add<T>, p);
}

inline NotifiedValue<bool>* flag(ValueMap& map, FlagAction a = store_true) {
	return flag(&map, &ValueMap::add<bool>, a);
}

}}
#endif
