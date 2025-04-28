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
#include <clasp/asp_preprocessor.h>
#include <clasp/logic_program.h>
#include <clasp/shared_context.h>
namespace Clasp { namespace Asp {
/////////////////////////////////////////////////////////////////////////////////////////
// simple preprocessing
//
// Simplifies the program by computing max consequences.
// Then assign variables to non-trivial supported bodies and atoms.
/////////////////////////////////////////////////////////////////////////////////////////
bool Preprocessor::preprocessSimple() {
	if (!prg_->propagate(true)) { return false; }
	uint32 startVar = prg_->ctx()->numVars() + 1;
	// start with initially supported bodies
	VarVec& supported = prg_->getSupportedBodies(true);
	VarVec  unitBodies;
	for (VarVec::size_type i = 0; i != supported.size(); ++i) {
		// set up body
		PrgBody* b = prg_->getBody(supported[i]);
		if (!b->simplify(*prg_, false)) { return false; }
		if (b->var() < startVar) {
			if  (b->size() != 1) { b->assignVar(*prg_); }
			else                 { unitBodies.push_back(supported[i]); }
		}
		// add all heads of b to the "upper"-closure and
		// remove any false/removed atoms from head
		if (!addHeadsToUpper(b) || !b->simplifyHeads(*prg_, true)) {
			return false;
		}
	}
	for (VarVec::const_iterator it = unitBodies.begin(), end = unitBodies.end(); it != end; ++it) {
		prg_->getBody(*it)->assignVar(*prg_);
	}
	return prg_->propagate();
}

bool Preprocessor::addHeadToUpper(PrgHead* head, PrgEdge support) {
	assert(head->relevant() && !head->inUpper());
	head->simplifySupports(*prg_, false);
	head->assignVar(*prg_, support, eq());
	head->clearSupports();
	head->setInUpper(true);
	if (head->isAtom()) {
		return propagateAtomVar(static_cast<PrgAtom*>(head), support);
	}
	// add all unseen atoms of disjunction to upper
	PrgDisj* d = static_cast<PrgDisj*>(head);
	support    = PrgEdge::newEdge(*d, PrgEdge::Choice);
	bool ok    = true;
	for (PrgDisj::atom_iterator it = d->begin(), end = d->end(); it != end && ok; ++it) {
		PrgAtom* at = prg_->getAtom(*it);
		if (!at->relevant()) { continue; }
		if (!at->inUpper())  { ok = addHeadToUpper(at, support); }
		at->addSupport(support);
	}
	return ok;
}
/////////////////////////////////////////////////////////////////////////////////////////
// equivalence preprocessing
//
// Computes max consequences and minimizes the number of necessary variables
// by computing equivalence-classes.
/////////////////////////////////////////////////////////////////////////////////////////
bool Preprocessor::preprocessEq(uint32 maxIters) {
	uint32 startVar = prg_->ctx()->numVars();
	ValueRep res    = value_true;
	pass_           = 0;
	maxPass_        = maxIters;
	HeadRange atoms = HeadRange(prg_->atom_begin() + prg_->startAtom(), prg_->atom_end());
	bodyInfo_.resize( prg_->numBodies() + 1 );
	do {
		if (++pass_ > 1) {
			for (HeadIter it = prg_->atom_begin(), end = atoms.second; it != end; ) {
				while (it != atoms.first){ (*it)->setInUpper(false); ++it; }
				while (it != end)        { (*it)->clearLiteral(false); (*it)->setInUpper(false); ++it; }
			}
			for (HeadIter it = prg_->disj_begin(), end = prg_->disj_end(); it != end; ++it) {
				(*it)->clearLiteral(false);
				(*it)->setInUpper(false);
			}
			prg_->ctx()->popVars(prg_->ctx()->numVars() - startVar);
			litToNode_.clear();
		}
		VarVec& supported = prg_->getSupportedBodies(true);
		if (!classifyProgram(supported)) { return false; }
		res = simplifyClassifiedProgram(atoms, pass_ != maxPass_, supported);
	} while (res == value_free && pass_ != maxPass_);
	return res != value_false;
}

// Computes necessary equivalence-classes starting from the supported bodies
// of a program.
bool Preprocessor::classifyProgram(const VarVec& supported) {
	Var bodyId; PrgBody* body;
	VarVec::size_type index = 0;
	follow_.clear();
	if (!prg_->propagate(true)) { return false; }
	for (VarVec::size_type i = 0;;) {
		while ( (bodyId = nextBodyId(index)) != varMax ) {
			body = addBodyVar(bodyId);
			if (prg_->hasConflict())    { return false; }
			if (!addHeadsToUpper(body)) { return false; }
		}
		follow_.clear();
		index = 0;
		// select next unclassified supported body
		for (; i < supported.size(); ++i) {
			bodyId  = supported[i];
			body    = prg_->getBody(bodyId);
			if (bodyInfo_[bodyId].bSeen == 0 && body->relevant()) {
				follow_.push_back(bodyId);
				break;
			}
			else if (!body->relevant() && body->hasVar()) {
				body->clearLiteral(false);
			}
		}
		if (follow_.empty()) break;
	}
	return !prg_->hasConflict();
}

ValueRep Preprocessor::simplifyClassifiedProgram(const HeadRange& atoms, bool more, VarVec& supported) {
	ValueRep res = value_true, simp;
	if (!prg_->propagate()) { return value_false; }
	supported.clear();
	// simplify supports
	for (uint32 i = 0; i != prg_->numBodies(); ++i) {
		PrgBody* b = prg_->getBody(i);
		if (bodyInfo_[i].bSeen == 0 || !b->relevant()) {
			// !bodyInfo_[i].bSeen: body is unsupported
			// !b->relevant()     : body is eq to other body or was derived to false
			// In either case, body is no longer relevant and can be ignored.
			b->clearLiteral(true);
			b->markRemoved();
		}
		else if ( (simp = simplifyBody(b, more, supported)) != value_true ) {
			if (simp == value_false) { return simp; }
			res = value_free;
		}
	}
	if (!prg_->propagate()) { return value_false; }
	PrgEdge noSup = PrgEdge::noEdge();
	for (LogicProgram::VarIter it = prg_->unfreeze_begin(), end = prg_->unfreeze_end(); it != end; ++it) {
		PrgAtom* a = prg_->getAtom(*it);
		ValueRep v = a->value();
		if      (!a->simplifySupports(*prg_, true)){ return value_false; }
		else if (!a->inUpper() && v != value_false){
			if (!prg_->assignValue(a, value_false, noSup)){ return value_false; }
			if (more && a->hasDep(PrgAtom::dep_all))      { res = value_free; }
		}
	}
	if (!prg_->propagate()) { return value_false; }
	bool strong = more && res == value_true;
	HeadRange heads[2] = { HeadRange(prg_->disj_begin(), prg_->disj_end()), atoms };
	for (const HeadRange* range = heads, *endR = heads+2; range != endR; ++range) {
		for (HeadIter it = range->first, end = range->second; it != end; ++it) {
			PrgHead* head = *it;
			if ((simp = simplifyHead(head, strong)) != value_true) {
				if      (simp == value_false){ return simp; }
				else if (strong)             { strong = false; res = value_free; }
			}
		}
	}
	if (!prg_->propagate()) { res = value_false; }
	return res;
}

// associates a variable with the body if necessary
PrgBody* Preprocessor::addBodyVar(Var bodyId) {
	// make sure we don't add an irrelevant body
	PrgBody* body = prg_->getBody(bodyId);
	assert((body->isSupported() && !body->eq()) || body->hasVar());
	body->clearLiteral(false);    // clear var in case we are iterating
	bodyInfo_[bodyId].bSeen = 1;  // mark as seen, so we don't classify the body again
	bool   known = bodyInfo_[bodyId].known == body->size();
	uint32 eqId;
	if (!body->simplifyBody(*prg_, known, &eqId) || !body->simplifyHeads(*prg_, false)) {
		prg_->setConflict();
		return body;
	}
	if (superfluous(body)) {
		body->markRemoved();
		return body;
	}
	if (eqId == bodyId) {
		// The body is unique
		body->assignVar(*prg_);
		PrgAtom* aEq = body->size() == 1 ? prg_->getAtom(body->goal(0).var()) : 0;
		if      (!known)                          { body->markDirty(); }
		else if (aEq && aEq->var() == body->var()){
			// Body is equivalent to an atom or its negation
			// Check if the atom is itself equivalent to a body.
			// If so, the body is equivalent to the atom's body.
			PrgBody* r = 0; // possible eq-body
			uint32 rId = varMax;
			if (body->goal(0).sign()) {
				Var dualAtom = getRootAtom(body->literal());
				aEq = dualAtom != varMax ? prg_->getAtom(dualAtom) : 0;
			}
			if (aEq && aEq->supports() && aEq->supps_begin()->isBody()) {
				rId = aEq->supps_begin()->node();
				r   = prg_->getBody(rId);
				if (r && r->var() == aEq->var()) {
					mergeEqBodies(body, rId, false);
				}
			}
		}
	}
	else {
		// body is eq to eq body
		mergeEqBodies(body, eqId, true);
	}
	return body;
}

// Adds all heads of body to the upper closure if not yet present and
// associates variables with the heads if necessary.
// The body b is the supported body that provides a support for the heads.
// RETURN: true if no conflict
// POST  : the addition of atoms to the closure was propagated
bool Preprocessor::addHeadsToUpper(PrgBody* body) {
	PrgHead* head;
	PrgEdge  support;
	bool ok  = !prg_->hasConflict();
	int dirty= 0;
	for (PrgBody::head_iterator it = body->heads_begin(), end = body->heads_end(); it != end && ok; ++it) {
		head   = prg_->getHead(*it);
		support= PrgEdge::newEdge(*body, it->type());
		if (head->relevant() && head->value() != value_false) {
			if (body->value() == value_true && head->isAtom()) {
				// Since b is true, it is always a valid support for head, head can never become unfounded.
				// So ignore it during SCC check and unfounded set computation.
				head->setIgnoreScc(true);
				if (support.isNormal() && head->isAtom()) {
					ok = propagateAtomValue(static_cast<PrgAtom*>(head), value_true, support);
				}
			}
			if (!head->inUpper()) {
				// first time we see this head - assign var...
				ok = addHeadToUpper(head, support);
			}
			else if (head->supports() && head->supps_begin()->isNormal()) {
				PrgEdge source = *head->supps_begin();
				assert(source.isBody());
				if (prg_->getBody(source.node())->var() == body->var()) {
					// Check if we really need a new variable for head.
					head->markDirty();
				}
			}
			head->addSupport(support, PrgHead::no_simplify);
		}
		dirty += (head->eq() || head->value() == value_false);
	}
	if (dirty) {
		// remove eq atoms from head
		prg_->getBody(body->id())->markHeadsDirty();
	}
	return ok;
}

// Propagates that a was added to the "upper"-closure.
// If atom a has a truth-value or is eq to a', we'll remove
// it from all bodies. If there is an atom x, s.th. a.lit == ~x.lit, we mark all
// bodies containing both a and x for simplification in order to detect
// duplicates/contradictory body-literals.
// In case that a == a', we also mark all bodies containing a
// for head simplification in order to detect rules like: a' :- a,B. and a' :- B,not a.
bool Preprocessor::propagateAtomVar(PrgAtom* a, PrgEdge source) {
	const Var aId   = a->id();
	PrgAtom* comp   = 0;
	ValueRep value  = a->value();
	bool fullEq     = eq();
	bool removeAtom = value == value_true || value == value_false;
	bool removeNeg  = removeAtom  || value == value_weak_true;
	Literal aLit    = a->literal();
	if (fullEq) {
		if (getRootAtom(aLit) == varMax) {
			setRootAtom(aLit, aId);
		}
		else if (prg_->mergeEqAtoms(a, getRootAtom(aLit))) {
			assert(source.isBody());
			removeAtom = true;
			removeNeg  = true;
			value      = a->value();
			PrgBody* B = prg_->getBody(source.node());
			a->setEqGoal(posLit(a->id()));
			// set positive eq goal - replace if a == {not a'}, replace a with not a' in bodies
			if (getRootAtom(~aLit) != varMax && B->literal() == aLit && B->size() == 1 && B->goal(0).sign()) {
				a->setEqGoal(negLit(getRootAtom(~aLit)));
			}
			a->clearLiteral(true); // equivalent atoms don't need vars
		}
		else { return false; }
	}
	if (getRootAtom(~aLit) != varMax) {
		PrgAtom* negA = prg_->getAtom(getRootAtom(~aLit));
		assert(aLit == ~negA->literal());
		// propagate any truth-value to complementary eq-class
		ValueRep cv   = value_free;
		uint32   mark = 0;
		if (value != value_free && (cv = (value_false | (value^value_true))) != negA->value()) {
			mark        = 1;
			if (!propagateAtomValue(negA, cv, PrgEdge::noEdge())) {
				return false;
			}
		}
		if ( !removeAtom ) {
			for (PrgAtom::dep_iterator it = (comp=negA)->deps_begin(); it != comp->deps_end(); ++it) {
				bodyInfo_[it->var()].mBody = 1;
				if (mark) { prg_->getBody(it->var())->markDirty(); }
			}
		}
	}
	for (PrgAtom::dep_iterator it = a->deps_begin(), end = a->deps_end(); it != end; ++it) {
		Var bodyId  = it->var();
		PrgBody* bn = prg_->getBody(bodyId);
		if (bn->relevant()) {
			bool wasSup = bn->isSupported();
			bool isSup  = wasSup || (value != value_false && !it->sign() && bn->propagateSupported(aId));
			bool seen   = false;
			bool dirty  = removeAtom || (removeNeg && it->sign());
			if (fullEq) {
				seen      = bodyInfo_[bodyId].bSeen != 0;
				dirty    |= bodyInfo_[bodyId].mBody == 1;
				if (++bodyInfo_[bodyId].known == bn->size() && !seen && isSup) {
					follow_.push_back( bodyId );
					seen    = true;
				}
			}
			if (!seen && isSup && !wasSup) {
				prg_->getSupportedBodies(false).push_back(bodyId);
			}
			if (dirty) {
				bn->markDirty();
				if (a->eq()) {
					bn->markHeadsDirty();
				}
			}
		}
	}
	if      (removeAtom) { a->clearDeps(PrgAtom::dep_all); }
	else if (removeNeg)  { a->clearDeps(PrgAtom::dep_neg); }
	if (comp) {
		for (PrgAtom::dep_iterator it = comp->deps_begin(), end = comp->deps_end(); it != end; ++it) {
			bodyInfo_[it->var()].mBody = 0;
		}
	}
	return true;
}

// Propagates the assignment of val to a.
bool Preprocessor::propagateAtomValue(PrgAtom* atom, ValueRep val, PrgEdge sup) {
	// No backpropagation possible because supports are not yet fully established.
	return prg_->assignValue(atom, val, sup) && prg_->propagate(false);
}

bool Preprocessor::mergeEqBodies(PrgBody* body, Var rootId, bool equalLits) {
	PrgBody* root = prg_->mergeEqBodies(body, rootId, equalLits, false);
	if (root && root != body && bodyInfo_[root->id()].bSeen == 0) {
		// If root is not yet classified, we can ignore body.
		// The heads of body are added to the "upper"-closure
		// once root is eventually classified.
		body->clearHeads();
		body->markRemoved();
	}
	return root != 0;
}

bool Preprocessor::hasRootLiteral(PrgBody* body) const {
	return body->size() >= 1
		&& getRootAtom(body->literal()) == varMax
		&& getRootAtom(~body->literal())== varMax;
}

// Pre: body is simplified!
bool Preprocessor::superfluous(PrgBody* body) const {
	if (!body->relevant()) { return true; }
	if (!body->inRule()) {
		if (body->value() == value_free) { return true; }
		if (body->bound() <= 0)          { return true; }
		if (body->size() == 1)           {
			// unit constraint
			ValueRep exp = body->value() ^ (int)body->goal(0).sign();
			ValueRep got = prg_->getAtom(body->goal(0).var())->value();
			assert(got != value_free || !prg_->options().backprop);
			return got != value_free && (got&value_true) == (exp&value_true);
		}
	}
	return false;
}

// Simplify the classified body with the given id.
// Return:
//  value_false    : conflict
//  value_true     : ok
//  value_weak_true: ok but program should be reclassified
ValueRep Preprocessor::simplifyBody(PrgBody* b, bool reclass, VarVec& supported) {
	assert(b->relevant() && bodyInfo_[b->id()].bSeen == 1);
	bodyInfo_[b->id()].bSeen = 0;
	bodyInfo_[b->id()].known = 0;
	bool   hadHeads = b->hasHeads();
	bool   hasRoot  = hasRootLiteral(b);
	uint32 eqId     = b->id();
	if (!b->simplify(*prg_, true, &eqId)) {
		return value_false;
	}
	ValueRep ret = value_true;
	if (reclass) {
		if (hadHeads && b->value() == value_false) {
			assert(b->hasHeads() == false);
			// New false body. If it was derived to false, we can ignore the body.
			// Otherwise, we have a new integrity constraint.
			if (!b->relevant()) {
				b->clearLiteral(true);
			}
		}
		else if (b->var() != 0 && superfluous(b)) {
			// Body is no longer needed. All heads are either superfluous or equivalent
			// to other atoms.
			// Reclassify only if var is not used
			if (getRootAtom(b->literal()) == varMax) { ret = value_weak_true; }
			b->clearLiteral(true);
			b->markRemoved();
		}
		else if (b->value() == value_true && b->var() != 0) {
			// New fact body
			for (PrgBody::head_iterator it = b->heads_begin(), end = b->heads_end(); it != end; ++it) {
				if (it->isNormal() && prg_->getHead(*it)->var() != 0) {
					ret = value_weak_true;
					break;
				}
			}
			b->markDirty();
		}
	}
	if (b->relevant() && eqId != b->id() && (reclass || prg_->getBody(eqId)->var() == b->var())) {
		// Body is now eq to some other body - reclassify if body var is not needed
		Var bVar = b->var();
		prg_->mergeEqBodies(b, eqId, true, true);
		if (hasRoot && bVar != b->var()) {
			ret = value_weak_true;
		}
	}
	if (b->relevant() && b->resetSupported()) {
		supported.push_back(b->id());
	}
	return ret;
}

// Simplify the classified head h.
// Update list of bodies defining this head and check
// if atom or disjunction has a distinct var although it is eq to some body.
// Return:
//  value_false    : conflict
//  value_true     : ok
//  value_weak_true: ok but atom should be reclassified
ValueRep Preprocessor::simplifyHead(PrgHead* h, bool more) {
	if (!h->hasVar() || !h->relevant()) {
		// unsupported or eq
		h->clearLiteral(false);
		h->markRemoved();
		h->clearSupports();
		h->setInUpper(false);
		return value_true;
	}
	assert(h->inUpper());
	ValueRep v       = h->value();
	ValueRep ret     = value_true;
	PrgEdge support  = h->supports() ? *h->supps_begin() : PrgEdge::noEdge();
	uint32 numSuppLits= 0;
	if (!h->simplifySupports(*prg_, true, &numSuppLits)) {
		return value_false;
	}
	if (v != h->value() && (h->value() == value_false || (h->value() == value_true && h->var() != 0))) {
		ret = value_weak_true;
	}
	if (more) {
		if (numSuppLits == 0 && h->hasVar()) {
			// unsupported head does not need a variable
			ret = value_weak_true;
		}
		else if (h->supports() > 0 && h->supps_begin()->rep != support.rep) {
			// support for head has changed
			ret = value_weak_true;
		}
		else if ((support.isNormal() && h->supports() == 1) || (h->supports() > 1 && numSuppLits == 1 && h->isAtom())) {
			assert(support.isBody());
			PrgBody* supBody = prg_->getBody(support.node());
			if (supBody->literal() != h->literal()) {
				if (h->supports() > 1) {
					// atom is equivalent to one of its bodies
					EdgeVec temp(h->supps_begin(), h->supps_end());
					h->clearSupports();
					support = temp[0];
					for (EdgeIterator it = temp.begin(), end = temp.end(); it != end; ++it) {
						assert(!it->isDisj());
						PrgBody* B = prg_->getBody(it->node());
						if (it->isNormal() && B->size() == 1 && B->goal(0).sign()) {
							support = *it;
						}
						B->removeHead(h, it->type());
					}
					supBody = prg_->getBody(support.node());
					supBody->addHead(h, support.type());
					if (!supBody->simplifyHeads(*prg_, true)) {
						return value_false;
					}
				}
				ret = value_weak_true;
				if (h->value() == value_weak_true || h->value() == value_true) {
					supBody->assignValue(h->value());
					supBody->propagateValue(*prg_, true);
				}
			}
		}
	}
	return ret;
}

} }
