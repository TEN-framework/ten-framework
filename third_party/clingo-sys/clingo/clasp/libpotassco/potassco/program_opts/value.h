//
// Copyright (c) 2004-2017 Benjamin Kaufmann
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
#ifndef PROGRAM_OPTIONS_VALUE_H_INCLUDED
#define PROGRAM_OPTIONS_VALUE_H_INCLUDED
#ifdef _MSC_VER
#pragma warning (disable : 4786)
#pragma warning (disable : 4503)
#endif
#include <string>
#include <typeinfo>
#include <cstddef>
#if defined(_MSC_VER) && _MSC_VER <= 1200
namespace std { using ::size_t; }
#endif

namespace Potassco { namespace ProgramOptions { namespace detail {
template <class T>
struct Owned {
	~Owned() { delete obj; }
	T* obj;
};
} // namespace detail
enum DescriptionLevel {
	desc_level_default = 0, /**< Always shown in description */
	desc_level_e1 = 1,
	desc_level_e2 = 2,
	desc_level_e3 = 3,
	desc_level_all = 4,
	desc_level_hidden = 5 /**< Never shown in description */
};

//! Manages the value of an option and defines how it is parsed from a string.
/*!
 * The library maintains a 1:1-relationship between options and their values.
 * That is, an option has exactly one value and a value has exactly one
 * state w.r.t its option.
 */
class Value {
public:
	//! Possible (tentative) states of an option value.
	enum State {
		value_unassigned = 0, /**< no value assigned */
		value_defaulted = 1, /**< a default value is assigned */
		value_fixed = 2  /**< a parsed value is assigned */
	};
	//! Possible value descriptions.
	enum DescType {
		desc_name = 1
		, desc_default = 2
		, desc_implicit = 4
	};
	virtual ~Value();

	//! Returns the current state of this value.
	State state() const { return static_cast<State>(state_); }

	/*!
	 * Sets the (initial) state of this value to s.
	 */
	Value* state(Value::State s) {
		state(true, s);
		return this;
	}

	//! Returns the name of this value.
	/*!
	 * \note The default name is "<arg>" unless isFlag() is true in which
	 *       case the default is "".
	 */
	const char* arg()          const;
	Value*      arg(const char* n) { return desc(desc_name, n); }

	//! Sets an alias name for the corresponding option.
	Value* alias(char c) { optAlias_ = c; return this; }
	char   alias() const { return optAlias_; }
	//! Sets a description level for the corresponding option.
	/*!
	 * Description levels can be used to suppress
	 * certain options when generating option descriptions.
	 */
	Value* level(DescriptionLevel lev) {
		unsigned x = (lev * level_shift);
		flags_ = static_cast<byte_t>(x | (flags_&(level_shift-1)));
		return this;
	}
	//! Returns the description level of the corresponding option.
	DescriptionLevel level() const { return DescriptionLevel(flags_ / level_shift); }

	//! Returns true if this is the value of an negatable option.
	/*!
	 * If an option '--option' is negatable, passing '--no-option'
	 * on the command-line will set the value of '--option' to 'no'.
	 */
	bool isNegatable() const { return hasProperty(property_negatable); }
	Value* negatable() { setProperty(property_negatable); return this; }


	//! Returns true if value can be implicitly created from an empty string.
	/*!
	 * \note the implicit value comes into play if the corresponding
	 *       option is present but without an adjacent value.
	 *
	 * \note an explicit value for an implicit value is only used if
	 *       it is unambiguously given. E.g. on the command-line one has
	 *       to use '--option=value' or '-ovalue' but *not* '--option value'
	 *       or '-o value'.
	 */
	bool   isImplicit() const { return hasProperty(property_implicit); }

	//! Returns true if this is the value of an option flag.
	/*!
	 * Similar to isImplicit but with the difference that
	 * no value is accepted on the command-line.
	 *
	 * Used for options like '--help' or '--version'.
	 */
	bool isFlag() const { return hasProperty(property_flag); }

	/*!
	 * Marks the value as flag.
	 * \see bool Value::isFlag() const
	 */
	Value* flag() { setProperty(property_flag); return this; }


	//! Returns true if the value of this option can be composed from multiple source.
	bool isComposing() const { return hasProperty(property_composing); }
	/*!
	 * Marks the value as composing.
	 * \see Value::isComposing()
	 */
	Value* composing() { setProperty(property_composing); return this; }

	/*!
	 * Sets a default value for this value.
	 */
	Value* defaultsTo(const char* v) { return desc(desc_default, v); }
	//! Returns the default value of this or 0 none exists
	const char* defaultsTo()  const { return desc(desc_default); }
	/*!
	 * Sets an implicit value, which will be used
	 * if option is given without an adjacent value,
	 * e.g. '--option' instead of '--option value'
	 * \see bool Value::isImplicit() const
	 */
	Value* implicit(const char* str) { return desc(desc_implicit, str); }
	//! Returns the implicit value of this or 0 if isImplicit() == false.
	const char* implicit() const;

	//! Parses the given string and updates the value's state.
	/*!
	 * \param name  The name of the option associated with this value.
	 * \param value The value to parse.
	 * \param st    The state to which the value should transition if parsing is successful.
	 *
	 * \return
	 * - true if the given string contains a valid value
	 * - false otherwise
	 *
	 * \post if true is returned, state() is st
	 */
	bool parse(const std::string& name, const std::string& value, State st = value_fixed);
protected:
	typedef unsigned char byte_t;
	enum Property {
		property_implicit = 1 // implicit value?
		, property_flag = 3 // implicit and type bool?
		, property_composing = 4 // multiple values allowed?
		, property_negatable = 8 // negatable form allowed?
		, property_location = 16 // fixed storage location?
		, not_a_property = 32
	};
	Value(byte_t flagSet, State initial = value_unassigned);
	void setProperty(Property f) { flags_ |= byte_t(f); }
	void clearProperty(Property f) { flags_ &= ~byte_t(f); }
	bool hasProperty(Property f)const { return (flags_ & byte_t(f)) == f; }
	bool state(bool b, State s) { if (b) { state_ = static_cast<byte_t>(s); } return b; }
	virtual bool doParse(const std::string& name, const std::string& value) = 0;
	const char* desc(DescType t) const;
	Value*      desc(DescType t, const char* d);
private:
	enum { desc_pack = 8, level_shift = not_a_property, levels = 255/level_shift };
	byte_t state_;       // state: one of State
	byte_t flags_;       // flag set holding properties
	byte_t descFlag_;    // either desc_pack or one of DescType
	byte_t optAlias_;    // alias name of option
	union ValueDesc {    // optional value descriptions either
		const char*  value;// a single value or
		const char** pack; // a pointer to a full pack
	}      desc_;
};
} // namespace ProgramOptions
} // namespace Potassco
#endif
