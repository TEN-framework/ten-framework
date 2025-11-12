//
// Copyright (c) 2016-2017 Benjamin Kaufmann
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
#ifndef POTASSCO_RULE_UTILS_H_INCLUDED
#define POTASSCO_RULE_UTILS_H_INCLUDED
#include <potassco/match_basic_types.h>
#include <new>
namespace Potassco {
/*!
 * \addtogroup BasicTypes
 */
///@{

//! A sum aggregate with a lower bound.
struct Sum_t {
	WeightLitSpan lits;  /**< Weight literals of the aggregate. */
	Weight_t      bound; /**< Lower bound of the aggregate. */
};
//! A type that can represent an aspif rule.
struct Rule_t {
	Head_t   ht;   /**< Head type of the rule. */
	AtomSpan head; /**< Head atoms of the rule. */
	Body_t   bt;   /**< Type of rule body. */
	union {
		LitSpan cond;
		Sum_t   agg;
	};
	//! Named constructor for creating a rule.
	static Rule_t normal(Head_t ht, const AtomSpan& head, const LitSpan& body);
	//! Named constructor for creating a sum rule.
	static Rule_t sum(Head_t ht, const AtomSpan& head, const Sum_t& sum);
	//! Named constructor for creating a sum rule.
	static Rule_t sum(Head_t ht, const AtomSpan& head, Weight_t bound, const WeightLitSpan& lits);
	//! Returns whether the rule has a normal body, i.e. whether the body is a conjunction of literals.
	bool normal() const { return bt == Body_t::Normal; }
	//! Returns whether the body of the rule is a sum aggregate.
	bool sum()    const { return bt != Body_t::Normal; }
};

//! A builder class for creating a rule.
class RuleBuilder {
public:
	RuleBuilder();
	RuleBuilder(const RuleBuilder&);
	RuleBuilder& operator=(const RuleBuilder&);
	~RuleBuilder();
	void swap(RuleBuilder& other);
	/*!
	 * \name Start functions
	 * Functions for starting the definition of a rule's head or body.
	 * If the active rule is frozen (i.e. end() was called), the active
	 * rule is discarded.
	 * \note The body of a rule can be defined before or after its head is defined
	 * but definitions of head and body must not be mixed.
	 */
	//@{
	//! Start definition of the rule's head, which can be either disjunctive or a choice.
	RuleBuilder& start(Head_t ht = Head_t::Disjunctive);
	//! Start definition of a minimize rule. No head allowed.
	RuleBuilder& startMinimize(Weight_t prio);
	//! Start definition of a conjunction to be used as the rule's body.
	RuleBuilder& startBody();
	//! Start definition of a sum aggregate to be used as the rule's body.
	RuleBuilder& startSum(Weight_t bound);
	//! Update lower bound of sum aggregate.
	RuleBuilder& setBound(Weight_t bound);
	//@}

	/*!
	 * \name Update functions
	 * Functions for adding elements to the active rule.
	 * \note Update functions shall not be called once a rule is frozen.
	 * \note Calling an update function implicitly starts the definition of the
	 * corresponding rule part.
	 */
	//@{
	//! Add a to the rule's head.
	RuleBuilder& addHead(Atom_t a);
	//! Add lit to the rule's body.
	RuleBuilder& addGoal(Lit_t lit) { WeightLit_t p = {lit, 1}; return addGoal(p); }
	//! Add lit with given weight to rule's body if body is a sum aggregate or rule is a minimize rule.
	RuleBuilder& addGoal(Lit_t lit, Weight_t w) { WeightLit_t p = {lit, w}; return addGoal(p); }
	RuleBuilder& addGoal(WeightLit_t lit);
	//@}

	//! Stop definition of rule and add rule to out if given.
	/*!
	 * Once end() was called, the active rule is considered frozen.
   */
	RuleBuilder& end(AbstractProgram* out = 0);
	//! Discard active rule and unfreeze builder.
	RuleBuilder& clear();
	//! Discard body of active rule but keep head if any.
	RuleBuilder& clearBody();
	//! Discard head of active rule but keep body if any.
	RuleBuilder& clearHead();
	//! Weaken active sum aggregate body to a normal body or count aggregate.
	RuleBuilder& weaken(Body_t to, bool resetWeights = true);

	/*!
	 * \name Query functions
	 * Functions for accessing parts of the active rule.
	 * \note The result of these functions is only valid until the next call to
	 * an update function.
	 */
	//@{
	AtomSpan      head()        const;
	Atom_t*       head_begin()  const;
	Atom_t*       head_end()    const;
	Body_t        bodyType()    const;
	LitSpan       body()        const;
	Sum_t         sum()         const;
	Rule_t        rule()        const;
	// low-level access:
	Lit_t*        lits_begin()  const;
	Lit_t*        lits_end()    const;
	WeightLit_t*  wlits_begin() const;
	WeightLit_t*  wlits_end()   const;
	Weight_t      bound()       const;
	//@}
	struct Rule;
private:
	void startBody(Body_t bt, Weight_t bnd);
	Weight_t* bound_() const;
	Rule*     rule_()  const;
	Rule*     unfreeze(bool clear);
	MemoryRegion mem_;
};
///@}

} // namespace Potassco
#endif
