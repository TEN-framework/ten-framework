//
// Copyright (c) 2006-2017 Benjamin Kaufmann
//
// This file is part of Clasp. See http://www.cs.uni-potsdam.de/clasp/
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
#include <clasp/logic_program_types.h>
#include <clasp/logic_program.h>
#include <clasp/solver.h>
#include <clasp/clause.h>
#include <clasp/weight_constraint.h>
#include <clasp/util/misc_types.h>

namespace Clasp {
namespace Asp {
static_assert((bk_lib::detail::same_type<Potassco::Weight_t, weight_t>::value), "unexpected weight type");
static_assert((bk_lib::detail::same_type<Potassco::Atom_t, Var>::value)       , "unexpected atom type");
static_assert((bk_lib::detail::same_type<Potassco::Id_t, uint32>::value)      , "unexpected id type");
static_assert((bk_lib::detail::same_type<Potassco::Lit_t, int32>::value)      , "unexpected literal type");
static_assert((bk_lib::detail::same_type<Potassco::Weight_t, int32>::value)   , "unexpected weight type");
/////////////////////////////////////////////////////////////////////////////////////////
// class RuleTransform
//
// class for transforming extended rules to normal rules
/////////////////////////////////////////////////////////////////////////////////////////
struct RuleTransform::Impl {
	Impl() : adapt_(0), prg_(0) { }
	struct TodoItem {
		TodoItem(uint32 i, weight_t w, Atom_t v) : idx(i), bound(w), head(v) {}
		uint32   idx;
		weight_t bound;
		Atom_t   head;
	};
	struct CmpW {
		template <class P>
		bool operator()(const P& lhs, const P& rhs) const {
			return Potassco::weight(lhs) > Potassco::weight(rhs);
		}
	};
	typedef PodQueue<TodoItem> TodoQueue;
	typedef Potassco::LitVec  LitVec;
	typedef Potassco::WLitVec WLitVec;
	ProgramAdapter* adapt_;
	LogicProgram*   prg_;
	LitVec          lits_;
	WLitVec         agg_;
	SumVec          sumR_;
	VarVec          aux_;
	TodoQueue       todo_;
	weight_t        bound_;
	Atom_t newAtom() const { return prg_ ? prg_->newAtom() : adapt_->newAtom(); }
	uint32 addRule(const Rule& r) const {
		if (prg_){ prg_->addRule(r); }
		else     { adapt_->addRule(r); }
		return 1;
	}
	uint32 addRule(Head_t ht, const Potassco::AtomSpan& head, const Potassco::LitSpan& b) const {
		return addRule(Rule::normal(ht, head, b));
	}
	uint32 addRule(Atom_t h, const Potassco::LitSpan& b) const {
		return addRule(Head_t::Disjunctive, Potassco::toSpan(&h, static_cast<std::size_t>(h != 0)), b);
	}
	uint32 transform(Atom_t head, weight_t bound, const Potassco::WeightLitSpan& lits, Strategy s);
	uint32 transformSelect(Atom_t head);
	uint32 transformSplit(Atom_t head);
	uint32 transformChoice(const Potassco::AtomSpan& r);
	uint32 transformDisjunction(const Potassco::AtomSpan& r);
	uint32 addRule(Atom_t head, bool add, uint32 idx, weight_t bound);
	Atom_t getAuxVar(uint32 idx, weight_t bound) {
		assert(bound > 0 && idx < agg_.size());
		uint32 k = static_cast<uint32>(bound - 1);
		if (aux_[k] == 0) {
			todo_.push(TodoItem(idx, bound, aux_[k] = newAtom()));
		}
		return aux_[k];
	}
};

RuleTransform::RuleTransform(ProgramAdapter& prg) : impl_(new Impl()) {
	impl_->adapt_ = &prg;
}
RuleTransform::RuleTransform(LogicProgram& prg) : impl_(new Impl()) {
	impl_->prg_ = &prg;
}
RuleTransform::~RuleTransform() {
	delete impl_;
}

uint32 RuleTransform::transform(const Rule& r, Strategy s) {
	if (r.sum()) {
		Atom_t h = !Potassco::empty(r.head) ? r.head[0] : 0;
		bool aux = r.ht == Head_t::Choice || size(r.head) > 1;
		if (aux) {
			h = impl_->newAtom();
			Potassco::Lit_t bl = Potassco::lit(h);
			impl_->addRule(r.ht, r.head, Potassco::toSpan(&bl, 1));
		}
		return uint32(aux) + impl_->transform(h, r.agg.bound, r.agg.lits, s);
	}
	else if (size(r.head) > static_cast<uint32>(r.ht == Head_t::Disjunctive)) {
		impl_->lits_.clear();
		uint32 nAux = (size(r.cond) > 1) && (size(r.head) > 1) && s != strategy_no_aux;
		if (nAux) {
			// make body eq to a new aux atom
			Atom_t auxB = impl_->newAtom();
			impl_->addRule(auxB, r.cond);
			impl_->lits_.push_back(Potassco::lit(auxB));
		}
		else {
			impl_->lits_.assign(begin(r.cond), end(r.cond));
		}
		return nAux + (r.ht == Head_t::Choice ? impl_->transformChoice(r.head) : impl_->transformDisjunction(r.head));
	}
	return impl_->addRule(r);
}

// A choice rule {h1,...hn} :- BODY is replaced with:
// h1   :- BODY, not aux1.
// aux1 :- not h1.
// ...
// hn   :- BODY, not auxN.
// auxN :- not hn.
uint32 RuleTransform::Impl::transformChoice(const Potassco::AtomSpan& atoms) {
	uint32 nRule = 0;
	Potassco::Lit_t   bLit = 0;
	Potassco::LitSpan bAux = Potassco::toSpan(&bLit, 1);
	Atom_t hAux;
	for (Potassco::AtomSpan::iterator it = Potassco::begin(atoms), end = Potassco::end(atoms); it != end; ++it) {
		hAux = newAtom();
		bLit = Potassco::neg(*it);
		lits_.push_back(Potassco::neg(hAux));
		nRule += addRule(*it, Potassco::toSpan(lits_));
		nRule += addRule(hAux, bAux);
		lits_.pop_back();
	}
	return nRule;
}

// A disjunctive rule h1|...|hn :- BODY is replaced with:
// hi   :- BODY, {not hj | 1 <= j != i <= n}.
uint32 RuleTransform::Impl::transformDisjunction(const Potassco::AtomSpan& atoms) {
	uint32 bIdx = sizeVec(lits_);
	for (Potassco::AtomSpan::iterator it = Potassco::begin(atoms) + 1, end = Potassco::end(atoms); it != end; ++it) {
		lits_.push_back(Potassco::neg(*it));
	}
	uint32 nRule = 0;
	for (Potassco::AtomSpan::iterator it = Potassco::begin(atoms), end = Potassco::end(atoms);;) {
		nRule += addRule(*it, Potassco::toSpan(lits_));
		if (++it == end) { break; }
		lits_[bIdx++] = Potassco::neg(*(it-1));
	}
	return nRule;
}

uint32 RuleTransform::Impl::transform(Atom_t head, weight_t bound, const Potassco::WeightLitSpan& wlits, Strategy s) {
	bound_ = bound;
	agg_.assign(begin(wlits), end(wlits));
	if (!isSorted(agg_.begin(), agg_.end(), CmpW())) {
		std::stable_sort(agg_.begin(), agg_.end(), CmpW());
	}
	wsum_t sum = 0;
	sumR_.resize(agg_.size());
	for (WLitVec::size_type i = agg_.size(); i--;) {
		agg_[i].weight = std::min(agg_[i].weight, bound_);
		sumR_[i] = (sum += agg_[i].weight);
		POTASSCO_REQUIRE(agg_[i].weight >= 0 && sum <= CLASP_WEIGHT_T_MAX, "invalid weight rule");
	}
	if      (bound_ > sum) { return 0; }
	else if (bound_ <= 0)  { return addRule(head, Potassco::toSpan<Potassco::Lit_t>()); }
	else if ((sum - agg_.back().weight) < bound_) { // normal rule
		lits_.clear();
		for (WLitVec::const_iterator it = agg_.begin(), end = agg_.end(); it != end; ++it) {
			lits_.push_back(lit(*it));
		}
		return addRule(head, Potassco::toSpan(lits_));
	}
	else {
		return ((s == strategy_no_aux || (sum < 6 && s == strategy_default))
			? transformSelect(head)
			: transformSplit(head));
	}
}

// Exponential transformation of cardinality and weight constraint.
// Creates minimal subsets, no aux atoms.
// E.g. a rule h = 2 {a,b,c,d} is translated into the following six rules:
// h :- a, b.
// h :- a, c.
// h :- a, d.
// h :- b, c.
// h :- b, d.
// h :- c, d.
uint32 RuleTransform::Impl::transformSelect(Atom_t h) {
	lits_.clear();
	uint32 nRule = 0;
	wsum_t cw = 0;
	assert(sumR_[0] >= bound_ && cw < bound_);
	aux_.clear();
	for (uint32 it = 0, end = (uint32)agg_.size();;) {
		while (cw < bound_) {
			cw += Potassco::weight(agg_[it]);
			lits_.push_back(Potassco::lit(agg_[it]));
			aux_.push_back(it++);
		}
		nRule += addRule(h, Potassco::toSpan(lits_));
		do {
			if (aux_.empty()) { return nRule; }
			it = aux_.back();
			aux_.pop_back();
			lits_.pop_back();
			cw -= Potassco::weight(agg_[it]);
		} while (++it == end || (cw + sumR_[it]) < bound_);
	}
}
// Quadratic transformation of cardinality and weight constraint.
// Introduces aux atoms.
// E.g. a rule h = 2 {a,b,c,d} is translated into the following eight rules:
// h       :- a, aux_1_1.
// h       :- aux_1_2.
// aux_1_1 :- b.
// aux_1_1 :- aux_2_1.
// aux_1_2 :- b, aux_2_1.
// aux_1_2 :- c, d.
// aux_2_1 :- c.
// aux_2_1 :- d.
uint32 RuleTransform::Impl::transformSplit(Atom_t h) {
	const weight_t bound = bound_;
	uint32 nRule = 0;
	uint32 level = 0;
	aux_.resize(bound, 0);
	todo_.clear();
	todo_.push(TodoItem(0, bound, h));
	while (!todo_.empty()) {
		TodoItem i = todo_.pop_ret();
		if (i.idx > level) {
			// We are about to start a new level of the tree - reset aux_
			level = i.idx;
			aux_.assign(bound, 0);
		}
		// For a todo item i with head h and lit x = agg_[i.idx] create at most two rules:
		// r1: h :- x, aux(i.idx+1, i.bound-weight(x))
		// r2: h :- aux(i.idx+1, i.bound).
		// The first rule r1 represents the case where x is true, while
		// the second rule encodes the case where the literal is false.
		nRule += addRule(i.head, true,  i.idx, i.bound - agg_[i.idx].weight);
		nRule += addRule(i.head, false, i.idx, i.bound);
	}
	return nRule;
}

// Creates a rule head :- agg_[idx], aux(idx+1, bound) or head :- aux(idx+1, bound) or depending on add.
uint32 RuleTransform::Impl::addRule(Atom_t head, bool add, uint32 bIdx, weight_t bound) {
	const weight_t minW = agg_.back().weight;
	const wsum_t   maxW = sumR_[bIdx + 1];
	if (bound <= 0) {
		assert(add);
		lits_.assign(1, agg_[bIdx].lit);
		return addRule(head, Potassco::toSpan(lits_));
	}
	if ((maxW - minW) < bound) {
		// remaining literals are all needed to satisfy bound
		bIdx += static_cast<uint32>(!add);
		if (maxW >= bound) {
			lits_.clear();
			for (; bIdx != agg_.size(); ++bIdx) { lits_.push_back(agg_[bIdx].lit); }
			return addRule(head, Potassco::toSpan(lits_));
		}
		return 0;
	}
	lits_.clear();
	if (add) { lits_.push_back(agg_[bIdx].lit); }
	lits_.push_back(Potassco::lit(getAuxVar(bIdx + 1, bound)));
	return addRule(head, Potassco::toSpan(lits_));
}
/////////////////////////////////////////////////////////////////////////////////////////
// class SccChecker
//
// SCC/cycle checking
/////////////////////////////////////////////////////////////////////////////////////////
SccChecker::SccChecker(LogicProgram& prg, AtomList& sccAtoms, uint32 startScc)
	: prg_(&prg), sccAtoms_(&sccAtoms), count_(0), sccs_(startScc) {
	for (uint32 i = 0; i != prg.numAtoms(); ++i) {
		visit(prg.getAtom(i));
	}
	for (uint32 i = 0; i != prg.numBodies(); ++i) {
		visit(prg.getBody(i));
	}
}

void SccChecker::visitDfs(PrgNode* node, NodeType t) {
	if (!prg_ || !doVisit(node)) {
		return;
	}
	callStack_.clear();
	nodeStack_.clear();
	count_ = 0;
	addCall(node, t, 0);
	while (!callStack_.empty()) {
		Call c = callStack_.back();
		callStack_.pop_back();
		if (!recurse(c)) {
			node = unpackNode(c.node);
			if (c.min < node->id()) {
				node->resetId( c.min, true );
			}
			else if (c.node == nodeStack_.back()) {
				// node is trivially-connected; all such nodes are in the same Pseudo-SCC
				if (isNode(nodeStack_.back(), PrgNode::Atom)) {
					static_cast<PrgAtom*>(node)->setScc(PrgNode::noScc);
				}
				node->resetId(PrgNode::noNode, true);
				nodeStack_.pop_back();
			}
			else { // non-trivial SCC
				PrgNode* succVertex;
				do {
					succVertex = unpackNode(nodeStack_.back());
					if (isNode(nodeStack_.back(), PrgNode::Atom)) {
						static_cast<PrgAtom*>(succVertex)->setScc(sccs_);
						sccAtoms_->push_back(static_cast<PrgAtom*>(succVertex));
					}
					nodeStack_.pop_back();
					succVertex->resetId(PrgNode::noNode, true);
				} while (succVertex != node);
				++sccs_;
			}
		}
	}
}

bool SccChecker::recurse(Call& c) {
	PrgNode* n = unpackNode(c.node);
	if (!n->seen()) {
		nodeStack_.push_back(c.node);
		c.min = count_++;
		n->resetId(c.min, true);
	}
	if (isNode(c.node, PrgNode::Body)) {
		PrgBody* b = static_cast<PrgBody*>(n);
		PrgHead* h = 0; NodeType t;
		for (PrgBody::head_iterator it = b->heads_begin() + c.next, end = b->heads_end(); it != end; ++it) {
			if   (it->isAtom()){ h = prg_->getAtom(it->node()); t = PrgNode::Atom; }
			else               { h = prg_->getDisj(it->node()); t = PrgNode::Disj; }
			if (doVisit(h, false) && onNode(h, t, c, static_cast<uint32>(it-b->heads_begin()))) {
				return true;
			}
		}
	}
	else if (isNode(c.node, PrgNode::Atom)) {
		PrgAtom* a = static_cast<PrgAtom*>(n);
		for (PrgAtom::dep_iterator it = a->deps_begin() + c.next, end = a->deps_end(); it != end; ++it) {
			if (it->sign()) continue;
			PrgBody* bn = prg_->getBody(it->var());
			if (doVisit(bn, false) && onNode(bn, PrgNode::Body, c, static_cast<uint32>(it-a->deps_begin()))) {
				return true;
			}
		}
	}
	else if (isNode(c.node, PrgNode::Disj)) {
		PrgDisj* d = static_cast<PrgDisj*>(n);
		for (PrgDisj::atom_iterator it = d->begin() + c.next, end = d->end(); it != end; ++it) {
			PrgAtom* a = prg_->getAtom(*it);
			if (doVisit(a, false) && onNode(a, PrgNode::Atom, c, static_cast<uint32>(it-d->begin()))) {
				return true;
			}
		}
	}
	return false;
}

bool SccChecker::onNode(PrgNode* n, NodeType t, Call& c, uint32 data) {
	if (!n->seen()) {
		Call rec = {c.node, c.min, data};
		callStack_.push_back(rec);
		addCall(n, t, 0);
		return true;
	}
	if (n->id() < c.min) {
		c.min = n->id();
	}
	return false;
}
/////////////////////////////////////////////////////////////////////////////////////////
PrgNode::PrgNode(uint32 id, bool checkScc)
	: litId_(noLit), noScc_(uint32(!checkScc)), id_(id), val_(value_free), eq_(0), seen_(0) {
	POTASSCO_CHECK(id < noNode, EOVERFLOW, "Id out of range");
	static_assert(sizeof(PrgNode) == sizeof(uint64), "Unsupported Alignment");
}
/////////////////////////////////////////////////////////////////////////////////////////
// class PrgHead
/////////////////////////////////////////////////////////////////////////////////////////
PrgHead::PrgHead(uint32 id, NodeType t, uint32 data, bool checkScc)
	: PrgNode(id, checkScc)
	, data_(data), upper_(0), dirty_(0), freeze_(0), isAtom_(t == PrgNode::Atom)  {
	struct X { uint64 x; EdgeVec y; uint32 z; };
	static_assert(sizeof(PrgHead) == sizeof(X), "Unsupported Alignment");
}
// Adds the node with given id as a support to this head
// and marks the head as dirty so that any duplicates/false/eq
// supports are removed once simplify() is called.
void PrgHead::addSupport(PrgEdge r, Simplify s) {
	supports_.push_back(r);
	if (s == force_simplify) { dirty_ = (supports_.size() > 1); }
}
// Removes the given node from the set of supports of this head.
void PrgHead::removeSupport(PrgEdge r) {
	if (relevant()) {
		supports_.erase(std::remove(supports_.begin(), supports_.end(), r), supports_.end());
	}
	dirty_ = 1;
}

void PrgHead::clearSupports() {
	supports_.clear();
	upper_  = 0;
	dirty_  = 0;
}
// Simplifies the set of predecessors supporting this head.
// Removes false/eq supports and returns the number of
// different supporting literals in numDiffSupps.
bool PrgHead::simplifySupports(LogicProgram& prg, bool strong, uint32* numDiffSupps) {
	uint32 numLits = supports();
	uint32 choices = 0;
	if (dirty_ == 1) {
		dirty_                  = 0;
		numLits                 = 0;
		SharedContext& ctx      = *prg.ctx();
		EdgeVec::iterator it,n,j= supports_.begin();
		for (it = supports_.begin(); it != supports_.end(); ++it) {
			PrgNode* x = prg.getSupp(*it);
			if (x->relevant() && x->value() != value_false && (!strong || x->hasVar())) {
				if      (!x->seen()) { *j++ = *it; x->setSeen(true); }
				else if (!choices)   { continue; }
				else                 {
					for (n = supports_.begin(); n != it && n->node() != it->node(); ++n) {;}
					if (*it < *n)      { *n = *it; }
					else               { continue; }
				}
				choices += (it->isBody() && it->isChoice());
				if (strong && !ctx.marked(x->literal())) {
					++numLits;
					ctx.mark(x->literal());
				}
			}
		}
		supports_.erase(j, supports_.end());
		uint32 dis = 0;
		choices    = 0;
		for (it = supports_.begin(); it != supports_.end(); ++it) {
			PrgNode* x = prg.getSupp(*it);
			x->setSeen(false);
			if (strong && ctx.marked(x->literal())) { ctx.unmark(x->var()); }
			if (it->isChoice()) {
				++choices;
				dis += it->isDisj();
			}
		}
		numLits += choices;
	}
	if (numDiffSupps) { *numDiffSupps = numLits; }
	return supports() > 0 || prg.assignValue(this, value_false, PrgEdge::noEdge());
}

// Assigns a variable to this head.
// No support: no var and value false
// More than one support or type not normal or no eq: new var
// Exactly one support that is normal: use that
void PrgHead::assignVar(LogicProgram& prg, PrgEdge support, bool allowEq) {
	if (hasVar() || !relevant()) { return; }
	uint32 numS = supports();
	if (numS == 0 && support == PrgEdge::noEdge()) {
		prg.assignValue(this, value_false, support);
	}
	else {
		PrgNode* sup = prg.getSupp(support);
		bool  newVar = numS > 1 || (!allowEq && Var_t::isAtom(prg.ctx()->varInfo(sup->var()).type()));
		if (support.isNormal() && sup->hasVar() && (!newVar || sup->value() == value_true)) {
			// head is equivalent to sup
			setLiteral(sup->literal());
			prg.ctx()->setVarEq(var(), true);
			prg.incEqs(Var_t::Hybrid);
		}
		else {
			setLiteral(posLit(prg.ctx()->addVar(Var_t::Atom, 0)));
		}
	}
}

// Backpropagates the value of this head to its supports.
bool PrgHead::backpropagate(LogicProgram& prg, ValueRep val, bool bpFull) {
	bool ok = true;
	if (val == value_false) {
		// a false head can't have supports
		EdgeVec temp; temp.swap(supports_);
		markDirty();
		for (EdgeIterator it = temp.begin(), end = temp.end(); it != end && ok; ++it) {
			if (it->isBody()) {
				ok = prg.getBody(it->node())->propagateAssigned(prg, this, it->type());
			}
			else { assert(it->isDisj());
				ok = prg.getDisj(it->node())->propagateAssigned(prg, this, it->type());
			}
		}
	}
	else if (val != value_free && supports() == 1 && bpFull) {
		// head must be true and only has one support, thus the only support must
		// be true, too.
		PrgBody* b   = 0;
		if (supports_[0].isBody()) {
			b   = prg.getBody(supports_[0].node());
		}
		else if (supports_[0].isDisj()) {
			PrgDisj* d = prg.getDisj(supports_[0].node());
			if (d->supports() == 1) { b = prg.getBody(d->supps_begin()->node()); }
		}
		ok = !b || b->value() == val || (b->assignValue(val) && b->propagateValue(prg, bpFull));
	}
	return ok;
}

/////////////////////////////////////////////////////////////////////////////////////////
// class PrgAtom
/////////////////////////////////////////////////////////////////////////////////////////
PrgAtom::PrgAtom(uint32 id, bool checkScc)
	: PrgHead(id, PrgNode::Atom, PrgNode::noScc, checkScc) {
	static_assert(sizeof(PrgAtom) == sizeof(PrgHead) + sizeof(LitVec), "Unsupported Alignment");
}

void PrgAtom::setEqGoal(Literal x) {
	if (eq()) {
		POTASSCO_CHECK(!x.sign() || x.var() < noScc, EOVERFLOW, "Id out of range");
		data_ = x.sign() ? x.var() : noScc;
	}
}
Literal PrgAtom::eqGoal(bool sign) const {
	if (!eq() || sign || data_ == noScc) {
		return Literal(id(), sign);
	}
	return negLit(data_);
}

// Adds a dependency between this atom and the body with
// the given id. If pos is true, atom appears positively
// in body, otherwise negatively.
void PrgAtom::addDep(Var bodyId, bool pos) {
	deps_.push_back(Literal(bodyId, !pos));
}

// Removes a dependency between this atom and the body with
// the given id. If pos is true, atom appears positively
// in body, otherwise negatively.
void PrgAtom::removeDep(Var bodyId, bool pos) {
	LitVec::iterator it = std::find(deps_.begin(), deps_.end(), Literal(bodyId, !pos));
	if (it != deps_.end()) { deps_.erase(it); }
}

// Removes the subset of dependencies given by d
void PrgAtom::clearDeps(Dependency d) {
	if (d == dep_all) {
		deps_.clear();
	}
	else {
		bool sign = d == dep_neg;
		LitVec::iterator j = deps_.begin();
		for (LitVec::iterator it = deps_.begin(), end = deps_.end(); it != end; ++it) {
			if (it->sign() != sign) { *j++ = *it; }
		}
		deps_.erase(j, deps_.end());
	}
}

bool PrgAtom::hasDep(Dependency d) const {
	if (d == dep_all) { return !deps_.empty(); }
	for (LitVec::const_iterator it = deps_.begin(), end = deps_.end(); it != end; ++it) {
		if (static_cast<Dependency>(it->sign()) == d) { return true; }
	}
	return false;
}

bool PrgAtom::inDisj() const {
	for (EdgeIterator it= supports_.begin(), end = supports_.end(); it != end; ++it) {
		if (it->isDisj()) { return true; }
	}
	return false;
}

// Propagates the value of this atom to its dependent bodies
// and, if backpropagation is enabled, to its supporting bodies/disjunctions.
// PRE: value() != value_free
bool PrgAtom::propagateValue(LogicProgram& prg, bool backprop) {
	ValueRep val = value();
	assert(val != value_free);
	// propagate value forward
	Literal dep = posLit(id());
	for (dep_iterator it = deps_.begin(), end = deps_end(); it != end; ++it) {
		if (!prg.getBody(it->var())->propagateAssigned(prg, dep ^ it->sign(), val)) {
			return false;
		}
	}
	if (inDisj() && prg.isFact(this)) {
		// - atom is true, thus all disjunctive rules containing it are superfluous
		EdgeVec temp; temp.swap(supports_);
		EdgeVec::iterator j = temp.begin();
		EdgeType t          = PrgEdge::Choice;
		for (EdgeIterator it= temp.begin(), end = temp.end(); it != end; ++it) {
			if      (!it->isDisj())                                            { *j++ = *it; }
			else if (!prg.getDisj(it->node())->propagateAssigned(prg, this, t)){ return false; }
		}
		temp.erase(j, temp.end());
		supports_.swap(temp);
	}
	return backpropagate(prg, val, backprop);
}

// Adds the atom-oriented nogoods for this atom in form of clauses.
// Adds the support clause [~a S1...Sn] (where each Si is a supporting node of a)
// representing the tableau-rules BTA and FFA.
// Furthermore, adds the clause [a ~Bj] representing tableau-rules FTA and BFA
// if Bj supports a via a "normal" edge.
bool PrgAtom::addConstraints(const LogicProgram& prg, ClauseCreator& gc) {
	SharedContext& ctx  = *prg.ctx();
	EdgeVec::iterator j = supports_.begin();
	bool           nant = false;
	gc.start().add(~literal());
	for (EdgeVec::iterator it = supports_.begin(); it != supports_.end(); ++it) {
		PrgNode* n = prg.getSupp(*it);
		Literal  B = n->literal();
		// consider only bodies which are part of the simplified program, i.e.
		// are associated with a variable in the solver.
		if (n->relevant() && n->hasVar()) {
			*j++ = *it;
			nant = nant || it->isChoice();
			if (!it->isDisj()) { gc.add(B); }
			if (it->isNormal() && !ctx.addBinary(literal(), ~B)) { // FTA/BFA
				return false;
			}
		}
	}
	supports_.erase(j, supports_.end());
	if (nant ||	hasDep(PrgAtom::dep_neg)) { ctx.setNant(var(), true); }
	return gc.end(ClauseCreator::clause_force_simplify);
}
/////////////////////////////////////////////////////////////////////////////////////////
// class PrgBody
/////////////////////////////////////////////////////////////////////////////////////////
PrgBody::PrgBody(uint32 id, LogicProgram& prg, const Potassco::LitSpan& lits, uint32 pos, bool addDeps)
	: PrgNode(id, true) {
	init(Body_t::Normal, toU32(Potassco::size(lits)));
	Norm* c = new (data_)Norm();
	unsupp_ = pos;
	// store B+ followed by B-
	Literal* p[2] = {c->lits, c->lits + pos};
	for (Potassco::LitSpan::iterator it = Potassco::begin(lits), end = Potassco::end(lits); it != end; ++it) {
		POTASSCO_REQUIRE(*it != 0, "body not simplified");
		Literal*& n = p[*it < 0];
		*n = toLit(*it);
		if (addDeps) { prg.getAtom(n->var())->addDep(id, !n->sign()); }
		++n;
	}
}

PrgBody::PrgBody(uint32 id, LogicProgram& prg, const Potassco::Sum_t& sum, bool hasWeights, uint32 pos, bool addDeps)
	: PrgNode(id, true) {
	init(hasWeights ? Body_t::Sum : Body_t::Count, toU32(Potassco::size(sum.lits)));
	Agg* a = new (data_) Agg();
	if (!hasWeights) {
		a->bound = sum.bound;
		unsupp_  = sum.bound - static_cast<weight_t>(size_ - pos);
	}
	else {
		a->sum  = SumData::create(size_, sum.bound, 0);
		unsupp_ = sum.bound;
	}
	// store B+ followed by B- followed by optional weights
	Literal* base = a->lits;
	Literal* p[2] = {base, base + pos};
	weight_t* weights = hasWeights ? a->sum->weights : 0;
	for (Potassco::WeightLitSpan::iterator it = Potassco::begin(sum.lits), end = Potassco::end(sum.lits); it != end; ++it) {
		POTASSCO_REQUIRE(it->lit != 0 && it->weight > 0, "body not simplified");
		Literal*& n = p[it->lit < 0];
		*n = toLit(Potassco::lit(*it));
		if (weights) { weights[n - base] = it->weight; a->sum->sumW += it->weight; if (n->sign()) { unsupp_ -= it->weight; } }
		if (addDeps) { prg.getAtom(n->var())->addDep(id, !n->sign()); }
		++n;
	}
}
void PrgBody::init(Body_t t, uint32 sz) {
	POTASSCO_REQUIRE(sz <= maxSize, "body too large");
	size_ = sz, head_ = 0, type_ = t, sBody_ = 0, sHead_ = 0, freeze_ = 0;
}
PrgBody* PrgBody::create(LogicProgram& prg, uint32 id, const Rule& r, uint32 pos, bool addDeps) {
	static_assert(sizeof(PrgBody) == 24 && sizeof(Agg) == sizeof(void*), "unexpected alignment");
	PrgBody* ret = 0;
	if (r.normal()) {
		size_t bytes = sizeof(PrgBody) + (Potassco::size(r.cond) * sizeof(Literal));
		ret = new (::operator new(bytes)) PrgBody(id, prg, r.cond, pos, addDeps);
	}
	else {
		const Potassco::Sum_t& sum = r.agg;
		size_t bytes = sizeof(PrgBody) + (Potassco::size(r.agg.lits) * sizeof(Literal)) + sizeof(Agg);
		ret = new (::operator new(bytes)) PrgBody(id, prg, sum, r.bt == Body_t::Sum, pos, addDeps);
		POTASSCO_REQUIRE(ret->bound() > 0 && ret->sumW() > ret->bound(), "body not simplified");
	}
	if (ret->bound() == 0) {
		ret->assignValue(value_true);
		ret->markDirty();
	}
	return ret;
}

PrgBody::~PrgBody() {
	clearHeads();
	if (hasWeights()) {
		sumData()->destroy();
	}
}

void PrgBody::destroy() {
	this->~PrgBody();
	::operator delete(this);
}

PrgBody::SumData* PrgBody::SumData::create(uint32 size, weight_t b, weight_t s) {
	uint32 bytes = sizeof(SumData) + (size * sizeof(weight_t));
	SumData* ret = new (::operator new(bytes)) SumData();
	ret->bound = b;
	ret->sumW = s;
	return ret;
}
void PrgBody::SumData::destroy() {
	::operator delete(this);
}

uint32 PrgBody::findLit(const LogicProgram& prg, Literal p) const {
	for (const Literal* it = goals_begin(), *end = it + size(); it != end; ++it) {
		Literal x = prg.getAtom(it->var())->literal();
		if (it->sign()) x = ~x;
		if (x == p) return static_cast<uint32>(it - goals_begin());
	}
	return varMax;
}

// Sets the unsupported counter back to
// bound() - negWeight()
bool PrgBody::resetSupported() {
	unsupp_ = bound();
	for (uint32 x = size(); x && goal(--x).sign(); ) {
		unsupp_ -= weight(x);
	}
	return isSupported();
}

// Removes all heads from this body *without* notifying them
void PrgBody::clearHeads() {
	if (!isSmallHead()) { delete largeHead(); }
	head_ = 0;
}

// Makes h a head-successor of this body and adds this
// body as a support for h.
void PrgBody::addHead(PrgHead* h, EdgeType t) {
	assert(relevant() && h->relevant());
	PrgEdge fwdEdge = PrgEdge::newEdge(*h, t);
	PrgEdge bwdEdge = PrgEdge::newEdge(*this, t);
	uint32  numHeads= static_cast<uint32>(heads_end() - heads_begin());
	uint32  numSupps= h->supports();
	bool    dup     = false;
	if (numHeads && numSupps && std::min(numHeads, numSupps) < 10) {
		if (numSupps < numHeads) { dup = std::find(h->supps_begin(), h->supps_end(), bwdEdge) != h->supps_end(); }
		else                     { dup = std::find(heads_begin(), heads_end(), fwdEdge) != heads_end(); }
	}
	if (dup) { return; }
	addHead(fwdEdge);
	h->addSupport(bwdEdge);
	// mark head-set as dirty
	if (head_ > 1) {  sHead_ = 1; }
}

void PrgBody::addHead(PrgEdge h) {
	if      (head_ < 2u)    { smallHead()[head_++] = h; }
	else if (!isSmallHead()){ largeHead()->push_back(h);    }
	else                    {
		EdgeVec* t  = new EdgeVec(heads_begin(), heads_end());
		headData_.ext = t;
		head_ = 3u;
		t->push_back(h);
	}
}

void PrgBody::removeHead(PrgHead* h, EdgeType t) {
	PrgEdge x = PrgEdge::newEdge(*h, t);
	if (eraseHead(x)) {
		h->removeSupport(PrgEdge::newEdge(*this, t)); // also remove back edge
	}
}

bool PrgBody::hasHead(PrgHead* h, EdgeType t) const {
	if (!hasHeads()) { return false;  }
	PrgEdge x = PrgEdge::newEdge(*h, t);
	head_iterator it = sHead_ != 0 || isSmallHead() ? std::find(heads_begin(), heads_end(), x) : std::lower_bound(heads_begin(), heads_end(), x);
	return it != heads_end() && *it == x;
}

bool PrgBody::eraseHead(PrgEdge h) {
	PrgEdge* it = const_cast<PrgEdge*>(std::find(heads_begin(), heads_end(), h));
	if      (it == heads_end()) { return false; }
	else if (isSmallHead())     { *it = smallHead()[1]; --head_; }
	else                        { largeHead()->erase(it); }
	return true;
}

bool PrgBody::toData(const LogicProgram& prg, Potassco::RuleBuilder& out) const {
	Body_t bt = type();
	weight_t sum = 0, bound = this->bound();
	bt == Body_t::Normal ?  out.startBody() : out.startSum(bound);
	for (uint32 i = 0, end = size(); i != end; ++i) {
		Potassco::WeightLit_t wl = {toInt(goal(i)), weight(i)};
		if (!prg.frozen() || prg.inProgram(Potassco::atom(wl))) {
			out.addGoal(wl);
			sum += wl.weight;
		}
		else if (wl.lit < 0)           { bound -= weight(i); }
		else if (bt == Body_t::Normal) { return false; }
	}
	if (bt != Body_t::Normal) {
		out.setBound(bound);
		if (bound <= 0 || bound >= sum) {
			if      (bound > sum) { return false; }
			else if (bound <= 0)  { out.clearBody(); }
			else                  { out.weaken(Body_t::Normal); }
		}
	}
	return true;
}

// Simplifies the body by removing assigned atoms & replacing eq atoms.
// Checks whether simplified body must be false (CONTRA) or is
// structurally equivalent to some other body.
// prg    The program containing this body
// strong If true, treats atoms that have no variable associated as false.
// eqId   The id of a body in prg that is equivalent to this body
bool PrgBody::simplifyBody(LogicProgram& prg, bool strong, uint32* eqId) {
	if (eqId)        { *eqId  = id(); }
	if (sBody_ == 0) { return true;   }
	// update body - compute old hash value
	SharedContext& ctx = *prg.ctx();
	uint32 oldHash     = 0;
	weight_t bound     = this->bound();
	weight_t w         = 1, *jw = hasWeights() ? sumData()->weights : 0;
	Literal* lits      = goals_begin();
	Literal* j         = lits;
	AtomState& todo    = prg.atomState();
	Var a;
	bool mark, isEq;
	int todos = 0;
	for (Literal* it = j, *end = j + size(); it != end; ++it) {
		a       = it->var();
		isEq    = a != prg.getRootId(a);
		oldHash+= hashLit(*it);
		if (isEq) {
			prg.getAtom(a)->removeDep(id(), !it->sign()); // remove old edge
			*it = prg.getAtom(a)->eqGoal(it->sign());     // replace with eq goal
			a   = it->var();                              // and check it
		}
		Literal aLit = it->sign() ? ~prg.getAtom(a)->literal() : prg.getAtom(a)->literal();
		ValueRep v   = prg.getAtom(a)->value();
		mark         = strong || prg.getAtom(a)->hasVar();
		if (strong && !prg.getAtom(a)->hasVar()) {
			v = value_false;
		}
		if (v == value_weak_true && it->sign()) {
			v = value_true;
		}
		if (v == value_true || v == value_false) { // truth value is known - remove subgoal
			if (v == trueValue(*it)) {
				// subgoal is true: decrease necessary lower bound
				bound -= weight(uint32(it - lits));
			}
			prg.getAtom(a)->removeDep(id(), !it->sign());
		}
		else if (!mark || !ctx.marked(aLit)) {
			if (mark) { ctx.mark(aLit); }
			if (isEq) { // remember to add edge for new goal later
				todo.addToBody(Literal(*it));
				++todos;
			}
			*j++ = *it;  // copy literal and optionally weight
			if (jw) { *jw++ = weight(uint32(it - lits)); }
		}
		else { // body contains aLit more than once
			if (type() != Body_t::Normal) { // merge subgoal
				if (!jw) {
					SumData* sum = SumData::create(size(), this->bound(), this->sumW());
					std::fill_n(sum->weights, size(), w = 1);
					aggData().sum = sum;
					type_ = Body_t::Sum;
					jw    = sum->weights + (it - lits);
				}
				else { w = weight(uint32(it - lits)); }
				uint32 pos = findLit(prg, aLit);
				sumData()->weights[pos] += w;
			}
			else { // ignore if normal
				--bound;
				if (!isEq) { // remove edge
					if (todo.inBody(*it)) { todo.clearBody(*it); --todos; }
					else                  { prg.getAtom(it->var())->removeDep(id(), !it->sign()); }
				}
			}
		}
	}
	// unmark atoms, compute new hash value,
	// and restore pos | neg partition in case
	// we changed some positive goals to negative ones
	size_          = j - lits;
	if (jw) jw     = sumData()->weights;
	uint32 newHash = 0;
	weight_t sumW  = 0, reachW = 0;
	for (uint32 p = 0, n = size_, i, h; p < n;) {
		if      (!lits[p].sign())      { h = hashLit(lits[i = p++]);  }
		else if (lits[n-1].sign())     { h = hashLit(lits[i = --n]);  }
		else /* restore pos|neg order */ {
			std::swap(lits[p], lits[n-1]);
			if (jw) { std::swap(jw[p], jw[n-1]); }
			continue;
		}
		a = lits[i].var();
		if (todos && todo.inBody(lits[i])) {
			prg.getAtom(a)->addDep(id(), !lits[i].sign());
			todo.clearBody(lits[i]);
			--todos;
		}
		Var v   = prg.getAtom(a)->var();
		w       = !jw ? 1 : jw[i];
		sumW   += w;
		reachW += w;
		if (ctx.marked(posLit(v)) && ctx.marked(negLit(v))) {
			// body contains aLit and ~aLit
			if  (!hasWeights()) { reachW -= 1; }
			else {
				Literal other = prg.getAtom(a)->literal() ^ !goal(i).sign();
				uint32 pos    = findLit(prg, other);
				assert(pos != varMax && pos != i);
				reachW       -= std::min(w, jw[pos]);
			}
		}
		ctx.unmark( v );
		newHash += h;
	}
	bool ok = normalize(prg, bound, sumW, reachW, newHash);
	if (ok) {
		Var xId = id();
		if (oldHash != newHash) {
			xId = prg.update(this, oldHash, newHash);
		}
		if (eqId) { *eqId = xId != varMax ? xId : id(); }
	}
	if (strong) sBody_ = 0;
	return ok && (value() == value_free || propagateValue(prg));
}

bool PrgBody::normalize(const LogicProgram& prg, weight_t bound, weight_t sumW, weight_t reachW, uint32& hashOut) {
	Body_t nt = (sumW == bound || size() == 1) ? Body_t::Normal : type();
	bool ok = true;
	if (sumW >= bound && type() != Body_t::Normal) {
		if (hasWeights()) {
			sumData()->bound = bound;
			sumData()->sumW  = sumW;
		}
		else {
			aggData().bound = bound;
		}
	}
	if (bound <= 0) {
		for (uint32 i = 0, myId = id(); i != size_; ++i) {
			prg.getAtom(goal(i).var())->removeDep(myId, !goal(i).sign());
		}
		size_ = 0; hashOut = 0, unsupp_ = 0;
		nt    = Body_t::Normal;
		ok    = assignValue(value_true);
	}
	else if (reachW < bound) {
		ok     = assignValue(value_false);
		sHead_ = 1;
		markRemoved();
	}
	if (nt != type()) {
		assert(nt == Body_t::Normal);
		Literal* from = aggData().lits;
		if (hasWeights()) {
			sumData()->destroy();
		}
		Literal* to = (new (data_)Norm())->lits;
		std::copy(from, from+size(), to);
		type_ = nt;
	}
	return ok;
}

// Marks the set of heads in rs and removes
// any duplicate heads.
void PrgBody::prepareSimplifyHeads(LogicProgram& prg, AtomState& rs) {
	head_iterator end = heads_end();
	uint32 size       = 0;
	for (PrgEdge*  j  = const_cast<PrgEdge*>(heads_begin()); j != end;) {
		if (!rs.inHead(*j)) {
			rs.addToHead(*j);
			++j; ++size;
		}
		else {
			prg.getHead(*j)->markDirty();
			*j = *--end;
		}
	}
	if (isSmallHead()) { head_ = size; }
	else               { shrinkVecTo(*largeHead(), size); }
}

// Simplifies the heads of this body wrt target.
// Removes superfluous/eq/unsupported heads and checks for self-blocking
// situations.
// PRE: prepareSimplifyHeads was called
bool PrgBody::simplifyHeadsImpl(LogicProgram& prg, PrgBody& target, AtomState& rs, bool strong) {
	PrgHead* cur;
	PrgEdge* j     = const_cast<PrgEdge*>(heads_begin());
	uint32 newSize = 0;
	bool merge     = this != &target;
	bool block     = value() == value_false || (merge && target.value() == value_false);
	for (head_iterator it = heads_begin(), end = heads_end(); it != end; ++it) {
		cur  = prg.getHead(*it);
		block= block || target.blockedHead(*it, rs);
		if (!cur->relevant() || (strong && !cur->hasVar())
			|| block || target.superfluousHead(prg, cur, *it, rs) || cur->value() == value_false) {
			// remove superfluous and unsupported heads
			cur->removeSupport(PrgEdge::newEdge(*this, it->type()));
			rs.clearHead(*it);
			block = block || (cur->value() == value_false && it->type() == PrgEdge::Normal);
		}
		else {
			*j++ = *it;
			++newSize;
			if (merge) { target.addHead(cur, it->type()); }
		}
	}
	if (isSmallHead()) { head_ = newSize; }
	else               { shrinkVecTo(*largeHead(), newSize); }
	return !block;
}

bool PrgBody::simplifyHeads(LogicProgram& prg, bool strong) {
	if (sHead_ == 0) { return true; }
	return PrgBody::mergeHeads(prg, *this, strong);
}

bool PrgBody::mergeHeads(LogicProgram& prg, PrgBody& heads, bool strong, bool simplify) {
	AtomState& rs = prg.atomState();
	bool       ok = true;
	assert((this == &heads || heads.sHead_ == 0) && "Heads to merge not simplified!");
	if (simplify || &heads == this) {
		// mark the body literals so that we can easily detect superfluous atoms
		// and selfblocking situations.
		for (const Literal* it = goals_begin(), *end = it + size(); it != end; ++it) {
			rs.addToBody(*it);
		}
		// remove duplicate/superfluous heads & check for blocked atoms
		prepareSimplifyHeads(prg, rs);
		if (this == &heads) {
			ok = simplifyHeadsImpl(prg, *this, rs, strong);
		}
		else {
			heads.prepareSimplifyHeads(prg, rs);
			if (!simplifyHeadsImpl(prg, *this, rs, strong) && !assignValue(value_false)) {
				clearRule(rs);
				return false;
			}
			ok = heads.simplifyHeadsImpl(prg, *this, rs, strong);
			if (!ok && (!heads.assignValue(value_false) || !heads.propagateValue(prg, false))) {
				clearRule(rs);
				return false;
			}
		}
		// clear temporary flags & reestablish ordering
		std::sort(const_cast<PrgEdge*>(heads_begin()), const_cast<PrgEdge*>(heads_end()));
		clearRule(rs);
		sHead_ = 0;
	}
	else if (relevant()) {
		for (head_iterator it = heads.heads_begin(), end = heads.heads_end(); it != end; ++it) {
			PrgHead* h = prg.getHead(*it);
			if (h->relevant()) { addHead(h, it->type()); }
		}
	}
	return ok || (assignValue(value_false) && propagateValue(prg));
}

// Checks whether the head is superfluous w.r.t this body, i.e.
//  - is needed to satisfy the body
//  - it appears in the body and is a choice
//  - it is a disjunction and one of the atoms is needed to satisfy the body
bool PrgBody::superfluousHead(const LogicProgram& prg, const PrgHead* head, PrgEdge it, const AtomState& rs) const {
	if (it.isAtom()) {
		// the head is an atom
		uint32 atomId = it.node();
		weight_t    w = 1;
		if (rs.inBody(posLit(atomId))) {
			if (hasWeights()) {
				const Literal* lits = aggData().lits;
				const Literal* x    = std::find(lits, lits + size(), posLit(atomId));
				assert(x != lits + size());
				w = sumData()->weights[ x - lits ];
			}
			if (it.isChoice() || (sumW() - w) < bound()) {
				return true;
			}
		}
		return it.isChoice() && (rs.inBody(negLit(atomId)) || rs.inHead(atomId));
	}
	else { assert(it.isDisj());
		// check each contained atom
		const PrgDisj* dis = static_cast<const PrgDisj*>(head);
		for (PrgDisj::atom_iterator aIt = dis->begin(), aEnd = dis->end(); aIt != aEnd; ++aIt) {
			if (rs.inBody(posLit(*aIt)) || rs.inHead(*aIt)) {
				return true;
			}
			if (prg.isFact(prg.getAtom(*aIt))) {
				return true;
			}
		}
		// check for subsumption
		if (prg.options().iters == LogicProgram::AspOptions::MAX_EQ_ITERS) {
			for (head_iterator hIt = heads_begin(), hEnd = heads_end(); hIt != hEnd; ++hIt) {
				if (hIt->isDisj() && prg.getDisj(hIt->node())->size() < dis->size()) {
					const PrgDisj* other = prg.getDisj(hIt->node());
					for (PrgDisj::atom_iterator a = other->begin(), aEnd = other->end(); a != aEnd && other; ++a) {
						if (std::find(dis->begin(), dis->end(), *a) == dis->end()) {
							other = 0;
						}
					}
					if (other && other->size() > 0) {
						return true;
					}
				}
			}
		}
	}
	return false;
}

// Checks whether the rule it.node() :- *this is selfblocking, i.e.
// from TB follows conflict
bool PrgBody::blockedHead(PrgEdge it, const AtomState& rs) const {
	if (it.isAtom() && it.isNormal() && rs.inBody(negLit(it.node()))) {
		weight_t w = 1;
		if (hasWeights()) {
			const Literal* lits = aggData().lits;
			const Literal* x    = std::find(lits, lits + size(), negLit(it.node()));
			assert(x != lits + size());
			w = sumData()->weights[ x - lits ];
		}
		return (sumW() - w) < bound();
	}
	return false;
}

void PrgBody::assignVar(LogicProgram& prg) {
	if (hasVar() || !relevant()) { return; }
	uint32 size = this->size();
	if (size == 0 || value() == value_true) {
		setLiteral(lit_true());
	}
	else if (size == 1 && prg.getAtom(goal(0).var())->hasVar()) {
		Literal x = prg.getAtom(goal(0).var())->literal();
		setLiteral(goal(0).sign() ? ~x : x);
		prg.ctx()->setVarEq(var(), true);
		prg.incEqs(Var_t::Hybrid);
	}
	else if (value() != value_false) {
		setLiteral(posLit(prg.ctx()->addVar(Var_t::Body, 0)));
	}
	else {
		setLiteral(lit_false());
	}
}

bool PrgBody::propagateSupported(Var v) {
	weight_t w = 1;
	if (hasWeights()) {
		uint32 pos = (uint32)std::distance(goals_begin(), std::find(goals_begin(), goals_end(), posLit(v)));
		w          = weight(pos);
	}
	return (unsupp_ -= w) <= 0;
}

bool PrgBody::propagateAssigned(LogicProgram& prg, Literal p, ValueRep v) {
	if (!relevant()) return true;
	assert(std::find(goals_begin(), goals_end(), p) != goals_end());
	markDirty();
	ValueRep x = v == value_weak_true ? value_true : v;
	weight_t w = 1; // TODO: find weight of p for weight rule
	if (x == falseValue(p) && (sumW() - w) < bound() && value() != value_false) {
		return assignValue(value_false) && propagateValue(prg);
	}
	else if (x == trueValue(p) && (bound() - w) <= 0 && value() != value_weak_true) {
		return assignValue(value_weak_true) && propagateValue(prg);
	}
	return true;
}

bool PrgBody::propagateAssigned(LogicProgram& prg, PrgHead* h, EdgeType t) {
	if (!relevant()) return true;
	markHeadsDirty();
	if (h->value() == value_false && hasHead(h, t) && t == PrgEdge::Normal) {
		return value() == value_false || (assignValue(value_false) && propagateValue(prg));
	}
	return true;
}

bool PrgBody::propagateValue(LogicProgram& prg, bool backprop) {
	ValueRep val = value();
	assert(value() != value_free);
	// propagate value forward
	for (head_iterator h = heads_begin(), end = heads_end(); h != end; ++h) {
		PrgHead* head = prg.getHead(*h);
		PrgEdge  supp = PrgEdge::newEdge(*this, h->type());
		if (val == value_false) {
			head->removeSupport(supp);
		}
		else if (!h->isChoice() && head->value() != val && !prg.assignValue(head, val, supp)) {
			return false;
		}
	}
	if (val == value_false) { clearHeads(); }
	// propagate value backward
	if (backprop && relevant()) {
		const uint32 W = hasWeights();
		weight_t MAX_W = 1;
		weight_t* wPos = W == 0 ? &MAX_W : sumData()->weights;
		MAX_W          = *std::max_element(wPos, wPos + (size() * W));
		weight_t bound = value()==value_false ? this->bound() : (sumW() - this->bound())+1;
		if (MAX_W >= bound) {
			ValueRep goalVal;
			for (const Literal* it = goals_begin(), *end = goals_end(); it != end; ++it) {
				if ((bound - *wPos) <= 0) {
					if (!it->sign()) { goalVal = val; }
					else             { goalVal = val == value_false ? value_weak_true : value_false; }
					if (!prg.assignValue(prg.getAtom(it->var()), goalVal, PrgEdge::noEdge())) {
						return false;
					}
				}
				wPos += W;
			}
		}
	}
	return true;
}
bool PrgBody::propagateValue(LogicProgram& prg) {
	return propagateValue(prg, prg.options().backprop != 0);
}

// Adds nogoods for the tableau-rules FFB and BTB as well as FTB, BFB.
// For normal bodies, clauses are used, i.e:
//   FFB and BTB:
//     - a binary clause [~b s] for every positive subgoal of b
//     - a binary clause [~b ~n] for every negative subgoal of b
//   FTB and BFB:
//     - a clause [b ~s1...~sn n1...nn] where si is a positive and ni a negative subgoal
// For count/sum bodies, a weight constraint is created
bool PrgBody::addConstraints(const LogicProgram& prg, ClauseCreator& gc) {
	if (type() == Body_t::Normal) {
		bool    taut= false;
		Literal negB= ~literal();
		gc.start().add(literal());
		for (const Literal* it = goals_begin(), *end = goals_end(); it != end; ++it) {
			Literal li = prg.getAtom(it->var())->literal() ^ it->sign();
			if (li == literal()) { taut = true; continue; }
			if (!prg.ctx()->addBinary(negB, li)) { // [~B li]
				return false;
			}
			if (li.var() != negB.var()) { gc.add(~li); }  // [B v ~l1 v ... v ~ln]
		}
		return taut || gc.end();
	}
	WeightLitVec lits;
	for (uint32 i = 0, end = size_; i != end; ++i) {
		Literal eq = prg.getAtom(goal(i).var())->literal() ^ goal(i).sign();
		lits.push_back(WeightLiteral(eq, weight(i)));
	}
	return WeightConstraint::create(*prg.ctx()->master(), literal(), lits, bound()).ok();
}

// Returns the SCC of body B, i.e.
// - scc if exist atom a in B.heads(), a' in B+, s.th. a.scc == a'.scc
// - noScc otherwise
uint32 PrgBody::scc(const LogicProgram& prg) const {
	uint64 sccMask = 0;
	uint32 end     = size();
	uint32 scc     = PrgNode::noScc;
	bool   large   = false;
	for (uint32 i  = 0; i != end; ++i) {
		if      (goal(i).sign()) {
			end = i;
			break;
		}
		else if ((scc = prg.getAtom(goal(i).var())->scc()) != PrgNode::noScc) {
			sccMask |= uint64(1) << (scc & 63);
			large   |= scc > 63;
		}
	}
	if (sccMask != 0) {
		PrgDisj::atom_iterator aIt = 0, aEnd = 0;
		Var head;
		for (head_iterator h = heads_begin(), hEnd = heads_end(); h != hEnd; ++h) {
			if (h->isAtom()) { head = h->node(); aIt = &head, aEnd = aIt + 1; }
			else             { PrgDisj* d = prg.getDisj(h->node()); aIt = d->begin(), aEnd = d->end(); }
			for (; aIt != aEnd; ++aIt) {
				scc = prg.getAtom(*aIt)->scc();
				if (scc != PrgNode::noScc && (sccMask & (uint64(1) << (scc&63))) != 0) {
					if (!large) { return scc; }
					for (uint32 j = 0; j != end; ++j) {
						if (scc == prg.getAtom(goal(j).var())->scc()) { return scc; }
					}
				}
			}
		}
	}
	return PrgNode::noScc;
}

/////////////////////////////////////////////////////////////////////////////////////////
// class PrgDisj
//
// Head of a disjunctive rule
/////////////////////////////////////////////////////////////////////////////////////////
PrgDisj* PrgDisj::create(uint32 id, const Potassco::AtomSpan& head) {
	void* m = ::operator new(sizeof(PrgDisj) + (Potassco::size(head)*sizeof(Var)));
	return new (m) PrgDisj(id, head);
}

PrgDisj::PrgDisj(uint32 id, const Potassco::AtomSpan& head) : PrgHead(id, PrgNode::Disj, (uint32)Potassco::size(head)) {
	std::copy(Potassco::begin(head), Potassco::end(head), atoms_);
	std::sort(atoms_, atoms_+size());
}
PrgDisj::~PrgDisj() {}
void PrgDisj::destroy() {
	this->~PrgDisj();
	::operator delete(this);
}

void PrgDisj::detach(LogicProgram& prg, bool full) {
	PrgEdge edge = PrgEdge::newEdge(*this, PrgEdge::Choice);
	for (atom_iterator it = begin(), end = this->end(); it != end; ++it) {
		prg.getAtom(*it)->removeSupport(edge);
	}
	EdgeVec temp; temp.swap(supports_);
	for (PrgDisj::sup_iterator it = temp.begin(), end = temp.end(); it != end; ++it) {
		prg.getBody(it->node())->removeHead(this, PrgEdge::Normal);
	}
	if (full) {
		clearSupports();
		markRemoved();
	}
	else {
		supports_.swap(temp);
	}
}

bool PrgDisj::propagateAssigned(LogicProgram& prg, PrgHead* head, EdgeType t) {
	assert(head->isAtom() && t == PrgEdge::Choice);
	if (prg.isFact(static_cast<PrgAtom*>(head)) || head->value() == value_false) {
		atom_iterator it = std::find(begin(), end(), head->id());
		if (it != end()) {
			if      (head->value() == value_true) { detach(prg); }
			else if (head->value() == value_false){
				head->removeSupport(PrgEdge::newEdge(*this, t));
				std::copy(it+1, end(), const_cast<Var*>(it));
				if (--data_ == 1) {
					PrgAtom* last = prg.getAtom(*begin());
					EdgeVec temp;
					clearSupports(temp);
					for (EdgeVec::const_iterator eIt = temp.begin(), eEnd = temp.end(); eIt != eEnd; ++eIt) {
						prg.getBody(eIt->node())->removeHead(this, PrgEdge::Normal);
						prg.getBody(eIt->node())->addHead(last, PrgEdge::Normal);
					}
					detach(prg);
				}
			}
		}
	}
	return true;
}
} }
