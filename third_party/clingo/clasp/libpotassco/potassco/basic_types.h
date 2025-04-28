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
#ifndef POTASSCO_BASIC_TYPES_H_INCLUDED
#define POTASSCO_BASIC_TYPES_H_INCLUDED
/*!
 * \mainpage notitle
 * A small library for parsing and converting logic programs in aspif and smodels format.
 *
 * The library contains parsers and writers for aspif and smodels format
 * as well as functions and types for converting between the two formats
 * to the extent possible.
 *
 * A specification of aspif can be found in Appendix A of:
 * http://www.cs.uni-potsdam.de/wv/pdfformat/gekakaosscwa16b.pdf
 */
#include <potassco/platform.h>

#define LIB_POTASSCO_VERSION_MAJOR 1
#define LIB_POTASSCO_VERSION_MINOR 1
#define LIB_POTASSCO_VERSION_PATCH 0
#define LIB_POTASSCO_VERSION \
	POTASSCO_STRING(LIB_POTASSCO_VERSION_MAJOR) "."\
	POTASSCO_STRING(LIB_POTASSCO_VERSION_MINOR) "."\
	POTASSCO_STRING(LIB_POTASSCO_VERSION_PATCH)

//! Root namespace for all types and functions of libpotassco.
namespace Potassco {

/*!
 * \defgroup WriteType Program writer types
 * \brief Types and functions for writing logic programs.
 */

/*!
 * \defgroup ParseType Program parser types
 * \brief Types and functions for parsing logic programs.
 */

/*!
 * \defgroup BasicTypes Basic Data Types
 * \brief Basic types for working with logic programs.
 */
///@{
//! Ids are non-negative integers in the range [0..idMax].
typedef uint32_t Id_t;
//! Maximum value for ids.
const Id_t idMax = static_cast<Id_t>(-1);
//! Atom ids are positive integers in the range [atomMin..atomMax].
typedef uint32_t Atom_t;
//! Minimum value for atom ids (must not be 0).
const Atom_t atomMin = static_cast<Atom_t>(1);
//! Maximum value for atom ids.
const Atom_t atomMax = static_cast<Atom_t>(((1u)<<31)-1);
//! Literals are signed atoms.
typedef int32_t Lit_t;
//! (Literal) weights are integers.
typedef int32_t Weight_t;
//! A literal with an associated weight.
struct WeightLit_t {
	Lit_t    lit;    /**< Literal. */
	Weight_t weight; /**< Associated weight. */
};
//! Supported rule head types.
struct Head_t {
	//! Named constants.
	POTASSCO_ENUM_CONSTANTS(Head_t, Disjunctive = 0, Choice = 1);
};
//! Supported rule body types.
struct Body_t {
	//! Named constants.
	POTASSCO_ENUM_CONSTANTS(Body_t, Normal = 0, Sum = 1, Count = 2);
};
//! Type representing an external value.
struct Value_t {
	//! Named constants.
	POTASSCO_ENUM_CONSTANTS(Value_t,
		Free  = 0, True    = 1,
		False = 2, Release = 3
	);
};

//! A span consists of a starting address and a length.
/*!
 * A span does not own the data and it is in general not safe to store a span.
 */
template <class T>
struct Span {
	//! Iterator type for iterating over the elements of a span.
	typedef const T*    iterator;
	typedef std::size_t size_t;
	//! Returns the element at the given position, which shall be < size.
	const T& operator[](std::size_t pos) const { assert(pos < size); return first[pos]; }

	const T* first; /**< Pointer to first element of this span. */
	size_t   size;  /**< Number of elements in this span. */
};
typedef Span<Id_t>        IdSpan;
typedef Span<Atom_t>      AtomSpan;
typedef Span<Lit_t>       LitSpan;
typedef Span<WeightLit_t> WeightLitSpan;
typedef Span<char>        StringSpan;

//! Supported modifications for domain heuristic.
struct Heuristic_t {
	//! Named constants.
	POTASSCO_ENUM_CONSTANTS(Heuristic_t,
		Level = 0, Sign = 1, Factor = 2,
		Init  = 3, True = 4, False  = 5
	);
	static const StringSpan pred; /**< zero-terminated predicate name. */
};
//! Supported aspif directives.
struct Directive_t {
	//! Named constants.
	POTASSCO_ENUM_CONSTANTS(Directive_t,
		End       = 0,
		Rule      = 1, Minimize = 2,
		Project   = 3, Output   = 4,
		External  = 5, Assume   = 6,
		Heuristic = 7, Edge     = 8,
		Theory    = 9, Comment  = 10
	);
};

//! Basic callback interface for constructing a logic program.
class AbstractProgram {
public:
	virtual ~AbstractProgram();
	//! Called once to prepare for a new logic program.
	virtual void initProgram(bool incremental);
	//! Called once before rules and directives of the current program step are added.
	virtual void beginStep();

	//! Add the given rule to the program.
	virtual void rule(Head_t ht, const AtomSpan& head, const LitSpan& body) = 0;
	//! Add the given sum rule to the program.
	virtual void rule(Head_t ht, const AtomSpan& head, Weight_t bound, const WeightLitSpan& body) = 0;
	//! Add the given minimize statement to the program.
	virtual void minimize(Weight_t prio, const WeightLitSpan& lits) = 0;

	/*!
	* \name Advanced
	* Functions for adding advanced constructs.
	* By default, functions in this group throw a std::logic_error()
	* to signal that advanced constructs are not supported.
	*/
	//@{
	//! Mark the given list of atoms as projection atoms.
	virtual void project(const AtomSpan& atoms);
	//! Output str whenever condition is true in a stable model.
	virtual void output(const StringSpan& str, const LitSpan& condition);
	//! If v is not equal to Value_t::Release, mark a as external and assume value v. Otherwise, treat a as regular atom.
	virtual void external(Atom_t a, Value_t v);
	//! Assume the given literals to true during solving.
	virtual void assume(const LitSpan& lits);
	//! Apply the given heuristic modification to atom a whenever condition is true.
	virtual void heuristic(Atom_t a, Heuristic_t t, int bias, unsigned prio, const LitSpan& condition);
	//! Assume an edge between s and t whenever condition is true.
	virtual void acycEdge(int s, int t, const LitSpan& condition);
	//@}

	/*!
	 * \name Theory data
	 * Functions for adding theory statements.
	 * By default, all theory function throw a std::logic_error().
	 * Note, ids shall be unique within one step.
	 */
	//@{
	//! Add a new number term.
	virtual void theoryTerm(Id_t termId, int number);
	//! Add a new symbolic term.
	virtual void theoryTerm(Id_t termId, const StringSpan& name);
	//! Add a new compound (function or tuple) term.
	virtual void theoryTerm(Id_t termId, int cId, const IdSpan& args);
	//! Add a new theory atom element.
	virtual void theoryElement(Id_t elementId, const IdSpan& terms, const LitSpan& cond);
	//! Add a new theory atom consisting of the given elements, which have to be added eventually.
	virtual void theoryAtom(Id_t atomOrZero, Id_t termId, const IdSpan& elements);
	//! Add a new theory atom with guard and right hand side.
	virtual void theoryAtom(Id_t atomOrZero, Id_t termId, const IdSpan& elements, Id_t op, Id_t rhs);
	//@}

	//! Called once after all rules and directives of the current program step were added.
	virtual void endStep();
};
typedef int(*ErrorHandler)(int line, const char* what);

/*!
 * \defgroup BasicFunc Basic functions
 * \brief Additional functions over with basic types.
 * \ingroup BasicTypes
 */
///@{
//! Returns the id of the given atom.
inline Id_t        id(Atom_t a)                  { return static_cast<Id_t>(a); }
//! Returns the id of the given literal.
inline Id_t        id(Lit_t lit)                 { return static_cast<Id_t>(lit); }
//! Returns the atom of the given literal.
inline Atom_t      atom(Lit_t lit)               { return static_cast<Atom_t>(lit >= 0 ? lit : -lit); }
//! Returns the atom of the given weight literal.
inline Atom_t      atom(const WeightLit_t& w)    { return atom(w.lit); }
//! Converts the given id back to a literal.
inline Lit_t       lit(Id_t a)                   { return static_cast<Lit_t>(a); }
//! Identity function for literals.
inline Lit_t       lit(Lit_t lit)                { return lit; }
//! Returns the literal of the given weight literal.
inline Lit_t       lit(const WeightLit_t& w)     { return w.lit; }
//! Returns the negative literal of the given atom.
inline Lit_t       neg(Atom_t a)                 { return -lit(a); }
//! Returns the complement of the given literal.
inline Lit_t       neg(Lit_t lit)                { return -lit; }
//! Returns the weight of the given atom, which is always 1.
inline Weight_t    weight(Atom_t)                { return 1; }
//! Returns the weight of the given literal, which is always 1.
inline Weight_t    weight(Lit_t)                 { return 1; }
//! Returns the weight of the given weight literal.
inline Weight_t    weight(const WeightLit_t& w)  { return w.weight; }
//! Returns whether the given span is empty, i.e. has size 0.
template <class T>
inline bool        empty(const Span<T>& s)       { return s.size == 0; }
//! Returns the size of the given span.
template <class T>
inline std::size_t size(const Span<T>& s)        { return s.size; }
//! Returns a pointer to the first element of the given span.
template <class T>
inline const T*    begin(const Span<T>& s)       { return s.first; }
//! Returns a pointer one past the end of the given span.
template <class T>
inline const T*    end(const Span<T>& s)         { return begin(s) + s.size; }
//! Returns the element at the given position of the given span.
/*!
 * \pre pos < size(s)
 */
template <class T>
inline const T&    at(const Span<T>& s, std::size_t pos) { return s[pos]; }

inline bool operator==(const WeightLit_t& lhs, const WeightLit_t& rhs) { return lit(lhs) == lit(rhs) && weight(lhs) == weight(rhs); }
inline bool operator==(Lit_t lhs, const WeightLit_t& rhs) { return lit(lhs) == lit(rhs) && weight(rhs) == 1; }
inline bool operator==(const WeightLit_t& lhs, Lit_t rhs) { return rhs == lhs; }
inline bool operator!=(const WeightLit_t& lhs, const WeightLit_t& rhs) { return !(lhs == rhs); }
inline bool operator!=(Lit_t lhs, const WeightLit_t& rhs) { return !(lhs == rhs); }
inline bool operator!=(const WeightLit_t& lhs, Lit_t rhs) { return rhs != lhs; }
inline bool operator<(const WeightLit_t& lhs, const WeightLit_t& rhs) { return lhs.lit != rhs.lit ? lhs.lit < rhs.lit : lhs.weight < rhs.weight; }

//! Creates a span from an array of size s.
template <class T>
inline Span<T> toSpan(const T* x, std::size_t s) {
	Span<T> r = {x, s};
	return r;
}
//! Creates an empty span.
template <class T>
inline Span<T> toSpan() { return toSpan(static_cast<const T*>(0), 0); }
//! Creates a span from a sequential (STL) container c.
template <class C>
inline Span<typename C::value_type> toSpan(const C& c) {
	return !c.empty() ? toSpan(&c[0], c.size()) : toSpan<typename C::value_type>();
}

//! Returns the string representation of the given heuristic modifier.
inline const char* toString(Heuristic_t t) {
	switch (t) {
		case Heuristic_t::Level: return "level";
		case Heuristic_t::Sign:  return "sign";
		case Heuristic_t::Factor:return "factor";
		case Heuristic_t::Init:  return "init";
		case Heuristic_t::True:  return "true";
		case Heuristic_t::False: return "false";
		default: return "";
	}
}

///@}
///@}

//! A (dynamic-sized) block of raw memory.
/*!
 * The class manages a (dynamic-sized) block of memory obtained by malloc/realloc
 * and uses a simple geometric scheme when the block needs to grow.
 *
 * \ingroup ParseType
 */
class MemoryRegion {
public:
	explicit MemoryRegion(std::size_t initialSize = 0);
	~MemoryRegion();
	//! Returns the current region size.
	std::size_t size()     const { return static_cast<std::size_t>(static_cast<unsigned char*>(end_) - static_cast<unsigned char*>(beg_)); }
	//! Returns a pointer to the beginning of the region.
	void*       begin()    const { return beg_; }
	//! Returns a pointer past-the-end of the region.
	void*       end()      const { return end_;  }
	//! Returns a pointer into the region at the given index.
	/*!
	 * \pre  idx < size()
	 * \note The returned pointer is only valid until the next grow operation.
	 */
	void*       operator[](std::size_t idx) const;
	//! Grows the region to at least n bytes.
	/*!
	 * \post size() >= n
	 */
	void        grow(std::size_t n = 0);
	//! Swaps this and other.
	void        swap(MemoryRegion& other);
	//! Releases the region and its memory.
	/*!
	 * \post size() == 0
	 */
	void        release();
private:
	MemoryRegion(const MemoryRegion&);
	MemoryRegion& operator=(const MemoryRegion&);
	void* beg_;
	void* end_;
};
inline void swap(MemoryRegion& lhs, MemoryRegion& rhs) { lhs.swap(rhs); }
class RuleBuilder;

} // namespace Potassco
#endif
