//
// Copyright (c) 2010-2017 Benjamin Kaufmann
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
#include <clasp/unfounded_check.h>
#include <clasp/clause.h>
#include <algorithm>
#include <cmath>
namespace Clasp {
/////////////////////////////////////////////////////////////////////////////////////////
// DefaultUnfoundedCheck - Construction/Destruction
/////////////////////////////////////////////////////////////////////////////////////////
// Choice rules are handled like normal rules with one exception:
//  Since BFA does not apply to choice rules, we manually trigger the removal of source pointers
//  whenever an atom sourced by a choice rule becomes false.
//
// The major problems with card/weight-rules are:
//  1. subgoals can circularly depend on the body
//  2. subgoal false -> body false, does not hold
//
// Regarding the first point, consider: {b}. a:- 1{a,b}.
// Since b is external to 1{a,b}, the body is a valid source for a. Therefore, 1{a,b} can source a.
// After a's source pointer is set to 1{a,b}, both subgoals of 1{a,b} have a source. Nevertheless,
// we must not count a because it (circularly) depends on the body. I.e. as soon as b
// becomes false, a is unfounded, because the loop {a} no longer has external bodies.
//
// The second point means that we have to watch *all* subgoals because we
// may need to trigger source pointer removal whenever one of the subgoals becomes false.
// Consider: {a,b,c}. t :- 2 {b,t,x}. x :- t,c. x :- a.
// Now assume {t,c} is true and a becomes false. In this case, both x and t lose their
// source pointer and we get the (conflicting) unfounded set {x, t}.
// Further assume that after some backtracking we have that both {t,c} and a
// become false. Therefore x is false, too. Since we do not update source pointers on
// conflicts, x and t still have no source. Thus no removal of source pointers is triggered.
// If we would not watch x in 2 {b,t,x}, we could not add t to the todo queue and
// we would therefore miss the unfounded set {t}.
//
// The implementation for extended bodies works as follows:
// - It distinguishes between internal literals, those that are in the same SCC as B
//   and external literals, those that are not.
// - Let W(l) be the weight of literal l in B and W(WS) be the sum of W(l) for each l in a set WS.
// - The goal is to maintain a set WS of literals l, s.th l in Body and hasSource(l) AND W(WS) >= bound.
// - Initially WS contains all non-false external literals of B.
// - Whenever one of the internal literals of B becomes sourced, it is added to WS
//   *only if* W(WS) < bound. In that case, it is guaranteed that the literal
//   currently does not circularly depend on the body.
// - As soon as W(WS) >= bound, we declare the body as valid source for its heads.
// - Whenever one of the literals l in WS loses its source, it is removed from WS.
//   If l is an external literal, new valid external literals are searched and added to WS
//   until the source condition holds again.
// - If the condition cannot be restored, the body is marked as invalid source.

DefaultUnfoundedCheck::DefaultUnfoundedCheck(DependencyGraph& g, ReasonStrategy st)
	: solver_(0)
	, graph_(&g)
	, mini_(0)
	, reasons_(0)
	, strategy_(st) {
	mini_.release();
}
DefaultUnfoundedCheck::~DefaultUnfoundedCheck() {
	for (ExtVec::size_type i = 0; i != extended_.size(); ++i) {
		::operator delete(extended_[i]);
	}
	delete [] reasons_;
}
/////////////////////////////////////////////////////////////////////////////////////////
// DefaultUnfoundedCheck - Initialization
/////////////////////////////////////////////////////////////////////////////////////////
void DefaultUnfoundedCheck::addWatch(Literal p, uint32 data, WatchType type) {
	assert(data < varMax);
	solver_->addWatch(p, this, static_cast<uint32>((data << 2) | type));
}

void DefaultUnfoundedCheck::setReasonStrategy(ReasonStrategy rs) {
	strategy_ = rs;
	if (strategy_ == only_reason && solver_ && !reasons_) {
		reasons_ = new LitVec[solver_->numVars()];
	}
}
// inits unfounded set checker with graph, i.e.
// - creates data objects for bodies and atoms
// - adds necessary watches to the solver
// - initializes source pointers
bool DefaultUnfoundedCheck::init(Solver& s) {
	assert(!solver_ || solver_ == &s);
	delete[] reasons_;
	reasons_ = 0;
	solver_  = &s;
	setReasonStrategy(s.searchMode() != SolverStrategies::no_learning ? strategy_ : no_reason);
	// process any leftovers from previous steps
	while (findUfs(s, false) != ufs_none) {
		while (!ufs_.empty()) {
			if (!s.force(~graph_->getAtom(ufs_.front()).lit, 0)) { return false; }
			atoms_[ufs_.pop_ret()].ufs = 0;
		}
	}
	AtomVec::size_type startAtom = atoms_.size();
	// set up new atoms
	atoms_.resize(graph_->numAtoms());
	AtomData& sentinel = atoms_[DependencyGraph::sentinel_atom];
	sentinel.resurrectSource();
	sentinel.todo = 1;
	sentinel.ufs  = 1;
	// set up new bodies
	for (uint32 i = (uint32)bodies_.size(); i != graph_->numBodies(); ++i) {
		bodies_.push_back(BodyData());
		BodyPtr n(getBody(i));
		if (!n.node->extended()) {
			initBody(n);
		}
		else {
			initExtBody(n);
		}
		// when a body becomes false, it can no longer be used as source
		addWatch(~n.node->lit, n.id, watch_source_false);
	}
	// check for initially unfounded atoms
	propagateSource();
	for (AtomVec::size_type i = startAtom, end = atoms_.size(); i != end; ++i) {
		const AtomNode& a = graph_->getAtom(NodeId(i));
		if (!atoms_[i].hasSource() && !solver_->force(~a.lit, 0)) {
			return false;
		}
		if (a.inChoice()) {
			addWatch(~a.lit, NodeId(i), watch_head_false);
		}
	}
	if (graph_->numNonHcfs() != 0) {
		mini_ = new MinimalityCheck(s.searchConfig().fwdCheck);
		if (const uint32 sd = mini_->fwd.signDef) {
			for (AtomVec::size_type i = startAtom, end = atoms_.size(); i != end; ++i) {
				const AtomNode& a = graph_->getAtom(NodeId(i));
				if (a.inDisjunctive() && solver_->value(a.lit.var()) == value_free) {
					ValueRep v = falseValue(a.lit);
					if (sd == SolverStrategies::sign_pos || (sd == SolverStrategies::sign_rnd && (i & 1) != 0)) {
						v ^= static_cast<ValueRep>(3u);
					}
					solver_->setPref(a.lit.var(), ValueSet::def_value, v);
				}
			}
		}
	}
	return true;
}

// initializes a "normal" body, i.e. a body where lower(B) == size(B)
void DefaultUnfoundedCheck::initBody(const BodyPtr& n) {
	assert(n.id < bodies_.size());
	BodyData& data = bodies_[n.id];
	// initialize lower to the number of predecessors from same scc that currently
	// have no source. Once lower is 0, the body can source successors in its scc
	data.lower_or_ext  = n.node->num_preds();
	initSuccessors(n, data.lower_or_ext);
}

// initializes an "extended" body, i.e. a count/sum
// creates & populates WS and adds watches to all subgoals
void DefaultUnfoundedCheck::initExtBody(const BodyPtr& n) {
	assert(n.id < bodies_.size() && n.node->extended());
	BodyData& data = bodies_[n.id];
	uint32 preds   = n.node->num_preds();
	ExtData* extra = new (::operator new(sizeof(ExtData) + (ExtData::flagSize(preds)*sizeof(uint32)))) ExtData(n.node->ext_bound(), preds);

	InitExtWatches addWatches = { this, &n, extra };
	graph_->visitBodyLiterals(*n.node, addWatches);

	data.lower_or_ext = (uint32)extended_.size();
	extended_.push_back(extra);
	initSuccessors(n, extra->lower);
}

// set n as source for its heads if possible and necessary
void DefaultUnfoundedCheck::initSuccessors(const BodyPtr& n, weight_t lower) {
	if (!solver_->isFalse(n.node->lit)) {
		for (const NodeId* x = n.node->heads_begin(); x != n.node->heads_end(); ++x) {
			const AtomNode& a = graph_->getAtom(*x);
			if (a.scc != n.node->scc || lower <= 0) {
				setSource(*x, n);
			}
		}
	}
}

// watches needed to implement extended rules
void DefaultUnfoundedCheck::addExtWatch(Literal p, const BodyPtr& B, uint32 data) {
	addWatch(p, static_cast<uint32>(watches_.size()), watch_subgoal_false);
	ExtWatch w = { B.id, data };
	watches_.push_back(w);
}
/////////////////////////////////////////////////////////////////////////////////////////
// DefaultUnfoundedCheck - Base interface
/////////////////////////////////////////////////////////////////////////////////////////
void DefaultUnfoundedCheck::reason(Solver&, Literal p, LitVec& r) {
	LitVec::const_iterator it, end;
	if (!activeClause_.empty() && activeClause_[0] == p) {
		it  = activeClause_.begin()+1;
		end = activeClause_.end();
	}
	else {
		assert(strategy_ == only_reason && reasons_);
		it  = reasons_[p.var()-1].begin();
		end = reasons_[p.var()-1].end();
	}
	for (; it != end; ++it) r.push_back( ~*it );
}

bool DefaultUnfoundedCheck::propagateFixpoint(Solver& s, PostPropagator* ctx) {
	bool checkMin = ctx == 0 && mini_.get() && mini_->partialCheck(s.decisionLevel());
	for (UfsType t; (t = findUfs(s, checkMin)) != ufs_none; ) {
		if (!falsifyUfs(t)) {
			resetTodo();
			return false;
		}
	}
	return true;
}

void DefaultUnfoundedCheck::reset() {
	assert(loopAtoms_.empty() && sourceQ_.empty() && ufs_.empty() && todo_.empty());
	// remember assignments from top-level -
	// the reset may come from a stop request and we might
	// want to continue later
	if (solver_->decisionLevel()) {
		invalidQ_.clear();
	}
}
bool DefaultUnfoundedCheck::valid(Solver& s) {
	if (!mini_.get() || findNonHcfUfs(s) == ufs_none) { return true; }
	falsifyUfs(ufs_non_poly);
	return false;
}
bool DefaultUnfoundedCheck::isModel(Solver& s) {
	return DefaultUnfoundedCheck::valid(s);
}
bool DefaultUnfoundedCheck::simplify(Solver& s, bool) {
	graph_->simplify(s);
	if (mini_.get()) { mini_->scc = 0; }
	return false;
}
void DefaultUnfoundedCheck::destroy(Solver* s, bool detach) {
	if (s && detach) {
		s->removePost(this);
		for (uint32 i = 0; i != (uint32)bodies_.size(); ++i) {
			BodyPtr n(getBody(i));
			s->removeWatch(~n.node->lit, this);
			if (n.node->extended()) {
				RemExtWatches remW = { static_cast<Constraint*>(this), s };
				graph_->visitBodyLiterals(*n.node, remW);
			}
		}
		for (AtomVec::size_type i = 0, end = atoms_.size(); i != end; ++i) {
			const AtomNode& a = graph_->getAtom(NodeId(i));
			if (a.inChoice()) { s->removeWatch(~a.lit, this); }
		}
	}
	PostPropagator::destroy(s, detach);
}
/////////////////////////////////////////////////////////////////////////////////////////
// DefaultUnfoundedCheck - source pointer propagation
/////////////////////////////////////////////////////////////////////////////////////////
// propagates recently set source pointers within one strong component.
void DefaultUnfoundedCheck::propagateSource() {
	for (LitVec::size_type i = 0; i < sourceQ_.size(); ++i) {
		NodeId atom = sourceQ_[i];
		if (atoms_[atom].hasSource()) {
			// propagate a newly added source-pointer
			graph_->getAtom(atom).visitSuccessors(AddSource(this));
		}
		else {
			graph_->getAtom(atom).visitSuccessors(RemoveSource(this));
		}
	}
	sourceQ_.clear();
}

// replaces current source of atom with n
void DefaultUnfoundedCheck::updateSource(AtomData& atom, const BodyPtr& n) {
	if (atom.watch() != AtomData::nill_source) {
		--bodies_[atom.watch()].watches;
	}
	atom.setSource(n.id);
	++bodies_[n.id].watches;
}

// an atom in extended body n has a new source, check if n is now a valid source
void DefaultUnfoundedCheck::AddSource::operator()(NodeId bodyId, uint32 idx) const {
	BodyPtr n(self->getBody(bodyId));
	ExtData* ext = self->extended_[self->bodies_[bodyId].lower_or_ext];
	if (ext->lower > 0 || self->bodies_[n.id].watches == 0) {
		// currently not a source - safely add pred to our watch set
		ext->addToWs(idx, n.node->pred_weight(idx, false));
	}
	if (!self->solver_->isFalse(n.node->lit) && ext->lower <= 0) {
		// valid source - propagate to heads
		self->forwardSource(n);
	}
}
// an atom in extended body n has lost its source, check if n is no longer a valid source
void DefaultUnfoundedCheck::RemoveSource::operator()(NodeId bodyId, uint32 idx) const {
	BodyPtr n(self->getBody(bodyId));
	ExtData* ext = self->extended_[self->bodies_[bodyId].lower_or_ext];
	ext->removeFromWs(idx, n.node->pred_weight(idx, false));
	if (ext->lower > 0 && self->bodies_[n.id].watches > 0) {
		// extended bodies don't always become false if a predecessor becomes false
		// eagerly enqueue all successors watching this body
		self->forwardUnsource(n, true);
	}
}

// n is a valid source again, forward propagate this information to its heads
void DefaultUnfoundedCheck::forwardSource(const BodyPtr& n) {
	for (const NodeId* x = n.node->heads_begin(); x != n.node->heads_end(); ++x) {
		setSource(*x, n);
	}
}

// n is no longer a valid source, forward propagate this information to its heads
void DefaultUnfoundedCheck::forwardUnsource(const BodyPtr& n, bool add) {
	for (const NodeId* x = n.node->heads_begin(); x != n.node->heads_end() && graph_->getAtom(*x).scc == n.node->scc; ++x) {
		if (atoms_[*x].hasSource() && atoms_[*x].watch() == n.id) {
			atoms_[*x].markSourceInvalid();
			sourceQ_.push_back(*x);
		}
		if (add && atoms_[*x].watch() == n.id) {
			pushTodo(*x);
		}
	}
}

// sets body as source for head if necessary.
// PRE: value(body) != value_false
// POST: source(head) != 0
void DefaultUnfoundedCheck::setSource(NodeId head, const BodyPtr& body) {
	assert(!solver_->isFalse(body.node->lit));
	// For normal rules from not false B follows not false head, but
	// for choice rules this is not the case. Therefore, the
	// check for isFalse(head) is needed so that we do not inadvertantly
	// source a head that is currently false.
	if (!atoms_[head].hasSource() && !solver_->isFalse(graph_->getAtom(head).lit)) {
		updateSource(atoms_[head], body);
		sourceQ_.push_back(head);
	}
}

// This function is called for each body that became invalid during propagation.
// Heads having the body as source have their source invalidated and are added
// to the todo queue. Furthermore, source pointer removal is propagated forward
void DefaultUnfoundedCheck::removeSource(NodeId bodyId) {
	const BodyNode& body = graph_->getBody(bodyId);
	for (const NodeId* x = body.heads_begin(); x != body.heads_end(); ++x) {
		if (atoms_[*x].watch() == bodyId) {
			if (atoms_[*x].hasSource()) {
				atoms_[*x].markSourceInvalid();
				sourceQ_.push_back(*x);
			}
			pushTodo(*x);
		}
	}
	propagateSource();
}

/////////////////////////////////////////////////////////////////////////////////////////
// DefaultUnfoundedCheck - Finding & propagating unfounded sets
/////////////////////////////////////////////////////////////////////////////////////////
void DefaultUnfoundedCheck::updateAssignment(Solver& s) {
	assert(sourceQ_.empty() && ufs_.empty() && pickedExt_.empty());
	for (VarVec::const_iterator it = invalidQ_.begin(), end = invalidQ_.end(); it != end; ++it) {
		uint32 index = (*it) >> 2;
		uint32 type  = (*it) & 3u;
		if (type == watch_source_false) {
			// a body became false - update atoms having the body as source
			removeSource(index);
		}
		else if (type == watch_head_false) {
			// an atom in the head of a choice rule became false
			// normally head false -> body false and hence the head has its source autmatically removed
			// for choice rules we must force source removal explicity
			if (atoms_[index].hasSource() && !s.isFalse(graph_->getBody(atoms_[index].watch()).lit)) {
				atoms_[index].markSourceInvalid();
				graph_->getAtom(index).visitSuccessors(RemoveSource(this, true));
				propagateSource();
			}
		}
		else if (type == watch_head_true) {
			// TODO: approx. ufs for disjunctive lp
		}
		else if (type == watch_subgoal_false) { // a literal p relevant to an extended body became false
			assert(index < watches_.size());
			const ExtWatch& w    = watches_[index];
			const BodyNode& body = graph_->getBody(w.bodyId);
			ExtData*        ext  = extended_[bodies_[w.bodyId].lower_or_ext];
			ext->removeFromWs(w.data>>1, body.pred_weight(w.data>>1, test_bit(w.data, 0) != 0));
			if (ext->lower > 0 && bodies_[w.bodyId].watches && !bodies_[w.bodyId].picked && !s.isFalse(body.lit)) {
				// The body is not a valid source but at least one head atom
				// still depends on it: mark body as invalid source
				removeSource(w.bodyId);
				pickedExt_.push_back(w.bodyId);
				bodies_[w.bodyId].picked = 1;
			}
		}
	}
	for (VarVec::size_type i = 0, end = pickedExt_.size(); i != end; ++i) {
		bodies_[pickedExt_[i]].picked = 0;
	}
	pickedExt_.clear();
	invalidQ_.clear();
}

DefaultUnfoundedCheck::UfsType DefaultUnfoundedCheck::findUfs(Solver& s, bool checkMin) {
	// first: remove all sources that were recently falsified
	updateAssignment(s);
	// second: try to re-establish sources.
	while (!todo_.empty()) {
		NodeId head       = todo_.pop_ret();
		atoms_[head].todo = 0;
		if (!atoms_[head].hasSource() && !s.isFalse(graph_->getAtom(head).lit) && !findSource(head)) {
			return ufs_poly;  // found an unfounded set - contained in unfounded_
		}
		assert(sourceQ_.empty());
	}
	todo_.clear();
	return !checkMin ? ufs_none : findNonHcfUfs(s);
}

// searches a new source for the atom node head.
// If a new source is found the function returns true.
// Otherwise the function returns false and unfounded_ contains head
// as well as atoms with no source that circularly depend on head.
bool DefaultUnfoundedCheck::findSource(NodeId headId) {
	assert(ufs_.empty() && invalidQ_.empty());
	const NodeId* bodyIt, *bodyEnd;
	uint32 newSource = 0;
	pushUfs(headId); // unfounded, unless we find a new source
	while (!ufs_.empty()) {
		headId         = ufs_.pop_ret(); // still marked and in vector!
		AtomData& head = atoms_[headId];
		if (!head.hasSource()) {
			const AtomNode& headNode = graph_->getAtom(headId);
			for (bodyIt = headNode.bodies_begin(), bodyEnd = headNode.bodies_end(); bodyIt != bodyEnd; ++bodyIt) {
				BodyPtr bodyNode(getBody(*bodyIt));
				if (!solver_->isFalse(bodyNode.node->lit)) {
					if (bodyNode.node->scc != headNode.scc || isValidSource(bodyNode)) {
						head.ufs = 0;               // found a new source,
						setSource(headId, bodyNode);// set the new source
						propagateSource();          // and propagate it forward
						++newSource;
						break;
					}
					else { addUnsourced(bodyNode); }
				}
			}
			if (!head.hasSource()) { // still no source - check again once we are done
				invalidQ_.push_back(headId);
			}
		}
		else {  // head has a source and is thus not unfounded
			++newSource;
			head.ufs = 0;
		}
	} // while unfounded_.emtpy() == false
	ufs_.rewind();
	if (newSource != 0) {
		// some atoms in unfounded_ have a new source
		// clear queue and check possible candidates for atoms that are still unfounded
		uint32 visited = sizeVec(ufs_.vec);
		ufs_.clear();
		if (visited != newSource) {
			// add elements that are still unfounded
			for (VarVec::iterator it = invalidQ_.begin(), end = invalidQ_.end(); it != end; ++it) {
				if ( (atoms_[*it].ufs = (1u - atoms_[*it].validS)) == 1 ) { ufs_.push(*it); }
			}
		}
	}
	invalidQ_.clear();
	return ufs_.empty();
}

// checks whether the body can source its heads
bool DefaultUnfoundedCheck::isValidSource(const BodyPtr& n) {
	if (!n.node->extended()) {
		return bodies_[n.id].lower_or_ext == 0;
	}
	ExtData* ext = extended_[bodies_[n.id].lower_or_ext];
	if (ext->lower > 0) {
		// Since n is currently not a source,
		// we here know that no literal with a source can depend on this body.
		// Hence, we can safely add all those literals to WS.

		// We check all internal literals here because there may be atoms
		// that were sourced *after* we established the watch set.
		const uint32 inc = n.node->pred_inc();
		const NodeId* x  = n.node->preds();
		uint32       p   = 0;
		for (; *x != idMax; x += inc, ++p) {
			if (atoms_[*x].hasSource() && !ext->inWs(p) && !solver_->isFalse(graph_->getAtom(*x).lit)) {
				ext->addToWs(p, n.node->pred_weight(p, false));
			}
		}
		// We check all external literals here because we do not update
		// the body on backtracking. Therefore some external literals that were false
		// may now be true/free.
		for (++x; *x != idMax; x += inc, ++p) {
			if (!solver_->isFalse(Literal::fromRep(*x)) && !ext->inWs(p)) {
				ext->addToWs(p, n.node->pred_weight(p, true));
			}
		}
	}
	return ext->lower <= 0;
}

// enqueues all predecessors of this body that currently lack a source
// PRE: isValidSource(n) == false
void DefaultUnfoundedCheck::addUnsourced(const BodyPtr& n) {
	const uint32 inc = n.node->pred_inc();
	for (const NodeId* x = n.node->preds(); *x != idMax; x += inc) {
		if (!atoms_[*x].hasSource() && !solver_->isFalse(graph_->getAtom(*x).lit)) {
			pushUfs(*x);
		}
	}
}

// falsifies the atoms one by one from the unfounded set stored in unfounded_
bool DefaultUnfoundedCheck::falsifyUfs(UfsType t) {
	activeClause_.clear();
	for (uint32 rDL = 0; !ufs_.empty(); ) {
		Literal a = graph_->getAtom(ufs_.front()).lit;
		if (!solver_->isFalse(a) && !(assertAtom(a, t) && solver_->propagateUntil(this))) {
			if (t == ufs_non_poly) {
				mini_->schedNext(solver_->decisionLevel(), false);
			}
			break;
		}
		assert(solver_->isFalse(a));
		atoms_[ufs_.pop_ret()].ufs = 0;
		if      (ufs_.qFront == 1)               { rDL = solver_->decisionLevel(); }
		else if (solver_->decisionLevel() != rDL){ break; /* atoms may no longer be unfounded after backtracking */ }
	}
	if (!loopAtoms_.empty()) {
		createLoopFormula();
	}
	resetUfs();
	activeClause_.clear();
	return !solver_->hasConflict();
}

// asserts an unfounded atom using the selected reason strategy
bool DefaultUnfoundedCheck::assertAtom(Literal a, UfsType t) {
	if (solver_->isTrue(a) || strategy_ == distinct_reason || activeClause_.empty()) {
		// Conflict, first atom of unfounded set, or distinct reason for each atom requested -
		// compute reason for a being unfounded.
		// We must flush any not yet created loop formula here - the
		// atoms in loopAtoms_ depend on the current reason which is about to be replaced.
		if (!loopAtoms_.empty()) {
			createLoopFormula();
		}
		activeClause_.assign(1, ~a);
		computeReason(t);
	}
	activeClause_[0] = ~a;
	bool tainted = info_.tagged() || info_.aux();
	bool noClause = solver_->isTrue(a) || strategy_ == no_reason || strategy_ == only_reason || (strategy_ == shared_reason && activeClause_.size() > 3 && !tainted);
	if (noClause) {
		if (!solver_->force(~a, this))  { return false; }
		if (strategy_ == only_reason)   { reasons_[a.var()-1].assign(activeClause_.begin()+1, activeClause_.end()); }
		else if (strategy_ != no_reason){ loopAtoms_.push_back(~a); }
		return true;
	}
	else { // learn nogood and assert ~a
		return ClauseCreator::create(*solver_, activeClause_, ClauseCreator::clause_no_prepare, info_).ok();
	}
}
void DefaultUnfoundedCheck::createLoopFormula() {
	assert(activeClause_.size() > 3 && !loopAtoms_.empty());
	Antecedent ante;
	activeClause_[0] = loopAtoms_[0];
	if (loopAtoms_.size() == 1) {
		ante = ClauseCreator::create(*solver_, activeClause_, ClauseCreator::clause_no_prepare, info_).local;
	}
	else {
		assert(!info_.tagged() && !info_.aux());
		ante = LoopFormula::newLoopFormula(*solver_
			, ClauseRep::prepared(&activeClause_[0], (uint32)activeClause_.size(), info_)
			, &loopAtoms_[0], (uint32)loopAtoms_.size());
		solver_->addLearnt(ante.constraint(), uint32(loopAtoms_.size() + activeClause_.size()), Constraint_t::Loop);
	}
	do {
		Literal p = loopAtoms_.back();
		assert(solver_->isTrue(p));
		if (solver_->reason(p).asUint() != ante.asUint()) {
			assert(solver_->reason(p) == this);
			solver_->setReason(p, ante);
		}
		loopAtoms_.pop_back();
	} while (!loopAtoms_.empty());
}

// computes the reason why a set of atoms is unfounded
void DefaultUnfoundedCheck::computeReason(UfsType t) {
	if (strategy_ == no_reason) { return; }
	uint32 ufsScc = graph_->getAtom(ufs_.front()).scc;
	for (VarVec::size_type i = ufs_.qFront; i != ufs_.vec.size(); ++i) {
		const AtomNode& atom = graph_->getAtom(ufs_.vec[i]);
		if (!solver_->isFalse(atom.lit)) {
			assert(atom.scc == ufsScc);
			for (const NodeId* x = atom.bodies_begin(); x != atom.bodies_end(); ++x) {
				BodyPtr body(getBody(*x));
				if (t == ufs_poly || !body.node->delta()){ addIfReason(body, ufsScc); }
				else                                     { addDeltaReason(body, ufsScc); }
			}
		}
	}
	for (VarVec::size_type i = 0; i != pickedExt_.size(); ++i) { bodies_[pickedExt_[i]].picked = 0; }
	pickedExt_.clear();
	info_     = ConstraintInfo(Constraint_t::Loop);
	uint32 rc = !solver_->isFalse(activeClause_[0]) && activeClause_.size() > 100 && activeClause_.size() > (solver_->decisionLevel() * 10);
	uint32 dl = solver_->finalizeConflictClause(activeClause_, info_, rc);
	uint32 cDL= solver_->decisionLevel();
	assert((t == ufs_non_poly || dl == cDL) && "Loop nogood must contain a literal from current DL!");
	if (dl < cDL && solver_->isUndoLevel()) {
		// cancel active propagation on cDL
		cancelPropagation();
		invalidQ_.clear();
		solver_->undoUntil(dl);
	}
}
// check whether n is external to the current unfounded set, i.e.
// does not depend on the atoms from the unfounded set
bool DefaultUnfoundedCheck::isExternal(const BodyPtr& n, weight_t& S) const {
#define IN_UFS(id) ( (atoms_[(id)].ufs) && !solver_->isFalse(graph_->getAtom((id)).lit) )
	if (!n.node->sum()) {
		for (const NodeId* x = n.node->preds(); *x != idMax && S >= 0; ++x) {
			if (IN_UFS(*x)) { S -= 1; }
		}
	}
	else {
		for (const NodeId* x = n.node->preds(); *x != idMax && S >= 0; x+=2) {
			if (IN_UFS(*x)) { S -= static_cast<weight_t>(x[1]); }
		}
	}
	return S >= 0;
#undef IN_UFS
}

// check if n is part of the reason for the current unfounded set
void DefaultUnfoundedCheck::addIfReason(const BodyPtr& n, uint32 uScc) {
	bool inF = solver_->isFalse(n.node->lit);
	weight_t S;
	if (!n.node->extended() || n.node->scc != uScc) {
		if (inF && !solver_->seen(n.node->lit) && (n.node->scc != uScc || isExternal(n, (S=0)))) {
			addReasonLit(n.node->lit);
		}
	}
	else if (bodies_[n.id].picked == 0) {
		if (isExternal(n, S = extended_[bodies_[n.id].lower_or_ext]->slack)) {
			AddReasonLit addFalseLits = { this, n.node, S };
			if (inF) { addReasonLit(n.node->lit); }
			else     { graph_->visitBodyLiterals(*n.node, addFalseLits); }
		}
		bodies_[n.id].picked = 1;
		pickedExt_.push_back(n.id);
	}
}

void DefaultUnfoundedCheck::addDeltaReason(const BodyPtr& n, uint32 uScc) {
	if (bodies_[n.id].picked != 0) return;
	uint32 bodyAbst = solver_->isFalse(n.node->lit) ? solver_->level(n.node->lit.var()) : solver_->decisionLevel() + 1;
	for (const NodeId* x = n.node->heads_begin(); x != n.node->heads_end(); ++x) {
		if (*x != DependencyGraph::sentinel_atom) { // normal head
			if (graph_->getAtom(*x).scc == uScc) {
				addIfReason(n, uScc);
			}
			continue;
		}
		else { // disjunctive head
			uint32  reasonAbst= bodyAbst;
			Literal reasonLit = n.node->lit;
			bool    inUfs     = false;
			Literal aLit;
			for (++x; *x; ++x) {
				if (atoms_[*x].ufs == 1) {
					inUfs = true;
				}
				else if (solver_->isTrue(aLit = graph_->getAtom(*x).lit) && solver_->level(aLit.var()) < reasonAbst) {
					reasonLit  = ~aLit;
					reasonAbst = solver_->level(reasonLit.var());
				}
			}
			if (inUfs && reasonAbst && reasonAbst <= solver_->decisionLevel()) {
				addReasonLit(reasonLit);
			}
		}
	}
	bodies_[n.id].picked = 1;
	pickedExt_.push_back(n.id);
}

void DefaultUnfoundedCheck::addReasonLit(Literal p) {
	if (!solver_->seen(p)) {
		solver_->markSeen(p);
		solver_->markLevel(solver_->level(p.var()));
		activeClause_.push_back(p);
		if (solver_->level(p.var()) > solver_->level(activeClause_[1].var())) {
			std::swap(activeClause_[1], activeClause_.back());
		}
	}
}

/////////////////////////////////////////////////////////////////////////////////////////
// DefaultUnfoundedCheck - Minimality check for disjunctive logic programs
/////////////////////////////////////////////////////////////////////////////////////////
DefaultUnfoundedCheck::UfsType DefaultUnfoundedCheck::findNonHcfUfs(Solver& s) {
	assert(invalidQ_.empty() && loopAtoms_.empty());
	typedef DependencyGraph::NonHcfIter HccIter;
	HccIter hIt  = graph_->nonHcfBegin() + mini_->scc;
	HccIter hEnd = graph_->nonHcfEnd();
	for (uint32 checks = graph_->numNonHcfs(); checks--;) {
		s.stats.addTest(s.numFreeVars() != 0);
		(*hIt)->assumptionsFromAssignment(s, loopAtoms_);
		if (!(*hIt)->test(s, loopAtoms_, invalidQ_) || s.hasConflict()) {
			uint32 pos = 0, minDL = UINT32_MAX;
			for (VarVec::const_iterator it = invalidQ_.begin(), end = invalidQ_.end(); it != end; ++it) {
				if (s.isTrue(graph_->getAtom(*it).lit) && s.level(graph_->getAtom(*it).lit.var()) < minDL) {
					minDL = s.level(graph_->getAtom(*it).lit.var());
					pos   = (uint32)ufs_.vec.size();
				}
				pushUfs(*it);
			}
			if (pos) {
				std::swap(ufs_.vec.front(), ufs_.vec[pos]);
			}
			invalidQ_.clear();
			loopAtoms_.clear();
			mini_->scc = static_cast<uint32>(hIt - graph_->nonHcfBegin());
			return ufs_non_poly;
		}
		if (++hIt == hEnd) { hIt = graph_->nonHcfBegin(); }
		loopAtoms_.clear();
	}
	mini_->schedNext(s.decisionLevel(), true);
	return ufs_none;
}

DefaultUnfoundedCheck::MinimalityCheck::MinimalityCheck(const FwdCheck& afwd) : fwd(afwd), high(UINT32_MAX), low(0), next(0), scc(0) {
	if (fwd.highPct > 100) { fwd.highPct  = 100; }
	if (fwd.highStep == 0) { fwd.highStep = ~fwd.highStep; }
	high = fwd.highStep;
}

bool DefaultUnfoundedCheck::MinimalityCheck::partialCheck(uint32 level) {
	if (level < low) {
		next -= (low - level);
		low   = level;
	}
	return !next || next == level;
}

void DefaultUnfoundedCheck::MinimalityCheck::schedNext(uint32 level, bool ok) {
	low  = 0;
	next = UINT32_MAX;
	if (!ok) {
		high = level;
		next = 0;
	}
	else if (fwd.highPct != 0) {
		double p = fwd.highPct / 100.0;
		high     = std::max(high, level);
		low      = level;
		if (low >= high) {
			high  += fwd.highStep;
		}
		next     = low + (uint32)std::ceil((high - low) * p);
	}
}
}
