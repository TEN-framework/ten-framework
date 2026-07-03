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
// NOTE: ProgramOptions is inspired by Boost.Program_options
//       see: www.boost.org/libs/program_options
//
#ifndef PROGRAM_OPTIONS_ERRORS_H_INCLUDED
#define PROGRAM_OPTIONS_ERRORS_H_INCLUDED
#include <stdexcept>
#include <string>
namespace Potassco { namespace ProgramOptions {

//! Base class for all exceptions.
class Error : public std::logic_error {
public:
	explicit Error(const std::string& what) : std::logic_error(what) {}
};

//! Used for signaling errors on command-line and in declaring options.
class SyntaxError : public Error {
public:
	enum Type {
		missing_value,
		extra_value,
		invalid_format
	};
	SyntaxError(Type t, const std::string& key);
	~SyntaxError() throw () {}
	Type               type() const { return type_; }
	const std::string& key()  const { return key_; }
private:
	std::string key_;
	Type        type_;
};

//! Used for signaling errors in OptionContext.
class ContextError : public Error {
public:
	enum Type {
		duplicate_option,
		unknown_option,
		ambiguous_option,
		unknown_group
	};
	ContextError(const std::string& ctx, Type t, const std::string& key, const std::string& desc = "");
	~ContextError() throw () {}
	Type               type() const { return type_; }
	const std::string& key()  const { return key_; }
	const std::string& ctx()  const { return ctx_; }
private:
	std::string ctx_;
	std::string key_;
	Type        type_;
};

class DuplicateOption : public ContextError {
public:
	DuplicateOption(const std::string& ctx, const std::string& key) : ContextError(ctx, ContextError::duplicate_option, key) {}
	~DuplicateOption() throw () {}
};
class UnknownOption : public ContextError {
public:
	UnknownOption(const std::string& ctx, const std::string& key) : ContextError(ctx, ContextError::unknown_option, key) {}
	~UnknownOption() throw () {}
};
class AmbiguousOption : public ContextError {
public:
	AmbiguousOption(const std::string& ctx, const std::string& key, const std::string& alt) : ContextError(ctx, ContextError::ambiguous_option, key, alt) {}
	~AmbiguousOption() throw () {}
};

//! Used for signaling validation errors when trying to assign option values.
class ValueError : public Error {
public:
	enum Type {
		multiple_occurrences,
		invalid_default,
		invalid_value
	};
	ValueError(const std::string& ctx, Type t, const std::string& opt, const std::string& value);
	~ValueError() throw () {}
	Type               type() const { return type_; }
	const std::string& key()  const { return key_; }
	const std::string& ctx()  const { return ctx_; }
	const std::string& value()const { return value_; }
private:
	std::string ctx_;
	std::string key_;
	std::string value_;
	Type        type_;
};


} // namespace ProgramOptions
} // namespace Potassco
#endif
