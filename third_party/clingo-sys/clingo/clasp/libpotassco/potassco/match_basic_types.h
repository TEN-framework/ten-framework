//
// Copyright (c) 2015-2017 Benjamin Kaufmann
//
// This file is part of Potassco.
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
#ifndef POTASSCO_MATCH_BASIC_TYPES_H
#define POTASSCO_MATCH_BASIC_TYPES_H
#include <potassco/basic_types.h>
#include <stdexcept>
#include <climits>
#include <iosfwd>
#include <stdint.h>
namespace Potassco {

/*!
 * \addtogroup ParseType
 */
///@{

//! A wrapper around an std::istream that provides buffering and a simple interface for extracting characters and integers.
class BufferedStream {
public:
	enum { BUF_SIZE = 4096 };
	//! Creates a new object wrapping the given stream.
	explicit BufferedStream(std::istream& str);
	~BufferedStream();
	//! Returns the next character in the input stream, without extracting it.
	char peek() const { return buf_[rpos_]; }
	//! Returns whether the end of the input stream was reached.
	bool end()  const { return peek() == 0; }
	//! Extracts the next character from the input stream or returns 0 if the end was reached.
	char get();
	//! Attempts to put back the given character into the read buffer.
	bool unget(char c);
	//! Attempts to read an integer from the input stream optionally skipping initial whitespace.
	bool match(int64_t& i, bool noSkipWs = false);
	//! Attempts to extract the given string from the input stream.
	/*!
	 * If the function returns false, no characters were extracted from the stream.
	 * \pre std::strlen(tok) <= BUF_SIZE
	 */
	bool match(const char* tok);
	//! Extracts initial whitespace from the input stream.
	void skipWs();
	//! Extracts up to max characters from the input stream and copies them into bufferOut.
	/*!
	 * \return The number of characters copied to bufferOut.
	 */
	int  copy(char* bufferOut, int max);
	//! Returns the current line number in the input stream, i.e. the number of '\n' characters extracted so far.
	unsigned line() const;
	//! Returns whether the given character is a decimal digit.
	static inline bool isDigit(char c) { return c >= '0' && c <= '9'; }
	//! Converts the given character to a decimal digit.
	static inline int  toDigit(char c) { return static_cast<int>(c - '0'); }
	static void fail(unsigned line, const char* error);

	void require(bool cnd, const char* error) const { if (!cnd) { fail(line(), error); } }
private:
	char rget();
	enum { ALLOC_SIZE = BUF_SIZE + 1 };
	BufferedStream& operator=(const BufferedStream&);
	void underflow(bool up = true);
	typedef char* BufferType;
	std::istream& str_;
	BufferType    buf_;
	std::size_t   rpos_;
	unsigned      line_;
};

/*!
 * \name Match
 * Match functions for extracting tokens from a stream/string.
 */
///@{
//! Attempts to extract the given string from the stream optionally skipping leading whitespace first.
inline bool match(BufferedStream& str, const char* word, bool skipWs) {
	if (skipWs) str.skipWs();
	return str.match(word);
}
//! Extracts an int in the given range or fails with an std::exception.
inline int matchInt(BufferedStream& str, int min = INT_MIN, int max = INT_MAX, const char* err = "integer expected") {
	int64_t x;
	str.require(str.match(x) && x >= min && x <= max, err);
	return static_cast<int>(x);
}
//! Extracts a positive integer in the range [0;max] or fails with an std::exception.
inline unsigned matchPos(BufferedStream& str, unsigned max, const char* err) {
	int64_t x;
	str.require(str.match(x) && x >= 0 && static_cast<uint64_t>(x) <= max, err);
	return static_cast<unsigned>(x);
}
//! Extracts a positive integer or fails with an std::exception.
inline unsigned matchPos(BufferedStream& str, const char* err = "non-negative integer expected") {
	return matchPos(str, static_cast<unsigned>(-1), err);
}
//! Extracts an atom (i.e. a positive integer > 0) or fails with an std::exception.
inline Atom_t matchAtom(BufferedStream& str, unsigned aMax = atomMax, const char* err = "atom expected") {
	int64_t x; int64_t max = static_cast<int64_t>(aMax);
	str.require(str.match(x) && x >= atomMin && x <= max, err);
	return static_cast<Atom_t>(x);
}
//! Extracts a literal (i.e. a signed integer != 0) or fails with an std::exception.
inline Lit_t matchLit(BufferedStream& str, unsigned aMax = atomMax, const char* err = "literal expected") {
	int64_t x; int64_t max = static_cast<int64_t>(aMax);
	str.require(str.match(x) && x != 0 && x >= -max && x <= max, err);
	return static_cast<Lit_t>(x);
}
//! Extracts a weight literal (i.e. a literal followed by an integer) or fails with an std::exception.
inline WeightLit_t matchWLit(BufferedStream& str, unsigned aMax = atomMax, Weight_t minW = 0, const char* err = "weight literal expected") {
	WeightLit_t wl;
	wl.lit = matchLit(str, aMax, err);
	wl.weight = matchInt(str, minW, INT_MAX, "invalid weight literal weight");
	return wl;
}
//! Returns whether input starts with word and if so sets input to input + std::strlen(word).
bool match(const char*& input, const char* word);
//! Returns whether input starts with a string representation of a heuristic modifier and if so extracts it.
/*!
 * \see toString(Heuristic_t)
 */
bool match(const char*& input, Heuristic_t& heuType);
//! Attempts to extract the next argument of a predicate.
bool matchAtomArg(const char*& input, StringSpan& arg);
//! Attempts to extract an integer from input and sets input to the first character after the integer.
bool match(const char*& input, int& out);
//! Tries to match potassco's special _heuristic/3 or _heuristic/4 predicate.
/*!
 * \return > 1 on match, 0 if in does not start with _heuristic, < 0 if in has wrong arity or parameters.
 */
int  matchDomHeuPred(const char*& in
	, StringSpan&  atom
	, Heuristic_t& type
	, int&         bias
	, unsigned&    prio);
//! Tries to match an _edge/2 or _acyc_/0 predicate.
int matchEdgePred(const char*& in, StringSpan& n0, StringSpan& n1);
///@}

//! Base class for input parsers.
class ProgramReader {
public:
	//! Enumeration type for supported read modes.
	enum ReadMode { Incremental, Complete };
	//! Creates a reader that is not yet associated with any input stream.
	ProgramReader();
	virtual ~ProgramReader();
	//! Associates the reader with the given input stream and returns whether the stream has the right format.
	bool accept(std::istream& str);
	//! Returns whether the input stream represents an incremental program.
	bool incremental() const;
	//! Attempts to parse the previously accepted input stream.
	/*!
	 * Depending on the given read mode, the function either parses the complete program
	 * or only the next incremental step.
	 */
	bool parse(ReadMode r = Incremental);
	//! Returns whether the input stream has more data or is exhausted.
	bool more();
	//! Resets this object to the state after default construction.
	void reset();
	//! Returns the current line number in the input stream.
	unsigned line() const;
	//! Sets the largest possible variable number.
	/*!
	 * The given value is used when matching atoms or literals.
	 * If a larger value is found in the input stream, an std::exception
	 * is raised.
	 */
	void setMaxVar(unsigned v) { varMax_ = v; }
protected:
	typedef BufferedStream StreamType;
	typedef WeightLit_t WLit_t;
	//! Shall return true if the format of the input stream is supported by this object.
	/*!
	 * \param[out] inc Whether the input stream represents an incremental program.
	 */
	virtual bool doAttach(bool& inc) = 0;
	//! Shall parse the next program step.
	virtual bool doParse() = 0;
	//! Shall reset any parsing state.
	virtual void doReset();
	//! Returns the associated input stream.
	StreamType*  stream() const;
	//! Returns the next character in the input stream, without extracting it.
	char     peek(bool skipws) const;
	//! Throws an std::exception with the current line and given message if cnd is false.
	bool     require(bool cnd, const char* msg) const;
	//! Attempts to match the given string.
	bool     match(const char* word, bool skipWs = true) { return Potassco::match(*stream(), word, skipWs); }
	//! Extracts an int in the given range or fails with an std::exception.
	int      matchInt(int min = INT_MIN, int max = INT_MAX, const char* err = "integer expected") { return Potassco::matchInt(*stream(), min, max, err); }
	//! Extracts a positive integer in the range [0;max] or fails with an std::exception.
	unsigned matchPos(unsigned max = static_cast<unsigned>(-1), const char* err = "unsigned integer expected") { return Potassco::matchPos(*stream(), max, err); }
	//! Extracts a positive integer or fails with an std::exception.
	unsigned matchPos(const char* err) { return matchPos(static_cast<unsigned>(-1), err); }
	//! Extracts an atom (i.e. a positive integer > 0) or fails with an std::exception.
	/*!
	 * \see setMaxVar(unsigned)
	 */
	Atom_t   matchAtom(const char* err = "atom expected") { return Potassco::matchAtom(*stream(), varMax_, err); }
	//! Extracts a literal (i.e. a signed integer != 0) or fails with an std::exception.
	Lit_t    matchLit(const char* err = "literal expected") { return Potassco::matchLit(*stream(), varMax_, err); }
	//! Extracts a weight literal (i.e. a literal followed by an integer) or fails with an std::exception.
	WLit_t   matchWLit(int min, const char* err = "weight literal expected") { return Potassco::matchWLit(*stream(), varMax_, min, err); }
	//! Extracts and discards characters up to and including the next newline.
	void     skipLine();
private:
	ProgramReader(const ProgramReader&);
	ProgramReader& operator=(const ProgramReader&);
	StreamType* str_;
	unsigned    varMax_;
	bool        inc_;
};
//! Attaches the given stream to r and calls ProgramReader::parse() with the read mode set to ProgramReader::Complete.
int readProgram(std::istream& str, ProgramReader& r, ErrorHandler err);
} // namespace Potassco
///@}

#endif
