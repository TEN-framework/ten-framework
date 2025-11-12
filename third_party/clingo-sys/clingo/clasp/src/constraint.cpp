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

#include <clasp/constraint.h>
namespace Clasp {
/////////////////////////////////////////////////////////////////////////////////////////
// Constraint
/////////////////////////////////////////////////////////////////////////////////////////
Constraint::Constraint()                  {}
Constraint::~Constraint()                 {}
void Constraint::destroy(Solver*, bool)   { delete this; }
ConstraintType Constraint::type() const   { return Constraint_t::Static; }
bool Constraint::simplify(Solver&, bool)  { return false; }
void Constraint::undoLevel(Solver&)       {}
uint32 Constraint::estimateComplexity(const Solver&) const { return 1;  }
bool Constraint::valid(Solver&)           { return true; }
ClauseHead* Constraint::clause()          { return 0; }
void Constraint::decreaseActivity()       {}
void Constraint::resetActivity()          {}
ConstraintScore Constraint::activity() const { return makeScore(); }
bool Constraint::locked(const Solver&) const { return true; }
uint32 Constraint::isOpen(const Solver&, const TypeSet&, LitVec&) { return 0; }
/////////////////////////////////////////////////////////////////////////////////////////
// PostPropagator
/////////////////////////////////////////////////////////////////////////////////////////
PostPropagator::PostPropagator() : next(0)           {}
PostPropagator::~PostPropagator()                    {}
bool PostPropagator::init(Solver&)                   { return true; }
void PostPropagator::reset()                         {}
bool PostPropagator::isModel(Solver& s)              { return valid(s); }
void PostPropagator::reason(Solver&, Literal, LitVec&) {}
Constraint::PropResult PostPropagator::propagate(Solver&, Literal, uint32&) {
	return PropResult(true, false);
}
void PostPropagator::cancelPropagation() {
	for (PostPropagator* n = this->next; n; n = n->next) { n->reset(); }
}
MessageHandler::MessageHandler() {}
/////////////////////////////////////////////////////////////////////////////////////////
// PropagatorList
/////////////////////////////////////////////////////////////////////////////////////////
PropagatorList::PropagatorList() : head_(0) {}
PropagatorList::~PropagatorList() { clear(); }
void PropagatorList::clear() {
	for (PostPropagator* r = head_; r;) {
		PostPropagator* t = r;
		r = r->next;
		t->destroy();
	}
	head_ = 0;
}
void PropagatorList::add(PostPropagator* p) {
	POTASSCO_REQUIRE(p && p->next == 0, "Invalid post propagator");
	uint32 prio = p->priority();
	for (PostPropagator** r = head(), *x;; r = &x->next) {
		if ((x = *r) == 0 || prio < (uint32)x->priority()) {
			p->next = x;
			*r      = p;
			break;
		}
	}
}
void PropagatorList::remove(PostPropagator* p) {
	POTASSCO_REQUIRE(p, "Invalid post propagator");
	for (PostPropagator** r = head(), *x; *r; r = &x->next) {
		if ((x = *r) == p) {
			*r      = x->next;
			p->next = 0;
			break;
		}
	}
}
PostPropagator* PropagatorList::find(uint32 prio) const {
	for (PostPropagator* x = head_; x; x = x->next) {
		uint32 xp = x->priority();
		if (xp >= prio) { return xp == prio ? x : 0; }
	}
	return 0;
}
}
