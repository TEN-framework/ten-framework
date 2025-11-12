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
#ifndef POTASSCO_THEORY_DATA_H_INCLUDED
#define POTASSCO_THEORY_DATA_H_INCLUDED

#include <potassco/basic_types.h>
#include <iterator>
#include <utility>
#include <new>
namespace Potassco {

class TheoryData;
struct FuncData;

/*!
 * \addtogroup BasicTypes
 */
///@{
//! Supported aspif theory directives.
struct Theory_t {
	//! Named constants.
	POTASSCO_ENUM_CONSTANTS(Theory_t,
		Number  = 0, Symbol = 1, Compound = 2, Reserved = 3,
		Element = 4,
		Atom    = 5, AtomWithGuard = 6
	);
};

//! Supported aspif theory tuple types.
struct Tuple_t {
	//! Named constants.
	POTASSCO_ENUM_CONSTANTS_T(Tuple_t, int, -3, Bracket = -3, Brace = -2, Paren = -1);
};
//! Returns starting and ending delimiters of the given tuple type.
inline const char* toString(Tuple_t t) {
	static const char* p = "()\0{}\0[]";
	int off = (-static_cast<int>(t)-1) * 3;
	return p + off;
}

//! A term is either a number, symbolic, or compound term (function or tuple).
class TheoryTerm {
public:
	//! Iterator type for iterating over arguments of a compound term.
	typedef const Id_t* iterator;
	//! Creates an invalid term.
	TheoryTerm();
	//! Creates a number term.
	explicit TheoryTerm(int num);
	//! Creates a symbolic term.
	explicit TheoryTerm(const char* sym);
	//! Creates a compound term.
	explicit TheoryTerm(const FuncData* c);
	//! Returns whether this object holds a valid number, symbol or compound.
	bool        valid()      const;
	//! Returns the type of this term.
	Theory_t    type()       const;
	//! Returns the number stored in this or throws if type() != Number.
	int         number()     const;
	//! Returns the symbol stored in this or throws if type() != Symbol.
	const char* symbol()     const;
	//! Returns the compound id (either term id or tuple type) stored in this or throws if type() != Compound.
	int         compound()   const;
	//! Returns whether this is a function.
	bool        isFunction() const;
	//! Returns the function id stored in this or throws if !isFunction().
	Id_t        function()   const;
	//! Returns whether this is a tuple.
	bool        isTuple()    const;
	//! Returns the tuple id stored in this or throws if !isTuple().
	Tuple_t     tuple()      const;
	//! Returns the number of arguments in this term.
	uint32_t    size()       const;
	//! Returns an iterator pointing to the first argument of this term.
	iterator    begin()      const;
	//! Returns an iterator marking the end of the arguments of this term.
	iterator    end()        const;
	//! Returns the range [begin(), end()).
	IdSpan      terms()      const { return toSpan(begin(), size()); }
private:
	friend class TheoryData;
	uint64_t  assertPtr(const void*) const;
	void      assertType(Theory_t) const;
	uintptr_t getPtr() const;
	FuncData* func() const;
	uint64_t data_;
};

//! A basic building block for a theory atom.
class TheoryElement {
public:
	//! Iterator type for iterating over the terms of an element.
	typedef const Id_t* iterator;
	//! Creates a new TheoryElement over the given terms.
	static TheoryElement* newElement(const IdSpan& terms, Id_t condition);
	//! Destroys the given TheoryElement.
	static void destroy(TheoryElement* a);
	//! Returns the number of terms belonging to this element.
	uint32_t size()  const { return nTerms_; }
	//! Returns an iterator pointing to the first term of this element.
	iterator begin() const { return term_; }
	//! Returns an iterator one past the last term of this element.
	iterator end()   const { return begin() + size(); }
	//! Returns the terms of this element.
	IdSpan   terms() const { return toSpan(begin(), size()); }
	//! Returns the condition associated with this element.
	Id_t     condition() const;
private:
	friend class TheoryData;
	TheoryElement(const IdSpan& terms, Id_t c);
	TheoryElement(const TheoryElement&);
	TheoryElement& operator=(const TheoryElement&);
	void setCondition(Id_t c);
	uint32_t nTerms_ : 31;
	uint32_t nCond_  :  1;
POTASSCO_WARNING_BEGIN_RELAXED
	Id_t     term_[0];
POTASSCO_WARNING_END_RELAXED
};

//! A theory atom.
class TheoryAtom {
public:
	//! Iterator type for iterating over the elements of a theory atom.
	typedef const Id_t* iterator;
	//! Creates a new theory atom.
	static TheoryAtom* newAtom(Id_t atom, Id_t term, const IdSpan& elements);
	//! Creates a new theory atom with guard.
	static TheoryAtom* newAtom(Id_t atom, Id_t term, const IdSpan& elements, Id_t op, Id_t rhs);
	//! Destroys the given theory atom.
	static void  destroy(TheoryAtom* a);

	//! Returns the associated program atom or 0 if this originated from a directive.
	Id_t       atom()       const { return static_cast<Id_t>(atom_); }
	//! Returns the term that is associated with this atom.
	Id_t       term()       const { return termId_; }
	//! Returns the number of elements in this atom.
	uint32_t   size()       const { return nTerms_; }
	//! Returns an iterator pointing to the first element of this atom.
	iterator   begin()      const { return term_; }
	//! Returns an iterator marking the end of elements of this atoms.
	iterator   end()        const { return begin() + size(); }
	//! Returns the range [begin(), end()).
	IdSpan     elements()   const { return toSpan(begin(), size()); }
	//! Returns a pointer to the id of the theory operator associated with this atom or 0 if atom has no guard.
	const Id_t* guard()     const;
	//! Returns a pointer to the term id of the right hand side of the theory operator or 0 if atom has no guard.
	const Id_t* rhs()       const;
private:
	TheoryAtom(Id_t atom, Id_t term, const IdSpan& elements, Id_t* op, Id_t* rhs);
	TheoryAtom(const TheoryAtom&);
	TheoryAtom& operator=(const TheoryAtom&);
	uint32_t atom_ : 31;
	uint32_t guard_:  1;
	Id_t     termId_;
	uint32_t nTerms_;
POTASSCO_WARNING_BEGIN_RELAXED
	Id_t     term_[0];
POTASSCO_WARNING_END_RELAXED
};

//! A type for storing and looking up theory atoms and their elements and terms.
class TheoryData {
public:
	//! Iterator type for iterating over the theory atoms of a TheoryData object.
	typedef const TheoryAtom*const* atom_iterator;
	typedef TheoryTerm    Term;
	typedef TheoryElement Element;
	TheoryData();
	~TheoryData();
	//! Sentinel for marking a condition to be set later.
	static const Id_t COND_DEFERRED = static_cast<Id_t>(-1);

	//! Resets this object to the state after default construction.
	void reset();
	//! May be called to distinguish between the current and a previous incremental step.
	void update();

	//! Adds a new theory atom.
	/*!
	 * Each element in elements shall be an id associated with an atom element
	 * eventually added via addElement().
	 */
	const TheoryAtom& addAtom(Id_t atomOrZero, Id_t termId, const IdSpan& elements);
	//! Adds a new theory atom with guard and right hand side.
	const TheoryAtom& addAtom(Id_t atomOrZero, Id_t termId, const IdSpan& elements, Id_t op, Id_t rhs);

	//! Adds a new theory atom element with the given id.
	/*!
	 * Each element in terms shall be an id of a theory term
	 * eventually added via one of the addTerm() overloads.
	 * \note If cond is COND_DEFERRED, the condition may later be changed via a call to setCondition().
	 */
	const TheoryElement& addElement(Id_t elementId, const IdSpan& terms, Id_t cond = COND_DEFERRED);
	//! Changes the condition of the element with the given id.
	/*!
	 * \pre The element was previously added with condition COND_DEFERRED.
	 */
	void setCondition(Id_t elementId, Id_t newCond);

	//! Adds a new number term with the given id.
	const TheoryTerm& addTerm(Id_t termId, int number);
	//! Adds a new symbolic term with the given name and id.
	const TheoryTerm& addTerm(Id_t termId, const StringSpan& name);
	//! Adds a new symbolic term with the given name and id.
	const TheoryTerm& addTerm(Id_t termId, const char* name);
	//! Adds a new function term with the given id.
	/*!
	 * The parameter funcSym represents the name of the function and shall be the id of a symbolic term.
	 * Each element in args shall be an id of a theory term.
	 */
	const TheoryTerm& addTerm(Id_t termId, Id_t funcSym, const IdSpan& args);
	//! Adds a new tuple term with the given id.
	const TheoryTerm& addTerm(Id_t termId, Tuple_t type, const IdSpan& args);

	//! Removes the term with the given id.
	/*!
	 * \note It is the caller's responsibility to ensure that the removed term is not referenced
	 * by any theory element.
	 * \note The term id of a removed term may be reused in a subsequent call to addTerm().
	 */
	void removeTerm(Id_t termId);

	//! Returns the number of stored theory atoms.
	uint32_t       numAtoms()           const;
	//! Returns an iterator pointing to the first theory atom.
	atom_iterator  begin()              const;
	//! Returns an iterator pointing to the first theory atom added after last call to update.
	atom_iterator  currBegin()          const;
	//! Returns an iterator marking the end of the range of theory atoms.
	atom_iterator  end()                const;
	//! Returns whether this object stores a term with the given id.
	bool           hasTerm(Id_t t)      const;
	//! Returns whether the given term was added after last call to update.
	bool           isNewTerm(Id_t t)    const;
	//! Returns whether this object stores an atom element with the given id.
	bool           hasElement(Id_t e)   const;
	//! Returns whether the given element was added after last call to update.
	bool           isNewElement(Id_t e) const;
	//! Returns the term with the given id or throws if no such term exists.
	const Term&    getTerm(Id_t t)      const;
	//! Returns the element with the given id or throws if no such element exists.
	const Element& getElement(Id_t e)   const;

	//! Removes all theory atoms a for which f(a) returns true.
	template <class F>
	void filter(const F& f) {
		TheoryAtom** j = const_cast<TheoryAtom**>(currBegin());
		uint32_t pop = 0;
		for (atom_iterator it = j, end = this->end(); it != end; ++it) {
			Id_t atom = (*it)->atom();
			if (!atom || !f(**it)) {
				*j++ = const_cast<TheoryAtom*>(*it);
			}
			else {
				++pop;
				TheoryAtom::destroy(const_cast<TheoryAtom*>(*it));
			}
		}
		resizeAtoms(numAtoms() - pop);
	}
	//! Interface for visiting a theory.
	class Visitor {
	public:
		virtual ~Visitor();
		//! Visit a theory term. Should call data.accept(t, *this) to visit any arguments of the term.
		virtual void visit(const TheoryData& data, Id_t termId, const TheoryTerm& t) = 0;
		//! Visit a theory element. Should call data.accept(e, *this) to visit the terms of the element.
		virtual void visit(const TheoryData& data, Id_t elemId, const TheoryElement& e) = 0;
		//! Visit the theory atom. Should call data.accept(a, *this) to visit the elements of the atom.
		virtual void visit(const TheoryData& data, const TheoryAtom& a) = 0;
	};
	//! Possible visitation modes.
	/*!
	 * Mode visit_current ignores atoms, elements, or terms
	 * that were added in previous steps, i.e. before the last call to update().
	 */
	enum VisitMode { visit_all , visit_current };
	//! Calls out.visit(*this, a) for all theory atoms.
	void accept(Visitor& out, VisitMode m = visit_current) const;
	//! Visits terms and elements of a.
	void accept(const TheoryAtom& a, Visitor& out, VisitMode m = visit_all) const;
	//! Visits terms of e.
	void accept(const TheoryElement& e, Visitor& out, VisitMode m = visit_all) const;
	//! If t is a compound term, visits subterms of t.
	void accept(const TheoryTerm& t, Visitor& out, VisitMode m = visit_all) const;
private:
	TheoryData(const TheoryData&);
	TheoryData& operator=(const TheoryData&);
	struct DestroyT;
	TheoryTerm&     setTerm(Id_t);
	TheoryTerm*     terms()    const;
	TheoryElement** elems()    const;
	TheoryAtom**    atoms()    const;
	uint32_t        numTerms() const;
	uint32_t        numElems() const;
	void            resizeAtoms(uint32_t n);
	bool doVisitTerm(VisitMode m, Id_t id) const { return m == visit_all || isNewTerm(id); }
	bool doVisitElem(VisitMode m, Id_t id) const { return m == visit_all || isNewElement(id); }
	struct Data;
	Data* data_;
};

/*!
 * Adaptor that couples an iterator returned from a theory atom, element, or term
 * with a TheoryData object so that dereferencing yields an object instead of an id.
 */
template <class T, const T& (TheoryData::*get)(Id_t) const>
class IteratorAdaptor : public std::iterator<std::bidirectional_iterator_tag, const T> {
public:
	typedef IteratorAdaptor this_type;
	typedef std::iterator<std::bidirectional_iterator_tag, const T> base_type;
	typedef typename base_type::reference reference;
	typedef typename base_type::pointer   pointer;
	IteratorAdaptor(const TheoryData& t, const Id_t* e) : data_(&t), elem_(e) {}
	IteratorAdaptor() : data_(0), elem_(0) {}
	this_type& operator++() { ++elem_; return *this; }
	this_type  operator++(int) {
		this_type t(*this);
		++*this;
		return t;
	}
	this_type& operator--() { --elem_; return *this; }
	this_type  operator--(int) {
		this_type t(*this);
		--*this;
		return t;
	}
	reference  operator*()  const { return (data_->*get)(*raw()); }
	pointer    operator->() const { return &**this; }

	friend void swap(this_type& lhs, this_type& rhs) {
		std::swap(lhs.data_, rhs.data_);
		std::swap(lhs.elem_, rhs.elem_);
	}
	friend bool operator==(const this_type& lhs, const this_type& rhs) {
		return lhs.data_ == rhs.data_ && lhs.elem_ == rhs.elem_;
	}
	friend bool operator!=(const this_type& lhs, const this_type& rhs) {
		return !(lhs == rhs);
	}
	const Id_t*       raw()    const { return elem_; }
	const TheoryData& theory() const { return *data_; }
private:
	const TheoryData* data_;
	const Id_t*       elem_;
};

typedef IteratorAdaptor<TheoryElement, &TheoryData::getElement> TheoryElementIterator;
typedef IteratorAdaptor<TheoryTerm, &TheoryData::getTerm>       TheoryTermIterator;

inline TheoryElementIterator begin(const TheoryData& t, const TheoryAtom& a)   { return TheoryElementIterator(t, a.begin()); }
inline TheoryElementIterator end(const TheoryData& t, const TheoryAtom& a)     { return TheoryElementIterator(t, a.end()); }
inline TheoryTermIterator    begin(const TheoryData& t, const TheoryElement& e){ return TheoryTermIterator(t, e.begin()); }
inline TheoryTermIterator    end(const TheoryData& t, const TheoryElement& e)  { return TheoryTermIterator(t, e.end()); }

StringSpan toSpan(const char* x);

inline void print(AbstractProgram& out, Id_t termId, const TheoryTerm& term) {
	switch (term.type()) {
		case Potassco::Theory_t::Number  : out.theoryTerm(termId, term.number()); break;
		case Potassco::Theory_t::Symbol  : out.theoryTerm(termId, Potassco::toSpan(term.symbol())); break;
		case Potassco::Theory_t::Compound: out.theoryTerm(termId, term.compound(), term.terms()); break;
	}
}
inline void print(AbstractProgram& out, const TheoryAtom& a) {
	if (a.guard()) { out.theoryAtom(a.atom(), a.term(), a.elements(), *a.guard(), *a.rhs()); }
	else           { out.theoryAtom(a.atom(), a.term(), a.elements()); }
}
///@}
} // namespace Potassco
#endif
